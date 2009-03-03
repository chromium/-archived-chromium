#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Handling of the <message> element.
'''

import re
import types

from grit.node import base

import grit.format.rc_header
import grit.format.rc

from grit import clique
from grit import exception
from grit import tclib
from grit import util


# Finds whitespace at the start and end of a string which can be multiline.
_WHITESPACE = re.compile('(?P<start>\s*)(?P<body>.+?)(?P<end>\s*)\Z',
                         re.DOTALL | re.MULTILINE)


class MessageNode(base.ContentNode):
  '''A <message> element.'''
  
  # For splitting a list of things that can be separated by commas or
  # whitespace
  _SPLIT_RE = re.compile('\s*,\s*|\s+')
  
  def __init__(self):
    super(type(self), self).__init__()
    # Valid after EndParsing, this is the MessageClique that contains the
    # source message and any translations of it that have been loaded.
    self.clique = None
    
    # We don't send leading and trailing whitespace into the translation
    # console, but rather tack it onto the source message and any
    # translations when formatting them into RC files or what have you.
    self.ws_at_start = ''  # Any whitespace characters at the start of the text
    self.ws_at_end = ''  # --"-- at the end of the text
    
    # A list of "shortcut groups" this message is in.  We check to make sure
    # that shortcut keys (e.g. &J) within each shortcut group are unique.
    self.shortcut_groups_ = []

  def _IsValidChild(self, child):
    return isinstance(child, (PhNode))

  def _IsValidAttribute(self, name, value):
    if name not in ['name', 'offset', 'translateable', 'desc', 'meaning',
                    'internal_comment', 'shortcut_groups', 'custom_type',
                    'validation_expr']:
      return False
    if name == 'translateable' and value not in ['true', 'false']:
      return False
    return True
  
  def MandatoryAttributes(self):
    return ['name|offset']
  
  def DefaultAttributes(self):
    return {
      'translateable' : 'true',
      'desc' : '',
      'meaning' : '',
      'internal_comment' : '',
      'shortcut_groups' : '',
      'custom_type' : '',
      'validation_expr' : '',
    }

  def GetTextualIds(self):
    '''
    Returns the concatenation of the parent's node first_id and
    this node's offset if it has one, otherwise just call the
    superclass' implementation
    '''
    if 'offset' in self.attrs:
      # we search for the first grouping node in the parents' list
      # to take care of the case where the first parent is an <if> node
      grouping_parent = self.parent
      import grit.node.empty
      while grouping_parent and not isinstance(grouping_parent,
                                               grit.node.empty.GroupingNode):
        grouping_parent = grouping_parent.parent
      
      assert 'first_id' in grouping_parent.attrs
      return [grouping_parent.attrs['first_id'] + '_' + self.attrs['offset']]
    else:
      return super(type(self), self).GetTextualIds()
  
  def IsTranslateable(self):
    return self.attrs['translateable'] == 'true'

  def ItemFormatter(self, t):
    if t == 'rc_header':
      return grit.format.rc_header.Item()
    elif (t in ['rc_all', 'rc_translateable', 'rc_nontranslateable'] and
          self.SatisfiesOutputCondition()):
      return grit.format.rc.Message()
    else:
      return super(type(self), self).ItemFormatter(t)

  def EndParsing(self):
    super(type(self), self).EndParsing()
    
    # Make the text (including placeholder references) and list of placeholders,
    # then strip and store leading and trailing whitespace and create the
    # tclib.Message() and a clique to contain it.
    
    text = ''
    placeholders = []
    for item in self.mixed_content:
      if isinstance(item, types.StringTypes):
        text += item
      else:
        presentation = item.attrs['name'].upper()
        text += presentation
        ex = ' '
        if len(item.children):
          ex = item.children[0].GetCdata()
        original = item.GetCdata()
        placeholders.append(tclib.Placeholder(presentation, original, ex))
    
    m = _WHITESPACE.match(text)
    if m:
      self.ws_at_start = m.group('start')
      self.ws_at_end = m.group('end')
      text = m.group('body')
    
    self.shortcut_groups_ = self._SPLIT_RE.split(self.attrs['shortcut_groups'])
    self.shortcut_groups_ = [i for i in self.shortcut_groups_ if i != '']
    
    description_or_id = self.attrs['desc']
    if description_or_id == '' and 'name' in self.attrs:
      description_or_id = 'ID: %s' % self.attrs['name']
    
    message = tclib.Message(text=text, placeholders=placeholders,
                            description=description_or_id,
                            meaning=self.attrs['meaning'])
    self.clique = self.UberClique().MakeClique(message, self.IsTranslateable())
    for group in self.shortcut_groups_:
      self.clique.AddToShortcutGroup(group)
    if self.attrs['custom_type'] != '':
      self.clique.SetCustomType(util.NewClassInstance(self.attrs['custom_type'],
                                                      clique.CustomType))
    elif self.attrs['validation_expr'] != '':
      self.clique.SetCustomType(
        clique.OneOffCustomType(self.attrs['validation_expr']))
      
  def GetCliques(self):
    if self.clique:
      return [self.clique]
    else:
      return []
  
  def Translate(self, lang):
    '''Returns a translated version of this message.
    '''
    assert self.clique
    msg = self.clique.MessageForLanguage(lang,
                                         self.PseudoIsAllowed(),
                                         self.ShouldFallbackToEnglish()
                                         ).GetRealContent()
    return msg.replace('[GRITLANGCODE]', lang)
  
  def NameOrOffset(self):
    if 'name' in self.attrs:
      return self.attrs['name']
    else:
      return self.attrs['offset']

  def GetDataPackPair(self, output_dir, lang):
    '''Returns a (id, string) pair that represents the string id and the string
    in utf8.  This is used to generate the data pack data file.
    '''
    from grit.format import rc_header
    id_map = rc_header.Item.tids_
    id = id_map[self.GetTextualIds()[0]]

    message = self.ws_at_start + self.Translate(lang) + self.ws_at_end
    # |message| is a python unicode string, so convert to a utf16 byte stream
    # because that's the format of datapacks.  We skip the first 2 bytes
    # because it is the BOM.
    return id, message.encode('utf16')[2:]

  # static method
  def Construct(parent, message, name, desc='', meaning='', translateable=True):
    '''Constructs a new message node that is a child of 'parent', with the
    name, desc, meaning and translateable attributes set using the same-named
    parameters and the text of the message and any placeholders taken from
    'message', which must be a tclib.Message() object.'''
    # Convert type to appropriate string
    if translateable:
      translateable = 'true'
    else:
      translateable = 'false'
    
    node = MessageNode()
    node.StartParsing('message', parent)
    node.HandleAttribute('name', name)
    node.HandleAttribute('desc', desc)
    node.HandleAttribute('meaning', meaning)
    node.HandleAttribute('translateable', translateable)
    
    items = message.GetContent()
    for ix in range(len(items)):
      if isinstance(items[ix], types.StringTypes):
        text = items[ix]
        
        # Ensure whitespace at front and back of message is correctly handled.
        if ix == 0:
          text = "'''" + text
        if ix == len(items) - 1:
          text = text + "'''"
        
        node.AppendContent(text)
      else:
        phnode = PhNode()
        phnode.StartParsing('ph', node)
        phnode.HandleAttribute('name', items[ix].GetPresentation())
        phnode.AppendContent(items[ix].GetOriginal())
        
        if len(items[ix].GetExample()) and items[ix].GetExample() != ' ':
          exnode = ExNode()
          exnode.StartParsing('ex', phnode)
          exnode.AppendContent(items[ix].GetExample())
          exnode.EndParsing()
          phnode.AddChild(exnode)
        
        phnode.EndParsing()
        node.AddChild(phnode)
    
    node.EndParsing()
    return node
  Construct = staticmethod(Construct)

class PhNode(base.ContentNode):
  '''A <ph> element.'''
  
  def _IsValidChild(self, child):
    return isinstance(child, ExNode)

  def MandatoryAttributes(self):
    return ['name']

  def EndParsing(self):
    super(type(self), self).EndParsing()
    # We only allow a single example for each placeholder
    if len(self.children) > 1:
      raise exception.TooManyExamples()


class ExNode(base.ContentNode):
  '''An <ex> element.'''
  pass

