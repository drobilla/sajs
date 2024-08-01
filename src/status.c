// Copyright 2017-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "sajs/sajs.h"

#define SAJS_NUM_STATUS 22U ///< The number of SajsStatus entries

static char const* const sajs_status_strings[SAJS_NUM_STATUS] = {
  "Success",
  "Non-fatal failure",
  "Reached end of value",
  "Unexpected end of input",
  "Stack overflow",
  "Stack underflow",
  "Failed write",
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
sajs_strerror(SajsStatus const status)
{
  return (unsigned)status < SAJS_NUM_STATUS
           ? sajs_status_strings[(unsigned)status]
           : "Unknown error";
}
