#ifndef SJTU_DEQUE_HPP
#define SJTU_DEQUE_HPP

#include "exceptions.hpp"

#include <cstddef>

namespace sjtu {

template <class T> class deque {
private:
static constexpr size_t BLOCK_SIZE = 16;
    static constexpr size_t MAX_BLOCK_SIZE = 256;
    static constexpr size_t MIN_BLOCK_SIZE = 40;
    struct block {
        T* data;
        size_t size;
        block *prev, *next;
        block() :data(nullptr), size(0), prev(nullptr), next(nullptr) {
          data = static_cast<T*>(::operator new(sizeof(T) * MAX_BLOCK_SIZE));
        }
        block(const block& other) : data(nullptr), size(other.size), prev(nullptr), next(nullptr) {
        data = static_cast<T*>(::operator new(sizeof(T) * MAX_BLOCK_SIZE));
        for (size_t i = 0; i < size; ++i) {
          new(data + i) T(other.data[i]);
        }
      }
        ~block() {
          clear();
          ::operator delete(data);
        }
        void clear() {
          for (size_t i = 0; i < size; ++i) {
            data[i].~T();
          }
          size = 0;
        }
        void push_back(const T& value) {
          if (size >= MAX_BLOCK_SIZE) return;
          new(data + size) T(value);
          ++size;
        }
        void push_front(const T& value) {
          if (size >= MAX_BLOCK_SIZE) return;
          for (size_t i = size; i > 0; --i) {
            new(data + i) T(std::move(data[i - 1]));
            data[i - 1].~T();
          }
          new(data) T(value);
          ++size;
        }
        void pop_front() {
          if (size == 0) return;
          data[0].~T();
          for (size_t i = 1; i < size; ++i) {
            new(data + i - 1) T(std::move(data[i]));
            data[i].~T();
          }
          size--;
        }
        void pop_back() {
          if (size == 0) return;
          data[size-1].~T();
          size--;
        }
        void insert(size_t pos, const T& value) {
          if (size > MAX_BLOCK_SIZE || pos > size) return;
          for (size_t i = size; i > pos; --i) {
            new(data + i) T(std::move(data[i - 1]));
            data[i - 1].~T();
          }
          new(data + pos) T(value);
          ++size;
        }
        void erase(size_t pos) {
          if (size == 0 || pos >= size) return;
          data[pos].~T();
          for (size_t i = pos + 1; i < size; ++i) {
            new(data + i - 1) T(std::move(data[i]));
            data[i].~T();
          }
          size--;
        }
    };
    block *head, *tail;
    size_t total_size;
    size_t block_count;
    std::pair<block*, size_t> locate(size_t index) const {
      if (index >= total_size) {
        return {nullptr, 0};
      }
      block* cur = head;
      size_t offset = index;
      while (cur && offset >= cur->size) {
        offset -= cur->size;
        cur = cur->next;
      }
      return {cur, offset};
    }
    void split_block(block* b) {
      if (b->size <= MAX_BLOCK_SIZE) return;
      block* new_block = new block();
      size_t mid = b->size / 2;
      for (size_t i = mid; i < b->size; ++i) {
        new_block->push_back(b->data[i]);
        b->data[i].~T();
      }
      b->size = mid;
      new_block->next = b->next;
      if (new_block->next) {
        new_block->next->prev = new_block;
      }
      b->next = new_block;
      new_block->prev = b;
      block_count++;
    }
    void merge_blocks(block* b) {
      if (!b) return;
      if (b->next && b->size + b->next->size <= MAX_BLOCK_SIZE) {
        block* next_block = b->next;
        for (size_t i = 0; i < next_block->size; ++i) {
          b->push_back(std::move(next_block->data[i]));
        }
        b->next = next_block->next;
        if (next_block->next) {
          next_block->next->prev = b;
        } else {
          tail = b;
        }
        delete next_block;
        block_count--;
        return;
      }
      if (b->prev && b->prev->size + b->size <= MAX_BLOCK_SIZE) {
        merge_blocks(b->prev);
      }
    }
    void rebalance() {
      block* cur = head;
      while (cur) {
        if (cur->size > MAX_BLOCK_SIZE) {
          split_block(cur);
          cur = cur->next;
        } else if (cur->size < MIN_BLOCK_SIZE && block_count > 1) {
          merge_blocks(cur);
          cur = cur->next;
        } else {
          cur = cur->next;
        }
      }
    }
public:
  class const_iterator;
  class iterator {
  private:
    /**
     * add data members.
     * just add whatever you want.
     */
    const deque* container;
    block* current_block;
    size_t offset;
    friend class const_iterator;
    
  public:
    iterator() : container(nullptr), current_block(nullptr), offset(0) {}
    iterator(const deque* c, block* b, size_t o) 
      : container(c), current_block(b), offset(o) {}
    iterator(const iterator& other) = default;
    /**
     * return a new iterator which points to the n-next element.
     * if there are not enough elements, the behaviour is undefined.
     * same for operator-.
     */
    iterator operator+(const int &n) const {
      if(n==0) return *this;
      iterator tmp = *this;
      tmp+=n;
      return tmp;
    }
    iterator operator-(const int &n) const {
      if(n==0) return *this;
      iterator tmp = *this;
      tmp-=n;
      return tmp;
    }

    /**
     * return the distance between two iterators.
     * if they point to different vectors, throw
     * invaild_iterator.
     */
    int operator-(const iterator& rhs) const {
      if (container != rhs.container) {
        throw invalid_iterator();
      }
      int pos1 = 0, pos2 = 0;
      block* cur = container->head;
      while (cur && cur != current_block) {
        pos1 += cur->size;
        cur = cur->next;
      }
      pos1 += offset;
      cur = container->head;
      while (cur && cur != rhs.current_block) {
        pos2 += cur->size;
        cur = cur->next;
      }
      pos2 += rhs.offset;
      return pos1 - pos2;
    }
    iterator &operator+=(const int &n) {
    if (n == 0) return *this;
    if (current_block == nullptr) {
    if (n < 0) {
        if (container->total_size == 0) return *this;
        current_block = container->tail;
        offset = current_block->size - 1;
        return *this += (n + 1);
    } else {
        return *this;
    }
}
    int remaining = n;
    if (remaining < 0) {
        remaining = -remaining;
        while (remaining > 0) {
            if (!current_block) {
                break;
            }
            if (remaining <= static_cast<int>(offset)) {
                offset -= remaining;
                remaining = 0;
            } else {
                remaining -= offset;
                current_block = current_block->prev;
                if (current_block) {
                    offset = current_block->size;
                } else {
                    offset = 0;
                }
            }
        }
        if (current_block && offset == current_block->size) {
    current_block = current_block->next;
    offset = 0;
}
        return *this;
    }
    while (remaining > 0) {
        if (!current_block) {
            break;
        }
        size_t remaining_in_block = current_block->size - offset;
        if (remaining <= static_cast<int>(remaining_in_block)) {
            offset += remaining;
            remaining = 0;
        } else {
            remaining -= remaining_in_block;
            current_block = current_block->next;
            offset = 0;
        }
    }
    if (current_block && offset == current_block->size) {
    current_block = current_block->next;
    offset = 0;
}
    return *this;
}
    iterator &operator-=(const int &n) {
      if (n == 0) return *this;
    if (current_block == nullptr && n > 0) {
        current_block = container->tail;
        if (current_block) {
            offset = current_block->size;
        }
        return *this += (-n);
    }
    if (current_block && offset == current_block->size) {
    current_block = current_block->next;
    offset = 0;
}
    return *this += (-n);
    }

    /**
     * iter++
     */
    iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }
    /**
     * ++iter
     */
    iterator &operator++() {
      if(!current_block) return *this;
      if(offset + 1 < current_block->size) {
        offset++;
      } else {
        current_block = current_block->next;
        offset = 0;
      }
      return *this;
    }
    /**
     * iter--
     */
    iterator operator--(int) {
      iterator tmp = *this;
      --(*this);
      return tmp;
    }
    /**
     * --iter
     */
    iterator &operator--() {
      if (current_block == nullptr) {
        current_block = container->tail;
        if (current_block) {
            offset = current_block->size;
        }
        if (offset > 0) {
            offset--;
        }
        return *this;
    }
      if(offset > 0) {
        offset--;
      } else {
        current_block = current_block->prev;
        if (current_block) {
          offset = current_block->size - 1;
        } else {
          offset = 0;
        }
      }
      return *this;
    }

    /**
     * *it
     */
    T &operator*() const {
      if(!current_block || offset >= current_block->size) {
        throw invalid_iterator();
      }
      return current_block->data[offset];
    }
    /**
     * it->field
     */
    T *operator->() const noexcept {
      return &(current_block->data[offset]);
    }

    /**
     * check whether two iterators are the same (pointing to the same
     * memory).
     */
    bool operator==(const iterator &rhs) const {
      return container == rhs.container && current_block == rhs.current_block && offset == rhs.offset;
    }
    bool operator==(const const_iterator &rhs) const {
      return container == rhs.container && current_block == rhs.current_block && offset == rhs.offset;
    }
    /**
     * some other operator for iterators.
     */
    bool operator!=(const iterator &rhs) const {
      return !(*this == rhs);
    }
    bool operator!=(const const_iterator &rhs) const {
      return !(*this == rhs);
    }
    friend class deque;
  };

