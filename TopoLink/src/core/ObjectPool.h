#ifndef OBJECT_POOL_H
#define OBJECT_POOL_H

#include <cstddef>
#include <memory>
#include <unordered_set>
#include <vector>

template <typename T, size_t BlockSize = 4096> class ObjectPool {
public:
  ObjectPool() = default;

  // Disable copy/move to prevent complex ownership issues
  ObjectPool(const ObjectPool &) = delete;
  ObjectPool &operator=(const ObjectPool &) = delete;

  ~ObjectPool() { clear(); }

  void clear() {
    // Identify free slots for fast lookup
    std::unordered_set<T *> freeSet(_freeList.begin(), _freeList.end());

    size_t count = BlockSize / sizeof(T);
    if (count == 0)
      count = 1;

    // Walk all blocks and destruct live (non-free) objects
    for (auto &block : _blocks) {
      char *start = block.get();
      for (size_t i = 0; i < count; ++i) {
        T *ptr = reinterpret_cast<T *>(start + i * sizeof(T));
        if (freeSet.find(ptr) == freeSet.end()) {
          ptr->~T();
        }
      }
    }

    _blocks.clear();
    _freeList.clear();
  }

  template <typename... Args> T *allocate(Args &&...args) {
    if (_freeList.empty()) {
      allocateBlock();
    }

    T *ptr = _freeList.back();
    _freeList.pop_back();

    // Construct in place
    new (ptr) T(std::forward<Args>(args)...);
    return ptr;
  }

  void deallocate(T *ptr) {
    if (!ptr)
      return;
    ptr->~T(); // Call destructor
    _freeList.push_back(ptr);
  }

private:
  void allocateBlock() {
    // Calculate how many objects fit in BlockSize (minimum 1)
    size_t count = BlockSize / sizeof(T);
    if (count == 0)
      count = 1;

    // Allocate a new block of raw memory
    // We use char[] to avoid default construction of T
    std::unique_ptr<char[]> block(new char[count * sizeof(T)]);

    // Add pointers to the free list
    char *start = block.get();
    for (size_t i = 0; i < count; ++i) {
      _freeList.push_back(reinterpret_cast<T *>(start + i * sizeof(T)));
    }

    _blocks.push_back(std::move(block));
  }

  std::vector<std::unique_ptr<char[]>> _blocks;
  std::vector<T *> _freeList;
};

#endif // OBJECT_POOL_H
