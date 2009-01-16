#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Handling of the <include> element.
'''


import grit.format.rc_header
import grit.format.rc

from grit.node import base
from grit import util

class IncludeNode(base.Node):
  '''An <include> element.'''
  
  def _IsValidChild(self, child):
    return False

  def MandatoryAttributes(self):
    return ['name', 'type', 'file']

  def DefaultAttributes(self):
    return {'translateable' : 'true', 
      'generateid': 'true', 
      'filenameonly': 'false',
      'relativepath': 'false',
      }

  def ItemFormatter(self, t):
    if t == 'rc_header':
      return grit.format.rc_header.Item()
    elif (t in ['rc_all', 'rc_translateable', 'rc_nontranslateable'] and
          self.SatisfiesOutputCondition()):
      return grit.format.rc.RcInclude(self.attrs['type'].upper(), 
        self.attrs['filenameonly'] == 'true',
        self.attrs['relativepath'] == 'true')
    else:
      return super(type(self), self).ItemFormatter(t)
  
  def FileForLanguage(self, lang, output_dir):
    '''Returns the file for the specified language.  This allows us to return
    different files for different language variants of the include file.
    '''
    return self.FilenameToOpen()

  def GetDataPackPair(self):
    '''Returns a (id, string) pair that represents the resource id and raw
    bytes of the data.  This is used to generate the data pack data file.
    '''
    from grit.format import rc_header
    id_map = rc_header.Item.tids_
    id = id_map[self.GetTextualIds()[0]]
    file = open(self.FilenameToOpen(), 'rb')
    data = file.read()
    file.close()
    return id, data

  # static method
  def Construct(parent, name, type, file, translateable=True, 
      filenameonly=False, relativepath=False):
    '''Creates a new node which is a child of 'parent', with attributes set
    by parameters of the same name.
    '''
    # Convert types to appropriate strings
    translateable = util.BoolToString(translateable)
    filenameonly = util.BoolToString(filenameonly)
    relativepath = util.BoolToString(relativepath)
    
    node = IncludeNode()
    node.StartParsing('include', parent)
    node.HandleAttribute('name', name)
    node.HandleAttribute('type', type)
    node.HandleAttribute('file', file)
    node.HandleAttribute('translateable', translateable)
    node.HandleAttribute('filenameonly', filenameonly)
    node.HandleAttribute('relativepath', relativepath)
    node.EndParsing()
    return node
  Construct = staticmethod(Construct)

