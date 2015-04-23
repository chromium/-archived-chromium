#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Count number of occurrences of a given message ID
'''

import getopt
import os
import types

from grit.tool import interface
from grit import grd_reader
from grit import util

from grit.extern import tclib


class CountMessage(interface.Tool):
  '''Count the number of times a given message ID is used.
'''

  def __init__(self):
    pass

  def ShortDescription(self):
    return 'Exports all translateable messages into an XMB file.'

  def Run(self, opts, args):
    self.SetOptions(opts)

    id = args[0]
    res_tree = grd_reader.Parse(opts.input, debug=opts.extra_verbose)
    res_tree.OnlyTheseTranslations([])
    res_tree.RunGatherers(True)

    count = 0
    for c in res_tree.UberClique().AllCliques():
      if c.GetId() == id:
        count += 1

    print "There are %d occurrences of message %s." % (count, id)

