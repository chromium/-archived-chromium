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
  'target_defaults': {
    'include_dirs': [
      '..',
      '../..',
      '../../<(gtestdir)',
    ],
  },
  'targets': [
    {
      'target_name': 'o3dSerializer',
      'type': 'static_library',
      'sources': [
        'cross/serializer.cc',
        'cross/serializer.h',
        'cross/serializer_binary.cc',
        'cross/serializer_binary.h',
        'cross/version.h',
      ],
    },
    {
      'target_name': 'o3dSerializerTest',
      'type': 'none',
      'dependencies': [
        'o3dSerializer',
      ],
      'direct_dependent_settings': {
        'sources': [
          'cross/serializer_test.cc',
        ],
      },
    },
  ],
}
