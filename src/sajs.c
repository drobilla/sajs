// Copyright 2017-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "sajs/sajs.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * Static Array Sizes (Must be kept in sync!)
 */

#define SAJS_NUM_STATES 24U ///< The number of SajsState entries
#define SAJS_NUM_STATUS 21U ///< The number of SajsStatus entries

/*
 * Types
 */

/// A lexer state expecting a certain input
typedef enum {
  STATE_START             = 0U,  ///< Start state: read any value
  STATE_ELEM_FIRST        = 1U,  ///< Start of first element in an array
  STATE_ELEM_SEP          = 2U,  ///< Value separator in an array (',')
  STATE_ELEM_NEXT         = 3U,  ///< Start of following element in an array
  STATE_MEM_NAME_FIRST    = 4U,  ///< Start of first key in object ('"')
  STATE_MEM_NAME_SEP      = 5U,  ///< Member name separator (':')
  STATE_MEM_VALUE_START   = 6U,  ///< Start of member value
  STATE_MEM_SEP           = 7U,  ///< Value separator in an object (',')
  STATE_MEM_NEXT          = 8U,  ///< Start of following member
  STATE_STRING            = 9U,  ///< Character in string
  STATE_STRING_ESC        = 10U, ///< Escape in string (after '\')
  STATE_STRING_ESC_HEX    = 11U, ///< Hex escape in string (after '\u')
  STATE_STRING_ESC_LO     = 12U, ///< Low UTF-16 surrogate hex escape
  STATE_NUM_INT_START     = 13U, ///< First digit of number (after sign)
  STATE_NUM_INT_CONT      = 14U, ///< Digit before decimal point
  STATE_NUM_INT_END       = 15U, ///< First character after integer
  STATE_NUM_FRAC_START    = 16U, ///< First digit after decimal point
  STATE_NUM_FRAC_CONT     = 17U, ///< Following digit after decimal point
  STATE_NUM_EXP_START     = 18U, ///< First character of exponent
  STATE_NUM_EXP_INT_START = 19U, ///< First digit of exponent (after sign)
  STATE_NUM_EXP_INT_CONT  = 20U, ///< Digit of exponent
  STATE_FALSE             = 21U, ///< Character in "false" literal
  STATE_NULL              = 22U, ///< Character in "null" literal
  STATE_TRUE              = 23U, ///< Character in "true" literal
} SajsState;

/// Lexer stack frame
typedef uint8_t SajsFrame;

/// Lexer state (followed by stack memory)
struct SajsLexerImpl {
  size_t    max_depth; ///< Maximum stack depth
  size_t    top;       ///< Current top of stack
  uint32_t  value;     ///< Temporary working value
  uint32_t  length;    ///< Temporary working length
  SajsFlags flags;     ///< Pending flags for the top frame
  uint8_t   bytes[4];  ///< Bytes for current event
};

/*
 * Status
 */

static char const* const sajs_status_strings[SAJS_NUM_STATUS] = {
  "Success",
  "Non-fatal failure",
  "Reached end of value",
  "Unexpected end of input",
  "Stack overflow",
  "Stack underflow",
  "Expected ':'",
  "Expected ','",
  "Expected continuation byte",
  "Expected '.'",
  "Expected digit",
  "Expected '+', '-', or digit",
  "Expected 0-9 or A-F or a-f",
  "Expected false, null, or true",
  "Expected printable character",
  "Expected '\"'",
  "Expected string escape",
  "Expected high surrogate escape",
  "Expected low surrogate escape",
  "Expected valid UTF-8 byte",
  "Expected value",
};

char const*
sajs_strerror(SajsStatus const st)
{
  return (unsigned)st < SAJS_NUM_STATUS ? sajs_status_strings[(unsigned)st]
                                        : "Unknown error";
}

/*
 * Utilities
 */

/* Character Classification */

static bool
is_digit(int const c)
{
  return c >= '0' && c <= '9';
}

