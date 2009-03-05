#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''A baseclass for simple gatherers based on regular expressions.
'''

import re
import types

from grit.gather import interface
from grit import clique
from grit import tclib


class RegexpGatherer(interface.GathererBase):
  '''Common functionality of gatherers based on parsing using a single
  regular expression.
  '''

  DescriptionMapping_ = {
      'CAPTION' : 'This is a caption for a dialog',
      'CHECKBOX' : 'This is a label for a checkbox',
      'CONTROL': 'This is the text on a control',
      'CTEXT': 'This is a label for a control',
      'DEFPUSHBUTTON': 'This is a button definition',
      'GROUPBOX': 'This is a label for a grouping',
      'ICON': 'This is a label for an icon',
      'LTEXT': 'This is the text for a label',
      'PUSHBUTTON': 'This is the text for a button',
    }

  def __init__(self, text):
    interface.GathererBase.__init__(self)
    # Original text of what we're parsing
    self.text_ = text.strip()
    # List of parts of the document. Translateable parts are clique.MessageClique
    # objects, nontranslateable parts are plain strings. Translated messages are
    # inserted back into the skeleton using the quoting rules defined by
    # self.Escape()
    self.skeleton_ = []
    # A list of the names of IDs that need to be defined for this resource
    # section to compile correctly.
    self.ids_ = []
    # True if Parse() has already been called.
    self.have_parsed_ = False
    # True if a translatable chunk has been added
    self.translatable_chunk_ = False
    # If not None, all parts of the document will be put into this single
    # message; otherwise the normal skeleton approach is used.
    self.single_message_ = None
    # Number to use for the next placeholder name.  Used only if single_message
    # is not None
    self.ph_counter_ = 1

  def GetText(self):
    '''Returns the original text of the section'''
    return self.text_

  def Escape(self, text):
    '''Subclasses can override.  Base impl is identity.
    '''
    return text

  def UnEscape(self, text):
    '''Subclasses can override. Base impl is identity.
    '''
    return text

  def GetTextualIds(self):
    '''Returns the list of textual IDs that need to be defined for this
    resource section to compile correctly.'''
    return self.ids_

  def GetCliques(self):
    '''Returns the message cliques for each translateable message in the
    resource section.'''
    return filter(lambda x: isinstance(x, clique.MessageClique), self.skeleton_)

  def Translate(self, lang, pseudo_if_not_available=True,
                skeleton_gatherer=None, fallback_to_english=False):
    if len(self.skeleton_) == 0:
      raise exception.NotReady()
    if skeleton_gatherer:
      assert len(skeleton_gatherer.skeleton_) == len(self.skeleton_)

    out = []
    for ix in range(len(self.skeleton_)):
      if isinstance(self.skeleton_[ix], types.StringTypes):
        if skeleton_gatherer:
          # Make sure the skeleton is like the original
          assert(isinstance(skeleton_gatherer.skeleton_[ix], types.StringTypes))
          out.append(skeleton_gatherer.skeleton_[ix])
        else:
          out.append(self.skeleton_[ix])
      else:
        if skeleton_gatherer:  # Make sure the skeleton is like the original
          assert(not isinstance(skeleton_gatherer.skeleton_[ix],
                                types.StringTypes))
        msg = self.skeleton_[ix].MessageForLanguage(lang,
                                                    pseudo_if_not_available,
                                                    fallback_to_english)

        def MyEscape(text):
          return self.Escape(text)
        text = msg.GetRealContent(escaping_function=MyEscape)
        out.append(text)
    return ''.join(out)

  # Contextualization elements. Used for adding additional information
  # to the message bundle description string from RC files.
  def AddDescriptionElement(self, string):
    if self.DescriptionMapping_.has_key(string):
      description = self.DescriptionMapping_[string]
    else:
      description = string
    if self.single_message_:
      self.single_message_.SetDescription(description)
    else:
      if (self.translatable_chunk_):
        message = self.skeleton_[len(self.skeleton_) - 1].GetMessage()
        message.SetDescription(description)

  def Parse(self):
    '''Parses the section.  Implemented by subclasses.  Idempotent.'''
    raise NotImplementedError()

  def _AddNontranslateableChunk(self, chunk):
    '''Adds a nontranslateable chunk.'''
    if self.single_message_:
      ph = tclib.Placeholder('XX%02dXX' % self.ph_counter_, chunk, chunk)
      self.ph_counter_ += 1
      self.single_message_.AppendPlaceholder(ph)
    else:
      self.skeleton_.append(chunk)

  def _AddTranslateableChunk(self, chunk):
    '''Adds a translateable chunk.  It will be unescaped before being added.'''
    # We don't want empty messages since they are redundant and the TC
    # doesn't allow them.
    if chunk == '':
      return

    unescaped_text = self.UnEscape(chunk)
    if self.single_message_:
      self.single_message_.AppendText(unescaped_text)
    else:
      self.skeleton_.append(self.uberclique.MakeClique(
        tclib.Message(text=unescaped_text)))
      self.translatable_chunk_ = True

  def _AddTextualId(self, id):
    self.ids_.append(id)

  def _RegExpParse(self, regexp, text_to_parse):
    '''An implementation of Parse() that can be used for resource sections that
    can be parsed using a single multi-line regular expression.

    All translateables must be in named groups that have names starting with
    'text'.  All textual IDs must be in named groups that have names starting
    with 'id'. All type definitions that can be included in the description
    field for contextualization purposes should have a name that starts with
    'type'.

    Args:
      regexp: re.compile('...', re.MULTILINE)
      text_to_parse:
    '''
    if self.have_parsed_:
      return
    self.have_parsed_ = True

    chunk_start = 0
    for match in regexp.finditer(text_to_parse):
      groups = match.groupdict()
      keys = groups.keys()
      keys.sort()
      self.translatable_chunk_ = False
      for group in keys:
        if group.startswith('id') and groups[group]:
          self._AddTextualId(groups[group])
        elif group.startswith('text') and groups[group]:
          self._AddNontranslateableChunk(
            text_to_parse[chunk_start : match.start(group)])
          chunk_start = match.end(group)  # Next chunk will start after the match
          self._AddTranslateableChunk(groups[group])
        elif group.startswith('type') and groups[group]:
          # Add the description to the skeleton_ list. This works because
          # we are using a sort set of keys, and because we assume that the
          # group name used for descriptions (type) will come after the "text"
          # group in alphabetical order. We also assume that there cannot be
          # more than one description per regular expression match.
          self.AddDescriptionElement(groups[group])

    self._AddNontranslateableChunk(text_to_parse[chunk_start:])

    if self.single_message_:
      self.skeleton_.append(self.uberclique.MakeClique(self.single_message_))

