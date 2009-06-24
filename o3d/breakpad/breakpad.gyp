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
      '../../breakpad/src',
    ],
  },
  'conditions': [
    ['OS=="win"',
      {
        'targets': [
          {
            'target_name': 'o3dBreakpad',
            'type': 'static_library',
            'dependencies': [
              '../../breakpad/breakpad.gyp:breakpad_sender',
              '../../breakpad/breakpad.gyp:breakpad_handler',
            ],
            'sources': [
              'win/exception_handler_win32.cc',
              'win/breakpad_config.cc',
              'win/bluescreen_detector.cc',
            ],
            'direct_dependent_settings': {
              'include_dirs': [
                '../../breakpad/src',
              ],
            },
          },
          {
            'target_name': 'reporter',
            'type': 'executable',
            'dependencies': [
              'o3dBreakpad',
            ],
            'sources': [
              'win/crash_sender_win32.cc',
              '../../<(breakpaddir)/client/windows/sender/crash_report_sender.cc',
            ],
          },
        ],
      },
    ],
  ],
}
