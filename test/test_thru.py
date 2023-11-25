#!/usr/bin/env python3

# Copyright 2022-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Test that JSON input is parsed and rewritten as the exact same bytes.

The input is read via stdin to avoid filesystem access for testing in node.
"""

import argparse
import shlex
import subprocess
import sys
import tempfile
import difflib
import os


def main():
    """Run the command line tool."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool", default="test/test_sajs", help="executable")
    parser.add_argument("input", help="valid JSON input file")
    args = parser.parse_args(sys.argv[1:])

    wrapper = shlex.split(os.environ.get("MESON_EXE_WRAPPER", ""))
    command = wrapper + [args.tool]

    status = 0
    with tempfile.TemporaryFile("w+", encoding="utf-8") as out_file:
        with open(args.input, "r", encoding="utf-8") as in_file:
            # Run input through command and write to output
            proc = subprocess.run(
                command,
                check=True,
                encoding="utf-8",
                stderr=subprocess.PIPE,
                stdin=in_file,
                stdout=out_file,
            )

            # Ensure there were no errors logged
            if "error:" in proc.stderr:
                sys.stderr.write(proc.stderr)
                return 1

            # Rewind and compare input and output
            in_file.seek(0)
            out_file.seek(0)
            for line in difflib.unified_diff(
                in_file.read().splitlines(),
                out_file.read().splitlines(),
                fromfile="input",
                tofile="output",
            ):
                sys.stderr.write(line.strip() + "\n")
                status = 1

    return status


if __name__ == "__main__":
    sys.exit(main())
