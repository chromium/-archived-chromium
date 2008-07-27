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

'''Support for "strings.xml" format used by Muppet plug-ins in Google Desktop.'''

import StringIO
import types
import re
import xml.sax
import xml.sax.handler
import xml.sax.saxutils

from grit.gather import regexp
from grit import util
from grit import tclib

# Placeholders can be defined in strings.xml files by putting the name of the
# placeholder between [![ and ]!] e.g. <MSG>Hello [![USER]!] how are you<MSG>
PLACEHOLDER_RE = re.compile('(\[!\[|\]!\])')


class MuppetStringsContentHandler(xml.sax.handler.ContentHandler):
  '''A very dumb parser for splitting the strings.xml file into translateable
  and nontranslateable chunks.'''
  
  def __init__(self, parent):
    self.curr_elem = ''
    self.curr_text = ''
    self.parent = parent
    self.description = ''
    self.meaning = ''
    self.translateable = True
  
  def startElement(self, name, attrs):
    if (name != 'strings'):
      self.curr_elem = name
      
      attr_names = attrs.getQNames()
      if 'desc' in attr_names:
        self.description = attrs.getValueByQName('desc')
      if 'meaning' in attr_names:
        self.meaning = attrs.getValueByQName('meaning')
      if 'translateable' in attr_names:
        value = attrs.getValueByQName('translateable')
        if value.lower() not in ['true', 'yes']:
          self.translateable = False
      
      att_text = []
      for attr_name in attr_names:
        att_text.append(' ')
        att_text.append(attr_name)
        att_text.append('=')
        att_text.append(
          xml.sax.saxutils.quoteattr(attrs.getValueByQName(attr_name)))
      
      self.parent._AddNontranslateableChunk("<%s%s>" %
                                            (name, ''.join(att_text)))
  
  def characters(self, content):
    if self.curr_elem != '':
      self.curr_text += content
    
  def endElement(self, name):
    if name != 'strings':
      self.parent.AddMessage(self.curr_text, self.description,
                             self.meaning, self.translateable)
      self.parent._AddNontranslateableChunk("</%s>\n" % name)
      self.curr_elem = ''
      self.curr_text = ''
      self.description = ''
      self.meaning = ''
      self.translateable = True
  
  def ignorableWhitespace(self, whitespace):
    pass

class MuppetStrings(regexp.RegexpGatherer):
  '''Supports the strings.xml format used by Muppet gadgets.'''
  
  def __init__(self, text):
    if util.IsExtraVerbose():
      print text
    regexp.RegexpGatherer.__init__(self, text)

  def AddMessage(self, msgtext, description, meaning, translateable):
    if msgtext == '':
      return
    
    msg = tclib.Message(description=description, meaning=meaning)
    
    unescaped_text = self.UnEscape(msgtext)
    parts = PLACEHOLDER_RE.split(unescaped_text)
    in_placeholder = False
    for part in parts:
      if part == '':
        continue
      elif part == '[![':
        in_placeholder = True
      elif part == ']!]':
        in_placeholder = False
      else:
        if in_placeholder:
          msg.AppendPlaceholder(tclib.Placeholder(part, '[![%s]!]' % part,
                                                  '(placeholder)'))
        else:
          msg.AppendText(part)
    
    self.skeleton_.append(
      self.uberclique.MakeClique(msg, translateable=translateable))
    
    # if statement needed because this is supposed to be idempotent (so never
    # set back to false)
    if translateable:
      self.translatable_chunk_ = True
  
  # Although we use the RegexpGatherer base class, we do not use the
  # _RegExpParse method of that class to implement Parse().  Instead, we
  # parse using a SAX parser.
  def Parse(self):
    if (self.have_parsed_):
      return
    self._AddNontranslateableChunk(u'<strings>\n')
    stream = StringIO.StringIO(self.text_)
    handler = MuppetStringsContentHandler(self)
    xml.sax.parse(stream, handler)
    self._AddNontranslateableChunk(u'</strings>\n')
  
  def Escape(self, text):
    return util.EncodeCdata(text)
  
  def FromFile(filename_or_stream, extkey=None, encoding='cp1252'):
    if isinstance(filename_or_stream, types.StringTypes):
      if util.IsVerbose():
        print "MuppetStrings reading file %s, encoding %s" % (
          filename_or_stream, encoding)
      filename_or_stream = util.WrapInputStream(file(filename_or_stream, 'r'), encoding)
    return MuppetStrings(filename_or_stream.read())
  FromFile = staticmethod(FromFile)
