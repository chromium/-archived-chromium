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
      'target_name': 'gtest',
      'type': '<(library)',
      'msvs_guid': 'BFE8E2A7-3B3B-43B0-A994-3058B852DB8B',
      'sources': [
        'gtest/include/gtest/internal/gtest-death-test-internal.h',
        'gtest/include/gtest/internal/gtest-filepath.h',
        'gtest/include/gtest/internal/gtest-internal.h',
        'gtest/include/gtest/internal/gtest-linked_ptr.h',
        'gtest/include/gtest/internal/gtest-param-util-generated.h',
        'gtest/include/gtest/internal/gtest-param-util.h',
        'gtest/include/gtest/internal/gtest-port.h',
        'gtest/include/gtest/internal/gtest-string.h',
        'gtest/include/gtest/internal/gtest-type-util.h',
        'gtest/include/gtest/gtest-death-test.h',
        'gtest/include/gtest/gtest-message.h',
        'gtest/include/gtest/gtest-param-test.h',
        'gtest/include/gtest/gtest-spi.h',
        'gtest/include/gtest/gtest-test-part.h',
        'gtest/include/gtest/gtest-typed-test.h',
        'gtest/include/gtest/gtest.h',
        'gtest/include/gtest/gtest_pred_impl.h',
        'gtest/include/gtest/gtest_prod.h',
        'gtest/src/gtest-test-part.cc',
        'gtest/src/gtest-typed-test.cc',
        'gtest/src/gtest-death-test.cc',
        'gtest/src/gtest-filepath.cc',
        'gtest/src/gtest-internal-inl.h',
        'gtest/src/gtest-port.cc',
        'gtest/src/gtest.cc',
        'gtest/src/gtest_main.cc',
        'multiprocess_func_list.cc',
        'multiprocess_func_list.h',
        'platform_test.h',
      ],
      'include_dirs': [
        'gtest',
        'gtest/include',
      ],
      'conditions': [
        [ 'OS == "mac"', { 'sources': [ 'platform_test_mac.mm' ] } ],
      ],
      'direct_dependent_settings': {
        'defines': [
          'UNIT_TEST',
        ],
        'include_dirs': [
          'gtest',
          'gtest/include',
        ],
        'target_conditions': [
          ['_type=="executable"', {'test': 1}],
        ],
      },
    },
  ],
}
