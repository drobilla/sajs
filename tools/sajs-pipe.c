// Copyright 2017-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "sajs/sajs.h"

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_STACK_SIZE 1024U

// GCC print format attributes
#if defined(__MINGW32__)
#  define SAJS_LOG_FUNC(fmt, a0) __attribute__((format(gnu_printf, fmt, a0)))
#elif defined(__GNUC__)
#  define SAJS_LOG_FUNC(fmt, a0) __attribute__((format(printf, fmt, a0)))
#else
#  define SAJS_LOG_FUNC(fmt, a0) ///< Has printf-like parameters
#endif

/// Command line options
typedef struct {
  char*  out_path;
  size_t stack_size;
  bool   terse;
} PipeOptions;

/// "Global" state passed as user data to callbacks
typedef struct {
  FILE*      in_stream;  ///< Input stream
  FILE*      out_stream; ///< Output stream
  SajsLexer* lexer;      ///< Lexer for reading input stream
  unsigned   num_values; ///< Number of top-level values parsed
  unsigned   depth;      ///< Stack depth
  bool       terse;      ///< True if writing terse output
} PipeState;

/// Write a newline with indentation
static int
write_newline(unsigned const indent, FILE* const out_stream)
{
  size_t written = fwrite("\n", 1U, 1U, out_stream);
  for (unsigned i = 0U; i < indent; ++i) {
    written += fwrite("  ", 1U, 2U, out_stream);
  }

  return written != 1U + 2U * indent;
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
static SajsStatus
write_output(const SajsTextOutput out, bool terse, FILE* const out_stream)
{
  return write_prefix(out, terse, out_stream) ? SAJS_BAD_WRITE
         : (!out.length ||
            fwrite(out.bytes, 1U, out.length, out_stream) == out.length)
           ? SAJS_SUCCESS
           : SAJS_BAD_WRITE;
}

// Update depth and return true if this was the end of the top value
static bool
update_depth(PipeState* const state, SajsResult const r)
{
  if (r.event == SAJS_EVENT_START) {
    ++state->depth;
  } else if (r.event == SAJS_EVENT_END) {
    return !(state->depth -= 1U);
  } else if (r.event == SAJS_EVENT_DOUBLE_END) {
    return !(state->depth -= 2U);
  }
  return false;
}

static int
run(PipeState* const state, FILE* const in_stream)
{
  uintptr_t         write_mem[8U] = {0U, 0U, 0U, 0U};
  SajsWriter* const writer = sajs_writer_init(sizeof(write_mem), write_mem);

  SajsStatus st = SAJS_SUCCESS;
  while (!st) {
    SajsResult const r = sajs_read_byte(state->lexer, fgetc(in_stream));
    if (!(st = r.status)) {
      // Update state
      bool const is_top_end = update_depth(state, r);

      // Write output
      SajsStringView const string = sajs_string(state->lexer);
      SajsTextOutput const out    = sajs_write_result(writer, r, string);
      st = write_output(out, state->terse, state->out_stream);

      if (!st && is_top_end) { // Write top-level trailing newline
        ++state->num_values;
        st = (fwrite("\n", 1U, 1U, state->out_stream) == 1U) ? SAJS_SUCCESS
                                                             : SAJS_BAD_WRITE;
      }
    }
  }

  if (st > SAJS_FAILURE) {
    (void)fprintf(stderr, "error: %s\n", sajs_strerror(st));
  }

  return (state->num_values != 1U) ? 65 // EX_DATAERR
         : (st == SAJS_FAILURE)    ? 0
                                   : ((int)st + 100);
}

static int
print_version(void)
{
  printf("sajs-pipe " SAJS_VERSION "\n");
  printf("Copyright 2017-2023 David Robillard <d@drobilla.net>.\n"
         "License ISC: <https://spdx.org/licenses/ISC>.\n"
         "This is free software; you are free to change and redistribute it."
         "\nThere is NO WARRANTY, to the extent permitted by law.\n");
  return 0;
}

static int
print_usage(char const* const name, bool const error)
{
  (void)fprintf(stderr,
                "Usage: %s [INPUT]\n"
                "Read and write JSON.\n\n"
                "  -V           Display version information and exit.\n"
                "  -h           Display this help and exit.\n"
                "  -o FILENAME  Write output to FILENAME instead of stdout.\n"
                "  -t           Write terse output without newlines.\n",
                name);
  return error ? -1 : 0;
}

SAJS_LOG_FUNC(1, 2)
static int
log_error(char const* const fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  (void)vfprintf(stderr, fmt, args);
  va_end(args);
  return -1;
}

SAJS_LOG_FUNC(1, 2)
static int
log_errno(char const* const fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  (void)vfprintf(stderr, fmt, args);
  (void)fprintf(stderr, " (%s)\n", strerror(errno));
  va_end(args);
  return -1;
}

static int
missing_arg(char const* const name, char const opt)
{
  log_error("%s: option requires an argument -- '%c'\n\n", name, opt);
  return print_usage(name, true);
}

static int
parse_flag(PipeOptions* const opts,
           int const          argc,
           char** const       argv,
           int const          a,
           int const          o)
{
  char const* const name = argv[0];

  switch (argv[a][o]) {
  case 'V':
    return print_version();
  case 'h':
    return print_usage(name, false);
  case 't':
    opts->terse = true;
    return 1;
  case 'k':
    if (argv[a][o + 1] || a + 1 == argc) {
      return missing_arg(name, 'k');
    }

    char const* const string = argv[a + 1];
    char*             endptr = NULL;
    long const        size   = strtol(string, &endptr, 10);
    if (size <= 0 || size == LONG_MAX || *endptr != '\0') {
      log_error("%s: invalid size \"%s\"\n\n", name, string);
      return print_usage(name, true);
    }

    opts->stack_size = (size_t)size;
    return 2;

  case 'o':
    if (argv[a][o + 1] || a + 1 == argc) {
      return missing_arg(name, 'o');
    }

    opts->out_path = argv[a + 1];
    return 2;

  default:
    log_error("%s: invalid option -- '%c'\n\n", name, argv[a][o]);
    return print_usage(name, true);
  }
}

static int
parse_args(PipeOptions* const opts, int const argc, char** const argv)
{
  int a = 1;
  for (; a < argc && argv[a][0] == '-'; ++a) {
    for (int o = 1; argv[a][o]; ++o) {
      int const rc = parse_flag(opts, argc, argv, a, o);
      if (rc <= 0) {
        return rc;
      }

      if (rc == 2) {
        ++a;
        break;
      }
    }
  }

  return a;
}

int
main(int const argc, char** const argv)
{
  // Parse command line options
  char const* const name = argv[0];
  PipeOptions       opts = {NULL, DEFAULT_STACK_SIZE, false};
  int const         a    = parse_args(&opts, argc, argv);
  if (a <= 0) {
    return a;
  }

  // Open input stream
  FILE* const in_stream = a < argc ? fopen(argv[a], "r") : stdin;
  if (!in_stream) {
    return log_errno("%s: failed to open input", name);
  }

  // Open output stream
  FILE* const out_file   = opts.out_path ? fopen(opts.out_path, "w") : NULL;
  FILE* const out_stream = opts.out_path ? out_file : stdout;
  if (opts.out_path && !out_file) {
    (void)fclose(in_stream);
    return log_errno("%s: failed to open output", name);
  }

  size_t const     mem_size = 64U + opts.stack_size;
  void*            mem      = malloc(mem_size);
  SajsLexer* const lexer    = sajs_lexer_init(mem_size, mem);
  PipeState        state = {in_stream, out_stream, lexer, 0U, 0U, opts.terse};

  int const rc0 = lexer ? run(&state, in_stream) : -12;
  int const rc1 = fclose(in_stream);
  int const rc2 = out_file ? fclose(out_file) : 0;

  free(mem);
  return rc0 ? rc0 : (rc1 || rc2) ? log_errno("%s: failed on close", name) : 0;
}
