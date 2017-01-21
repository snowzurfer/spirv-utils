#include <iostream>
#include <fstream>
#include <spv_utils.h>
#include <cstdint>

static void Callback(sut::TokenIterator &itor) {
  if (itor.GetOpcode() == spv::Op::OpCapability) {
    std::cout << "Inside if statement, where itor.GetOpcode() == spv::Op::OpCapability\n";
  }
}

int main() {
  // Read spv binary module from a file
  std::ifstream spv_file("../sample_spv_modules/test.frag.spv", std::ios::binary |
                         std::ios::ate | std::ios::in);
  if (spv_file.is_open()) {
    std::streampos size = spv_file.tellg();

    spv_file.seekg(0, std::ios::beg);
    char *data = new char[size];
    spv_file.read(data, size);
    spv_file.close();

    // Parse the module
    sut::TokenStream stream(data, size);
    if (stream.IsValid()) {
      stream.FilterModule(&Callback);
    }

    delete[] data;
    data = nullptr;
  }

  return 0;
};
