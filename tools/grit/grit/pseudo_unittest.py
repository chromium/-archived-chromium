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

'''Unit tests for grit.pseudo'''

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '..'))

import unittest

from grit import pseudo
from grit import tclib


class PseudoUnittest(unittest.TestCase):
  def testVowelMapping(self):
    self.failUnless(pseudo.MapVowels('abebibobuby') ==
                    u'\u00e5b\u00e9b\u00efb\u00f4b\u00fcb\u00fd')
    self.failUnless(pseudo.MapVowels('ABEBIBOBUBY') ==
                    u'\u00c5B\u00c9B\u00cfB\u00d4B\u00dcB\u00dd')
  
  def testPseudoString(self):
    out = pseudo.PseudoString('hello')
    self.failUnless(out == pseudo.MapVowels(u'hePelloPo', True))
  
  def testConsecutiveVowels(self):
    out = pseudo.PseudoString("beautiful weather, ain't it?")
    self.failUnless(out == pseudo.MapVowels(
      u"beauPeautiPifuPul weaPeathePer, aiPain't iPit?", 1))
  
  def testCapitals(self):
    out = pseudo.PseudoString("HOWDIE DOODIE, DR. JONES")
    self.failUnless(out == pseudo.MapVowels(
      u"HOPOWDIEPIE DOOPOODIEPIE, DR. JOPONEPES", 1))

  def testPseudoMessage(self):
    msg = tclib.Message(text='Hello USERNAME, how are you?',
                        placeholders=[
                          tclib.Placeholder('USERNAME', '%s', 'Joi')])
    trans = pseudo.PseudoMessage(msg)
    # TODO(joi) It would be nicer if 'you' -> 'youPou' instead of
    # 'you' -> 'youPyou' and if we handled the silent e in 'are'
    self.failUnless(trans.GetPresentableContent() ==
                    pseudo.MapVowels(
                      u'HePelloPo USERNAME, hoPow aParePe youPyou?', 1))


if __name__ == '__main__':
  unittest.main()