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

'''Unit test that checks preprocessing of files.
   Tests preprocessing by adding having the preprocessor
   provide the actual rctext data.
'''

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '..'))

import unittest

import grit.tool.preprocess_interface
from grit.tool import rc2grd


class PreProcessingUnittest(unittest.TestCase):
  
  def testPreProcessing(self):
    tool = rc2grd.Rc2Grd()
    class DummyOpts(object):
      verbose = False
      extra_verbose = False
    tool.o = DummyOpts()  
    tool.pre_process = 'grit.tool.preprocess_unittest.DummyPreProcessor'
    result = tool.Process('', '.\resource.rc')
    
    self.failUnless(
      result.children[2].children[2].children[0].attrs['name'] == 'DUMMY_STRING_1')
    
class DummyPreProcessor(grit.tool.preprocess_interface.PreProcessor):
  def Process(self, rctext, rcpath):
    rctext = '''STRINGTABLE
BEGIN
  DUMMY_STRING_1         "String 1"
  // Some random description 
  DUMMY_STRING_2        "This text was added during preprocessing"
END  
    '''
    return rctext

if __name__ == '__main__':
  unittest.main()
