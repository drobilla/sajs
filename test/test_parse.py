#!/usr/bin/env python3

# Copyright 2022-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Test that JSON input is successfully parsed.

The input is read via stdin to avoid filesystem access for testing in node.
"""

import argparse
import shlex
import subprocess
import sys
import os


def main():
    """Run the command line tool."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool", default="test/test_sajs", help="executable")
    parser.add_argument("input", help="valid JSON input file")
    args = parser.parse_args(sys.argv[1:])

    wrapper = shlex.split(os.environ.get("MESON_EXE_WRAPPER", ""))
    command = wrapper + [args.tool]

    with open(args.input, "r", encoding="utf-8") as in_file:
        proc = subprocess.run(
            command,
            check=True,
            stdin=in_file,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )

        if len(proc.stderr):
            sys.stderr.write("error: Output written to stderr on success:\n")
            sys.stderr.write(proc.stderr.decode("utf-8"))
            return 0

    return 0


if __name__ == "__main__":
    sys.exit(main())
