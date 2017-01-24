#include <spv_utils.h>
#include <cassert>

namespace sut {

static const size_t kSpvIndexMagicNumber = 0;
static const size_t kSpvIndexVersionNumber = 1;
static const size_t kSpvIndexGeneratorNumber = 2;
static const size_t kSpvIndexBound = 3;
static const size_t kSpvIndexSchema = 4;
static const size_t kSpvIndexInstruction = 5;

struct OpcodeHeader {
  uint16_t words_count;
  uint16_t opcode;
}; // struct OpCodeHeader

static OpcodeHeader SplitSpvOpCode(uint32_t word) {
  return{
    static_cast<uint16_t>((0xFFFF0000 & word) >> 16U),
    static_cast<uint16_t>(0x0000FFFF & word) };
}

static uint32_t MergeSpvOpCode(const OpcodeHeader &header) {
  return (
    (static_cast<uint32_t>(header.words_count) << 16U) |
    (static_cast<uint32_t>(header.opcode)));
}

OpcodeStream::OpcodeStream() 
    : module_stream_(),
      parse_result_(Result::SPV_ERROR_INTERNAL),
      offsets_table_() {}
  
OpcodeStream::OpcodeStream(const void *module_stream, size_t binary_size)
    : module_stream_(),
      parse_result_(Result::SPV_ERROR_INTERNAL),
      offsets_table_() {
  parse_result_ = ParseModule(module_stream, binary_size);
}

Result OpcodeStream::ParseModuleInternal() {
  const size_t words_count = module_stream_.size();
  if (words_count < kSpvIndexInstruction) {
    return Result::SPV_ERROR_INTERNAL;
  }

  // Set tokens for theader; these always take the same amount of words
  InsertOffsetInTable(kSpvIndexMagicNumber);
  InsertOffsetInTable(kSpvIndexVersionNumber);
  InsertOffsetInTable(kSpvIndexGeneratorNumber);
  InsertOffsetInTable(kSpvIndexBound);
  InsertOffsetInTable(kSpvIndexSchema);

  size_t word_index = kSpvIndexInstruction;

  // Once the loop is finished, the offsets table will contain one entry
  // per instruction
  while (word_index < words_count) {
    size_t inst_word_count = 0;
    if (ParseInstruction(word_index, &inst_word_count)
        != Result::SPV_SUCCESS) {
      return Result::SPV_ERROR_INTERNAL;
    }

    // Insert offset for the current instruction
    InsertOffsetInTable(word_index);

    // Advance the word index by the size of the instruction
    word_index += inst_word_count;
  }

  // Append end terminator to table
  InsertOffsetInTable(words_count);

  // Append end terminator word to original words stream
  InsertWordHeaderInOriginalStream({0U, static_cast<uint16_t>(spv::Op::OpNop)});

  return Result::SPV_SUCCESS;
}
  
void OpcodeStream::InsertWordHeaderInOriginalStream(const OpcodeHeader &header) {
  module_stream_.push_back(
      MergeSpvOpCode(header));
}
  
void OpcodeStream::InsertOffsetInTable(size_t offset) {
  offsets_table_.push_back(OpcodeOffset(offset, *this));
}
  
bool OpcodeStream::IsValid() const {
  switch (parse_result_) {
    case Result::SPV_SUCCESS: {
      return true;
    }
    default: {
      return false;
    }
  }
}

OpcodeStream::operator bool() const {
  return IsValid();
}

Result OpcodeStream::ParseInstruction(
    size_t start_index,
    size_t *words_count) {
  // Read the first word of the instruction, which
  // contains the word count
  uint32_t first_word = PeekAt(start_index);
  
  // Decompose and read the word count
  OpcodeHeader header = SplitSpvOpCode(first_word);

  if (header.words_count < 1U) {
    return Result::SPV_ERROR_INTERNAL;
  }

  *words_count = static_cast<size_t>(header.words_count);

  return Result::SPV_SUCCESS;
}
  
void OpcodeStream::Reset() {
  module_stream_.clear();
  offsets_table_.clear();
  parse_result_ = Result::SPV_ERROR_INTERNAL;
}
  
Result OpcodeStream::ParseModule(const void *module_stream, size_t binary_size) {
  assert(binary_size && module_stream);

  // Make a copy of the data
  const uint32_t *stream = static_cast<const uint32_t *>(module_stream);
  module_stream_.assign(stream, stream + (binary_size / 4));

  return ParseModuleInternal();
}

uint32_t OpcodeStream::PeekAt(size_t index) {
  return module_stream_[index];
}

std::vector<uint32_t> OpcodeStream::EmitFilteredStream() const {
  std::vector<uint32_t> new_stream;

  for (std::vector<OpcodeOffset>::const_iterator oi = offsets_table_.begin();
       oi != (offsets_table_.end() - 1);
       oi++) {
    // If there are pending Insert Before operations, emit those ones first
    if (!oi->words_before_.empty()) {
      new_stream.insert(new_stream.end(), oi->words_before_.begin(),
                        oi->words_before_.end());
    }
    // If the original instruction is not to be removed, emit it
    if (!oi->remove_) {
      new_stream.insert(
          new_stream.end(),
          module_stream_.begin() + oi->offset_,
          module_stream_.begin() + ((oi + 1)->offset_));
    }
    // If there are pending Insert After operations, emit them at the end 
    if (!oi->words_after_.empty()) {
      new_stream.insert(new_stream.end(), oi->words_after_.begin(),
                        oi->words_after_.end());
    }
  }

  return new_stream;
}
  
OpcodeStream::iterator OpcodeStream::begin() {
  return offsets_table_.begin();
}

OpcodeStream::iterator OpcodeStream::end() {
  return offsets_table_.end();
}
  
OpcodeOffset::OpcodeOffset(size_t offset, const OpcodeStream &opcode_stream)
  : offset_(offset),
    words_before_(),
    words_after_(),
    remove_(false),
    opcode_stream_(opcode_stream) {}

spv::Op OpcodeOffset::GetOpcode() const {
  uint32_t header_word = opcode_stream_.module_stream()[offset_];

  return static_cast<spv::Op>(SplitSpvOpCode(header_word).opcode);
}
  
void OpcodeOffset::InsertBefore(const uint32_t *instructions,
                                  size_t words_count) {
  assert(instructions && words_count);
  words_before_.insert(
      words_before_.begin(),
      instructions,
      instructions + words_count);
}

void OpcodeOffset::InsertAfter(const uint32_t *instructions,
                                 size_t words_count) {
  assert(instructions && words_count);
  words_after_.insert(
      words_after_.begin(),
      instructions,
      instructions + words_count);
}

void OpcodeOffset::Remove() {
  remove_ = true;
}

} // namespace sut