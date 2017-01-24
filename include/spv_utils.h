#ifndef SPV_UTILS_H
#define SPV_UTILS_H

#include <vector>
#include <cstdint>
#include <tuple>
#include <spirv/1.1/spirv.hpp11>
#include <functional>
#include <memory>

namespace sut {

enum class Result : int8_t {
  SPV_SUCCESS = 0,
  SPV_ERROR_INTERNAL = -1,
};

class OpcodeIterator;
class OpcodeStream;


class OpcodeOffset {
 public:
  OpcodeOffset() = delete;

  spv::Op GetOpcode() const;

  void InsertBefore(const uint32_t *instructions, size_t words_count);
  void InsertAfter(const uint32_t *instructions, size_t words_count);
  void Remove();

 private:
  OpcodeOffset(size_t offset, const OpcodeStream &opcode_stream);
  friend class OpcodeStream;

  size_t offset_;

  template <size_t num>
  void Insert(const uint32_t *instructions, size_t words_count) {
    WordsVector &words_before = std::get<num>(opcode_stream_.ops_table_[idx_]);
    words_before.insert(words_before.end(), instructions,
                        instructions + words_count);
  }

  std::vector<uint32_t> words_before_;
  std::vector<uint32_t> words_after_;
  bool remove_;
  const OpcodeStream &opcode_stream_;

}; // class Opcode


class OpcodeStream {
 public:
  typedef std::vector<OpcodeOffset>::iterator iterator;

 public:
  OpcodeStream(const void *module_stream, size_t binary_size);
  OpcodeStream();

  //OpcodeStream(const OpcodeStream &rhs);
  //OpcodeStream(OpcodeStream &&rhs);
  //OpcodeStream &operator==(const OpcodeStream &rhs);
  //OpcodeStream &operator==(OpcodeStream &&rhs);

  // Parses a module, creating an internal table which is then used for
  // filtering and emission of the filtered stream
  Result ParseModule(const void *module_stream, size_t binary_size);

  iterator begin();
  iterator end();

  // Apply pending operations and emit filtered stream
  std::vector<uint32_t> EmitFilteredStream() const;

  // Check whether the object is in a valid state so that it can be used.
  bool IsValid() const;
  operator bool() const;

  const std::vector<uint32_t> &module_stream() const { return module_stream_; }

 private:
  // Stream of words representing the original module
  std::vector<uint32_t> module_stream_;

  // The two tables are of the same lenght
  std::vector<OpcodeOffset> offsets_table_;

  Result parse_result_;

  void InsertOffsetInTable(size_t offset);
  void InsertWordHeaderInOriginalStream(const struct OpcodeHeader &header);
  Result ParseModuleInternal();
  Result ParseInstruction(
      size_t start_index,
      size_t *words_count);
  uint32_t PeekAt(size_t index);
  void Reset();

}; // class TokenStream 

} // namespace sut

#endif
