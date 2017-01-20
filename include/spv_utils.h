#ifndef SPV_UTILS_H
#define SPV_UTILS_H

#include <vector>
#include <cstdint>
#include <tuple>

namespace sut {

enum class Result {
  SPV_SUCCESS = 0,
  SPV_ERROR_INTERNAL = -1,
};

class TokenStream {
 public:
  TokenStream(const void *module_stream, size_t binary_size);
  ~TokenStream();

  Result ParseModule(const void *module_stream, size_t binary_size);

  bool IsValid() const;

 private:
  // Offset is in words
  typedef std::tuple<size_t, int32_t, int32_t, bool> OpEntry;
  typedef std::vector<OpEntry> OpsTable;

  size_t words_count_;
  Result parse_result_;
  OpsTable ops_table_;

  void Reset();

  void InsertTokenInTable(size_t offset);
  Result ParseToken(const uint32_t *bin_stream, size_t start_index,
                    size_t *words_count);
  uint32_t PeekAt(const uint32_t *bin_stream, size_t index);
  void SplitSpvOpCode(uint32_t word, uint16_t *word_count, uint16_t *opcode);

}; // class TokenStream 

} // namespace sut

#endif
