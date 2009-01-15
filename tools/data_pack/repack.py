#!/usr/bin/python
# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A simple utility function to merge data pack files into a single data pack.
See base/pack_file* for details about the file format.
"""

import exceptions
import struct
import sys

import data_pack

def RePack(output_file, input_files):
  """Write a new data pack to |output_file| based on a list of filenames
  (|input_files|)"""
  resources = {}
  for filename in input_files:
    new_resources = data_pack.ReadDataPack(filename)

    # Make sure we have no dups.
    duplicate_keys = set(new_resources.keys()) & set(resources.keys())
    if len(duplicate_keys) != 0:
      raise exceptions.KeyError("Duplicate keys: " + str(list(duplicate_keys)))

    resources.update(new_resources)

  data_pack.WriteDataPack(resources, output_file)

def main(argv):
  if len(argv) < 4:
    print ("Usage:\n  %s <output_filename> <input_file1> <input_file2> "
        "[input_file3] ..." % argv[0])
    sys.exit(-1)
  RePack(argv[1], argv[2:])

if '__main__' == __name__:
  main(sys.argv)
