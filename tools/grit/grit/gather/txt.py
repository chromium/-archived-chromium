#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Supports making amessage from a text file.
'''

import types

from grit.gather import interface
from grit import tclib
from grit import util


class TxtFile(interface.GathererBase):
  '''A text file gatherer.  Very simple, all text from the file becomes a
  single clique.
  '''

  def __init__(self, contents):
    super(type(self), self).__init__()
    self.text_ = contents
    self.clique_ = None

  def Parse(self):
    self.clique_ = self.uberclique.MakeClique(tclib.Message(text=self.text_))
    pass

  def GetText(self):
    '''Returns the text of what is being gathered.'''
    return self.text_

  def GetTextualIds(self):
    return []

  def GetCliques(self):
    '''Returns the MessageClique objects for all translateable portions.'''
    return [self.clique_]

  def Translate(self, lang, pseudo_if_not_available=True,
                skeleton_gatherer=None, fallback_to_english=False):
    return self.clique_.MessageForLanguage(lang,
                                           pseudo_if_not_available,
                                           fallback_to_english).GetRealContent()

  def FromFile(filename_or_stream, extkey=None, encoding = 'cp1252'):
    if isinstance(filename_or_stream, types.StringTypes):
      filename_or_stream = util.WrapInputStream(file(filename_or_stream, 'rb'), encoding)
    return TxtFile(filename_or_stream.read())
  FromFile = staticmethod(FromFile)

