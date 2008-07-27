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

'''Unit tests for grit.node.message'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../..'))

import unittest
import StringIO

from grit.node import message
from grit import grd_reader
from grit import tclib

class MessageUnittest(unittest.TestCase):
  def testMessage(self):
    buf = StringIO.StringIO('''<message name="IDS_GREETING"
      desc="Printed to greet the currently logged in user">
      Hello <ph name="USERNAME">%s<ex>Joi</ex></ph>, how are you doing today?
      </message>''')
    res = grd_reader.Parse(buf, flexible_root = True)
    cliques = res.GetCliques()
    content = cliques[0].GetMessage().GetPresentableContent()
    self.failUnless(content == 'Hello USERNAME, how are you doing today?')
  
  def testMessageWithWhitespace(self):
    buf = StringIO.StringIO('<message name="IDS_BLA" desc="">'
                            '\'\'\'  Hello there <ph name="USERNAME">%s</ph>   \'\'\''
                            '</message>')
    res = grd_reader.Parse(buf, flexible_root = True)
    content = res.GetCliques()[0].GetMessage().GetPresentableContent()
    self.failUnless(content == 'Hello there USERNAME')
    self.failUnless(res.ws_at_start == '  ')
    self.failUnless(res.ws_at_end == '   ')

  def testConstruct(self):
    msg = tclib.Message(text="   Hello USERNAME, how are you?   BINGO\t\t",
                        placeholders=[tclib.Placeholder('USERNAME', '%s', 'Joi'),
                                      tclib.Placeholder('BINGO', '%d', '11')])
    msg_node = message.MessageNode.Construct(None, msg, 'BINGOBONGO')
    self.failUnless(msg_node.children[0].name == 'ph')
    self.failUnless(msg_node.children[0].children[0].name == 'ex')
    self.failUnless(msg_node.children[0].children[0].GetCdata() == 'Joi')
    self.failUnless(msg_node.children[1].children[0].GetCdata() == '11')
    self.failUnless(msg_node.ws_at_start == '   ')
    self.failUnless(msg_node.ws_at_end == '\t\t')
  
  def testUnicodeConstruct(self):
    text = u'Howdie \u00fe'
    msg = tclib.Message(text=text)
    msg_node = message.MessageNode.Construct(None, msg, 'BINGOBONGO')
    msg_from_node = msg_node.GetCdata()
    self.failUnless(msg_from_node == text)

if __name__ == '__main__':
  unittest.main()