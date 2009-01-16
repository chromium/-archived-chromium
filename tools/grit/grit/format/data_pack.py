#!/usr/bin/python2.4
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Support for formatting a data pack file used for platform agnostic resource
files.
'''

import struct

from grit.format import interface
from grit.node import misc
from grit.node import include


PACK_FILE_VERSION = 1


class DataPack(interface.ItemFormatter):
  '''Writes out the data pack file format (platform agnostic resource file).'''
  def Format(self, item, lang='en', begin_item=True, output_dir='.'):
    if not begin_item:
      return ''
    assert isinstance(item, misc.ReleaseNode)

    nodes = DataPack.GetDataNodes(item)
    data = {}
    for node in nodes:
      id, value = node.GetDataPackPair()
      data[id] = value
    return DataPack.WriteDataPack(data)

  @staticmethod
  def GetDataNodes(item):
    '''Returns a list of nodes that can be packed into the data pack file.'''
    nodes = []
    if isinstance(item, include.IncludeNode):
      return [item]
    # TODO(tc): Handle message nodes.
    for child in item.children:
      nodes.extend(DataPack.GetDataNodes(child))
    return nodes

  @staticmethod
  def WriteDataPack(resources):
    """Write a map of id=>data into a string in the data pack format and return
    it."""
    ids = sorted(resources.keys())
    ret = []

    # Write file header.
    ret.append(struct.pack("<II", PACK_FILE_VERSION, len(ids)))
    HEADER_LENGTH = 2 * 4             # Two uint32s.

    index_length = len(ids) * 3 * 4   # Each entry is 3 uint32s.

    # Write index.
    data_offset = HEADER_LENGTH + index_length
    for id in ids:
      ret.append(struct.pack("<III", id, data_offset, len(resources[id])))
      data_offset += len(resources[id])

    # Write data.
    for id in ids:
      ret.append(resources[id])
    return ''.join(ret)
