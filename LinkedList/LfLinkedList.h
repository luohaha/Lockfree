#include<iostream>
#include<string>
#include "../RetireStation/RetireStation.h"

namespace Lf {

struct Node {
  std::string key;
  std::string value;
  bool operator==(const struct Node& that) {
    return this->key == that.key;
  }
  bool operator>(const struct Node& that) {
    return this->key > that.key;
  }
  bool operator<(const struct Node& that) {
    return this->key < that.key;
  }
  bool operator!=(const struct Node& that) {
    return this->key != that.key;
  }
};

class LinkedNode {
 public:
  LinkedNode() {
    next_ = nullptr;
  }
  ~LinkedNode() {}
  void Insert(LinkedNode* node) {
    
  }
  Node data_;
  LinkedNode* next_;
};

struct FindPair
{
  LinkedNode* prev;
  LinkedNode* curr;
};

#define ATOMIC_LOAD(ptr) __atomic_load_n(ptr, __ATOMIC_SEQ_CST)
#define ATOMIC_STORE(ptr, val) __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST)
#define ATOMIC_CAS(ptr, oldv, newv) __sync_bool_compare_and_swap(ptr, oldv, newv) 
#define IS_DEL(ptr) (((uint64_t)ptr & 0x1) != 0)
#define IS_NOT_DEL(ptr) (((uint64_t)ptr & 0x1) == 0)
#define MARK_NOT_DEL(ptr) ((uint64_t)ptr & (~0x1))
#define MARK_DEL(ptr) ((uint64_t)ptr | 0x1)

class LinkedList {
 public:
  LinkedList() {}
  ~LinkedList() {}
  void Insert(Node& node) {
    RetireGuard guard;
    LinkedNode* newnode = new LinkedNode();
    while (true) {
      FindPair pair;
      while (!FindLarger(node, pair)) {}
      newnode->data_ = node;
      newnode->next_ = pair.curr;
      if (ATOMIC_CAS(&pair.prev->next_, pair.curr, newnode)) {
	return;
      }
    }
  }

  bool Delete(Node& node) {
    RetireGuard guard;
    while (true) {
      FindPair pair;
      while (!FindLarger(node, pair)) {}
      if (pair.curr == nullptr || pair.curr->data_ != node) {
	return false;
      }
      LinkedNode* n_node = ATOMIC_LOAD(&pair.curr->next_);
      if (!ATOMIC_CAS(&pair.curr->next_, 
		      (LinkedNode*)MARK_NOT_DEL(n_node), 
		      (LinkedNode*)MARK_DEL(n_node))) {
	continue;
      }
      if (ATOMIC_CAS(&pair.prev->next_, pair.curr, n_node)) {
	// retire curr
	RS.Retire(pair.curr);
	return true;
      } else {
	// release "lock"
	ATOMIC_STORE(&pair.curr->next_, 
		     (LinkedNode*)MARK_NOT_DEL(n_node));
      }
    }
  }

  bool Search(Node& node, Node& ret) {
    RetireGuard guard;
    FindPair pair;
    while (!FindLarger(node, pair)) {}
    if (pair.curr == nullptr || pair.curr->data_ != node) {
      return false;
    } else {
      ret = pair.curr->data_;
      return true;
    }
  }
  
 private:
  bool FindLarger(Node& node, FindPair& pair) {
    LinkedNode *p = ATOMIC_LOAD(&head_.next_);
    if (IS_DEL(p)) {
      return false;
    }
    pair.prev = &head_;
    pair.curr = p;
    while (p != nullptr && p->data_ < node) {
      pair.prev = p;
      pair.curr = ATOMIC_LOAD(&p->next_);
      p = pair.curr;
      if (IS_DEL(p)) {
	return false;
      }
    }
    return true;
  }

  LinkedNode head_;
};

} // Lf
