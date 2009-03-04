#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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
    assert lang == self.attrs['lang'], ('The XTB file you '
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
  


