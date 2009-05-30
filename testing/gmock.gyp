# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'gmock',
      'type': '<(library)',
      'msvs_guid': 'F9D886ED-B09F-4B74-932F-D8E4691E6B7F',
      'dependencies': [
        'gtest.gyp:gtest',
      ],
      'sources': [
        'gmock/include/gmock/gmock-actions.h',
        'gmock/include/gmock/gmock-cardinalities.h',
        'gmock/include/gmock/gmock-generated-actions.h',
        'gmock/include/gmock/gmock-generated-function-mockers.h',
        'gmock/include/gmock/gmock-generated-matchers.h',
        'gmock/include/gmock/gmock-generated-nice-strict.h',
        'gmock/include/gmock/gmock-matchers.h',
        'gmock/include/gmock/gmock-printers.h',
        'gmock/include/gmock/gmock-spec-builders.h',
        'gmock/include/gmock/gmock.h',
        'gmock/include/gmock/internal/gmock-generated-internal-utils.h',
        'gmock/include/gmock/internal/gmock-internal-utils.h',
        'gmock/include/gmock/internal/gmock-port.h',
        'gmock/src/gmock-all.cc',
        'gmock/src/gmock-cardinalities.cc',
        'gmock/src/gmock-internal-utils.cc',
        'gmock/src/gmock-matchers.cc',
        'gmock/src/gmock-printers.cc',
        'gmock/src/gmock-spec-builders.cc',
        'gmock/src/gmock.cc',
      ],
      'sources!': [
        'gmock/src/gmock-all.cc',  # Not needed by our build.
      ],
      'include_dirs': [
        'gmock',
        'gmock/include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'gmock/include',  # So that gmock headers can find themselves.
        ],
      },
      'export_dependent_settings': [
        'gtest.gyp:gtest',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '../third_party/boost/boost.gyp:boost_tuple',
          ],
          'export_dependent_settings': [
            '../third_party/boost/boost.gyp:boost_tuple',
          ]
        }],
      ],
    },
    {
      # Note that calling this "gmock_main" confuses the scons build,
      # which uses "_main" on scons files to produce special behavior.
      'target_name': 'gmockmain',
      'type': '<(library)',
      'dependencies': [
        'gmock',
      ],
      'sources': [
        'gmock/src/gmock_main.cc',
      ],
    },
  ],
}
