// Copyright 2017-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SAJS_SAJS_H
#define SAJS_SAJS_H

#include <stddef.h>
#include <stdint.h>

/**
   @defgroup sajs Sajs
   A lightweight streaming SAX-like JSON parser.
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

/** Status code. */
typedef enum {
  SAJS_SUCCESS,                ///< Success
  SAJS_FAILURE,                ///< Non-fatal failure
  SAJS_RETRY,                  ///< Operation not completed
  SAJS_NO_DATA,                ///< Unexpected end of input
  SAJS_OVERFLOW,               ///< Stack overflow
  SAJS_UNDERFLOW,              ///< Stack underflow
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

     Many characters, like whitespace, produce no output when parsed.
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
   Result of parsing some input.

   This is the result returned by parsing, which describes both the status of
   the operation, and the output produced, if any.  If the `event` field isn't
   #SAJS_EVENT_NOTHING, then the trailing fields describe the event.
*/
typedef struct {
  SajsStatus    status : 8; ///< Status of operation
  SajsEvent     event : 8;  ///< Event produced
  SajsValueKind kind : 8;   ///< Start/end event value kind
  SajsFlags     flags : 8;  ///< Start event value flags
} SajsResult;

/**
   JSON parser state.

   This small amount of opaque parser state is followed by the parsing stack.
*/
typedef struct SajsLexerImpl SajsLexer;

/**
   Return a string describing a status code.

   Returns a non-null pointer to a static constant string, in English,
   capitalized without a trailing period.
*/
SAJS_API char const*
sajs_strerror(SajsStatus st);

/**
   Set up a JSON parser in provided memory.

   The initial few bytes of the memory will be used for lexer state, and
   following memory will be used as a stack.  One byte of stack is needed for
   each level of value nesting in the input.
*/
SAJS_API SajsLexer*
sajs_init(size_t mem_size, void* mem);

/**
   Parse one byte and return any result.

   Accepts a single character as an int, with -1 representing EOF, as with
   `fgetc`.  The returned struct includes the `status`, and possibly an `event`
   if parsing the character produced output.
*/
SAJS_API SajsResult
sajs_read_byte(SajsLexer* lexer, int c);

/**
   Return a pointer to the bytes for the last parsed character.

   If the last event returned from #sajs_read_byte indicates that bytes are
   available, this function will return a pointer to them until the state is
   changed by reading another character or reset.
*/
SAJS_API uint8_t const*
sajs_bytes(SajsLexer const* lexer);

/**
   @}
*/

#endif /* SAJS_SAJS_H */
