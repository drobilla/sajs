<!-- Copyright 2021-2023 David Robillard <d@drobilla.net> -->
<!-- SPDX-License-Identifier: ISC -->

Sajs
====

Sajs (Simple Api for JSon) is a streaming SAX-style JSON parser implemented in
C99.

Sajs provides an interface for reading a single byte at a time, which fires
callbacks on events that describe the input, like the start and end of a value.
It has no dependencies, not even the standard library.

 -- David Robillard <d@drobilla.net>
