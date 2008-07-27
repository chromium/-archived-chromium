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

'''Unit test that checks postprocessing of files.
   Tests postprocessing by having the postprocessor
   modify the grd data tree, changing the message name attributes.
'''

import os
import re
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '..'))

import unittest

import grit.tool.postprocess_interface
from grit.tool import rc2grd


class PostProcessingUnittest(unittest.TestCase):
  
  def testPostProcessing(self):
    rctext = '''STRINGTABLE
BEGIN
  DUMMY_STRING_1         "String 1"
  // Some random description 
  DUMMY_STRING_2        "This text was added during preprocessing"
END  
    '''
    tool = rc2grd.Rc2Grd()
    class DummyOpts(object):
      verbose = False
      extra_verbose = False
    tool.o = DummyOpts()  
    tool.post_process = 'grit.tool.postprocess_unittest.DummyPostProcessor'
    result = tool.Process(rctext, '.\resource.rc')
    
    self.failUnless(
      result.children[2].children[2].children[0].attrs['name'] == 'SMART_STRING_1')
    self.failUnless(
      result.children[2].children[2].children[1].attrs['name'] == 'SMART_STRING_2')
    
class DummyPostProcessor(grit.tool.postprocess_interface.PostProcessor):
  '''
  Post processing replaces all message name attributes containing "DUMMY" to
  "SMART".
  '''
  def Process(self, rctext, rcpath, grdnode):
    smarter = re.compile(r'(DUMMY)(.*)')
    messages = grdnode.children[2].children[2]
    for node in messages.children:
      name_attr = node.attrs['name']
      m = smarter.search(name_attr)
      if m:
         node.attrs['name'] = 'SMART' + m.group(2)
    return grdnode 

if __name__ == '__main__':
  unittest.main()
