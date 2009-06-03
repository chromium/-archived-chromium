# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'hunspell',
      'type': '<(library)',
      'msvs_guid': 'D5E8DCB2-9C61-446F-8BEE-B18CA0E0936E',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../third_party/icu38/icu38.gyp:icuuc',
      ],
      'defines': [
        'HUNSPELL_CHROME_CLIENT',
        'OPENOFFICEORG',
      ],
      'sources': [
        'google/bdict.h',
        'google/bdict_reader.cc',
        'google/bdict_reader.h',
        'google/bdict_writer.cc',
        'google/bdict_writer.h',
        'src/hunspell/affentry.cxx',
        'src/hunspell/affentry.hxx',
        'src/hunspell/affixmgr.cxx',
        'src/hunspell/affixmgr.hxx',
        'src/hunspell/atypes.hxx',
        'src/hunspell/baseaffix.hxx',
        'src/hunspell/csutil.cxx',
        'src/hunspell/csutil.hxx',
        'src/hunspell/dictmgr.cxx',
        'src/hunspell/dictmgr.hxx',
        'src/hunspell/hashmgr.cxx',
        'src/hunspell/hashmgr.hxx',
        'src/hunspell/htypes.hxx',
        'src/hunspell/hunspell.cxx',
        'src/hunspell/hunspell.h',
        'src/hunspell/hunspell.hxx',
        'src/hunspell/langnum.hxx',
        'src/hunspell/suggestmgr.cxx',
        'src/hunspell/suggestmgr.hxx',
        'src/hunspell/utf_info.hxx',
        'src/parsers/textparser.cxx',
        'src/parsers/textparser.hxx',
      ],
      'direct_dependent_settings': {
        'defines': [
          'HUNSPELL_CHROME_CLIENT',
          'USE_HUNSPELL',
        ],
      },
      'conditions': [
        ['OS=="linux"', {
          'cflags': [
            '-Wno-unused-value',
            '-Wno-unused-variable',
            '-Wno-write-strings',
          ],
        }],
      ],
    },
  ],
}
