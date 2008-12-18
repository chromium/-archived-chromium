# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

Import('env')

env = env.Clone()

env.Prepend(
    CPPPATH = [
        'src',
        '$CHROME_SRC_DIR',
    ],
)

if env.Bit('windows'):
  env.Append(
      CCFLAGS = [
          '/TP',
          '/wd4800',
      ],
  )

  sender_input_files = [
      'src/client/windows/sender/crash_report_sender.cc',
      'src/common/windows/http_upload.cc',
  ]

  env.ChromeStaticLibrary('breakpad_sender', sender_input_files)


if env.Bit('windows'):
  handler_input_files = [
      'src/client/windows/crash_generation/client_info.cc',
      'src/client/windows/crash_generation/minidump_generator.cc',
      'src/common/windows/guid_string.cc',
      'src/client/windows/handler/exception_handler.cc',
      'src/client/windows/crash_generation/crash_generation_server.cc',
      'src/client/windows/crash_generation/crash_generation_client.cc',
  ]
elif env.Bit('linux'):
  handler_input_files = [
      'src/common/linux/guid_creator.cc',
      'src/client/linux/handler/exception_handler.cc',
      'src/client/linux/handler/minidump_generator.cc',
      'src/client/linux/handler/linux_thread.cc',
  ]

env.ChromeStaticLibrary('breakpad_handler', handler_input_files)

