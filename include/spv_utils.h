#ifndef SPV_UTILS_H
#define SPV_UTILS_H

#include <vector>
#include <cstdint>
#include <tuple>
#include <spirv/1.1/spirv.hpp11>
#include <functional>
#include <memory>
#include <stdexcept>

namespace sut {

class OpcodeIterator;
class OpcodeStream;

class InvalidParameter final : public std::runtime_error {
 public:
  explicit InvalidParameter(const std::string &what_arg);
}; // class InvalidParameter

class InvalidStream final : public std::runtime_error {
 public:
  explicit InvalidStream(const std::string &what_arg);
}; // class InvalidStream


struct PendingOp final {
  enum class OpType : uint8_t {
    INSERT_BEFORE,
    INSERT_AFTER,
    REMOVE
  }; // enum class OpType

  size_t offset;
  size_t words_count;
  OpType op_type;

}; // class PendingOp


class OpcodeOffset final {
 public:
  OpcodeOffset(size_t offset, std::vector<uint32_t> &words,
               std::vector<PendingOp> &ops);

  spv::Op GetOpcode() const;

  void InsertBefore(const uint32_t *instructions, size_t words_count);
  void InsertAfter(const uint32_t *instructions, size_t words_count);
  void Remove();

  size_t offset() const { return offset_; }
  const std::vector<size_t> &ops_offsets() const { return ops_offsets_; }

 private:
  // Data relative to this instruction
  size_t offset_;

  // List of words to be used to otput the filtered stream
  std::vector<uint32_t> &words_;

  // List of pending operations
  std::vector<PendingOp> &ops_;

  // Indices into the list of pending operations; they represent the operations
  // relative to this opcode offset
  std::vector<size_t> ops_offsets_;

  void Insert(const uint32_t *instructions, size_t words_count,
              PendingOp::OpType op_type);

}; // class Opcode


class OpcodeStream final {
 public:
  typedef std::vector<OpcodeOffset>::iterator iterator;
  typedef std::vector<OpcodeOffset>::const_iterator const_iterator;

 public:
  explicit OpcodeStream(const void *module_stream, size_t binary_size);

  iterator begin();
  iterator end();
  const_iterator begin() const;
  const_iterator cbegin() const;
  const_iterator end() const;
  const_iterator cend() const;

  // Apply pending operations and emit filtered stream
  std::vector<uint32_t> EmitFilteredStream() const;

 private:
  typedef std::vector<uint32_t> WordsStream;
  typedef std::vector<OpcodeOffset> OffsetsList;
  typedef std::vector<PendingOp> OpsList;
  
  // Stream of words representing the module as it has been modified
  WordsStream module_stream_;
  // One entry per instruction, with entries coming only from the original
  // module, i.e. without the filtering
  OffsetsList offsets_table_;
  // Operations to be applied when emitting the filtered version of the module
  OpsList pending_ops_;

  void InsertOffsetInTable(size_t offset);
  void InsertWordHeaderInOriginalStream(const struct OpcodeHeader &header);

  // Parse the module stream into an offset table; called by the ctor
  void ParseModule();

  // Return the word count of a given instruction starting at start_index
  size_t ParseInstructionWordCount(size_t start_index);

  // Return the word at a given index in the module stream
  uint32_t PeekAt(size_t index);

}; // class OpcodeStream 

} // namespace sut

#endif
