#include <iostream>
#include <spv_utils.h>
#include <cstdint>
#include <catch.hpp>
#include <fstream>


TEST_CASE("Factorials are computed", "[factorial]") {

  // Read spv binary module from a file
  std::ifstream spv_file("../sample_spv_modules/test.frag.spv", std::ios::binary |
                         std::ios::ate | std::ios::in);
  REQUIRE(spv_file.is_open() == true);
  std::streampos size = spv_file.tellg();

  spv_file.seekg(0, std::ios::beg);
  char *data = new char[size];
  spv_file.read(data, size);
  spv_file.close();

  // Parse the module
  SECTION("Passing a null ptr for the data throws an exception") {
    sut::OpcodeStream stream(data, size);

  }
  //for (auto &i : stream) {
  //  if (i.GetOpcode() == spv::Op::OpCapability) {
  //    uint32_t instruction = 0xDEADBEEF;
  //    std::vector<uint32_t> longer_instruction = {
  //      0xDEADBEEF,
  //      0xDEADBEEF,
  //      0xDEADBEEF,
  //      0xDEADBEEF
  //    };
  //    std::vector<uint32_t> longer_instruction_2 = {
  //      0x1EADBEEF,
  //      0x1EADBEEF,
  //      0x1EADBEEF,
  //      0x1EADBEEF
  //    };
  //    i.InsertBefore(longer_instruction.data(), longer_instruction.size());
  //    i.InsertAfter(&instruction, 1U);
  //    i.InsertAfter(longer_instruction.data(), longer_instruction.size());
  //    i.InsertAfter(longer_instruction.data(), longer_instruction.size());
  //    i.InsertBefore(longer_instruction_2.data(), longer_instruction_2.size());
  //    i.Remove();
  //  }
  //}

  //std::vector<uint32_t> filtered_stream = stream.EmitFilteredStream();

  //  delete[] data;
  //  data = nullptr;
  //}
  //REQUIRE(Factorial(0) == 1);
  //REQUIRE(Factorial(1) == 1);
  //REQUIRE(Factorial(2) == 2);
  //REQUIRE(Factorial(3) == 6);
  //REQUIRE(Factorial(10) == 3628800);
}