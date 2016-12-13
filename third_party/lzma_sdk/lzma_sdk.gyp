# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'lzma_sdk',
      'type': '<(library)',
      'defines': [
        '_LZMA_PROB32',
        '_LZMA_IN_CB',
      ],
      'include_dirs': [
        '.',
      ],
      # TODO:  original configuration had /wd4800, add if
      # necessary and delete if not.
      #        '/wd4800',
      'msvs_guid': 'B84553C8-5676-427B-B3E4-23DDDC4DBC7B',
      'sources': [
        '7zCrc.c',
        '7zCrc.h',
        'Archive/7z/7zAlloc.c',
        'Archive/7z/7zAlloc.h',
        'Archive/7z/7zBuffer.c',
        'Archive/7z/7zBuffer.h',
        'Archive/7z/7zDecode.c',
        'Archive/7z/7zDecode.h',
        'Archive/7z/7zExtract.c',
        'Archive/7z/7zExtract.h',
        'Archive/7z/7zHeader.c',
        'Archive/7z/7zHeader.h',
        'Archive/7z/7zIn.c',
        'Archive/7z/7zIn.h',
        'Archive/7z/7zItem.c',
        'Archive/7z/7zItem.h',
        'Archive/7z/7zMethodID.c',
        'Archive/7z/7zMethodID.h',
        'Compress/Branch/BranchTypes.h',
        'Compress/Branch/BranchX86.c',
        'Compress/Branch/BranchX86.h',
        'Compress/Branch/BranchX86_2.c',
        'Compress/Branch/BranchX86_2.h',
        'Compress/Lzma/LzmaDecode.c',
        'Compress/Lzma/LzmaDecode.h',
        'Compress/Lzma/LzmaTypes.h',
        'Types.h',
      ],
      'direct_dependent_settings': {
        'defines': [
          '_LZMA_IN_CB',
        ],
        'include_dirs': [
          '.',
        ],
      },
    },
  ],
}