  class const_iterator {
    /**
     * it should has similar member method as iterator.
     * you can copy them, but with care!
     * and it should be able to be constructed from an iterator.
     */
  private:
    const deque* container;
    block* current_block;
    size_t offset;
    friend class iterator;
  public:
    const_iterator() : container(nullptr), current_block(nullptr), offset(0) {}
    const_iterator(const deque* c, block* b, size_t o)
      : container(c), current_block(b), offset(o) {}
    const_iterator(const const_iterator& other) = default;
    const_iterator(const iterator& it) : container(it.container), current_block(it.current_block), offset(it.offset) {}
    const_iterator operator+(const int &n) const {
      if(n==0) return *this;
      const_iterator tmp = *this;
      tmp+=n;
      return tmp;
    }
    const_iterator operator-(const int &n) const {
      if(n==0) return *this;
      const_iterator tmp = *this;
      tmp-=n;
      return tmp;
    }
    int operator-(const const_iterator& rhs) const {
      if (container != rhs.container) {
        throw invalid_iterator();
      }
      int pos1 = 0, pos2 = 0;
      block* cur = container->head;
      while (cur && cur != current_block) {
        pos1 += cur->size;
        cur = cur->next;
      }
      pos1 += offset;
      cur = container->head;
      while (cur && cur != rhs.current_block) {
        pos2 += cur->size;
        cur = cur->next;
      }
      pos2 += rhs.offset;
      return pos1 - pos2;
    }
    const_iterator &operator+=(const int &n) {
    if (n == 0) return *this;
    if (current_block == nullptr) {
    if (n < 0) {
        if (container->total_size == 0) return *this;
        current_block = container->tail;
        offset = current_block->size - 1; 
        return *this += (n + 1); 
    } else {
        return *this;
    }
}
    int remaining = n;
    if (remaining < 0) {
        remaining = -remaining;
        while (remaining > 0) {
            if (!current_block) break;
            if (remaining <= static_cast<int>(offset)) {
                offset -= remaining;
                remaining = 0;
            } else {
                remaining -= offset;
                current_block = current_block->prev;
                if (current_block) {
                    offset = current_block->size;
                } else {
                    offset = 0;
                }
            }
        }
        if (current_block && offset == current_block->size) {
            current_block = current_block->next;
            offset = 0;
        }
        return *this;
    }
    while (remaining > 0) {
        if (!current_block) break;
        size_t remaining_in_block = current_block->size - offset;
        if (remaining <= static_cast<int>(remaining_in_block)) {
            offset += remaining;
            remaining = 0;
        } else {
            remaining -= remaining_in_block;
            current_block = current_block->next;
            offset = 0;
        }
    }
    if (current_block && offset == current_block->size) {
        current_block = current_block->next;
        offset = 0;
    }
    return *this;
}
    const_iterator &operator-=(const int &n) {
      if (n == 0) return *this;
    if (current_block == nullptr && n > 0) {
        current_block = container->tail;
        if (current_block) {
            offset = current_block->size;
        }
        return *this += (-n);
    }
    if (current_block && offset == current_block->size) {
    current_block = current_block->next;
    offset = 0;
}
      return *this += (-n);
    }   
    const_iterator operator++(int) {
      const_iterator tmp = *this;
      ++(*this);
      return tmp;
    }
    const_iterator &operator++() {
      if(!current_block) return *this;
      if(offset + 1 < current_block->size) {
        offset++;
      } else {
        current_block = current_block->next;
        offset = 0;
      }
      return *this;
    }
    const_iterator operator--(int) {
      const_iterator tmp = *this;
      --(*this);
      return tmp;
    }
    const_iterator &operator--() {
      if (current_block == nullptr) {
        current_block = container->tail;
        if (current_block) {
            offset = current_block->size;
        }
        if (offset > 0) {
            offset--;
        }
        return *this;
    }
      if(offset > 0) {
        offset--;
      } else {
        current_block = current_block->prev;
        if (current_block) {
          offset = current_block->size - 1;
        } else {
          offset = 0;
        }
      }
      return *this;
    }
    const T &operator*() const {
      if(!current_block || offset >= current_block->size) {
        throw invalid_iterator();
      }
      return current_block->data[offset];
    }
    const T *operator->() const noexcept {
      return &(current_block->data[offset]);
    }
    bool operator==(const iterator &rhs) const {
      return container == rhs.container && current_block == rhs.current_block && offset == rhs.offset;
    }
    bool operator==(const const_iterator &rhs) const {
      return container == rhs.container && current_block == rhs.current_block && offset == rhs.offset;
    }
    bool operator!=(const iterator &rhs) const {
      return !(*this == rhs);
    }
    bool operator!=(const const_iterator &rhs) const {
      return !(*this == rhs);
    }
    friend class deque;
  };


