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
      'target_name': 'ipc',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_gfx',
      ],
      'sources': [
        'ipc_channel.h',
        'ipc_channel_proxy.cc',
        'ipc_channel_proxy.h',
        'ipc_counters.cc',
        'ipc_logging.cc',
        'ipc_logging.h',
        'ipc_message.cc',
        'ipc_message.h',
        'ipc_message_macros.h',
        'ipc_message_utils.cc',
        'ipc_message_utils.h',
        'ipc_sync_channel.cc',
        'ipc_sync_channel.h',
        'ipc_sync_message.cc',
        'ipc_sync_message.h',
        'ipc_switches.cc',
      ],

      'conditions': [
        ['OS=="linux"', {'sources': [
          'ipc_channel_posix.cc',
          'ipc_channel_posix.h',
          'file_descriptor_set_posix.cc',
        ]}],
        ['OS=="mac"', {'sources': [
          'ipc_channel_posix.cc',
          'ipc_channel_posix.h',
        ]}],
        ['OS=="win"', {'sources': [
          'ipc_channel_win.cc',
          'ipc_channel_win.h',
        ]}],
      ],
    },
    {
      'target_name': 'ipc_tests',
      'type': 'executable',
      'dependencies': [
        'ipc',
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'ipc_fuzzing_tests.cc',
        'ipc_sync_channel_unittest.cc',
        'ipc_sync_message_unittest.cc',
        'ipc_sync_message_unittest.h',
        'ipc_tests.cc',
        'ipc_tests.h',
      ],
      'conditions': [
        ['OS=="linux"', {'sources': [
          'ipc_send_fds_test.cc',
        ]}],
        ['OS=="mac"', {'sources': [
          'ipc_send_fds_test.cc',
        ]}],
      ],
    },
  ],
}
