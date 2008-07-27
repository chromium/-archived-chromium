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

'''Unit tests for base.Node functionality (as used in various subclasses)'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../..'))

import unittest

from grit.node import base
from grit.node import message
from grit.node import structure
from grit.node import variant

def MakePlaceholder(phname='BINGO'):
  ph = message.PhNode()
  ph.StartParsing(u'ph', None)
  ph.HandleAttribute(u'name', phname)
  ph.AppendContent(u'bongo')
  ph.EndParsing()
  return ph


class NodeUnittest(unittest.TestCase):
  def testWhitespaceHandling(self):
    # We test using the Message node type.
    node = message.MessageNode()
    node.StartParsing(u'hello', None)
    node.HandleAttribute(u'name', u'bla')
    node.AppendContent(u" '''  two spaces  ")
    node.EndParsing()
    self.failUnless(node.GetCdata() == u'  two spaces')
    
    node = message.MessageNode()
    node.StartParsing(u'message', None)
    node.HandleAttribute(u'name', u'bla')
    node.AppendContent(u"  two spaces  '''  ")
    node.EndParsing()
    self.failUnless(node.GetCdata() == u'two spaces  ')

  def testWhitespaceHandlingWithChildren(self):
    # We test using the Message node type.
    node = message.MessageNode()
    node.StartParsing(u'message', None)
    node.HandleAttribute(u'name', u'bla')
    node.AppendContent(u" '''  two spaces  ")
    node.AddChild(MakePlaceholder())
    node.AppendContent(u' space before and after ')
    node.AddChild(MakePlaceholder('BONGO'))
    node.AppendContent(u" space before two after  '''")
    node.EndParsing()
    self.failUnless(node.mixed_content[0] == u'  two spaces  ')
    self.failUnless(node.mixed_content[2] == u' space before and after ')
    self.failUnless(node.mixed_content[-1] == u' space before two after  ')

  def testXmlFormatMixedContent(self):
    # Again test using the Message node type, because it is the only mixed
    # content node.
    node = message.MessageNode()
    node.StartParsing(u'message', None)
    node.HandleAttribute(u'name', u'name')
    node.AppendContent(u'Hello <young> ')
    
    ph = message.PhNode()
    ph.StartParsing(u'ph', None)
    ph.HandleAttribute(u'name', u'USERNAME')
    ph.AppendContent(u'$1')
    ex = message.ExNode()
    ex.StartParsing(u'ex', None)
    ex.AppendContent(u'Joi')
    ex.EndParsing()
    ph.AddChild(ex)
    ph.EndParsing()
    
    node.AddChild(ph)
    node.EndParsing()
    
    non_indented_xml = node.Format(node)
    self.failUnless(non_indented_xml == u'<message name="name">\n  Hello '
                    u'&lt;young&gt; <ph name="USERNAME">$1<ex>Joi</ex></ph>'
                    u'\n</message>')
    
    indented_xml = node.FormatXml(u'  ')
    self.failUnless(indented_xml == u'  <message name="name">\n    Hello '
                    u'&lt;young&gt; <ph name="USERNAME">$1<ex>Joi</ex></ph>'
                    u'\n  </message>')

  def testXmlFormatMixedContentWithLeadingWhitespace(self):
    # Again test using the Message node type, because it is the only mixed
    # content node.
    node = message.MessageNode()
    node.StartParsing(u'message', None)
    node.HandleAttribute(u'name', u'name')
    node.AppendContent(u"'''   Hello <young> ")
    
    ph = message.PhNode()
    ph.StartParsing(u'ph', None)
    ph.HandleAttribute(u'name', u'USERNAME')
    ph.AppendContent(u'$1')
    ex = message.ExNode()
    ex.StartParsing(u'ex', None)
    ex.AppendContent(u'Joi')
    ex.EndParsing()
    ph.AddChild(ex)
    ph.EndParsing()
    
    node.AddChild(ph)
    node.AppendContent(u" yessiree '''")
    node.EndParsing()
    
    non_indented_xml = node.Format(node)
    self.failUnless(non_indented_xml ==
                    u"<message name=\"name\">\n  '''   Hello"
                    u' &lt;young&gt; <ph name="USERNAME">$1<ex>Joi</ex></ph>'
                    u" yessiree '''\n</message>")
    
    indented_xml = node.FormatXml(u'  ')
    self.failUnless(indented_xml ==
                    u"  <message name=\"name\">\n    '''   Hello"
                    u' &lt;young&gt; <ph name="USERNAME">$1<ex>Joi</ex></ph>'
                    u" yessiree '''\n  </message>")
    
    self.failUnless(node.GetNodeById('name'))
  
  def testXmlFormatContentWithEntities(self):
    '''Tests a bug where &nbsp; would not be escaped correctly.'''
    from grit import tclib
    msg_node = message.MessageNode.Construct(None, tclib.Message(
      text = 'BEGIN_BOLDHelloWHITESPACEthere!END_BOLD Bingo!',
      placeholders = [
        tclib.Placeholder('BEGIN_BOLD', '<b>', 'bla'),
        tclib.Placeholder('WHITESPACE', '&nbsp;', 'bla'),
        tclib.Placeholder('END_BOLD', '</b>', 'bla')]),
                                             'BINGOBONGO')
    xml = msg_node.FormatXml()
    self.failUnless(xml.find('&nbsp;') == -1, 'should have no entities')
    
  def testIter(self):
    # First build a little tree of message and ph nodes.
    node = message.MessageNode()
    node.StartParsing(u'message', None)
    node.HandleAttribute(u'name', u'bla')
    node.AppendContent(u" '''  two spaces  ")
    node.AppendContent(u' space before and after ')
    ph = message.PhNode()
    ph.StartParsing(u'ph', None)
    ph.AddChild(message.ExNode())
    ph.HandleAttribute(u'name', u'BINGO')
    ph.AppendContent(u'bongo')
    node.AddChild(ph)
    node.AddChild(message.PhNode())
    node.AppendContent(u" space before two after  '''")
    
    order = [message.MessageNode, message.PhNode, message.ExNode, message.PhNode]
    for n in node:
      self.failUnless(type(n) == order[0])
      order = order[1:]
    self.failUnless(len(order) == 0)


if __name__ == '__main__':
  unittest.main()