  /**
   * constructors.
   */
  deque():head(nullptr), tail(nullptr), total_size(0),block_count(0) {}
  deque(const deque &other):head(nullptr), tail(nullptr), total_size(0),block_count(0) {
    for (block* cur = other.head; cur; cur = cur->next) {
      block* new_block = new block(*cur);
      if (!head) {
        head = tail = new_block;
      } else {
        tail->next = new_block;
        new_block->prev = tail;
        tail = new_block;
      }
      total_size += new_block->size;
      block_count++;
    }
  }

  /**
   * deconstructor.
   */
  ~deque() {
    clear();
  }

  /**
   * assignment operator.
   */
  deque &operator=(const deque &other) {
    if (this == &other) return *this;
    clear();
    for (block* cur = other.head; cur; cur = cur->next) {
      block* new_block = new block(*cur);
      if (!head) {
        head = tail = new_block;
      } else {
        tail->next = new_block;
        new_block->prev = tail;
        tail = new_block;
      }
      total_size += new_block->size;
      block_count++;
    }
    return *this;
  }

  /**
   * access a specified element with bound checking.
   * throw index_out_of_bound if out of bound.
   */
  T &at(const size_t &pos) {
    if (pos >= total_size) {
      throw index_out_of_bound();
    }
    auto [b, offset] = locate(pos);
    return b->data[offset];
  }
  const T &at(const size_t &pos) const {
    if (pos >= total_size) {
      throw index_out_of_bound();
    }
    auto [b, offset] = locate(pos);
    return b->data[offset];
  }
  T &operator[](const size_t &pos) {
    return at(pos);
  }
  const T &operator[](const size_t &pos) const {
    return at(pos);
  }

