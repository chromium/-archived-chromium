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
      'target_name': 'courgette_lib',
      'type': '<(library)',
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/lzma_sdk/lzma_sdk.gyp:lzma_sdk',
      ],
      'msvs_guid': '9A72A362-E617-4205-B9F2-43C6FB280FA1',
      'sources': [
        'adjustment_method.cc',
        'adjustment_method_2.cc',
        'adjustment_method.h',
        'assembly_program.cc',
        'assembly_program.h',
        'third_party/bsdiff.h',
        'third_party/bsdiff_apply.cc',
        'third_party/bsdiff_create.cc',
        'courgette.h',
        'crc.cc',
        'crc.h',
        'difference_estimator.cc',
        'difference_estimator.h',
        'disassembler.cc',
        'disassembler.h',
        'encoded_program.cc',
        'encoded_program.h',
        'ensemble.cc',
        'ensemble.h',
        'ensemble_apply.cc',
        'ensemble_create.cc',
        'image_info.cc',
        'image_info.h',
        'region.h',
        'simple_delta.cc',
        'simple_delta.h',
        'streams.cc',
        'streams.h',
        'win32_x86_generator.h',
        'win32_x86_patcher.h',
      ],
    },
   {
      'target_name': 'courgette',
      'type': 'executable',
      'msvs_guid': '4EA8CE12-9C6F-45E5-9D08-720383FE3685',
      'sources': [
        'courgette_tool.cc',
       ],
      'dependencies': [
        'courgette_lib',
        '../base/base.gyp:base',
      ],
    },
   {
      'target_name': 'courgette_minimal_tool',
      'type': 'executable',
      'msvs_guid': 'EB79415F-2F17-4BDC-AADD-4CA4C2D21B73',
      'sources': [
        'courgette_minimal_tool.cc',
       ],
      'dependencies': [
        'courgette_lib',
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'courgette_unittests',
      'type': 'executable',
      'msvs_guid': '24309F1A-4035-46F9-A3D8-F47DC4BCC2B8',
      'sources': [
        'adjustment_method_unittest.cc',
        'bsdiff_memory_unittest.cc',
        'difference_estimator_unittest.cc',
        'encoded_program_unittest.cc',
        'encode_decode_unittest.cc',
        'image_info_unittest.cc',
        'run_all_unittests.cc',
        'streams_unittest.cc',
       ],
      'dependencies': [
        'courgette_lib',
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
      ],
    },
   {
      'target_name': 'courgette_fuzz',
      'type': 'executable',
      'msvs_guid': '57C27529-8CA9-4FC3-9C02-DA05B172F785',
      'sources': [
        'encoded_program_fuzz_unittest.cc',
       ],
      'dependencies': [
        'courgette_lib',
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
      ],
    },
  ],
}
