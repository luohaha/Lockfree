#include<iostream>
#include<thread>
#include<vector>
#include <unistd.h>
#include <sys/time.h>
#include<set>

namespace Lf {

#define FETCH_AND_ADD(ptr, val) \
  __atomic_fetch_add(ptr, val, __ATOMIC_SEQ_CST);

#define ADD_AND_FETCH(ptr, val) \
  __atomic_add_fetch(ptr, val, __ATOMIC_SEQ_CST);

#define ATOMIC_CAS(ptr, oldv, newv) \
  __sync_bool_compare_and_swap(ptr, oldv, newv) 

#define ATOMIC_LOAD(ptr) __atomic_load_n(ptr, __ATOMIC_SEQ_CST)
#define ATOMIC_STORE(ptr, val) \
  __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST)

#define REF_MARK 0x7

#define GET_REFCNT(ptr) \
  ((uint64_t)ptr & (uint64_t)REF_MARK)

#define SET_REFCNT(ptr, val) \
  (((uint64_t)ptr & ~(uint64_t)REF_MARK) | val)

uint64_t current_time() {
  timeval curTime;
  gettimeofday(&curTime, NULL);
  uint64_t micro = curTime.tv_sec*(uint64_t)1000000+curTime.tv_usec;
  return micro;
}

#define MAX_THREAD_CNT 2048
static uint64_t global_clock;
// TODO : be thread local
static uint64_t thread_clock[MAX_THREAD_CNT];

static uint32_t thread_id = 0;

class RetireGuard {
 public:
  RetireGuard() {
    tid_ = FETCH_AND_ADD(&thread_id, 1);
    thread_clock[tid_ & (MAX_THREAD_CNT - 1)] = ATOMIC_LOAD(&global_clock);
  }
  ~RetireGuard() {
    thread_clock[tid_ & (MAX_THREAD_CNT -1)] = UINT64_MAX;
  }
 private:
  uint32_t tid_;
};

struct RetireNode {
  void* ptr = nullptr;
  RetireNode* next = nullptr;
};

class RetireStation {
 public:
  RetireStation() {
    global_clock = 1;
    stop_ = false;
    cur_index_ = 0;
    for (int i = 0; i < MAX_THREAD_CNT; i++) {
      thread_clock[i] = UINT64_MAX;
    }
    thread_vec.emplace_back(&RetireStation::RetireWorker, this);
  }
  ~RetireStation() {
    stop_ = true;
    for (auto& th : thread_vec) {
      th.join();
    }
  }
  void Retire(void* ptr) {
    int cur_index = ATOMIC_LOAD(&cur_index_) % 2;
    RetireNode* rn = new RetireNode();
    rn->ptr = ptr;
    do {
      rn->next = ptr_head_[cur_index].next;
    } while (!ATOMIC_CAS(&ptr_head_[cur_index].next, 
			 rn->next, rn));
  }
  void RetireWorker() {
    while (!stop_) {
      sleep(1);
      int cur_index = ATOMIC_LOAD(&cur_index_);
      int next_index = cur_index + 1;
      ATOMIC_STORE(&cur_index_, next_index % 2);
      uint64_t target = FETCH_AND_ADD(&global_clock, 1);
      // wait all thread finish
      while (true) {
	uint64_t min_clock = UINT64_MAX;
	for (int i = 0; i < MAX_THREAD_CNT; i++) {
	  min_clock = std::min(min_clock, thread_clock[i]);
	}
	if (min_clock > target) {
	  // safe to free
	  RetireNode* rn = ptr_head_[cur_index % 2].next;
	  while (rn != nullptr) {
	    RetireNode* to_free = rn;
	    rn = rn->next;
	    free(to_free->ptr);
	    free(to_free);
	    //printf("free %x next_index %d\n", 
	    //to_free, next_index);
	  }
	  break;
	}
	sleep(1);
      } // while (true)
    }
  }

  static RetireStation& get_instance() {
    static RetireStation rs;
    return rs;
  }
 private:
  bool stop_;
  int cur_index_;
  RetireNode ptr_head_[2];
  std::vector<std::thread> thread_vec;
 };

#define RS RetireStation::get_instance()

} // namespace Lf
