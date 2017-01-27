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
static const uint32_t kMarker = 0xFFFFFFFF;

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
      offsets_table_() {
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
  offsets_table_.push_back(OpcodeOffset(offset, module_stream_));
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
  WordsStream new_stream;
  // The new stream will roughly be as large as the original one
  new_stream.reserve(module_stream_.size());

  for (OffsetsList::const_iterator oi = offsets_table_.begin();
       oi != (offsets_table_.end() - 1);
       oi++) {
    if (oi->insert_before_count() > 0) {
      EmitByType(new_stream, oi->insert_before_offset(), oi->insert_before_count());
    }
    if (!oi->remove()) {
      new_stream.insert(
          new_stream.end(),
          module_stream_.begin() + oi->offset(),
          module_stream_.begin() + (oi + 1)->offset());
    }
    if (oi->insert_after_count() > 0) {
      EmitByType(new_stream, oi->insert_after_offset(), oi->insert_after_count());
    }
  }

  return new_stream;
}
  
void OpcodeStream::EmitByType(
    WordsStream &new_stream,
    size_t start_offset,
    size_t count) const {
  new_stream.insert(
      new_stream.end(),
      module_stream_.begin() + start_offset,
      module_stream_.begin() + start_offset + count);

  // If there isn't a marker after the last word, it means that there
  // are other parts to output which are appended in the stream of words
  if (*(module_stream_.begin() + start_offset + count) != kMarker) {
    // Go through the makers until you get to the last one
    const uint32_t *current_marker =
      &module_stream_[start_offset + count];

    while (*current_marker != kMarker) {
      uint32_t next_index = ((0xFFFF0000 & *current_marker) >> 16U);
      uint32_t next_count = (0x0000FFFF & *current_marker);
      
      // Output
      new_stream.insert(
          new_stream.end(),
          module_stream_.begin() + next_index,
          module_stream_.begin() + next_index + next_count);
      
      current_marker = &module_stream_[next_index + next_count];
    }
  }
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
  
OpcodeOffset::OpcodeOffset(size_t offset, std::vector<uint32_t> &words)
    : offset_(offset),
      insert_before_offset_(0),
      insert_before_count_(0),
      insert_after_offset_(0),
      insert_after_count_(0),
      remove_(false),
      words_(words) {}

spv::Op OpcodeOffset::GetOpcode() const {
  uint32_t header_word = words_[offset_];

  return static_cast<spv::Op>(SplitSpvOpCode(header_word).opcode);
}
  
void OpcodeOffset::InsertBefore(const uint32_t *instructions,
                                size_t words_count) {
  assert(instructions && words_count);

  size_t new_offset = words_.size();

  words_.insert(
      words_.end(),
      instructions,
      instructions + words_count);
  // Add the end marker
  words_.push_back(kMarker);

  // Only set this the first time; afterwards use markers
  if (insert_before_count_ == 0) {
    insert_before_offset_ = new_offset;
    insert_before_count_ = words_count;
  }
  else {
    uint32_t *latest_marker =
      GetLatestMaker(insert_before_offset_, insert_before_count_);

    // Write at the last marker
    *latest_marker = ((0xFFFF0000 & (new_offset << 16U)) |
                      (0x0000FFFF & words_count));
  }
}

void OpcodeOffset::InsertAfter(const uint32_t *instructions,
                               size_t words_count) {
  assert(instructions && words_count);

  size_t new_offset = words_.size();

  // +1 is for the end marker
  words_.reserve(new_offset + words_count + 1);

  words_.insert(
      words_.end(),
      instructions,
      instructions + words_count);
  // Add the end marker
  words_.push_back(kMarker);

  // Only set this the first time; afterwards use markers
  if (insert_after_count_ == 0) {
    insert_after_offset_ = new_offset;
    insert_after_count_ = words_count;
  }
  else {
    uint32_t *latest_marker =
      GetLatestMaker(insert_after_offset_, insert_after_count_);
    
    // Write at the last marker
    *latest_marker = ((0xFFFF0000 & (new_offset << 16U)) |
                       (0x0000FFFF & words_count));
  }
}
  
uint32_t *OpcodeOffset::GetLatestMaker(
    size_t initial_offset,
    size_t initial_count) const {
    uint32_t *current_marker = &words_[initial_offset + initial_count];

    while (*current_marker != kMarker) {
      uint32_t next_index = ((0xFFFF0000 & *current_marker) >> 16U);
      uint32_t next_count = (0x0000FFFF & *current_marker);
      current_marker = &words_[next_index + next_count];
    }

    return current_marker;
}

void OpcodeOffset::Remove() {
  remove_ = true;
}

} // namespace sut
