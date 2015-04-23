#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# test_runner

import optparse
import os
import re
import subprocess
import sys

import google.logging_utils
import google.path_utils

import common


def GetAllTests(exe):
  cmd = [exe, "--gtest_list_tests"]
  p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
  line = p.stdout.readline().rstrip()
  test = line
  tests = []
  while line:
    line = p.stdout.readline().rstrip()
    if line.startswith(' '):
      tests.append(test + line.lstrip())
    else:
      test = line
  return tests


def RunTestsSingly(exe, tests):
  for test in tests:
    filter = test
    if len(sys.argv) > 2:
      filter = filter + ":" + sys.argv[2]
    cmd = [exe, "--gtest_filter=" + filter]
    common.RunSubprocess(cmd)


def main():
  exe = sys.argv[1]
  all_tests = GetAllTests(exe)
  RunTestsSingly(exe, all_tests)


if __name__ == "__main__":
  main()
