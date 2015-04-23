#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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

