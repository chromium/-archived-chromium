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
