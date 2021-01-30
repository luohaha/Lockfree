#include "LfHashMap.h"
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

using namespace Lf;

void handler(int sig) {
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

void RandomSearchTest(int index, LfHashMap* map) {
  for (int i = 0; i < 10000; i++) {
    Node node;
    Node ret;
    uint32_t r = std::rand();
    node.key = std::to_string(index) + "_" + std::to_string(r);
    node.value = std::to_string(r);
    node.hash_code = std::hash<std::string>{}(node.key);
    map->Insert(node);
    assert(map->Search(node, ret) == true);
    assert(node.key == ret.key);
    assert(node.value == ret.value);
    if (i % 2 == 0) {
      assert(map->Delete(node) == true);
    }
    node.key += "tmp";
    node.hash_code = std::hash<std::string>{}(node.key);
    assert(map->Search(node, ret) == false);
    //std::cout<<std::to_string(r)<<std::endl;
  }
}

void MultiThRandomSearchTest() {
  LfHashMap map(16, 4);
  std::vector<std::thread> thread_vec;
  for (int i = 0; i < 30; i++) {
    thread_vec.emplace_back(&RandomSearchTest, i, &map);
  }
  for (auto& each : thread_vec) {
    each.join();
  }
}

int main() {
  signal(SIGSEGV, handler);
  signal(SIGABRT, handler);
  MultiThRandomSearchTest();
  return 0;
}