static bool
is_xdigit(int const c)
{
  return is_digit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static bool
is_space(int const c)
{
  return c == '\t' || c == '\n' || c == '\r' || c == ' ';
}

/* Text Encoding */

static uint32_t
utf16_surrogates_codepoint(uint16_t const high, uint16_t const low)
{
  return ((high - 0xD800U) * 0x400U) + (low - 0xDC00U) + 0x10000U;
}

static inline uint8_t
utf8_num_bytes_for_codepoint(uint32_t const code)
{
  return (code < 0x00000080U)   ? 1U
         : (code < 0x00000800U) ? 2U
         : (code < 0x00010000U) ? 3U
         : (code < 0x00110000U) ? 4U
                                : 0U;
}

static uint8_t
utf8_from_codepoint(uint8_t out[4], uint32_t const code)
{
  static uint32_t const masks[4] = {0x0U, 0xC0U, 0x800U, 0x10000U};

  uint8_t const size = utf8_num_bytes_for_codepoint(code);
  if (size) {
    uint32_t c = code;
    for (uint8_t s = size; --s;) {
      out[s] = (uint8_t)(0x80U | (c & 0x3FU));
      c >>= 6U;
      c |= masks[s];
    }

    out[0] = (uint8_t)c;
  }

  return size;
}

static uint8_t
hex_nibble(uint8_t const c)
{
  return (uint8_t)(is_digit(c)              ? (c - '0')
                   : (c >= 'A' && c <= 'F') ? (10 + c - 'A')
                   : (c >= 'a' && c <= 'f') ? (10 + c - 'a')
                                            : UINT8_MAX);
}

/*
 * Lexer State
 */

static SajsFrame*
top_frame(SajsLexer* const lexer)
{
  SajsFrame* const stack = (SajsFrame*)(lexer + 1U);
  return &stack[lexer->top];
}

SajsLexer*
sajs_init(size_t const mem_size, void* const mem)
{
  if (mem_size < sizeof(SajsLexer) + sizeof(SajsFrame)) {
    return NULL;
  }

  SajsLexer* const lexer      = (SajsLexer*)mem;
  size_t const     stack_size = mem_size - sizeof(SajsLexer);

  lexer->max_depth  = stack_size / sizeof(SajsFrame);
  lexer->top        = 0U;
  lexer->value      = 0U;
  lexer->length     = 0U;
  lexer->flags      = 0U;
  *top_frame(lexer) = STATE_START;

  return lexer;
}

/*
 * Results
 */

/* Simple Data */

static SajsResult
do_nothing(SajsStatus const st)
{
  SajsResult const r = {st, SAJS_EVENT_NOTHING, (SajsValueKind)0U, 0U};
  return r;
}

static SajsResult
do_byte(SajsLexer* const lexer, uint8_t const c)
{
  lexer->bytes[0]    = c;
  lexer->bytes[1]    = '\0';
  SajsResult const r = {
    SAJS_SUCCESS, SAJS_EVENT_BYTES, (SajsValueKind)0U, SAJS_HAS_BYTES};
  return r;
}

static SajsResult
do_utf8_bytes(SajsLexer* const lexer, uint32_t const codepoint)
{
  uint8_t const len = utf8_from_codepoint(lexer->bytes, codepoint);
  if (!len) {
    return do_nothing(SAJS_EXPECTED_UTF8);
  }

  SajsResult const r = {
    SAJS_SUCCESS, SAJS_EVENT_BYTES, SAJS_STRING, SAJS_HAS_BYTES};
  return r;
}

/* Stack Changes */

static SajsResult
push(SajsLexer* const    lexer,
     SajsValueKind const kind,
     SajsFlags const     flags,
     SajsState const     state,
     uint8_t const       first)
{
  if (lexer->top + 1U >= lexer->max_depth) {
    return do_nothing(SAJS_OVERFLOW);
  }

  SajsFrame* const stack = (SajsFrame*)(lexer + 1U);
  SajsFrame* const frame = &stack[++lexer->top];

  *frame          = (SajsFrame)state;
  lexer->flags    = flags;
  lexer->length   = first ? 1U : 0U;
  lexer->bytes[0] = first;
  lexer->bytes[1] = 0U;

  const SajsFlags  rflags = flags | (first ? SAJS_HAS_BYTES : 0U);
  SajsResult const r = {SAJS_SUCCESS, SAJS_EVENT_START, kind, (uint8_t)rflags};
  return r;
}

static SajsResult
pop(SajsLexer* const    lexer,
    SajsValueKind const kind,
    SajsStatus const    success_status,
    uint8_t const       last)
{
  lexer->bytes[0] = last;
  lexer->bytes[1] = 0U;

  uint8_t const flags = last ? (uint8_t)SAJS_HAS_BYTES : 0U;
  SajsResult    r     = {SAJS_UNDERFLOW, SAJS_EVENT_END, kind, flags};

  if (lexer->top) {
    --lexer->top;
    r.status = success_status;
  }

  lexer->length = 0U;
  lexer->flags  = 0U;
  return r;
}

/* State Transitions */

/// Reset the top frame and flags, return success
static SajsResult
do_reset(SajsLexer* const lexer,
         SajsFrame* const frame,
         SajsState const  state,
         SajsFlags const  flags)
{
  *frame       = (SajsFrame)state;
  lexer->flags = flags;
  return do_nothing(SAJS_SUCCESS);
}

/// Change the top frame to a new state, return success
static SajsResult
do_change(SajsFrame* const frame, SajsState const state)
{
  *frame = (SajsFrame)state;
  return do_nothing(SAJS_SUCCESS);
}

/// Change the top frame to a new state, if a result was successful
static SajsResult
do_change_if(SajsFrame* const frame, SajsState const state, SajsResult const r)
{
  if (!r.status) {
    *frame = (SajsFrame)state;
  }

  return r;
}

/// Change the top frame to a new state, and produce a byte
static SajsResult
do_byte_change(SajsLexer* const lexer,
               SajsFrame* const frame,
               SajsState const  state,
               uint8_t const    c)
{
  *frame = (SajsFrame)state;
  return do_byte(lexer, c);
}

/*
 * Character Handlers
 */

/* Values */

static SajsResult
eat_value(SajsLexer* const lexer, SajsFrame* const frame, uint8_t const c)
{
  (void)frame;

  switch (c) {
  case '\"':
    return push(lexer, SAJS_STRING, lexer->flags, STATE_STRING, 0);
  case '-':
    return push(lexer, SAJS_NUMBER, lexer->flags, STATE_NUM_INT_START, c);
  case '0':
    return push(lexer, SAJS_NUMBER, lexer->flags, STATE_NUM_INT_END, c);
  case '[':
    return push(lexer, SAJS_ARRAY, SAJS_IS_FIRST, STATE_ELEM_FIRST, 0);
  case '{':
    return push(lexer, SAJS_OBJECT, SAJS_IS_FIRST, STATE_MEM_NAME_FIRST, 0);
  case 'f':
    return push(lexer, SAJS_LITERAL, lexer->flags, STATE_FALSE, c);
  case 'n':
    return push(lexer, SAJS_LITERAL, lexer->flags, STATE_NULL, c);
  case 't':
    return push(lexer, SAJS_LITERAL, lexer->flags, STATE_TRUE, c);
  default:
    break;
  }

  return is_digit(c)
           ? push(lexer, SAJS_NUMBER, lexer->flags, STATE_NUM_INT_CONT, c)
           : do_nothing(SAJS_EXPECTED_VALUE);
}

/* Arrays */

static SajsResult
eat_elem_first(SajsLexer* const lexer, SajsFrame* const frame, uint8_t const c)
{
  return (c == ']')
           ? pop(lexer, SAJS_ARRAY, SAJS_SUCCESS, 0U)
           : do_change_if(frame, STATE_ELEM_SEP, eat_value(lexer, frame, c));
}

static SajsResult
eat_elem_sep(SajsLexer* const lexer, SajsFrame* const frame, uint8_t const c)
{
  return (c == ']')   ? pop(lexer, SAJS_ARRAY, SAJS_SUCCESS, 0U)
         : (c == ',') ? do_reset(lexer, frame, STATE_ELEM_NEXT, SAJS_IS_ELEMENT)
                      : do_nothing(SAJS_EXPECTED_COMMA);
}

static SajsResult
eat_elem_next(SajsLexer* const lexer, SajsFrame* const frame, uint8_t const c)
{
  return do_change_if(frame, STATE_ELEM_SEP, eat_value(lexer, frame, c));
}

/* Objects */

static SajsResult
eat_mem_name_first(SajsLexer* const lexer,
                   SajsFrame* const frame,
                   uint8_t const    c)
{
  return (c == '}')   ? pop(lexer, SAJS_OBJECT, SAJS_SUCCESS, 0U)
         : (c != '"') ? do_nothing(SAJS_EXPECTED_QUOTE)
                      : do_change_if(frame,
                                     STATE_MEM_NAME_SEP,
                                     push(lexer,
                                          SAJS_STRING,
                                          SAJS_IS_FIRST | SAJS_IS_MEMBER_NAME,
                                          STATE_STRING,
                                          0));
}

static SajsResult
eat_mem_name_sep(SajsLexer* const lexer,
                 SajsFrame* const frame,
                 uint8_t const    c)
{
  return (c == ':')
           ? do_reset(lexer, frame, STATE_MEM_VALUE_START, SAJS_IS_MEMBER_VALUE)
           : do_nothing(SAJS_EXPECTED_COLON);
}

static SajsResult
eat_mem_value_start(SajsLexer* const lexer,
                    SajsFrame* const frame,
                    uint8_t const    c)
{
  return do_change_if(frame, STATE_MEM_SEP, eat_value(lexer, frame, c));
}

static SajsResult
eat_mem_sep(SajsLexer* const lexer, SajsFrame* const frame, uint8_t const c)
{
  return (c == ',')   ? do_reset(lexer, frame, STATE_MEM_NEXT, 0U)
         : (c == '}') ? pop(lexer, SAJS_OBJECT, SAJS_SUCCESS, 0U)
                      : do_nothing(SAJS_EXPECTED_COMMA);
}

static SajsResult
eat_mem_next(SajsLexer* const lexer, SajsFrame* const frame, uint8_t const c)
{
  return (c == '\"')
           ? do_change_if(
               frame,
               STATE_MEM_NAME_SEP,
               push(lexer, SAJS_STRING, SAJS_IS_MEMBER_NAME, STATE_STRING, 0))
           : do_nothing(SAJS_EXPECTED_QUOTE);
}

/* Strings */

static SajsResult
eat_string(SajsLexer* const lexer, SajsFrame* const frame, uint8_t const c)
{
  return (c == '\"')   ? pop(lexer, SAJS_STRING, SAJS_SUCCESS, 0U)
         : (c == '\\') ? do_change(frame, STATE_STRING_ESC)
         : (c < ' ')   ? pop(lexer, SAJS_STRING, SAJS_EXPECTED_PRINTABLE, 0U)
                       : do_byte(lexer, c);
}

static SajsResult
eat_string_esc(SajsLexer* const lexer, SajsFrame* const frame, uint8_t const c)
{
  switch (c) {
  case '\"':
  case '/':
  case '\\':
    return do_byte_change(lexer, frame, STATE_STRING, c);
  case 'b':
    return do_byte_change(lexer, frame, STATE_STRING, '\b');
  case 'f':
    return do_byte_change(lexer, frame, STATE_STRING, '\f');
  case 'n':
    return do_byte_change(lexer, frame, STATE_STRING, '\n');
  case 'r':
    return do_byte_change(lexer, frame, STATE_STRING, '\r');
  case 't':
    return do_byte_change(lexer, frame, STATE_STRING, '\t');
  case 'u':
    return do_change(frame, STATE_STRING_ESC_HEX);
  default:
    break;
  }

  return do_nothing(SAJS_EXPECTED_STRING_ESCAPE);
}

static SajsResult
eat_string_esc_hex(SajsLexer* const lexer,
                   SajsFrame* const frame,
                   uint8_t const    c)
{
  if (!is_xdigit(c)) {
    return do_nothing(SAJS_EXPECTED_HEX);
  }

  // Append this nibble to the value
  lexer->value = (lexer->length ? (lexer->value << 4U) : 0U) | hex_nibble(c);
  if (++lexer->length < 4U) {
    return do_nothing(SAJS_SUCCESS); // Still reading hex digits
  }

  if (lexer->value >= 0xDC00U && lexer->value <= 0xDFFFU) {
    return do_nothing(SAJS_EXPECTED_UTF16_HI); // Lone low surrogate
  }

  if (lexer->value >= 0xD800U && lexer->value <= 0xDBFFU) {
    // High surrogate, wait for following low surrogate escape
    return do_change(frame, STATE_STRING_ESC_LO);
  }

  // Emit UTF-8 character and return to normal string state
  SajsResult const r = do_utf8_bytes(lexer, lexer->value);
  lexer->length      = 0U;
  *frame             = STATE_STRING;
  return r;
}

static SajsResult
eat_string_esc_lo(SajsLexer* const lexer,
                  SajsFrame* const frame,
                  uint8_t const    c)
{
  if ((lexer->length == 4U && c == '\\') || (lexer->length == 5U && c == 'u')) {
    ++lexer->length;
    return do_nothing(SAJS_SUCCESS);
  }

  if (!is_xdigit(c)) {
    return do_nothing(SAJS_EXPECTED_HEX);
  }

  // Append this nibble to the value
  lexer->value = (lexer->value << 4U) | hex_nibble(c);
  if (++lexer->length < 10U) {
    return do_nothing(SAJS_SUCCESS); // Still reading hex digits
  }

  // Combine surrogates into codepoint
  uint16_t const hi        = (uint16_t)((lexer->value & 0xFFFF0000U) >> 16U);
  uint16_t const lo        = lexer->value & 0x0000FFFFU;
  uint32_t const codepoint = utf16_surrogates_codepoint(hi, lo);
  if (lo < 0xDC00U || lo > 0xDFFFU) {
    return do_nothing(SAJS_EXPECTED_UTF16_LO);
  }

  // Emit UTF-8 character and return to normal string state
  SajsResult const r = do_utf8_bytes(lexer, codepoint);
  lexer->length      = 0U;
  *frame             = STATE_STRING;
  return r;
}

/* Numbers */

static bool
is_number_end(uint8_t const c)
{
  return is_space(c) || c == ',' || c == ']' || c == '}';
}

static SajsResult
eat_num_int_start(SajsLexer* const lexer,
                  SajsFrame* const frame,
                  uint8_t const    c)
{
  return (c == '0')    ? do_byte_change(lexer, frame, STATE_NUM_INT_END, c)
         : is_digit(c) ? do_byte_change(lexer, frame, STATE_NUM_INT_CONT, c)
                       : do_nothing(SAJS_EXPECTED_DIGIT);
}

static SajsResult
eat_num_int_end(SajsLexer* const lexer, SajsFrame* const frame, uint8_t const c)
{
  return is_number_end(c) ? pop(lexer, SAJS_NUMBER, SAJS_RETRY, 0U)
         : (c == '.') ? do_byte_change(lexer, frame, STATE_NUM_FRAC_START, c)
         : (c == 'E' || c == 'e')
           ? do_byte_change(lexer, frame, STATE_NUM_EXP_START, c)
           : do_nothing(SAJS_EXPECTED_DECIMAL);
}

static SajsResult
eat_num_int_cont(SajsLexer* const lexer,
                 SajsFrame* const frame,
                 uint8_t const    c)
{
  return is_digit(c) ? do_byte(lexer, c) : eat_num_int_end(lexer, frame, c);
}

static SajsResult
eat_num_frac_start(SajsLexer* const lexer,
                   SajsFrame* const frame,
                   uint8_t const    c)
{
  return is_digit(c) ? do_byte_change(lexer, frame, STATE_NUM_FRAC_CONT, c)
                     : do_nothing(SAJS_EXPECTED_DIGIT);
}

static SajsResult
eat_num_frac_cont(SajsLexer* const lexer,
                  SajsFrame* const frame,
                  uint8_t const    c)
{
  return is_digit(c)  ? do_byte(lexer, c)
         : (c == 'e') ? do_byte_change(lexer, frame, STATE_NUM_EXP_START, c)
                      : pop(lexer, SAJS_NUMBER, SAJS_RETRY, 0U);
}

static SajsResult
eat_num_exp_int_start(SajsLexer* const lexer,
                      SajsFrame* const frame,
                      uint8_t const    c)
{
  return is_digit(c) ? do_byte_change(lexer, frame, STATE_NUM_EXP_INT_CONT, c)
                     : do_nothing(SAJS_EXPECTED_DIGIT);
}

static SajsResult
eat_num_exp_start(SajsLexer* const lexer,
                  SajsFrame* const frame,
                  uint8_t const    c)
{
  return (c == '+' || c == '-')
           ? do_byte_change(lexer, frame, STATE_NUM_EXP_INT_START, c)
           : eat_num_exp_int_start(lexer, frame, c);
}

static SajsResult
eat_num_exp_int_cont(SajsLexer* const lexer,
                     SajsFrame* const frame,
                     uint8_t const    c)
{
  (void)frame;
  return is_digit(c)        ? do_byte(lexer, c)
         : is_number_end(c) ? pop(lexer, SAJS_NUMBER, SAJS_RETRY, 0U)
                            : pop(lexer, SAJS_NUMBER, SAJS_EXPECTED_DIGIT, 0U);
}

/* Literals */

static SajsResult
eat_literal(SajsLexer* const  lexer,
            SajsFrame* const  frame,
            size_t const      length,
            char const* const string,
            uint8_t const     c)
{
  (void)frame;
  return ((char)c != string[lexer->length]) ? do_nothing(SAJS_EXPECTED_LITERAL)
         : (++lexer->length == length)
           ? pop(lexer, SAJS_LITERAL, SAJS_SUCCESS, c)
           : do_byte(lexer, c);
}

static SajsResult
eat_false(SajsLexer* const lexer, SajsFrame* const frame, uint8_t const c)
{
  return eat_literal(lexer, frame, 5U, "false", c);
}

static SajsResult
eat_null(SajsLexer* const lexer, SajsFrame* const frame, uint8_t const c)
{
  return eat_literal(lexer, frame, 4U, "null", c);
}

static SajsResult
eat_true(SajsLexer* const lexer, SajsFrame* const frame, uint8_t const c)
{
  return eat_literal(lexer, frame, 4U, "true", c);
}

/*
 * Interface
 */

static SajsResult
sajs_process_byte(SajsLexer* const lexer, int const c)
{
  SajsFrame* const frame = top_frame(lexer);
  SajsState const  state = (SajsState)*frame;

  if (c < 0) {
    // NUM_INT_CONT, NUM_INT_END, NUM_FRAC_CONT, NUM_EXP_INT_CONT
    static uint32_t const last_digit_mask = 0x0012C000U;
    return (lexer->top == 1U && ((1U << state) & last_digit_mask))
             ? pop(lexer, SAJS_NUMBER, SAJS_SUCCESS, 0U)
             : do_nothing(state ? SAJS_NO_DATA : SAJS_FAILURE);
  }

  if (is_space(c) && state <= STATE_MEM_NEXT) {
    return do_nothing(SAJS_SUCCESS);
  }

  typedef SajsResult (*const SajsEatFunc)(SajsLexer*, SajsFrame*, uint8_t c);

  static SajsEatFunc handlers[SAJS_NUM_STATES] = {
    eat_value,
    eat_elem_first,
    eat_elem_sep,
    eat_elem_next,
    eat_mem_name_first,
    eat_mem_name_sep,
    eat_mem_value_start,
    eat_mem_sep,
    eat_mem_next,
    eat_string,
    eat_string_esc,
    eat_string_esc_hex,
    eat_string_esc_lo,
    eat_num_int_start,
    eat_num_int_cont,
    eat_num_int_end,
    eat_num_frac_start,
    eat_num_frac_cont,
    eat_num_exp_start,
    eat_num_exp_int_start,
    eat_num_exp_int_cont,
    eat_false,
    eat_null,
    eat_true,
  };

  return handlers[state](lexer, frame, (uint8_t)c);
}

SajsResult
sajs_read_byte(SajsLexer* const lexer, int const c)
{
  SajsResult r = sajs_process_byte(lexer, c);

  if (r.status == SAJS_RETRY) {
    SajsResult s = sajs_process_byte(lexer, c);
    r.status     = s.status;
    if (r.event == SAJS_EVENT_END && s.event == SAJS_EVENT_END) {
      r.type  = s.type;
      r.event = SAJS_EVENT_DOUBLE_END;
    }
  }

  return r;
}

uint8_t const*
sajs_bytes(SajsLexer const* const lexer)
{
  return lexer->bytes;
}
