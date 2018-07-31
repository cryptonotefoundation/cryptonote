#include <string>
#include <iostream>
#include "common/base58.h"

int main(int argc, char** argv) {

  if (argc > 1) {
    std::string s = argv[1];
    for (uint64_t i = 0; i < (uint64_t)(-1); i++) {
      std::string r = tools::base58::encode_addr(i, "test");
      if (s == r.substr(0, s.size()))
        std::cout << r << " 0x" << std::hex << i << std::dec << std::endl;
    }
  } else {
    for (uint64_t i = 0; i < (uint64_t)(-1); i++) {
      std::string r = tools::base58::encode_addr(i, "test");
      std::cout << r << " 0x" << std::hex << i << std::dec << std::endl;
    }
  }
  return 0;
}