  /**
   * access the first element.
   * throw container_is_empty when the container is empty.
   */
  const T &front() const {
    if (empty()) {
      throw container_is_empty();
    }
    return head->data[0];
  }
  /**
   * access the last element.
   * throw container_is_empty when the container is empty.
   */
  const T &back() const {
    if (empty()) {
      throw container_is_empty();
    }
    return tail->data[tail->size - 1];
  }

  /**
   * return an iterator to the beginning.
   */
  iterator begin() {
    return iterator(this, head, 0);
  }
  const_iterator cbegin() const {
    return const_iterator(this, head, 0);
  }

  /**
   * return an iterator to the end.
   */
  iterator end() {
    return iterator(this, nullptr, 0);
  }
  const_iterator cend() const {
    return const_iterator(this, nullptr, 0);
  }

  /**
   * check whether the container is empty.
   */
  bool empty() const {
    return total_size == 0;
  }

  /**
   * return the number of elements.
   */
  size_t size() const {
    return total_size;
  }

  /**
   * clear all contents.
   */
  void clear() {
    block* cur = head;
    while (cur) {
      block* next = cur->next;
      delete cur;
      cur = next;
    }
    head = tail = nullptr;
    total_size = 0;
    block_count = 0;
  }

  /**
   * insert value before pos.
   * return an iterator pointing to the inserted value.
   * throw if the iterator is invalid or it points to a wrong place.
   */
  iterator insert(iterator pos, const T &value) {
    if (pos.container != this) { 
        throw invalid_iterator();
    }
    if(pos==end()) {
      push_back(value);
      return iterator(this, tail, tail->size - 1);
    }
    if(!pos.current_block) {
      throw invalid_iterator();
    }
    block* b = pos.current_block;
    size_t offset = pos.offset;
    if(b->size < MAX_BLOCK_SIZE) {
      b->insert(offset, value);
      total_size++;
      return iterator(this, b, offset);
    } 
    size_t mid = b->size / 2;
    block* new_block = new block();
    for (size_t i = mid; i < b->size; ++i) {
      new_block->push_back(std::move(b->data[i]));
      b->data[i].~T();
    }
    b->size = mid;
    new_block->prev = b;
    new_block->next = b->next;
    if (b->next) {
      b->next->prev = new_block;
    } else {
      tail = new_block;
    }
    b->next = new_block;
    block_count++;
    if (offset <= mid) {
      b->insert(offset, value);
      total_size++;
      return iterator(this, b, offset);
    } else {
      new_block->insert(offset - mid, value);
      total_size++;
      return iterator(this, new_block, offset - mid);
    }
  }

