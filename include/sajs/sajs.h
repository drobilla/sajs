// Copyright 2017-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SAJS_SAJS_H
#define SAJS_SAJS_H

#include <stddef.h>

/**
   @defgroup sajs Sajs
   A minimal streaming JSON reader and writer.
   @{
*/

// SAJS_API exposes symbols in the public API
#ifndef SAJS_API
#  if defined(_WIN32) && !defined(SAJS_STATIC) && defined(SAJS_INTERNAL)
#    define SAJS_API __declspec(dllexport)
#  elif defined(_WIN32) && !defined(SAJS_STATIC)
#    define SAJS_API __declspec(dllimport)
#  elif defined(__GNUC__)
#    define SAJS_API __attribute__((visibility("default")))
#  else
#    define SAJS_API
#  endif
#endif

// GCC function attributes
#ifdef __GNUC__
#  define SAJS_PURE_FUNC __attribute__((pure))
#  define SAJS_CONST_FUNC __attribute__((const))
#else
#  define SAJS_PURE_FUNC  ///< Only reads memory
#  define SAJS_CONST_FUNC ///< Only reads its parameters
#endif

// Clang nullability attributes
#if defined(__clang__) && __clang_major__ >= 7
#  define SAJS_NONNULL _Nonnull
#  define SAJS_ALLOCATED _Null_unspecified
#else
#  define SAJS_NONNULL   ///< A non-null pointer
#  define SAJS_ALLOCATED ///< A new pointer (only null if out of memory)
#endif

/** Status code. */
typedef enum {
  SAJS_SUCCESS,                ///< Success
  SAJS_FAILURE,                ///< Non-fatal failure
  SAJS_RETRY,                  ///< Operation not completed
  SAJS_NO_DATA,                ///< Unexpected end of input
  SAJS_OVERFLOW,               ///< Stack overflow
  SAJS_UNDERFLOW,              ///< Stack underflow
  SAJS_BAD_WRITE,              ///< Failed write
  SAJS_EXPECTED_COLON,         ///< Expected ':'
  SAJS_EXPECTED_COMMA,         ///< Expected ','
  SAJS_EXPECTED_CONTINUATION,  ///< Expected UTF-8 continuation byte
  SAJS_EXPECTED_DECIMAL,       ///< Expected '.'
  SAJS_EXPECTED_DIGIT,         ///< Expected digit
  SAJS_EXPECTED_EXPONENT,      ///< Expected '+', '-', or digit
  SAJS_EXPECTED_HEX,           ///< Expected 0-9 or A-F or a-f
  SAJS_EXPECTED_LITERAL,       ///< Expected "false", "null", or "true"
  SAJS_EXPECTED_PRINTABLE,     ///< Expected printable character
  SAJS_EXPECTED_QUOTE,         ///< Expected '"'
  SAJS_EXPECTED_STRING_ESCAPE, ///< Expected string escape
  SAJS_EXPECTED_UTF16_HI,      ///< Expected UTF-16 high surrogate escape
  SAJS_EXPECTED_UTF16_LO,      ///< Expected UTF-16 low surrogate escape
  SAJS_EXPECTED_UTF8,          ///< Expected valid UTF-8 bytes
  SAJS_EXPECTED_VALUE,         ///< Expected value
} SajsStatus;

#define SAJS_NUM_STATUS 22U ///< The number of SajsStatus entries

/**
   Kind of JSON value.

   A JSON value is an object, array, string, number, or one of three special
   literals: false, null, and true.
*/
typedef enum {
  SAJS_OBJECT  = 1U, ///< Object container
  SAJS_ARRAY   = 2U, ///< Array container
  SAJS_STRING  = 3U, ///< String value
  SAJS_NUMBER  = 4U, ///< Number value
  SAJS_LITERAL = 5U, ///< Literal value (false, null, or true)
} SajsValueKind;

/**
   Flags describing an event and/or value.
*/
typedef enum {
  SAJS_IS_MEMBER_NAME  = 1U << 0U, ///< Object member name
  SAJS_IS_MEMBER_VALUE = 1U << 1U, ///< Object member value
  SAJS_IS_ELEMENT      = 1U << 2U, ///< Array element
  SAJS_IS_FIRST        = 1U << 3U, ///< First element or member in container
  SAJS_HAS_BYTES       = 1U << 4U, ///< Event has bytes
} SajsFlag;

/** Bitwise OR of SajsFlag values. */
typedef unsigned SajsFlags;

typedef enum {
  /**
     Ignored input.

     Many characters, like whitespace, produce no output when read.
  */
  SAJS_EVENT_NOTHING,

  /**
     The start of any value.

     May include character bytes for numbers or literals.
  */
  SAJS_EVENT_START,

  /**
     The end of any value.

     May include character bytes for numbers or literals.
  */
  SAJS_EVENT_END,

  /**
     The end of both a value and its container.

     This happens when a single character, '}' or ']', ends both the current
     number/literal and the object/array it's in.  The `kind` will be set to
     the kind of the container (the kind of the number/literal is implicit).
  */
  SAJS_EVENT_DOUBLE_END,

  /**
     Character bytes for a string, number, or literal.

     One or more of these may occur after the start, and before the end, of a
     string, number, or literal.  Each event represents one character, given as
     up to four bytes in UTF-8 encoding.
  */
  SAJS_EVENT_BYTES,
} SajsEvent;

