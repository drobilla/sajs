// Copyright 2017-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "sajs/sajs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef char SajsByte;

struct SajsWriterImpl {
  unsigned      depth;        ///< Container depth
  SajsValueKind top_kind;     ///< Current value kind
  SajsFlags     top_flags;    ///< Current string/number/literal flags
  SajsByte      top_bytes[8]; ///< Last written character bytes
};

static SajsTextOutput
make_output(SajsStatus const      status,
            SajsTextPrefix        prefix,
            unsigned const        depth,
            size_t const          length,
            SajsByte const* const bytes)
{
  SajsTextOutput const out = {status, depth, length, bytes, prefix};
  return out;
}

// Write nothing, return success
static SajsTextOutput
emit_nothing(void)
{
  return make_output(SAJS_SUCCESS, SAJS_PREFIX_NONE, 0U, 0U, "");
}

// Write a single byte to the output
static SajsTextOutput
emit_byte(SajsWriter* const writer, SajsByte const byte)
{
  writer->top_bytes[0] = byte;
  writer->top_bytes[1] = '\0';
  return make_output(SAJS_SUCCESS, SAJS_PREFIX_NONE, 0U, 1U, writer->top_bytes);
}

// Write a separator followed by a byte to the output
static SajsTextOutput
emit_sep(SajsWriter* const    writer,
         SajsTextPrefix const prefix,
         unsigned const       depth,
         SajsByte const       byte)
{
  writer->top_bytes[0] = byte;
  writer->top_bytes[1] = '\0';
  return make_output(SAJS_SUCCESS, prefix, depth, 1U, writer->top_bytes);
}

// Write two adjacent bytes to the output
static SajsTextOutput
emit_pair(SajsWriter* const writer, SajsByte const a, SajsByte const b)
{
  writer->top_bytes[0U] = a;
  writer->top_bytes[1U] = b;
  writer->top_bytes[2U] = '\0';

  return make_output(SAJS_SUCCESS, SAJS_PREFIX_NONE, 0U, 2U, writer->top_bytes);
}

// Called when a value is started
static SajsTextOutput
on_start(SajsWriter* const   writer,
         SajsValueKind const kind,
         SajsFlags const     flags,
         SajsByte const      head)
{
  writer->top_kind  = kind;
  writer->top_flags = flags;

  bool const           is_first = flags & SAJS_IS_FIRST;
  SajsTextPrefix const prefix =
    (flags & SAJS_IS_MEMBER_VALUE) ? SAJS_PREFIX_MEMBER_COLON
    : (flags & SAJS_IS_MEMBER_NAME)
      ? (is_first ? SAJS_PREFIX_OBJECT_START : SAJS_PREFIX_MEMBER_COMMA)
    : (flags & SAJS_IS_ELEMENT)
      ? (is_first ? SAJS_PREFIX_ARRAY_START : SAJS_PREFIX_ARRAY_COMMA)
      : SAJS_PREFIX_NONE;

  switch (kind) {
  case SAJS_OBJECT:
    return emit_sep(writer, prefix, writer->depth++, '{');
  case SAJS_ARRAY:
    return emit_sep(writer, prefix, writer->depth++, '[');
  case SAJS_STRING:
    return emit_sep(writer, prefix, writer->depth, '"');
  case SAJS_NUMBER:
  case SAJS_LITERAL:
    break;
  }

  return emit_sep(writer, prefix, writer->depth, head);
}

// Called on characters in a value
static SajsTextOutput
on_byte(SajsWriter* const writer, SajsByte const byte)
{
  if (writer->top_kind != SAJS_STRING) {
    // Write syntactic (non-string) character directly
    return emit_byte(writer, byte);
  }

  switch (byte) {
  case '\"':
  case '\\':
    return emit_pair(writer, '\\', byte);
  case '\b':
    return emit_pair(writer, '\\', 'b');
  case '\f':
    return emit_pair(writer, '\\', 'f');
  case '\n':
    return emit_pair(writer, '\\', 'n');
  case '\r':
    return emit_pair(writer, '\\', 'r');
  case '\t':
    return emit_pair(writer, '\\', 't');
  default:
    break;
  }

  if ((uint8_t)byte >= 0x20) {
    return emit_byte(writer, byte); // Printable ASCII or UTF-8
  }

  // Generic control character escape
  writer->top_bytes[0U] = '\\';
  writer->top_bytes[1U] = 'u';
  writer->top_bytes[2U] = '0';
  writer->top_bytes[3U] = '0';
  writer->top_bytes[4U] = (SajsByte)('0' + (((uint8_t)byte & 0xF0U) >> 4U));
  writer->top_bytes[5U] = (SajsByte)('0' + ((uint8_t)byte & 0x0FU));
  writer->top_bytes[6U] = '\0';
  return make_output(
    SAJS_SUCCESS, SAJS_PREFIX_NONE, writer->depth, 6U, writer->top_bytes);
}

// Called when a value is finished
static SajsTextOutput
on_end(SajsWriter* const writer, SajsValueKind const kind, SajsByte const tail)
{
  writer->top_flags = 0U;

  switch (kind) {
  case SAJS_OBJECT:
    return emit_sep(writer, SAJS_PREFIX_OBJECT_END, --writer->depth, '}');
  case SAJS_ARRAY:
    return emit_sep(writer, SAJS_PREFIX_ARRAY_END, --writer->depth, ']');
  case SAJS_STRING:
    return emit_byte(writer, '\"');
  case SAJS_NUMBER:
  case SAJS_LITERAL:
    break;
  }

  return tail ? emit_byte(writer, tail) : emit_nothing();
}

SajsWriter*
sajs_writer_init(size_t const mem_size, void* const mem)
{
  if (mem_size < sizeof(SajsWriter)) {
    return NULL;
  }

  SajsWriter* const writer = (SajsWriter*)mem;
  writer->top_kind         = (SajsValueKind)0U;
  writer->top_flags        = 0U;
  writer->top_bytes[0]     = 0U;
  return writer;
}

SajsTextOutput
sajs_write_event(SajsWriter* const    writer,
                 SajsEvent const      event,
                 SajsStringView const string)
{
  if (event.type == SAJS_EVENT_START) {
    return on_start(
      writer,
      event.kind,
      event.flags,
      (SajsByte)((event.flags & SAJS_HAS_BYTES) ? string.data[0] : 0));
  }

  if (event.type == SAJS_EVENT_END) {
    return on_end(
      writer,
      event.kind,
      (SajsByte)((event.flags & SAJS_HAS_BYTES) ? string.data[0] : 0));
  }

  if (event.type == SAJS_EVENT_DOUBLE_END) {
    on_end(writer, writer->top_kind, '\0');
    return on_end(writer, event.kind, '\0');
  }

  if (event.type == SAJS_EVENT_BYTES) {
    return (string.length == 1U) ? on_byte(writer, string.data[0])
                                 : make_output(SAJS_SUCCESS,
                                               SAJS_PREFIX_NONE,
                                               0U,
                                               string.length,
                                               string.data);
  }

  return emit_nothing();
}
