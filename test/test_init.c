// Copyright 2017-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "sajs/sajs.h"

#include <assert.h>

static void
test_init(void)
{
  char mem[64];

  assert(!sajs_lexer_init(0U, mem));
  assert(!sajs_lexer_init(1U, mem));
  assert(!sajs_lexer_init(8U, mem));
  assert(sajs_lexer_init(sizeof(mem), mem));

  assert(!sajs_writer_init(0U, mem));
  assert(!sajs_writer_init(1U, mem));
  assert(!sajs_writer_init(8U, mem));
  assert(sajs_writer_init(sizeof(mem), mem));
}

int
main(void)
{
  test_init();
  return 0;
}
