#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Base classes for item formatters and file formatters.
'''


import re


class ItemFormatter(object):
  '''Base class for a formatter that knows how to format a single item.'''

  def Format(self, item, lang='', begin_item=True, output_dir='.'):
    '''Returns a Unicode string representing 'item' in the format known by this
    item formatter, for the language 'lang'.  May be called once at the
    start of the item (begin_item == True) and again at the end
    (begin_item == False), or only at the start of the item (begin_item == True)

    Args:
      item: anything
      lang: 'en'
      begin_item: True | False
      output_dir: '.'

    Return:
      u'hello'
    '''
    raise NotImplementedError()

