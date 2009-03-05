#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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

