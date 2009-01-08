#!/usr/bin/python
# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A simple utility function to produce data pack files.
See base/pack_file* for details.
"""

import struct

version = 1

def WriteDataPack(resources, output_file):
  """Write a map of id=>data into output_file as a data pack."""
  ids = sorted(resources.keys())
  file = open(output_file, "wb")

  # Write file header.
  file.write(struct.pack("<II", version, len(ids)))
  header_length = 2 * 4             # Two uint32s.

  index_length = len(ids) * 3 * 4   # Each entry is 3 uint32s.

  # Write index.
  data_offset = header_length + index_length
  for id in ids:
    file.write(struct.pack("<III", id, data_offset, len(resources[id])))
    data_offset += len(resources[id])

  # Write data.
  for id in ids:
    file.write(resources[id])

def main():
  # Just write a simple file.
  data = { 1: "", 4: "this is id 4", 6: "this is id 6", 10: "" }
  WriteDataPack(data, "datapack")
  print "wrote datapack to current directory."

if __name__ == '__main__':
  main()
