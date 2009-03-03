# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'breakpad_handler',
      'type': 'static_library',
      'sources': [
        'src/client/windows/crash_generation/client_info.cc',
        'src/client/windows/crash_generation/client_info.h',
        'src/client/windows/crash_generation/crash_generation_client.cc',
        'src/client/windows/crash_generation/crash_generation_client.h',
        'src/client/windows/crash_generation/crash_generation_server.cc',
        'src/client/windows/crash_generation/crash_generation_server.h',
        'src/client/windows/handler/exception_handler.cc',
        'src/client/windows/handler/exception_handler.h',
        'src/common/windows/guid_string.cc',
        'src/common/windows/guid_string.h',
        'src/google_breakpad/common/minidump_format.h',
        'src/client/windows/crash_generation/minidump_generator.cc',
        'src/client/windows/crash_generation/minidump_generator.h',
        'src/common/windows/string_utils-inl.h',
      ],
      'include_dirs': [
        'src',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src',
        ],
      },
    },
  ],
}
