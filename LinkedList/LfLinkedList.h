#include<iostream>
#include<string>
#include "../RetireStation/RetireStation.h"

namespace Lf {

struct Node {
  Node(const std::string& k, const std::string& v) {
    key = k;
    value = v;
    hash_code = std::hash<std::string>{}(key);
  }
  Node() {}
  uint32_t hash_code;
  std::string key;
  std::string value;
  bool operator==(const struct Node& that) {
    return this->hash_code == that.hash_code 
    && this->key == that.key;
  }
  bool operator>(const struct Node& that) {
    if (this->hash_code > that.hash_code) {
      return true;
    } else if (this->hash_code < that.hash_code) {
      return false;
    } else {
      return this->key > that.key;
    }
  }
  bool operator<(const struct Node& that) {
    if (this->hash_code < that.hash_code) {
      return true;
    } else if (this->hash_code > that.hash_code) {
      return false;
    } else {
      return this->key < that.key;
    }
  }
  bool operator!=(const struct Node& that) {
    return this->hash_code != that.hash_code 
      || this->key != that.key;
  }
};

class LinkedNode {
 public:
  LinkedNode() {
    next_ = nullptr;
  }
  ~LinkedNode() {}
  Node data_;
  LinkedNode* next_;
};

struct FindPair
{
  LinkedNode* prev = nullptr;
  LinkedNode* curr = nullptr;
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
  LinkedList() : size_(0) { head_.next_ = nullptr; }
  ~LinkedList() {}
  void Insert(Node& node) {
    return InsertFrom(&head_, node);
  }

  void InsertFrom(LinkedNode* begin_node, Node& node) {
    RetireGuard guard;
    LinkedNode* newnode = new LinkedNode();
    while (true) {
      FindPair pair;
      while (!FindLarger(begin_node, node, pair)) {}
      newnode->data_ = node;
      newnode->next_ = pair.curr;
      if (ATOMIC_CAS(&pair.prev->next_, pair.curr, newnode)) {
	FETCH_AND_ADD(&size_, 1);
	return;
      }
    }
  }

  void InsertLinkedNode(LinkedNode* node) {
    RetireGuard guard;
    while (true) {
      FindPair pair;
      while (!FindLarger(&head_, node->data_, pair)) {}
      node->next_ = pair.curr;
      if (ATOMIC_CAS(&pair.prev->next_, pair.curr, node)) {
	return;
      }
    }
  }

  bool Delete(Node& node) {
    return DeleteFrom(&head_, node);
  }

  bool Search(Node& node, Node& ret) {
    return SearchFrom(&head_, node, ret);
  }

  bool SearchFrom(LinkedNode* begin_node, 
		  Node& node, Node& ret) {
    RetireGuard guard;
    FindPair pair;
    while (!FindLarger(begin_node, node, pair)) {}
    if (pair.curr == nullptr || pair.curr->data_ != node) {
      return false;
    } else {
      ret = pair.curr->data_;
      return true;
    }
  }

  bool DeleteFrom(LinkedNode* begin_node, Node& node) {
    RetireGuard guard;
    while (true) {
      FindPair pair;
      while (!FindLarger(begin_node, node, pair)) {}
      if (pair.curr == nullptr || pair.curr->data_ != node) {
	return false;
      }
      LinkedNode* n_node = ATOMIC_LOAD(&pair.curr->next_);
      if (!ATOMIC_CAS(&pair.curr->next_, 
		      (LinkedNode*)MARK_NOT_DEL(n_node), 
		      (LinkedNode*)MARK_DEL(n_node))) {
	continue;
      } else {
	// lazy delete
	return true;
      }
    }
  }

  uint64_t Size() { return ATOMIC_LOAD(&size_); }
  
 private:
  bool FindLarger(LinkedNode* const begin_node, 
		  Node& node, 
		  FindPair& pair) {
    assert(begin_node != nullptr);
    assert(!IS_DEL(begin_node->next_));
    pair.prev = begin_node;
    pair.curr = begin_node->next_;
    while (pair.curr != nullptr
	   && pair.curr->data_ < node) {
      LinkedNode* p = pair.curr->next_;
      if (IS_DEL(p)) {
	// pair.curr is del
	if (ATOMIC_CAS(&pair.prev->next_, 
		       pair.curr, 
		       (LinkedNode*)MARK_NOT_DEL(p))) {
	  RS.Retire(pair.curr);
	  FETCH_AND_ADD(&size_, -1);
	}
	return false;
      } else {
	pair.prev = pair.curr;
	pair.curr = p;
      }
    }
    if (pair.curr != nullptr  
	&& IS_DEL(pair.curr->next_)) {
      	// pair.curr is del
       if (ATOMIC_CAS(&pair.prev->next_, 
		      pair.curr, 
		      (LinkedNode*)MARK_NOT_DEL(pair.curr->next_))) {
	 RS.Retire(pair.curr);
	 FETCH_AND_ADD(&size_, -1);
       }
       return false;
    }
    return true;
  }

  LinkedNode head_;
  uint64_t size_;
};

} // Lf
