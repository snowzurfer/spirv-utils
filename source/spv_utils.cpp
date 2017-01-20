#include <spv_utils.h>
#include <cassert>


namespace sut {

static const size_t kSpvIndexMagicNumber = 0;
static const size_t kSpvIndexVersionNumber = 1;
static const size_t kSpvIndexGeneratorNumber = 2;
static const size_t kSpvIndexBound = 3;
static const size_t kSpvIndexSchema = 4;
static const size_t kSpvIndexInstruction = 5;

TokenStream::TokenStream(const void *module_stream, size_t binary_size) 
    : words_count_(0),
      parse_result_(Result::SPV_ERROR_INTERNAL),
      ops_table_() {
  parse_result_ = ParseModule(module_stream, binary_size);
}

TokenStream::~TokenStream() {}

bool TokenStream::IsValid() const {
  switch (parse_result_) {
    case Result::SPV_SUCCESS: {
      return true;
    }
    default: {
      return false;
    }
  }
}

void TokenStream::Reset() {
  words_count_ = 0;
  parse_result_ = Result::SPV_ERROR_INTERNAL;
  ops_table_.clear();
}

Result TokenStream::ParseModule(const void *module_stream, size_t binary_size) {
  Reset();

  words_count_ = binary_size / 4;
  if (words_count_ < kSpvIndexInstruction) {
    return Result::SPV_ERROR_INTERNAL;
  }

  const uint32_t * bin_stream = static_cast<const uint32_t *>(module_stream);

  // Set tokens for theader; these always take the same amount of words
  InsertTokenInTable(kSpvIndexMagicNumber);
  InsertTokenInTable(kSpvIndexVersionNumber);
  InsertTokenInTable(kSpvIndexGeneratorNumber);
  InsertTokenInTable(kSpvIndexBound);
  InsertTokenInTable(kSpvIndexSchema);

  size_t word_index = kSpvIndexInstruction;

  // Once the loop is finished, the tokens table will contain one entry
  // per instruction
  while (word_index < words_count_) {
    size_t inst_word_count = 0;
    if (ParseToken(bin_stream, word_index, &inst_word_count)
        != Result::SPV_SUCCESS) {
      return Result::SPV_ERROR_INTERNAL;
    }

    // Insert offset for the current instruction
    InsertTokenInTable(word_index);

    // Advance the word index by the size of the instruction
    word_index += inst_word_count;
  }

  return Result::SPV_SUCCESS;
}
  
void TokenStream::InsertTokenInTable(size_t offset) {
  ops_table_.push_back(OpEntry(offset, -1, -1, false));
}

Result TokenStream::ParseToken(const uint32_t *bin_stream, size_t start_index,
                               size_t *words_count) {
  // Read the first word of the instruction, which
  // contains the word count
  uint32_t first_word = PeekAt(bin_stream, start_index);
  
  // Decompose and read the word count
  uint16_t inst_word_count = 0U;
  uint16_t inst_opcode = 0U;
  SplitSpvOpCode(first_word, &inst_word_count, &inst_opcode);

  if (inst_word_count < 1U) {
    return Result::SPV_ERROR_INTERNAL;
  }

  *words_count = static_cast<size_t>(inst_word_count);

  return Result::SPV_SUCCESS;
}

uint32_t TokenStream::PeekAt(const uint32_t *bin_stream, size_t index) {
  assert(index < words_count_);
  assert(bin_stream != nullptr);
  return bin_stream[index];
}

void TokenStream::SplitSpvOpCode(uint32_t word, uint16_t *word_count,
                                 uint16_t *opcode) {
  *word_count = static_cast<uint16_t>((0xFFFF0000 & word) >> 16U);
  *opcode = static_cast<uint16_t>(0x0000FFFF & word);
}

} // namespace sut
