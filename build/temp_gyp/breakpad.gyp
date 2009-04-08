# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../common.gypi',
  ],
  'conditions': [
    [ 'OS=="win"', {
      'targets': [
        {
          'target_name': 'breakpad_handler',
          'type': '<(library)',
          'msvs_guid': 'B55CA863-B374-4BAF-95AC-539E4FA4C90C',
          'sources': [
            '../../breakpad/src/client/windows/crash_generation/client_info.cc',
            '../../breakpad/src/client/windows/crash_generation/client_info.h',
            '../../breakpad/src/client/windows/crash_generation/crash_generation_client.cc',
            '../../breakpad/src/client/windows/crash_generation/crash_generation_client.h',
            '../../breakpad/src/client/windows/crash_generation/crash_generation_server.cc',
            '../../breakpad/src/client/windows/crash_generation/crash_generation_server.h',
            '../../breakpad/src/client/windows/handler/exception_handler.cc',
            '../../breakpad/src/client/windows/handler/exception_handler.h',
            '../../breakpad/src/common/windows/guid_string.cc',
            '../../breakpad/src/common/windows/guid_string.h',
            '../../breakpad/src/google_breakpad/common/minidump_format.h',
            '../../breakpad/src/client/windows/crash_generation/minidump_generator.cc',
            '../../breakpad/src/client/windows/crash_generation/minidump_generator.h',
            '../../breakpad/src/common/windows/string_utils-inl.h',
          ],
          'include_dirs': [
            '../../breakpad/src',
          ],
          'link_settings': {
            'libraries': [
              '-lurlmon.lib',
            ],
          },
          'direct_dependent_settings': {
            'include_dirs': [
              '../../breakpad/src',
            ],
          },
        },
        {
          'target_name': 'breakpad_sender',
          'type': '<(library)',
          'msvs_guid': '9946A048-043B-4F8F-9E07-9297B204714C',
          'sources': [
            '../../breakpad/src/client/windows/sender/crash_report_sender.cc',
            '../../breakpad/src/common/windows/http_upload.cc',
            '../../breakpad/src/client/windows/sender/crash_report_sender.h',
            '../../breakpad/src/common/windows/http_upload.h',
          ],
          'include_dirs': [
            '../../breakpad/src',
          ],
          'link_settings': {
            'libraries': [
              '-lurlmon.lib',
            ],
          },
          'direct_dependent_settings': {
            'include_dirs': [
              '../../breakpad/src',
            ],
          },
        },
      ],
    }],
  ],
}
