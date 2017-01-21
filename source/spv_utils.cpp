#include <spv_utils.h>
#include <cassert>

namespace sut {

static const size_t kSpvIndexMagicNumber = 0;
static const size_t kSpvIndexVersionNumber = 1;
static const size_t kSpvIndexGeneratorNumber = 2;
static const size_t kSpvIndexBound = 3;
static const size_t kSpvIndexSchema = 4;
static const size_t kSpvIndexInstruction = 5;

static void SplitSpvOpCode(uint32_t word, uint16_t *word_count,
                           uint16_t *opcode) {
  *word_count = static_cast<uint16_t>((0xFFFF0000 & word) >> 16U);
  *opcode = static_cast<uint16_t>(0x0000FFFF & word);
}

static void SplitSpvOpCode(uint32_t word,
                           uint16_t *opcode) {
  *opcode = static_cast<uint16_t>(0x0000FFFF & word);
}

TokenStream::TokenStream(const void *module_stream,
                         size_t binary_size) 
    : module_stream_(nullptr),
      words_count_(0),
      parse_result_(Result::SPV_ERROR_INTERNAL),
      ops_table_() {
  ParseModule(module_stream, binary_size);
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
  //module_stream_.reset();
  module_stream_ = nullptr;
  words_count_ = 0;
  parse_result_ = Result::SPV_ERROR_INTERNAL;
  ops_table_.clear();
}
  
void TokenStream::ParseModule(const void *module_stream, 
                              size_t binary_size) {
  parse_result_ = ParseModuleInternal(module_stream, binary_size);
}

Result TokenStream::ParseModuleInternal(
    const void *module_stream, 
    size_t binary_size) {
  assert(module_stream);
  Reset();

  words_count_ = binary_size / 4;
  if (words_count_ < kSpvIndexInstruction) {
    return Result::SPV_ERROR_INTERNAL;
  }

  module_stream_ = static_cast<const uint32_t *>(module_stream);

  //module_stream_.reset(bin_stream, std::default_delete<char[]>());

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
    if (ParseToken(module_stream_, word_index, &inst_word_count)
        != Result::SPV_SUCCESS) {
      return Result::SPV_ERROR_INTERNAL;
    }

    // Insert offset for the current instruction
    InsertTokenInTable(word_index);

    // Advance the word index by the size of the instruction
    word_index += inst_word_count;
  }

  // Append end terminator to table
  InsertTokenInTable(words_count_);

  return Result::SPV_SUCCESS;
}
  
void TokenStream::InsertTokenInTable(size_t offset) {
  ops_table_.push_back(OpEntry(offset, 0, 0, false));
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

void TokenStream::FilterModule(std::function<void(TokenIterator &)> callback) {
  assert(module_stream_);
  if ((!IsValid()) || (!callback)) {
    return;
  }

  // Callback per-token
  for (OpsTable::iterator i = ops_table_.begin(); i != ops_table_.end(); i++) {
    callback(TokenIterator(i, module_stream_,
                              std::distance(ops_table_.begin(), i)));
  }
}

uint32_t TokenStream::PeekAt(const uint32_t *bin_stream, size_t index) {
  assert(index < words_count_);
  assert(bin_stream);
  return bin_stream[index];
}

std::vector<uint32_t> TokenStream::EmitFilteredModule() const {
  assert(module_stream_);
  std::vector<uint32_t> new_stream;

  for (OpsTable::const_iterator i = ops_table_.begin();
       i != (ops_table_.end() - 1);
       i++) {
    // If there are pending Insert Before operations, emit those ones first
    if (std::get<1>(*i).size() > 0) {
      new_stream.insert(new_stream.end(), std::get<1>(*i).begin(),
                        std::get<1>(*i).end());
    }
    // If the original instruction is not to be removed, emit it
    if (std::get<3>(*i) != true) {
      new_stream.insert(
          new_stream.end(),
          module_stream_[std::get<0>(*(i))],
          module_stream_[std::get<0>(*(i + 1))]);
    }
    // If there are pending Insert After operations, emit them at the end 
    if (std::get<2>(*i).size() > 0) {
      new_stream.insert(new_stream.end(), std::get<2>(*i).begin(),
                        std::get<2>(*i).end());
    }
  }

  return new_stream;
}

TokenIterator::TokenIterator(Iterator itor, const uint32_t *module_stream,
                             size_t idx)
  : itor_(itor),
    module_stream_(module_stream),
    idx_(idx) {
  assert(module_stream_ != nullptr);
}

spv::Op TokenIterator::GetOpcode() const {
  uint16_t inst_opcode = 0U;
  uint32_t word = module_stream_[std::get<0>(*itor_)];

  SplitSpvOpCode(word, &inst_opcode);

  return static_cast<spv::Op>(inst_opcode);
}
  
void TokenIterator::InsertBefore(const uint32_t *instructions,
                                 size_t words_count) {
  std::get<1>(*itor_).insert(std::get<1>(*itor_).end(), instructions,
                             instructions + words_count);
}

void TokenIterator::InsertAfter(const uint32_t *instructions,
                                 size_t words_count) {
  std::get<2>(*itor_).insert(std::get<2>(*itor_).end(), instructions,
                             instructions + words_count);
}

void TokenIterator::Remove() {
  std::get<3>(*itor_) = true;
}
  
size_t TokenIterator::GetTokenIndex() const {
  return idx_;
}

} // namespace sut