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
      'target_name': 'printing',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'units.cc',
        'units.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'conditions': [
        ['OS!="linux"', {'sources/': [['exclude', '_linux\\.cc$']]}],
        ['OS!="mac"', {'sources/': [['exclude', '_mac\\.(cc|mm?)$']]}],
        ['OS!="win"', {
          'sources/': [['exclude', '_win\\.cc$']]
        }, {  # else: OS=="win"
          'sources/': [['exclude', '_posix\\.cc$']]
        }],
      ],
    },
    {
      'target_name': 'printing_unittests',
      'type': 'executable',
      'dependencies': [
        'printing',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'units_unittest.cc',
      ],
    },
  ],
}
