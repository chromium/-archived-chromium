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

'''Container nodes that don't have any logic.
'''


from grit.node import base
from grit.node import include
from grit.node import structure
from grit.node import message
from grit.node import io
from grit.node import misc


class GroupingNode(base.Node):
  '''Base class for all the grouping elements (<structures>, <includes>,
  <messages> and <identifiers>).'''
  def DefaultAttributes(self):
    return {
      'first_id' : '',
      'comment' : '',
      'fallback_to_english' : 'false',
    }


class IncludesNode(GroupingNode):
  '''The <includes> element.'''
  def _IsValidChild(self, child):
    return isinstance(child, (include.IncludeNode, misc.IfNode))


class MessagesNode(GroupingNode):
  '''The <messages> element.'''
  def _IsValidChild(self, child):
    return isinstance(child, (message.MessageNode, misc.IfNode))
  
  def ItemFormatter(self, t):
    '''Return the stringtable itemformatter if an RC is being formatted.'''
    if t in ['rc_all', 'rc_translateable', 'rc_nontranslateable']:
      from grit.format import rc  # avoid circular dep by importing here
      return rc.StringTable()


class StructuresNode(GroupingNode):
  '''The <structures> element.'''
  def _IsValidChild(self, child):
    return isinstance(child, (structure.StructureNode, misc.IfNode))


class TranslationsNode(base.Node):
  '''The <translations> element.'''
  def _IsValidChild(self, child):
    return isinstance(child, io.FileNode)


class OutputsNode(base.Node):
  '''The <outputs> element.'''
  def _IsValidChild(self, child):
    return isinstance(child, io.OutputNode)


class IdentifiersNode(GroupingNode):
  '''The <identifiers> element.'''
  def _IsValidChild(self, child):
    from grit.node import misc
    return isinstance(child, misc.IdentifierNode)
