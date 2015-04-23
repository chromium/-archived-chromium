# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'bspatch',
      'type': '<(library)',
      'msvs_guid': 'D7ED06E8-6138-4CE3-A906-5EF1D9C804E0',
      'dependencies': [
        '../lzma_sdk/lzma_sdk.gyp:lzma_sdk',
      ],
      'include_dirs': [
        '.',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
      'sources': [
        'mbspatch.cc',
        'mbspatch.h',
      ],
    },
  ],
}
