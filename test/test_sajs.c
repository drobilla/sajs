// Copyright 2017-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "sajs/sajs.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_MEM_BYTES 1024U

// "Global" test state passed as user data to callbacks
typedef struct {
  FILE*      in_stream;  ///< Input stream
  FILE*      out_stream; ///< Output stream
  SajsLexer* lexer;      ///< Lexer for reading input stream
  unsigned   num_values; ///< Number of top-level values parsed
  unsigned   depth;      ///< Stack depth
  bool       terse;      ///< True if writing terse output
} TestState;

// Write a newline with indentation
static int
write_newline(unsigned const indent, FILE* const out_stream)
{
  if (fwrite("\n", 1U, 1U, out_stream) != 1U) {
    return 1;
  }

  for (unsigned i = 0U; i < indent; ++i) {
    if (fwrite("  ", 1U, 2U, out_stream) != 2U) {
      return 1;
    }
  }

  return 0;
}

// Write an output prefix (delimiter and whitespace) in normal or terse mode
static int
write_prefix(const SajsTextOutput out, bool terse, FILE* const out_stream)
{
  switch (out.prefix) {
  case SAJS_PREFIX_NONE:
    break;

  case SAJS_PREFIX_OBJECT_START:
  case SAJS_PREFIX_ARRAY_START:
  case SAJS_PREFIX_OBJECT_END:
  case SAJS_PREFIX_ARRAY_END:
    return terse ? 0 : write_newline(out.indent, out_stream);

  case SAJS_PREFIX_MEMBER_COLON:
    return fwrite(": ", 1U, 2U - terse, out_stream) != 2U - terse;

  case SAJS_PREFIX_MEMBER_COMMA:
  case SAJS_PREFIX_ARRAY_COMMA:
    return (fwrite(",", 1U, 1U, out_stream) != 1U) ? 1
           : terse                                 ? 0
                   : write_newline(out.indent, out_stream);
  }

  return 0;
}

// Write an output fragment with prefix
static int
write_output(const SajsTextOutput out, bool terse, FILE* const out_stream)
{
  if (write_prefix(out, terse, out_stream)) {
    return 1;
  }

  // Write bytes
  if (out.length) {
    if (fwrite(out.bytes, 1U, out.length, out_stream) != out.length) {
      return 1;
    }
  }

  return 0;
}

static int
test(TestState* const state, FILE* const in_stream)
{
  uintptr_t         write_mem[8U] = {0U, 0U, 0U, 0U};
  SajsWriter* const writer = sajs_writer_init(sizeof(write_mem), write_mem);

  SajsResult r = {SAJS_SUCCESS, SAJS_EVENT_NOTHING, (SajsValueKind)0U, 0U};
  while (!(r = sajs_read_byte(state->lexer, fgetc(in_stream))).status) {
    int  rc         = 0;
    bool is_top_end = false;
    switch (r.event) {
    case SAJS_EVENT_NOTHING:
      break;
    case SAJS_EVENT_START:
      ++state->depth;
      break;
    case SAJS_EVENT_END:
      if (!(--state->depth)) {
        ++state->num_values;
        is_top_end = true;
      }
      break;
    case SAJS_EVENT_DOUBLE_END:
      if (!(state->depth -= 2U)) {
        ++state->num_values;
        is_top_end = true;
      }
      break;
    case SAJS_EVENT_BYTES:
      break;
    }

    if (rc) {
      return rc;
    }

    const SajsTextOutput out =
      sajs_write_result(writer, r, sajs_string(state->lexer));

    if ((rc = write_output(out, state->terse, state->out_stream))) {
      return rc;
    }

    if (is_top_end && fwrite("\n", 1U, 1U, state->out_stream) != 1U) {
      return 1;
    }
  }

  if (r.status > SAJS_FAILURE) {
    (void)fprintf(stderr, "error: %s\n", sajs_strerror(r.status));
  }

  return (state->num_values != 1U)    ? 65 // EX_DATAERR
         : (r.status == SAJS_FAILURE) ? 0
                                      : ((int)r.status + 100);
}

static int
print_usage(char const* const name, bool const error)
{
  (void)fprintf(stderr,
                "Usage: %s [INPUT]\n"
                "Read and write JSON.\n\n"
                "  -h  Display this help and exit.\n"
                "  -t  Write terse output without newlines.\n",
                name);
  return error ? 1 : 0;
}

int
main(int const argc, char** const argv)
{
  int  a     = 1;
  bool terse = false;
  for (; a < argc && argv[a][0] == '-'; ++a) {
    for (int o = 1; argv[a][o]; ++o) {
      char const opt = argv[a][o];
      if (opt == 'h') {
        return print_usage(argv[0], false);
      }

      if (opt == 't') {
        terse = true;
      } else {
        (void)fprintf(stderr, "%s: invalid option -- '%c'\n\n", argv[0], opt);
        return print_usage(argv[0], true);
      }
    }
  }

  if (a + 1 < argc) {
    return print_usage(argv[0], true);
  }

  FILE* const in_stream = a < argc ? fopen(argv[a], "r") : stdin;
  if (!in_stream) {
    (void)fprintf(stderr, "error opening input (%s)\n", strerror(errno));
    return 1;
  }

  uintptr_t mem[TEST_MEM_BYTES / sizeof(uintptr_t)] = {0};

  TestState state = {
    in_stream, stdout, sajs_lexer_init(sizeof(mem), mem), 0U, 0U, terse};

  int const rc = test(&state, in_stream);

  if (fclose(in_stream)) {
    (void)fprintf(stderr, "error closing input (%s)\n", strerror(errno));
    return 1;
  }

  return rc;
}
