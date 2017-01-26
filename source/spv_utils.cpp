#include <spv_utils.h>
#include <cassert>
#include <sstream>

namespace sut {

static const size_t kSpvIndexMagicNumber = 0;
static const size_t kSpvIndexVersionNumber = 1;
static const size_t kSpvIndexGeneratorNumber = 2;
static const size_t kSpvIndexBound = 3;
static const size_t kSpvIndexSchema = 4;
static const size_t kSpvIndexInstruction = 5;
static const size_t kPendingOpsInitialReserve = 5;

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
  
InvalidParameter::InvalidParameter(const std::string &what_arg) :
  runtime_error(what_arg) {}

InvalidStream::InvalidStream(const std::string &what_arg) :
  runtime_error(what_arg) {}
  
OpcodeStream::OpcodeStream(const void *module_stream, size_t binary_size)
    : module_stream_(),
      offsets_table_(),
      pending_ops_() {
  if (!module_stream || !binary_size || ((binary_size % 4) != 0) ||
      ((binary_size / 4) < kSpvIndexInstruction)) {
    throw InvalidParameter("Invalid parameter in ctor of OpcodeStream!");
  }

  // The +1 is because we will append a null-terminator to the stream
  module_stream_.reserve((binary_size / 4) + 1);
  module_stream_.insert(
      module_stream_.begin(),
      static_cast<const uint32_t *>(module_stream),
      static_cast<const uint32_t *>(module_stream) + (binary_size / 4));

  // Reserve enough memory for the worst case scenario, where each instruction
  // is one word long
  offsets_table_.reserve(module_stream_.size());

  // Reserve enough memory for one operation per-offset
  pending_ops_.reserve(offsets_table_.size());

  ParseModule();
}

void OpcodeStream::ParseModule() {
  const size_t words_count = module_stream_.size();

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
    size_t inst_word_count = ParseInstructionWordCount(word_index);

    // Insert offset for the current instruction
    InsertOffsetInTable(word_index);

    // Advance the word index by the size of the instruction
    word_index += inst_word_count;
  }

  // Append end terminator to table
  InsertOffsetInTable(words_count);

  // Append end terminator word to original words stream
  InsertWordHeaderInOriginalStream({0U, static_cast<uint16_t>(spv::Op::OpNop)});
}
  
void OpcodeStream::InsertWordHeaderInOriginalStream(const OpcodeHeader &header) {
  module_stream_.push_back(
      MergeSpvOpCode(header));
}
  
void OpcodeStream::InsertOffsetInTable(size_t offset) {
  offsets_table_.push_back(OpcodeOffset(offset, module_stream_, pending_ops_));
}

size_t OpcodeStream::ParseInstructionWordCount(size_t start_index) {
  // Read the first word of the instruction, which
  // contains the word count
  uint32_t first_word = PeekAt(start_index);
  
  // Decompose and read the word count
  OpcodeHeader header = SplitSpvOpCode(first_word);

  if (header.words_count < 1U) {
    std::stringstream msg_stream;
    msg_stream << "Word with index " << start_index << " has word count of " <<
                  header.words_count;
    std::string msg = msg_stream.str();
    throw InvalidStream(msg);
  }

  return static_cast<size_t>(header.words_count);
}

uint32_t OpcodeStream::PeekAt(size_t index) {
  return module_stream_[index];
}

std::vector<uint32_t> OpcodeStream::EmitFilteredStream() const {
  typedef std::vector<size_t> OpsOffsets;
  WordsStream new_stream;
  // The new stream will roughly be as large as the original one
  new_stream.reserve(module_stream_.size());


  for (OffsetsList::const_iterator oi = offsets_table_.begin();
       oi != (offsets_table_.end() - 1);
       oi++) {
    size_t before_offset = new_stream.size();
    size_t after_offset = before_offset;
    bool has_used_remove = false;

    // For all the pending operations for this offset
    for (OpsOffsets::const_iterator opsoffi = oi->ops_offsets().begin();
         opsoffi != oi->ops_offsets().end();
         opsoffi++) {
      // Get an iterator to the pending operation currently processed
      OpsList::const_iterator pendi = pending_ops_.begin() + (*opsoffi);

      if (pendi->op_type == PendingOp::OpType::INSERT_BEFORE) {
        WordsStream::iterator before = new_stream.begin() + before_offset;

        new_stream.insert(
            before,
            module_stream_[pendi->offset],
            module_stream_[pendi->offset] + pendi->words_count);

        before_offset += pendi->words_count;
        after_offset += pendi->words_count;
      }
      else if (pendi->op_type == PendingOp::OpType::REMOVE && !has_used_remove) {
        has_used_remove = true;
      }
      else if (pendi->op_type == PendingOp::OpType::INSERT_BEFORE) {
        WordsStream::iterator after = new_stream.begin() + after_offset;
      WordsStream::const_iterator module_start =
        module_stream_.begin() + oi->offset();
      WordsStream::const_iterator module_end =
        module_stream_.begin() + (oi + 1)->offset();
        new_stream.insert(
            after,
            module_stream_[pendi->offset],
            module_stream_[pendi->offset] + pendi->words_count);

        after_offset += pendi->words_count;
      }
    }

    if (!has_used_remove) {
      WordsStream::iterator before = new_stream.begin() + before_offset;
      WordsStream::const_iterator module_start =
        module_stream_.begin() + oi->offset();
      WordsStream::const_iterator module_end =
        module_stream_.begin() + (oi + 1)->offset();
        
      new_stream.insert(before, module_start, module_end);
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

OpcodeStream::const_iterator OpcodeStream::end() const {
  return offsets_table_.end();
}

OpcodeStream::const_iterator OpcodeStream::begin() const {
  return offsets_table_.begin();
}

OpcodeStream::const_iterator OpcodeStream::cbegin() const {
  return offsets_table_.cbegin();
}

OpcodeStream::const_iterator OpcodeStream::cend() const {
  return offsets_table_.cend();
}
  
OpcodeOffset::OpcodeOffset(size_t offset, std::vector<uint32_t> &words,
                           std::vector<PendingOp> &ops) 
    : offset_(offset),
      words_(words),
      ops_(ops),
      ops_offsets_() {
  assert(!words.empty());
  ops_offsets_.reserve(kPendingOpsInitialReserve);
}

spv::Op OpcodeOffset::GetOpcode() const {
  uint32_t header_word = words_[offset_];

  return static_cast<spv::Op>(SplitSpvOpCode(header_word).opcode);
}
  
void OpcodeOffset::InsertBefore(const uint32_t *instructions,
                                size_t words_count) {
  assert(instructions && words_count);

  Insert(instructions, words_count, PendingOp::OpType::INSERT_BEFORE);
}

void OpcodeOffset::InsertAfter(const uint32_t *instructions,
                               size_t words_count) {
  assert(instructions && words_count);

  Insert(instructions, words_count, PendingOp::OpType::INSERT_AFTER);
}

void OpcodeOffset::Remove() {
  Insert(nullptr, 0, PendingOp::OpType::REMOVE);
}

void OpcodeOffset::Insert(const uint32_t *instructions, size_t words_count,
                          PendingOp::OpType op_type) {
  size_t ops_offset = ops_.size();
  size_t words_offset = words_.size();

  // Append words
  if (op_type != PendingOp::OpType::REMOVE) {
    words_.insert(
        words_.end(),
        instructions,
        instructions + words_count);
  }

  // Append operation
  ops_.push_back({words_offset, words_count, op_type});

  // Save operation offset
  ops_offsets_.push_back(ops_offset);
}

} // namespace sut