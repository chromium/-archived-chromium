# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Import('env')

env = env.Clone()

env.Prepend(
    CPPPATH = [
        'src',
        '#/..',
    ],
)

if env['PLATFORM'] == 'win32':
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


if env['PLATFORM'] == 'win32':
  handler_input_files = [
      'src/client/windows/crash_generation/client_info.cc',
      'src/client/windows/crash_generation/minidump_generator.cc',
      'src/common/windows/guid_string.cc',
      'src/client/windows/handler/exception_handler.cc',
      'src/client/windows/crash_generation/crash_generation_server.cc',
      'src/client/windows/crash_generation/crash_generation_client.cc',
  ]
elif env['PLATFORM'] == 'posix':
  handler_input_files = [
      'src/common/linux/guid_creator.cc',
      'src/client/linux/handler/exception_handler.cc',
      'src/client/linux/handler/minidump_generator.cc',
      'src/client/linux/handler/linux_thread.cc',
  ]

env.ChromeStaticLibrary('breakpad_handler', handler_input_files)
