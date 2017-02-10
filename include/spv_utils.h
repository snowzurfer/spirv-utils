#ifndef SPV_UTILS_H
#define SPV_UTILS_H

#include <cstdint>
#include <functional>
#include <memory>
#include <spirv/1.1/spirv.hpp11>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace sut {

class OpcodeIterator;
class OpcodeStream;

class InvalidParameter final : public std::runtime_error {
 public:
  explicit InvalidParameter(const std::string &what_arg);
};  // class InvalidParameter

class InvalidStream final : public std::runtime_error {
 public:
  explicit InvalidStream(const std::string &what_arg);
};  // class InvalidStream

class OpcodeOffset final {
 public:
  OpcodeOffset(size_t offset, std::vector<uint32_t> &words);

  // Get the opcode from the first word of the instruction
  spv::Op GetOpcode() const;

  void InsertBefore(const uint32_t *instructions, size_t words_count);
  void InsertAfter(const uint32_t *instructions, size_t words_count);
  void Remove();

 private:
  size_t offset() const { return offset_; }
  bool is_removed() const { return remove_; }
  size_t insert_before_offset() const { return insert_before_offset_; }
  size_t insert_before_count() const { return insert_before_count_; }
  size_t insert_after_offset() const { return insert_after_offset_; }
  size_t insert_after_count() const { return insert_after_count_; }

  friend class OpcodeStream;

  // Data relative to this instruction
  size_t offset_;

  // List of words to be used to otput the filtered stream
  size_t insert_before_offset_;
  size_t insert_before_count_;
  size_t insert_after_offset_;
  size_t insert_after_count_;
  bool remove_;

  uint32_t *GetLatestMaker(size_t initial_offset, size_t initial_count) const;

  std::vector<uint32_t> &words_;

};  // class Opcode

class OpcodeStream final {
 public:
  typedef std::vector<OpcodeOffset>::iterator iterator;
  typedef std::vector<OpcodeOffset>::const_iterator const_iterator;

 public:
  explicit OpcodeStream(const void *module_stream, size_t binary_size);
  explicit OpcodeStream(const std::vector<uint32_t> &module_stream);
  explicit OpcodeStream(std::vector<uint32_t> &&module_stream);

  iterator begin();
  iterator end();
  const_iterator begin() const;
  const_iterator cbegin() const;
  const_iterator end() const;
  const_iterator cend() const;
  // Number of instructions in the stream
  size_t size() const;

  // Apply pending operations and emit filtered stream
  OpcodeStream EmitFilteredStream() const;

  // Get the raw words stream, unfiltered
  std::vector<uint32_t> GetWordsStream() const;

 private:
  typedef std::vector<uint32_t> WordsStream;
  typedef std::vector<OpcodeOffset> OffsetsList;

  // Stream of words representing the module as it has been modified
  WordsStream module_stream_;

  size_t original_module_size_;

  // One entry per instruction, with entries coming only from the original
  // module, i.e. without the filtering
  OffsetsList offsets_table_;

  void InsertOffsetInTable(size_t offset);
  void InsertWordHeaderInOriginalStream(const struct OpcodeHeader &header);

  // Parse the module stream into an offset table; called by the ctor
  void ParseModule();

  // Return the word count of a given instruction starting at start_index
  size_t ParseInstructionWordCount(size_t start_index);

  // Return the word at a given index in the module stream
  uint32_t PeekAt(size_t index) const;

  // Emit the words for a given type of operation; the different type
  // depends only on where the words are read from, so passing the start offset
  // and count is sufficient to distinguish
  void EmitByType(WordsStream &new_stream, size_t start_offset,
                  size_t count) const;

};  // class OpcodeStream

}  // namespace sut

#endif
