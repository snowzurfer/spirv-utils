# spirv-utils
Library which allows parsing and filtering of a spir-v binary module.

## Getting Started
These instructions will get you a copy of the project up and running on your
local machine for development and testing purposes.

### Example
```cpp
#include <spv_utils.h>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

int main() {
  // Read spv binary module from a file
  std::ifstream spv_file("sample_spv_modules/test.frag.spv",
                         std::ios::binary | std::ios::ate | std::ios::in);
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
        std::array<uint32_t, 4> longer_instruction = {0xDEADBEEF, 0xDEADBEEF,
                                                    0xDEADBEEF, 0xDEADBEEF};
        std::array<uint32_t, 4> longer_instruction_2 = {0x1EADBEEF, 0x1EADBEEF,
                                                      0x1EADBEEF, 0x1EADBEEF};
        i.InsertBefore(longer_instruction.data(), longer_instruction.size());
        i.InsertAfter(&instruction, 1U);
        i.InsertAfter(longer_instruction.data(), longer_instruction.size());
        i.InsertAfter(longer_instruction.data(), longer_instruction.size());
        i.InsertBefore(longer_instruction_2.data(),
                       longer_instruction_2.size());
        i.Remove();
        
        // Replace() calls Remove(), but is used along with it here to show 
        // its usage
        i.Replace(longer_instruction_2.data(),
                  longer_instruction_2.size());
      }
    }

    sut::OpcodeStream filtered_stream = stream.EmitFilteredStream();
    std::vector<uint32_t> filtered_module = filtered_stream.GetWordsStream();

    // Use the filtered module here...
    // ...
    //

    std::vector<uint32_t> original_module = stream.GetWordsStream();

    // Use the original module here...
    // ...
    //

    delete[] data;
    data = nullptr;
  }

  return 0;
};
```

### Prerequisites
* CMake 2.8

### Installing
#### CMake usage
Clone the repo:
```
git clone https://github.com/snowzurfer/spirv-utils.git
```

Add the project as a subdirectory in your root CMakeLists:
```
add_subdirectory(${PATH_TO_SUT_ROOT_DIR})    
```
    
The CMakeLists defines the library  
```
sut  
```
the variables  
```
${SUT_HEADERS} , ${SUT_SOURCES}  
```
and the option  
```
SUT_BUILD_TESTS
```
  
  By default, the tests won't be built (the option is set to
  `OFF`)
  and the build
  type will be `Debug`.    

  You can set these options either via the tick boxes
  in the CMake GUI or via
  command-line arguments.


Link the library
```
target_link_libraries(your_target, sut)
```

Use the library as shown in the example at the top of the `README`.

## Running the tests
Set the option `SUT_BUILD_TESTS` to `ON` and re-build your project.
Now, every time you build, the tests will be run on the unit tests defined
in the `CMakeLists` of the `unit_tests` folder.

## Built with
* [Catch](http://github.com/philsquared/Catch) - The unit testing framework used
* [SPIRV-Headers](http://github.com/KhronosGroup/SPIRV-Headers/)
* [CMake](http://cmake.org)

## Tested platforms and compilers
* ArchLinux, kernel 4.9.6-1 x86_64, g++ 6.3.1 20170109
* Windows 10 x86_64, Visual Studio 2015, Toolset v140

## Authors
* **Alberto Taiuti** - *Developer* - [@snowzurfer](https://github.com/snowzurfer)

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details

## Acknowledgments
* **Dr Matthäus G. Chajdas** - *Mentoring and design* - [@Anteru](https://github.com/Anteru)
