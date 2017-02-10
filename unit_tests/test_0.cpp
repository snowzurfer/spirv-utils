#include <spv_utils.h>
#include <array>
#include <catch.hpp>
#include <cstdint>
#include <fstream>
#include <iostream>

#define STR_EXPAND(str) #str
#define STR(str) STR_EXPAND(str)

TEST_CASE("spv utils is tested with correct spir-v binary",
          "[spv-utils-correct-spvbin]") {
  // Read spv binary module from a file
  std::ifstream spv_file(STR(SPV_ASSETS_FOLDER) "/test.frag.spv",
                         std::ios::binary | std::ios::ate | std::ios::in);
  REQUIRE(spv_file.is_open() == true);
  std::streampos size = spv_file.tellg();
  CHECK(size > 0);

  spv_file.seekg(0, std::ios::beg);
  char *data = new char[size];
  spv_file.read(data, size);
  spv_file.close();

  SECTION("Passing a null ptr for the data throws an exception") {
    REQUIRE_THROWS_AS(sut::OpcodeStream(nullptr, size), sut::InvalidParameter);
  }
  SECTION("Passing size zero for the size throws an exception") {
    REQUIRE_THROWS_AS(sut::OpcodeStream(data, 0), sut::InvalidParameter);
  }
  SECTION("Passing size zero for the size and nullptr throws an exception") {
    REQUIRE_THROWS_AS(sut::OpcodeStream(nullptr, 0), sut::InvalidParameter);
  }
  SECTION("Passing correct data ptr and size creates the object") {
    REQUIRE_NOTHROW(sut::OpcodeStream(data, size));


    uint32_t instruction = 0xDEADBEEF;
    std::array<uint32_t, 4U> longer_instruction = {0xDEADBEEF, 0xDEADBEEF,
                                                   0xDEADBEEF, 0xDEADBEEF};
    std::array<uint32_t, 4U> longer_instruction_2 = {0x1EADBEEF, 0x1EADBEEF,
                                                     0x1EADBEEF, 0x1EADBEEF};
    SECTION(
        "Inserting before, after and removing produces an output of the right "
        "size") {
      sut::OpcodeStream stream(data, size);
      for (auto &i : stream) {
        if (i.GetOpcode() == spv::Op::OpCapability) {
          i.InsertBefore(longer_instruction.data(), longer_instruction.size());
          i.InsertAfter(&instruction, 1U);
          i.InsertAfter(longer_instruction.data(), longer_instruction.size());
          i.InsertAfter(longer_instruction.data(), longer_instruction.size());
          i.InsertBefore(longer_instruction_2.data(),
                         longer_instruction_2.size());
          i.Remove();
        }
      }

      sut::OpcodeStream new_stream = stream.EmitFilteredStream();
      std::vector<uint32_t> new_module = new_stream.GetWordsStream();

      // -1 is due to removing the instruction OpCapability which is 2 words
      // long
      // and adding one instruction one word long.
      REQUIRE(new_module.size() ==
              ((size / 4) + (longer_instruction.size() * 3) +
               longer_instruction_2.size() - 1));
    }

    SECTION("Replacing produces an output of the right size") {
      sut::OpcodeStream stream(data, size);
      for (auto &i : stream) {
        if (i.GetOpcode() == spv::Op::OpCapability) {
          i.Replace(longer_instruction.data(), longer_instruction.size());
        }
      }

      sut::OpcodeStream new_stream = stream.EmitFilteredStream();
      std::vector<uint32_t> new_module = new_stream.GetWordsStream();

      // -2 is due to removing the instruction OpCapability which is 2 words
      // long
      REQUIRE(new_module.size() ==
              ((size / 4) + longer_instruction.size() - 2));
    }
    
    SECTION("Inserting does not alter the size of the original raw module") {
      sut::OpcodeStream stream(data, size);
      for (auto &i : stream) {
        if (i.GetOpcode() == spv::Op::OpCapability) {
          i.InsertAfter(longer_instruction.data(), longer_instruction.size());
        }
      }

      std::vector<uint32_t> old_module = stream.GetWordsStream();

      // -2 is due to removing the instruction OpCapability which is 2 words
      // long
      REQUIRE(old_module.size() == size / 4 );
    }
  }
}
