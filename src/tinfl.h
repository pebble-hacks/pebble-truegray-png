#ifndef _TINFL_H_
#define _TINFL_H_


#include <stdlib.h>  // size_t

typedef unsigned char mz_uint8;
typedef signed short mz_int16;
typedef unsigned short mz_uint16;
typedef unsigned int mz_uint32;
typedef unsigned long long mz_uint64;

typedef mz_uint32 tinfl_bit_buf_t;

// Return status.
typedef enum
{
  TINFL_STATUS_BAD_PARAM = -3,
  TINFL_STATUS_ADLER32_MISMATCH = -2,
  TINFL_STATUS_FAILED = -1,
  TINFL_STATUS_DONE = 0,
  TINFL_STATUS_NEEDS_MORE_INPUT = 1,
  TINFL_STATUS_HAS_MORE_OUTPUT = 2
} tinfl_status;

enum {
  TINFL_MAX_HUFF_TABLES = 3, 
  TINFL_MAX_HUFF_SYMBOLS_0 = 288, 
  TINFL_MAX_HUFF_SYMBOLS_1 = 32, 
  TINFL_MAX_HUFF_SYMBOLS_2 = 19,
  TINFL_FAST_LOOKUP_BITS = 10, 
  TINFL_FAST_LOOKUP_SIZE = 1 << TINFL_FAST_LOOKUP_BITS
};

typedef struct {
  mz_uint8 m_code_size[TINFL_MAX_HUFF_SYMBOLS_0];
  mz_int16 m_look_up[TINFL_FAST_LOOKUP_SIZE], m_tree[TINFL_MAX_HUFF_SYMBOLS_0 * 2];
} tinfl_huff_table;


struct tinfl_decompressor_tag {
  mz_uint32 m_state, m_num_bits, m_zhdr0, m_zhdr1, m_z_adler32, m_final, m_type, m_check_adler32, m_dist, m_counter, m_num_extra, m_table_sizes[TINFL_MAX_HUFF_TABLES];
  tinfl_bit_buf_t m_bit_buf;
  size_t m_dist_from_out_buf_start;
  tinfl_huff_table m_tables[TINFL_MAX_HUFF_TABLES];
  mz_uint8 m_raw_header[4], m_len_codes[TINFL_MAX_HUFF_SYMBOLS_0 + TINFL_MAX_HUFF_SYMBOLS_1 + 137];
};

typedef struct tinfl_decompressor_tag tinfl_decompressor;

// Works around MSVC's spammy "warning C4127: conditional expression is constant" message.
#ifdef _MSC_VER
  #define MZ_MACRO_END while (0, 0)
#else
  #define MZ_MACRO_END while (0)
#endif

// Initializes the decompressor to its initial state.
#define tinfl_init(r) do { (r)->m_state = 0; } MZ_MACRO_END

tinfl_status tinfl_decompress(tinfl_decompressor *r, 
  const mz_uint8 *pIn_buf_next, size_t *pIn_buf_size, 
  mz_uint8 *pOut_buf_start, mz_uint8 *pOut_buf_next, size_t *pOut_buf_size, 
  const mz_uint32 decomp_flags);

#endif // _TINFL_H_
