#include "LfLinkedList.h"

namespace Lf {

  // 2 ^ 20
#define MAX_BUCKET_SIZE 1024

inline uint32_t reverse(uint32_t x)
{
  x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
  x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
  x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
  x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
  return((x >> 16) | (x << 16));
}

class LfHashMap {
 public:
 LfHashMap(uint32_t bs, double threshold) 
   : bucket_size_(bs), threshold_(threshold) {
  for (uint32_t i = 0; i < bs; i++) {
    bucket_[i] = new LinkedNode();
    bucket_[i]->data_.hash_code = reverse(i);
    list_.InsertLinkedNode(bucket_[i]);
  }
  for (uint32_t i = bs; i < MAX_BUCKET_SIZE; i++) {
    bucket_[i] = nullptr;
  }
  }
  ~LfHashMap() {
    for (uint32_t i = 0; i < bucket_size_; i++) {
      delete bucket_[i];
    }
  }

  void Insert(Node& node) {
    const uint32_t mark = Mark();
    uint32_t hc = node.hash_code;
    uint32_t bucket_id = reverse(hc) & mark;
    list_.InsertFrom(GetBucket(bucket_id, mark), node);
    ExpandMap();
  }

  bool Search(Node& node, Node& ret) {
    const uint32_t mark = Mark();
    uint32_t hc = node.hash_code;
    uint32_t bucket_id = reverse(hc) & mark;
    return list_.SearchFrom(GetBucket(bucket_id, mark), node, ret);
  }

  bool Delete(Node& node) {
    const uint32_t mark = Mark();
    uint32_t hc = node.hash_code;
    uint32_t bucket_id = reverse(hc) & mark;
    return list_.DeleteFrom(GetBucket(bucket_id, mark), node);
  }
  
 private:
  uint32_t Mark() {
    return ATOMIC_LOAD(&bucket_size_) - 1;
  }
  LinkedNode* GetBucket(const uint32_t bucket_id, const uint32_t mark) {
    LinkedNode* bn = ATOMIC_LOAD(&bucket_[bucket_id]);
    while (nullptr == bn) {
      LinkedNode* nn = new LinkedNode();
      nn->data_.hash_code = reverse(bucket_id);
      if (ATOMIC_CAS(&bucket_[bucket_id], nullptr, nn)) {
	// if task hung here, it still be lock-free
	list_.InsertLinkedNode(nn);
      } else {
	delete nn;
      }
      bn = ATOMIC_LOAD(&bucket_[bucket_id]);
    }
    assert(bn != nullptr);
    const uint32_t next_mark = ((mark + 1) >> 1) - 1;
    return (bn->next_ == nullptr && bucket_id > 1) 
      ? GetBucket(bucket_id & next_mark, next_mark) : bn;
  }
  void ExpandMap() {
    uint64_t size = list_.Size();
    uint64_t bucket_size = bucket_size_;
    if ((double)size / (double)bucket_size > threshold_ 
	&& 2 * bucket_size < MAX_BUCKET_SIZE) {
      // expand
      ATOMIC_CAS(&bucket_size_, bucket_size, 2 * bucket_size);
    }
  }
  LinkedList list_;
  double threshold_;
  uint32_t bucket_size_;
  LinkedNode* bucket_[MAX_BUCKET_SIZE];
};

}
