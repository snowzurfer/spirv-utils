#include <iostream>
#include <fstream>
#include <spv_utils.h>
#include <cstdint>

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
    sut::OpcodeStream stream(data, size);
    for (auto &i : stream) {
      if (i.GetOpcode() == spv::Op::OpCapability) {
        uint32_t instruction = 0xDEADBEEF;
        std::vector<uint32_t> longer_instruction = {
          0xFFFFEEEE,
          0xFFFFDDDD,
          0xFFFFFFFF,
          0xDEADBEEF
        };
        uint32_t instruction2 = 0xDEADBEEF;
        i.InsertBefore(longer_instruction.data(), longer_instruction.size());
        i.InsertAfter(&instruction, 1U);
        i.Remove();
      }
    }
    std::vector<uint32_t> filtered_stream = stream.EmitFilteredStream();

    delete[] data;
    data = nullptr;
  }

  return 0;
};
