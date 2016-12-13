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
      'target_name': 'o3dUtils',
      'type': 'static_library',
      'sources': [
        "cross/file_path_utils.cc",
        "cross/file_path_utils.h",
        "cross/file_text_reader.cc",
        "cross/file_text_reader.h",
        "cross/file_text_writer.cc",
        "cross/file_text_writer.h",
        "cross/json_writer.cc",
        "cross/json_writer.h",
        "cross/string_reader.cc",
        "cross/string_reader.h",
        "cross/string_writer.cc",
        "cross/string_writer.h",
        "cross/structured_writer.h",
        "cross/temporary_file.cc",
        "cross/temporary_file.h",
        "cross/text_reader.cc",
        "cross/text_reader.h",
        "cross/text_writer.cc",
        "cross/text_writer.h",
      ],
    },
    {
      'target_name': 'o3dUtilsTest',
      'type': 'none',
      'dependencies': [
        'o3dUtils',
      ],
      'direct_dependent_settings': {
        'sources': [
          "cross/file_path_utils_test.cc",
          "cross/file_text_reader_test.cc",
          "cross/json_writer_test.cc",
          "cross/string_reader_test.cc",
          "cross/string_writer_test.cc",
          "cross/temporary_file_test.cc",
        ],
      },
    },
  ],
}
