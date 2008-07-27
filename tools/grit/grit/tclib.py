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

'''Adaptation of the extern.tclib classes for our needs.
'''


import re
import types

from grit import exception
import grit.extern.tclib

def Identity(i):
  return i


class BaseMessage(object):
  '''Base class with methods shared by Message and Translation.
  '''
  
  def __init__(self, text='', placeholders=[], description='', meaning=''):
    self.parts = []
    self.placeholders = []
    self.description = description
    self.meaning = meaning
    self.dirty = True  # True if self.id is (or might be) wrong
    self.id = 0
    
    if text != '':
      if not placeholders or placeholders == []:
        self.AppendText(text)
      else:
        tag_map = {}
        for placeholder in placeholders:
          tag_map[placeholder.GetPresentation()] = [placeholder, 0]
        tag_re = '(' + '|'.join(tag_map.keys()) + ')'
        # This creates a regexp like '(TAG1|TAG2|TAG3)'
        chunked_text = re.split(tag_re, text)
        for chunk in chunked_text:
          if chunk: # ignore empty chunk
            if tag_map.has_key(chunk):
              self.AppendPlaceholder(tag_map[chunk][0])
              tag_map[chunk][1] += 1 # increase placeholder use count
            else:
              self.AppendText(chunk)
        for key in tag_map.keys():
          assert tag_map[key][1] != 0
  
  def GetRealContent(self, escaping_function=Identity):
    '''Returns the original content, i.e. what your application and users
    will see.
    
    Specify a function to escape each translateable bit, if you like.
    '''
    bits = []
    for item in self.parts:
      if isinstance(item, types.StringTypes):
        bits.append(escaping_function(item))
      else:
        bits.append(item.GetOriginal())
    return ''.join(bits)
  
  def GetPresentableContent(self):
    presentable_content = []
    for part in self.parts:
      if isinstance(part, Placeholder):
        presentable_content.append(part.GetPresentation())
      else:
        presentable_content.append(part)
    return ''.join(presentable_content)
  
  def AppendPlaceholder(self, placeholder):
    assert isinstance(placeholder, Placeholder)
    dup = False
    for other in self.GetPlaceholders():
      if (other.presentation.find(placeholder.presentation) != -1 or
          placeholder.presentation.find(other.presentation) != -1):
        assert(False, "Placeholder names must be unique and must not overlap")
      if other.presentation == placeholder.presentation:
        assert other.original == placeholder.original
        dup = True
    
    if not dup:
      self.placeholders.append(placeholder)
    self.parts.append(placeholder)
    self.dirty = True
  
  def AppendText(self, text):
    assert isinstance(text, types.StringTypes)
    assert text != ''
    
    self.parts.append(text)
    self.dirty = True
  
  def GetContent(self):
    '''Returns the parts of the message.  You may modify parts if you wish.
    Note that you must not call GetId() on this object until you have finished
    modifying the contents.
    '''
    self.dirty = True  # user might modify content
    return self.parts
  
  def GetDescription(self):
    return self.description
  
  def SetDescription(self, description):
    self.description = description
  
  def GetMeaning(self):
    return self.meaning
  
  def GetId(self):
    if self.dirty:
      self.id = self.GenerateId()
      self.dirty = False
    return self.id
  
  def GenerateId(self):
    # Must use a UTF-8 encoded version of the presentable content, along with
    # the meaning attribute, to match the TC.
    return grit.extern.tclib.GenerateMessageId(
      self.GetPresentableContent().encode('utf-8'), self.meaning)
  
  def GetPlaceholders(self):
    return self.placeholders
  
  def FillTclibBaseMessage(self, msg):
    msg.SetDescription(self.description.encode('utf-8'))
    
    for part in self.parts:
      if isinstance(part, Placeholder):
        ph = grit.extern.tclib.Placeholder(
          part.presentation.encode('utf-8'),
          part.original.encode('utf-8'),
          part.example.encode('utf-8'))
        msg.AppendPlaceholder(ph)
      else:
        msg.AppendText(part.encode('utf-8'))


class Message(BaseMessage):
  '''A message.'''  
  
  def __init__(self, text='', placeholders=[], description='', meaning=''):
    BaseMessage.__init__(self, text, placeholders, description, meaning)
  
  def ToTclibMessage(self):
    msg = grit.extern.tclib.Message('utf-8', meaning=self.meaning)
    self.FillTclibBaseMessage(msg)
    return msg

class Translation(BaseMessage):
  '''A translation.'''
  
  def __init__(self, text='', id='', placeholders=[], description='', meaning=''):
    BaseMessage.__init__(self, text, placeholders, description, meaning)
    self.id = id
  
  def GetId(self):
    assert id != '', "ID has not been set."
    return self.id
  
  def SetId(self, id):
    self.id = id
  
  def ToTclibMessage(self):
    msg = grit.extern.tclib.Message(
      'utf-8', id=self.id, meaning=self.meaning)
    self.FillTclibBaseMessage(msg)
    return msg


class Placeholder(grit.extern.tclib.Placeholder):
  '''Modifies constructor to accept a Unicode string
  '''
  
  # Must match placeholder presentation names
  _NAME_RE = re.compile('[A-Za-z0-9_]+')
  
  def __init__(self, presentation, original, example):
    '''Creates a new placeholder.
    
    Args:
      presentation: 'USERNAME'
      original: '%s'
      example: 'Joi'
    '''
    assert presentation != ''
    assert original != ''
    assert example != ''
    if not self._NAME_RE.match(presentation):
      raise exception.InvalidPlaceholderName(presentation)
    self.presentation = presentation
    self.original = original
    self.example = example
  
  def GetPresentation(self):
    return self.presentation
  
  def GetOriginal(self):
    return self.original
  
  def GetExample(self):
    return self.example