/**
   Result of reading some input.

   This describes both the status of the read operation, and the output
   produced, if any.  If the `event` field isn't #SAJS_EVENT_NOTHING, then the
   trailing fields describe the event.
*/
typedef struct {
  SajsStatus    status : 8; ///< Status of operation
  SajsEvent     event : 8;  ///< Event produced
  SajsValueKind kind : 8;   ///< Start/end event value kind
  SajsFlags     flags : 8;  ///< Start event value flags
} SajsResult;

/**
   JSON reader state.

   This is a few words of lexer state which is followed by a stack.
*/
typedef struct SajsLexerImpl SajsLexer;

/**
   Return a string describing a status code.

   Returns a non-null pointer to a static constant string, in English,
   capitalized without a trailing period.
*/
SAJS_API SAJS_CONST_FUNC char const* SAJS_NONNULL
sajs_strerror(SajsStatus st);

/**
   Set up a JSON lexer in provided memory.

   The initial few bytes of the memory will be used for lexer state, and
   following memory will be used as a stack.  One byte of stack is needed for
   each level of value nesting in the input.

   The memory must be word-aligned and at least 48 bytes.  NULL is returned if
   not enough space is available.
*/
SAJS_API SajsLexer* SAJS_ALLOCATED
sajs_lexer_init(size_t mem_size, void* SAJS_NONNULL mem);

/**
   Read one byte and return any result.

   Accepts a single character as an int, with -1 representing EOF, as with
   `fgetc`.  The returned struct includes the `status`, and possibly an `event`
   if reading the character produced output.
*/
SAJS_API SajsResult
sajs_read_byte(SajsLexer* SAJS_NONNULL lexer, int c);

/**
   A view of an immutable string slice with a length.
*/
typedef struct {
  char const* SAJS_NONNULL data;   ///< Pointer to the first character
  size_t                   length; ///< Length of string in bytes
} SajsStringView;

/**
   Return a view of the bytes for the last read character.

   If the last event returned from #sajs_read_byte indicates that bytes are
   available, this function will return them until the state is
   changed by reading another character, or is reset.
*/
SAJS_API SAJS_PURE_FUNC SajsStringView
sajs_string(SajsLexer const* SAJS_NONNULL lexer);

/**
   JSON writer state.

   This is a few words of writer state.
*/
typedef struct SajsWriterImpl SajsWriter;

/**
   Set up a JSON writer in provided memory.

   The writer uses a small fixed amount of memory.  The memory must be
   word-aligned and at least 32 bytes.  NULL is returned if not enough space is
   available.
*/
SAJS_API SajsWriter* SAJS_ALLOCATED
sajs_writer_init(size_t mem_size, void* SAJS_NONNULL mem);

/**
   A prefix of some text output.
*/
typedef enum {
  SAJS_PREFIX_NONE,
  SAJS_PREFIX_OBJECT_START, ///< Space before first object element
  SAJS_PREFIX_ARRAY_START,  ///< Space before first array element
  SAJS_PREFIX_OBJECT_END,   ///< Space before object end brace
  SAJS_PREFIX_ARRAY_END,    ///< Space before array end bracket
  SAJS_PREFIX_MEMBER_COLON, ///< Colon before member value
  SAJS_PREFIX_MEMBER_COMMA, ///< Comma before following member name
  SAJS_PREFIX_ARRAY_COMMA,  ///< Comma before following array element
} SajsTextPrefix;

/**
   Text output produced by writing a result.

   This describes both the status of the write operation, and the fragment of
   text emitted, if any.  To allow streaming arbitrary data in constant memory,
   an output has two optional prefixes: a leading comma, and a newline after
   that.  This prefix is followed by a string of UTF-8 bytes.

   This way, the indentation level is handled by the caller, and there's no
   need for a contiguous string that doesn't exist in a token (including
   whitespace for indentation, which can be arbitrarily long).
*/
typedef struct {
  SajsStatus               status; ///< Status of write operation
  unsigned                 indent; ///< Indent level (nested container count)
  size_t                   length; ///< Length of bytes
  char const* SAJS_NONNULL bytes;  ///< UTF-8 bytes
  SajsTextPrefix           prefix; ///< Text prefix (before bytes)
} SajsTextOutput;

/**
   Write a lexed result as a JSON text fragment.

   If the returned output has `status` #SAJS_SUCCESS, then it represents some
   bytes of text output.  To write this as a flat stream of bytes, the caller
   must write the prefix (which may have a pre-defined delimiter, followed by
   optional whitespace), then write the `length` UTF-8 `bytes.
*/
SAJS_API SajsTextOutput
sajs_write_result(SajsWriter* SAJS_NONNULL writer,
                  SajsResult               r,
                  SajsStringView           string);

/**
   @}
*/

#endif /* SAJS_SAJS_H */
