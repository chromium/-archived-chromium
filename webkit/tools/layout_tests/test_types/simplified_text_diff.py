# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compares the text output to the expected text output while ignoring
positions and sizes of elements/text.

If the output doesn't match, returns FailureSimplifiedTextMismatch and outputs
the diff files into the layout test results directory.
"""

import difflib
import os.path
import re

from layout_package import test_failures
from test_types import text_diff

class SimplifiedTextDiff(text_diff.TestTextDiff):
  def _SimplifyText(self, text):
    """Removes position and size information from a render tree dump.  This
    also combines contiguous lines of text together so lines that wrap between
    different words match. Returns the simplified text."""

    # SVG render paths are a little complicated: we want to strip digits after
    # a decimal point only for strings that begin with "RenderPath.*data".
    def simplify_svg_path(match):
      return match.group(1) + re.sub(r"([0-9]*)\.[0-9]{2}", "\\1", match.group(2))

    # Regular expressions to remove or substitue text.
    simplifications = (
      # Ignore TypeError and ReferenceError, V8 has different error text.
      (re.compile(r"Message\tTypeError:.*"), 'Message\tTypeError:'),
      (re.compile(r"Message\tReferenceError:.*"), 'Message\tReferenceError:'),

      # Ignore uncaught exceptions because V8s error messages are different.
      (re.compile(r"CONSOLE MESSAGE: line \d+: .*"), 'CONSOLE MESSAGE: line'),

      # Remove position and size information from text.
      (re.compile(r"at \(-?[0-9]+(\.[0-9]+)?,-?[0-9]+(\.[0-9]+)?\) *"), ''),
      (re.compile(r"size -?[0-9]+(\.[0-9]+)?x-?[0-9]+(\.[0-9]+)? *"), ''),
      (re.compile(r' RTL: "\."'), ': ""'),
      (re.compile(r" (RTL|LTR)( override)?: "), ': '),
      (re.compile(r"text run width -?[0-9]+: "), ''),
      (re.compile(r"\([0-9]+px"), ''),
      (re.compile(r"scrollHeight -?[0-9]+"), ''),
      (re.compile(r"scrollWidth -?[0-9]+"), ''),
      (re.compile(r"scrollX -?[0-9]+"), ''),
      (re.compile(r"scrollY -?[0-9]+"), ''),
      (re.compile(r"scrolled to [0-9]+,[0-9]+"), 'scrolled to'),
      (re.compile(r"caret: position -?[0-9]+"), ''),

      # The file select widget has different text on Mac and Win.
      (re.compile(r"Choose File"), "Browse..."),

      # Remove trailing spaces at the end of a line of text.
      (re.compile(r' "\n'), '"\n'),
      # Remove leading spaces at the beginning of a line of text.
      (re.compile(r'(\s+") '), '\\1'),
      # Remove empty lines (this only seems to happen with <textareas>)
      (re.compile(r'^\s*""\n', re.M), ''),
      # Merge text lines together.  Lines ending in anything other than a
      # hyphen get a space inserted when joined.
      (re.compile(r'-"\s+"', re.M), '-'),
      (re.compile(r'"\s+"', re.M), ' '),

      # Handle RTL "...Browse" text.  The space gets inserted when text lines
      # are merged together in the step above.
      (re.compile(r"... Browse"), "Browse..."),

      # Some SVG tests inexplicably emit -0.00 rather than 0.00 in the expected results
      (re.compile(r"-0\.00"), '0.00'),

      # Remove size information from SVG text
      (re.compile(r"(chunk.*width )([0-9]+\.[0-9]{2})"), '\\1'),

      # Remove decimals from SVG paths
      (re.compile(r"(RenderPath.*data)(.*)"), simplify_svg_path),
    )

    for regex, subst in simplifications:
      text = re.sub(regex, subst, text)

    return text

  def CompareOutput(self, filename, proc, output, test_args, target):
    """Implementation of CompareOutput that removes most numbers before
    computing the diff.

    This test does not save new baseline results.

    """
    failures = []

    # If we're generating a new baseline, we pass.
    if test_args.new_baseline:
      return failures

    # Normalize text to diff
    output = self.GetNormalizedOutputText(output)
    # The full text diff already gave the result file if requested, so we'll
    # ignore the |show_sources| option here.
    expected = self.GetNormalizedExpectedText(filename, show_sources=False)

    # Don't bother with the simplified text diff if we match before simplifying
    # the text.
    if output == expected:
      return failures

    # Make the simplified text.
    output = self._SimplifyText(output)
    expected = self._SimplifyText(expected)

    if output != expected:
      # Text doesn't match, write output files.
      self.WriteOutputFiles(filename, "-simp", ".txt", output, expected)

      # Add failure to return list, unless it's a new test.
      if expected != '':
        failures.append(test_failures.FailureSimplifiedTextMismatch(self))

    return failures

