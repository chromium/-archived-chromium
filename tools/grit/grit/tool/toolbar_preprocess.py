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

''' Toolbar preprocessing code. Turns all IDS_COMMAND macros in the RC file
into simpler constructs that can be understood by GRIT. Also deals with
expansion of $lf; placeholders into the correct linefeed character.
'''

import preprocess_interface

import re
import sys
import codecs

class ToolbarPreProcessor(preprocess_interface.PreProcessor):
  ''' Toolbar PreProcessing class.  
  '''

  _IDS_COMMAND_MACRO = re.compile(r'(.*IDS_COMMAND)\s*\(([a-zA-Z0-9_]*)\s*,\s*([a-zA-Z0-9_]*)\)(.*)')
  _LINE_FEED_PH = re.compile(r'\$lf;')
  _PH_COMMENT = re.compile(r'PHRWR')
  _COMMENT = re.compile(r'^(\s*)//.*')


  def Process(self, rctext, rcpath):
    ''' Processes the data in rctext.
    Args:
      rctext: string containing the contents of the RC file being processed
      rcpath: the path used to access the file.
    
    Return:
      The processed text.
    '''

    ret = ''
    rclines = rctext.splitlines()
    for line in rclines:

      if self._LINE_FEED_PH.search(line):
        # Replace "$lf;" placeholder comments by an empty line.
        # this will not be put into the processed result
        if self._PH_COMMENT.search(line):
          mm = self._COMMENT.search(line)
          if mm:
            line = '%s//' % mm.group(1)
            
        else:  
          # Replace $lf by the right linefeed character
          line = self._LINE_FEED_PH.sub(r'\\n', line)

      # Deal with IDS_COMMAND_MACRO stuff
      mo = self._IDS_COMMAND_MACRO.search(line)
      if mo:
        line = '%s_%s_%s%s' % (mo.group(1), mo.group(2), mo.group(3), mo.group(4))
      
      ret += (line + '\n') 

    return ret

