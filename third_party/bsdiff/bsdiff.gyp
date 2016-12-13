# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'bsdiff',
      'type': 'executable',
      'msvs_guid': 'E1D0B89E-257B-4BCA-A0C6-A2CD997A2FDC',
      'dependencies': [
        '../bspatch/bspatch.gyp:bspatch',
      ],
      'link_settings': {
        'libraries': [
          '-lWs2_32.lib',
        ],
      },
      'sources': [
        'mbsdiff.cc',
      ],
    },
  ],
}
