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

'''The <output> and <file> elements.
'''

import os
import re
import grit.format.rc_header

from grit.node import base
from grit import exception
from grit import util
from grit import xtb_reader


class FileNode(base.Node):
  '''A <file> element.'''
  
  def __init__(self):
    super(type(self), self).__init__()
    self.re = None
    self.should_load_ = True
  
  def IsTranslation(self):
    return True
  
  def GetLang(self):
    return self.attrs['lang']
  
  def DisableLoading(self):
    self.should_load_ = False
  
  def MandatoryAttributes(self):
    return ['path', 'lang']
  
  def RunGatherers(self, recursive=False, debug=False):
    if not self.should_load_:
      return
    
    xtb_file = file(self.GetFilePath())
    try:
      lang = xtb_reader.Parse(xtb_file,
                              self.UberClique().GenerateXtbParserCallback(
                                self.attrs['lang'], debug=debug))
    except:
      print "Exception during parsing of %s" % self.GetFilePath()
      raise
    assert (lang == self.attrs['lang'], 'The XTB file you '
            'reference must contain messages in the language specified\n'
            'by the \'lang\' attribute.')
  
  def GetFilePath(self):
    return self.ToRealPath(os.path.expandvars(self.attrs['path']))


class OutputNode(base.Node):
  '''An <output> element.'''
  
  def MandatoryAttributes(self):
    return ['filename', 'type']
  
  def DefaultAttributes(self):
    return { 'lang' : '', # empty lang indicates all languages
             'language_section' : 'neutral' # defines a language neutral section
             } 
  
  def GetType(self):
    return self.attrs['type']

  def GetLanguage(self):
    '''Returns the language ID, default 'en'.'''
    return self.attrs['lang']

  def GetFilename(self):
    return self.attrs['filename']

  def GetOutputFilename(self):
    if hasattr(self, 'output_filename'):
      return self.output_filename
    else:
      return self.attrs['filename']

  def _IsValidChild(self, child):
    return isinstance(child, EmitNode)

class EmitNode(base.ContentNode):
  ''' An <emit> element.'''

  def DefaultAttributes(self):
    return { 'emit_type' : 'prepend'}

  def GetEmitType(self):
    '''Returns the emit_type for this node. Default is 'append'.'''
    return self.attrs['emit_type']

  def ItemFormatter(self, t):
    if t == 'rc_header':
      return grit.format.rc_header.EmitAppender() 
    else:
      return super(type(self), self).ItemFormatter(t) 
  