  /**
   * remove the element at pos.
   * return an iterator pointing to the following element. if pos points to
   * the last element, return end(). throw if the container is empty,
   * the iterator is invalid, or it points to a wrong place.
   */
  iterator erase(iterator pos) {
    if (pos.container != this) { 
        throw invalid_iterator();
    }
    if (empty()) {
        throw container_is_empty();
    }
    if (pos == end()) {
        throw invalid_iterator();
    }
    if (!pos.current_block) {
        throw invalid_iterator();
    }
    block* b = pos.current_block;
    size_t offset = pos.offset;
    b->erase(offset);
    total_size--;
    
    iterator next_it = pos;
    if (offset == b->size) {
        next_it.current_block = b->next;
        next_it.offset = 0;
    }
    if (b->size == 0) {
        if (b->prev) {
            b->prev->next = b->next;
        } else {
            head = b->next;
        }
        if (b->next) {
            b->next->prev = b->prev;
        } else {
            tail = b->prev;
        }
        if (next_it.current_block == b) {
            next_it.current_block = b->next;
            next_it.offset = 0;
        }
        delete b;
        block_count--;
    }
    
    return next_it;
}

  /**
   * add an element to the end.
   */
  void push_back(const T &value) {
    if (!tail || tail->size >= MAX_BLOCK_SIZE) {
      block* new_block = new block();
      if(!head){
        head = tail = new_block;
      } else {
        tail->next = new_block;
        new_block->prev = tail;
        tail = new_block;
      }
      block_count++;
    }
    tail->push_back(value);
    total_size++;
  }

  /**
   * remove the last element.
   * throw when the container is empty.
   */
  void pop_back() {
    if (empty()) {
        throw container_is_empty();
    }
    tail->pop_back();
    total_size--;
    
    if (tail->size == 0) {
        block* to_delete = tail;
        tail = tail->prev;
        if (tail) {
            tail->next = nullptr;
        } else {
            head = nullptr;
        }
        delete to_delete;
        block_count--;
    }
}

  /**
   * insert an element to the beginning.
   */
  void push_front(const T &value) {
    if (!head || head->size >= MAX_BLOCK_SIZE) {
      block* new_block = new block();
      if (!head) {
        head = tail = new_block;
      } else {
        head->prev = new_block;
        new_block->next = head;
        head = new_block;
      }
      block_count++;
    }
    head->push_front(value);
    total_size++;
  }

  /**
   * remove the first element.
   * throw when the container is empty.
   */
  void pop_front() {
    if (empty()) {
        throw container_is_empty();
    }
    head->pop_front();
    total_size--;
    
    if (head->size == 0) {
        block* to_delete = head;
        head = head->next;
        if (head) {
            head->prev = nullptr;
        } else {
            tail = nullptr;
        }
        delete to_delete;
        block_count--;
    }
}
};

} // namespace sjtu

#endif
