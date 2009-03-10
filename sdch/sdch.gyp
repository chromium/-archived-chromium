# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'sdch',
      'type': 'static_library',
      'msvs_guid': 'F54ABC59-5C00-414A-A9BA-BAF26D1699F0',
      'sources': [
        'open-vcdiff/src/addrcache.cc',
        'open-vcdiff/src/adler32.c',
        'open-vcdiff/src/blockhash.cc',
        'open-vcdiff/src/blockhash.h',
        'open-vcdiff/src/checksum.h',
        'open-vcdiff/src/codetable.cc',
        'open-vcdiff/src/codetable.h',
        'open-vcdiff/src/compile_assert.h',
        'open-vcdiff/src/decodetable.cc',
        'open-vcdiff/src/decodetable.h',
        'open-vcdiff/src/encodetable.cc',
        'open-vcdiff/src/encodetable.h',
        'open-vcdiff/src/google/output_string.h',
        'open-vcdiff/src/google/vcdecoder.h',
        'open-vcdiff/src/headerparser.cc',
        'open-vcdiff/src/headerparser.h',
        'open-vcdiff/src/instruction_map.cc',
        'open-vcdiff/src/instruction_map.h',
        'open-vcdiff/src/logging.cc',
        'open-vcdiff/src/logging.h',
        'open-vcdiff/src/rolling_hash.h',
        'open-vcdiff/src/testing.h',
        'open-vcdiff/src/varint_bigendian.cc',
        'open-vcdiff/src/varint_bigendian.h',
        'open-vcdiff/src/vcdecoder.cc',
        'open-vcdiff/src/vcdiff_defs.h',
        'open-vcdiff/src/vcdiffengine.cc',
        'open-vcdiff/src/vcdiffengine.h',
        'open-vcdiff/src/zconf.h',
        'open-vcdiff/src/zlib.h',
        'open-vcdiff/vsprojects/config.h',
        'open-vcdiff/vsprojects/stdint.h',
      ],
      'include_dirs': [
        'open-vcdiff/src',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'open-vcdiff/src',
        ],
      },
      'conditions': [
        [ 'OS == "linux"', { 'include_dirs': [ 'linux' ] } ],
        [ 'OS == "mac"', { 'include_dirs': [ 'mac' ] } ],
        [ 'OS == "win"', { 'include_dirs': [ 'open-vcdiff/vsprojects' ] } ],
      ],
    },
  ],
}
