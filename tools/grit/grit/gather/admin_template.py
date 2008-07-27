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

'''Gatherer for administrative template files.
'''

import re
import types

from grit.gather import regexp
from grit import exception
from grit import tclib
from grit import util


class MalformedAdminTemplateException(exception.Base):
  '''This file doesn't look like a .adm file to me.'''
  def __init__(self, msg=''):
    exception.Base.__init__(self, msg)


class AdmGatherer(regexp.RegexpGatherer):
  '''Gatherer for the translateable portions of an admin template.
  
  This gatherer currently makes the following assumptions:
  - there is only one [strings] section and it is always the last section
    of the file
  - translateable strings do not need to be escaped.
  '''

  # Finds the strings section as the group named 'strings'
  _STRINGS_SECTION = re.compile('(?P<first_part>.+^\[strings\])(?P<strings>.+)\Z',
                                re.MULTILINE | re.DOTALL)
  
  # Finds the translateable sections from within the [strings] section.
  _TRANSLATEABLES = re.compile('^\s*[A-Za-z0-9_]+\s*=\s*"(?P<text>.+)"\s*$',
                               re.MULTILINE)
  
  def __init__(self, text):
    regexp.RegexpGatherer.__init__(self, text)
  
  def Escape(self, text):
    return text.replace('\n', '\\n')
  
  def UnEscape(self, text):
    return text.replace('\\n', '\n')

  def Parse(self):
    if self.have_parsed_:
      return
    m = self._STRINGS_SECTION.match(self.text_)
    if not m:
      raise MalformedAdminTemplateException()
    # Add the first part, which is all nontranslateable, to the skeleton
    self._AddNontranslateableChunk(m.group('first_part'))
    # Then parse the rest using the _TRANSLATEABLES regexp.
    self._RegExpParse(self._TRANSLATEABLES, m.group('strings'))
  
  # static method
  def FromFile(adm_file, ext_key=None, encoding='cp1252'):
    '''Loads the contents of 'adm_file' in encoding 'encoding' and creates
    an AdmGatherer instance that gathers from those contents.
    
    The 'ext_key' parameter is ignored.
    
    Args:
      adm_file: file('bingo.rc') | 'filename.rc'
      encoding: 'utf-8'
    
    Return:
      AdmGatherer(contents_of_file)
    '''
    if isinstance(adm_file, types.StringTypes):
      adm_file = util.WrapInputStream(file(adm_file, 'r'), encoding)
    return AdmGatherer(adm_file.read())
  FromFile = staticmethod(FromFile)
