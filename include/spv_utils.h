#ifndef SPV_UTILS_H
#define SPV_UTILS_H

#include <vector>
#include <cstdint>
#include <tuple>
#include <spirv/1.1/spirv.hpp11>
#include <functional>
#include <memory>

namespace sut {

enum class Result {
  SPV_SUCCESS = 0,
  SPV_ERROR_INTERNAL = -1,
};

typedef void(*TokenFilterCallbackFn) (class TokenIterator &);
// Offset is in words
typedef std::tuple<size_t, std::vector<uint32_t>,
                   std::vector<uint32_t>, bool> OpEntry;
typedef std::vector<OpEntry> OpsTable;
typedef OpsTable::iterator Iterator;

//template <typename ST>
//class Stream {
//  Stream(const ST *module_stream);
//
//
//}; // class Stream

class TokenIterator {
 public:
  TokenIterator(Iterator itor, const uint32_t *module_stream, size_t idx);

  // Get the index of the token within the original words stream
  size_t GetTokenIndex() const;
  spv::Op GetOpcode() const;

  void InsertBefore(const uint32_t *instructions, size_t words_count);
  void InsertAfter(const uint32_t *instructions, size_t words_count);
  void Remove();

 private:
  Iterator itor_;
  const uint32_t *module_stream_;
  size_t idx_;

}; // class TokenIterator

// Represents a modifiable stream of words making a SPIR-V module.
class TokenStream {
 public:
  // Sets up the object and calls ParseModule. After calling the constructor,
  // it's possible to filter the module if the parsing was successful.
  TokenStream(const void *module_stream,
              size_t binary_size);
  ~TokenStream();

  // Parses a module, creating an internal table which is then used for
  // filtering and emission of the filtered stream
  void ParseModule(const void *module_stream, 
                   size_t binary_size);

  // Filter the module which has been set either via the ctor or via
  // ParseModule
  void FilterModule(std::function<void(TokenIterator &)> callback);

  // Use the actions set via filtering to emit a filtered module
  std::vector<uint32_t> EmitFilteredModule() const;

  // Check whether the object is in a valid state so that it can be used.
  bool IsValid() const;

 private:
  //std::shared_ptr<uint32_t> module_stream_;
  const uint32_t *module_stream_;
  size_t words_count_;
  Result parse_result_;
  OpsTable ops_table_;

  void Reset();

  void InsertTokenInTable(size_t offset);
  Result ParseModuleInternal(const void *bin_stream, 
                             size_t binary_size);
  Result ParseToken(const uint32_t *bin_stream, size_t start_index,
                    size_t *words_count);
  uint32_t PeekAt(const uint32_t *bin_stream, size_t index);

}; // class TokenStream 

} // namespace sut

#endif
