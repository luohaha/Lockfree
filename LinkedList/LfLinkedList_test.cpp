#include "LfLinkedList.h"
#include<vector>
#include<thread>

using namespace Lf;

LinkedList list;

void RandomSearchTest(int index) {
  for (int i = 0; i < 10000000; i++) {
    Node node;
    Node ret;
    uint32_t r = std::rand();
    node.key = std::to_string(index) + "_" + std::to_string(r);
    node.value = std::to_string(r);
    list.Insert(node);
    assert(list.Search(node, ret) == true);
    assert(node.key == ret.key);
    assert(node.value == ret.value);
    assert(list.Delete(node) == true);
    assert(list.Search(node, ret) == false);
    //std::cout<<std::to_string(r)<<std::endl;
  }
}

void MultiThRandomSearchTest() {
  std::vector<std::thread> thread_vec;
  for (int i = 0; i < 10; i++) {
    thread_vec.emplace_back(&RandomSearchTest, i);
  }
  for (auto& each : thread_vec) {
    each.join();
  }
}

int main() {
  MultiThRandomSearchTest();
  return 0;
}
