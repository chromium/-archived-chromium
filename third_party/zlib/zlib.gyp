# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'zlib',
      'type': 'static_library',
      'msvs_guid': '8423AF0D-4B88-4EBF-94E1-E4D00D00E21C',
      'sources': [
        'contrib/minizip/ioapi.c',
        'contrib/minizip/ioapi.h',
        'contrib/minizip/iowin32.c',
        'contrib/minizip/iowin32.h',
        'contrib/minizip/unzip.c',
        'contrib/minizip/unzip.h',
        'adler32.c',
        'compress.c',
        'crc32.c',
        'crc32.h',
        'deflate.c',
        'deflate.h',
        'gzio.c',
        'infback.c',
        'inffast.c',
        'inffast.h',
        'inffixed.h',
        'inflate.c',
        'inflate.h',
        'inftrees.c',
        'inftrees.h',
        'mozzconf.h',
        'trees.c',
        'trees.h',
        'uncompr.c',
        'zconf.h',
        'zlib.h',
        'zutil.c',
        'zutil.h',
      ],
      'include_dirs': [
        '.',
        # For contrib/minizip
        '../..',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
      'conditions': [
        ['OS!="win"', {
          'product_name': 'z',
          'sources!': [
            'contrib/minizip/iowin32.c'
          ],
        }],
      ],
    },
  ],
}
