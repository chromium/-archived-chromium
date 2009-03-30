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
      'msvs_guid': '9E5416B9-B91B-4029-93F4-102C1AD5CAF4',
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
      'msvs_guid': '8B2EE5D9-41BC-4AA2-A401-2DC143A05D2E',
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
