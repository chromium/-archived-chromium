#!/usr/bin/python2.4
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
