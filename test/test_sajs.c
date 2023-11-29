// Copyright 2017-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "sajs/sajs.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_MEM_BYTES 1024U

// "Global" test state passed as user data to callbacks
typedef struct {
  FILE*         in_stream;  ///< Input stream
  FILE*         out_stream; ///< Output stream
  SajsLexer*    lexer;      ///< Lexer for reading input stream
  unsigned      num_values; ///< Number of top-level values parsed
  unsigned      depth;      ///< Stack depth
  SajsValueKind top_kind;   ///< Kind for current value
  SajsFlags     top_flags;  ///< Flags for current string/number/literal
} TestState;

// Write a single byte to the output
static SajsStatus
stream_write(uint8_t const byte, FILE* const stream)
{
  return fwrite(&byte, 1U, 1U, stream) == 1U ? SAJS_SUCCESS : SAJS_FAILURE;
}

// Write a two-byte string escape (backslash then a letter) to the output
static SajsStatus
stream_write_escape(uint8_t const letter, FILE* const stream)
{
  return (fwrite("\\", 1U, 1U, stream) + fwrite(&letter, 1U, 1U, stream) == 2U)
           ? SAJS_SUCCESS
           : SAJS_FAILURE;
}

// Called when a value is started
static SajsStatus
on_start(TestState* const    state,
         SajsValueKind const kind,
         SajsFlags const     flags)
{
  SajsStatus  st     = SAJS_SUCCESS;
  FILE* const stream = state->out_stream;
  if ((flags & (SAJS_IS_MEMBER_NAME | SAJS_IS_ELEMENT)) &&
      !(flags & SAJS_IS_FIRST)) {
    st = stream_write(',', stream);
  }

  switch (kind) {
  case SAJS_OBJECT:
    st = stream_write('{', stream);
    break;
  case SAJS_ARRAY:
    st = stream_write('[', stream);
    break;
  case SAJS_STRING:
    st = stream_write('\"', stream);
    break;
  case SAJS_NUMBER:
  case SAJS_LITERAL:
    break;
  }

  ++state->depth;
  state->top_kind  = kind;
  state->top_flags = flags;
  return st;
}

// Called when a byte in a value is read
static SajsStatus
on_byte(TestState* const state, uint8_t const byte)
{
  FILE* const out = state->out_stream;

  if (state->top_kind != SAJS_STRING) {
    // Write syntactic (non-string) character directly
    return stream_write(byte, out);
  }

  switch (byte) {
  case '\"':
  case '\\':
    return stream_write_escape(byte, out);
  case '\b':
    return stream_write_escape('b', out);
  case '\f':
    return stream_write_escape('f', out);
  case '\n':
    return stream_write_escape('n', out);
  case '\r':
    return stream_write_escape('r', out);
  case '\t':
    return stream_write_escape('t', out);

  default:
    return (byte < 0x20U)
             // Generic control character escape
             ? ((fwrite("\\u00", 1U, 4U, out) == 4U) &&
                !stream_write((uint8_t)('0' + ((byte & 0xF0U) >> 4U)), out) &&
                !stream_write((uint8_t)('0' + (char)(byte & 0x0FU)), out))
                 ? SAJS_SUCCESS
                 : SAJS_FAILURE

             // Any other byte (printable ASCII or UTF-8)
             : stream_write(byte, out);
  }
}

// Called when a value is finished
static SajsStatus
on_end(TestState* const state, SajsValueKind const kind)
{
  SajsStatus  st     = SAJS_SUCCESS;
  FILE* const stream = state->out_stream;

  switch (kind) {
  case SAJS_OBJECT:
    st = stream_write('}', stream);
    break;
  case SAJS_ARRAY:
    st = stream_write(']', stream);
    break;
  case SAJS_STRING:
    st = stream_write('\"', stream);
    break;
  case SAJS_NUMBER:
  case SAJS_LITERAL:
    break;
  }

  if (state->top_flags & SAJS_IS_MEMBER_NAME) {
    st = stream_write(':', stream);
  }

  if (--state->depth == 0U) {
    ++state->num_values;
  }

  state->top_flags = 0U;
  return st;
}

static SajsStatus
on_result(TestState* const state, SajsResult const r)
{
  uint8_t const* const bytes = sajs_bytes(state->lexer);

  switch (r.event) {
  case SAJS_EVENT_NOTHING:
    break;
  case SAJS_EVENT_START:
    on_start(state, r.kind, r.flags);
    if (r.flags & SAJS_HAS_BYTES) {
      on_byte(state, bytes[0]);
    }
    break;
  case SAJS_EVENT_END:
    if (r.flags & SAJS_HAS_BYTES) {
      on_byte(state, bytes[0]);
    }
    on_end(state, r.kind);
    break;
  case SAJS_EVENT_DOUBLE_END:
    on_end(state, state->top_kind);
    on_end(state, r.kind);
    break;
  case SAJS_EVENT_BYTES:
    on_byte(state, bytes[0]);
    for (uint8_t i = 1U; i < 4U && bytes[i]; ++i) {
      on_byte(state, bytes[i]);
    }
    break;
  }

  return SAJS_SUCCESS;
}

static int
test(TestState* const state, FILE* const stream)
{
  SajsResult r = {SAJS_SUCCESS, SAJS_EVENT_NOTHING, (SajsValueKind)0U, 0U};
  while (!(r = sajs_read_byte(state->lexer, fgetc(stream))).status) {
    on_result(state, r);
    r.status = SAJS_SUCCESS;
  }

  if (r.status > SAJS_FAILURE) {
    (void)fprintf(stderr, "error: %s\n", sajs_strerror(r.status));
  }

  if (fclose(stream)) {
    (void)fprintf(stderr, "error closing output (%s)\n", strerror(errno));
  }

  return (state->num_values != 1U)    ? 65 // EX_DATAERR
         : (r.status == SAJS_FAILURE) ? 0
                                      : ((int)r.status + 100);
}

int
main(int const argc, char** const argv)
{
  if (argc > 2) {
    (void)fprintf(stderr, "Usage: %s [INPUT]\n", argv[0]);
    return 2;
  }

  FILE* const stream = argc > 1 ? fopen(argv[1], "r") : stdin;
  if (!stream) {
    (void)fprintf(stderr, "error opening input (%s)\n", strerror(errno));
    return 1;
  }

  uintptr_t mem[TEST_MEM_BYTES / sizeof(uintptr_t)] = {0};

  TestState state = {
    stream,
    stdout,
    sajs_init(sizeof(mem), mem),
    0U,
    0U,
    (SajsValueKind)0U,
    0U,
  };

  return test(&state, stream);
}
