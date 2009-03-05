#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Maps each node type to an implementation class.
When adding a new node type, you add to this mapping.
'''


from grit import exception

from grit.node import empty
from grit.node import message
from grit.node import misc
from grit.node import variant
from grit.node import structure
from grit.node import include
from grit.node import io


_ELEMENT_TO_CLASS = {
  'includes'      : empty.IncludesNode,
  'messages'      : empty.MessagesNode,
  'structures'    : empty.StructuresNode,
  'translations'  : empty.TranslationsNode,
  'outputs'       : empty.OutputsNode,
  'message'       : message.MessageNode,
  'ph'            : message.PhNode,
  'ex'            : message.ExNode,
  'grit'          : misc.GritNode,
  'include'       : include.IncludeNode,
  'structure'     : structure.StructureNode,
  'skeleton'      : variant.SkeletonNode,
  'release'       : misc.ReleaseNode,
  'file'          : io.FileNode,
  'output'        : io.OutputNode,
  'emit'          : io.EmitNode,
  'identifiers'   : empty.IdentifiersNode,
  'identifier'    : misc.IdentifierNode,
  'if'            : misc.IfNode,
}


def ElementToClass(name, typeattr):
  '''Maps an element to a class that handles the element.

  Args:
    name: 'element' (the name of the element)
    typeattr: 'type' (the value of the type attribute, if present, else None)

  Return:
    type
  '''
  if not _ELEMENT_TO_CLASS.has_key(name):
    raise exception.UnknownElement()
  return _ELEMENT_TO_CLASS[name]

