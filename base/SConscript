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
env_tests = env.Clone()

env.Prepend(
    CPPPATH = [
        '$ICU38_DIR/public/common',
        '$ICU38_DIR/public/i18n',
        '..',
    ],
    CPPDEFINES = [
        'U_STATIC_IMPLEMENTATION',
    ],
)

if env['PLATFORM'] == 'win32':
  env.Prepend(
      CCFLAGS = [
          '/TP',
          '/Wp64',
      ],
  )

input_files = [
    'at_exit.cc',
    'base_drag_source.cc',
    'base_drop_target.cc',
    'base_paths.cc',
    'base_switches.cc',
    'bzip2_error_handler.cc',
    'clipboard.cc',
    'clipboard_util.cc',
    'command_line.cc',
    'condition_variable.cc',
    'debug_on_start.cc',
    'debug_util.cc',
    'event_recorder.cc',
    'file_util.cc',
    'file_version_info.cc',
    'histogram.cc',
    'hmac.cc',
    'iat_patch.cc',
    'icu_util.cc',
    'idle_timer.cc',
    'image_util.cc',
    'json_reader.cc',
    'json_writer.cc',
    'lock.cc',
    'logging.cc',
    'md5.cc',
    'memory_debug.cc',
    'message_loop.cc',
    'non_thread_safe.cc',
    'object_watcher.cc',
    'path_service.cc',
    'pe_image.cc',
    'pickle.cc',
    'platform_thread.cc',
    'process.cc',
    'process_util.cc',
    'registry.cc',
    'resource_util.cc',
    'revocable_store.cc',
    'sha2.cc',
    'shared_event.cc',
    'shared_memory.cc',
    'stats_table.cc',
    'string_escape.cc',
    'string_piece.cc',
    'string_util.cc',
    'string_util_icu.cc',
    'third_party/nspr/prtime.cc',
    'third_party/nss/sha512.cc',
    'thread.cc',
    'time.cc',
    'timer.cc',
    'tracked.cc',
    'tracked_objects.cc',
    'values.cc',
    'watchdog.cc',
    'win_util.cc',
    'wmi_util.cc',
    'word_iterator.cc',
    'worker_pool.cc',
]

if env['PLATFORM'] == 'win32':
  input_files.extend([
      'base_paths_win.cc',
      'file_util_win.cc',
      'lock_impl_win.cc',
      'string_util_win.cc',
      'sys_string_conversions_win.cc',
      'thread_local_storage_win.cc',
      'time_win.cc',
  ])

if env['PLATFORM'] == 'darwin':
  input_files.extend([
      'sys_string_conversions_mac.cc',
  ])

env.ChromeStaticLibrary('base', input_files)


env_tests.Prepend(
    CPPPATH = [
        '$SKIA_DIR/include',
        '$SKIA_DIR/include/corecg',
        '$SKIA_DIR/platform',
        '$ZLIB_DIR',
        '$LIBPNG_DIR',
        '$ICU38_DIR/public/common',
        '$ICU38/_DIRpublic/i18n',
        '..',
    ],
    CPPDEFINES = [
        'UNIT_TEST',
        'PNG_USER_CONFIG',
        'CHROME_PNG_WRITE_SUPPORT',
        'U_STATIC_IMPLEMENTATION',
        '_WIN32_WINNT=0x0600',
        'WINVER=0x0600',
        '_HAS_EXCEPTIONS=0',
    ],
    LIBS = [
        'base',
        'base_gfx',
        'skia',
        'libpng',
        'gtest',
        'icuuc',
        'zlib',
    ]
)

env_tests.Append(
    CPPPATH = [
        '$GTEST_DIR/include',
    ],
)

if env['PLATFORM'] == 'win32':
  env_tests.Prepend(
      CCFLAGS = [
          '/TP',
          '/WX',
          '/Wp64',
      ],
      LINKFLAGS = [
          '/MANIFEST',
          '/DELAYLOAD:"dwmapi.dll"',
          '/DELAYLOAD:"uxtheme.dll"',
          '/MACHINE:X86',
          '/FIXED:No',

          '/safeseh',
          '/dynamicbase',
          '/ignore:4199',
          '/nxcompat',
      ],
  )


env_tests_dll = env_tests.Clone()
env_tests_dll.Append(
    CPPDEFINES = [
        '_WINDLL',
        'SINGLETON_UNITTEST_EXPORTS',
    ],
)
dll = env_tests_dll.ChromeSharedLibrary(['singleton_dll_unittest.dll',
                                   'singleton_dll_unittest.lib',
                                   'singleton_dll_unittest.ilk',
                                   'singleton_dll_unittest.pdb'],
                                  ['singleton_dll_unittest.cc',
                                   'build/singleton_dll_unittest.def'])
i = env.Install('$TARGET_ROOT', dll[0])
env.Alias('base', i)

env_tests.ChromeTestProgram(['debug_message.exe',
                   'debug_message.ilk',
                   'debug_message.pdb'],
                  ['debug_message.cc'])

test_files = [
    'at_exit_unittest.cc',
    'atomic_unittest.cc',
    'check_handler_unittest.cc',
    'clipboard_unittest.cc',
    'command_line_unittest.cc',
    'condition_variable_test.cc',
    'file_util_unittest.cc',
    'file_version_info_unittest.cc',
    'gfx/convolver_unittest.cc',
    'gfx/image_operations_unittest.cc',
    'gfx/native_theme_unittest.cc',
    'gfx/platform_canvas_unittest.cc',
    'gfx/png_codec_unittest.cc',
    'gfx/rect_unittest.cc',
    'gfx/uniscribe_unittest.cc',
    'gfx/vector_canvas_unittest.cc',
    'hmac_unittest.cc',
    'json_reader_unittest.cc',
    'json_writer_unittest.cc',
    'linked_ptr_unittest.cc',
    'message_loop_unittest.cc',
    'object_watcher_unittest.cc',
    'path_service_unittest.cc',
    'pe_image_unittest.cc',
    'pickle_unittest.cc',
    'pr_time_test.cc',
    'process_util_unittest.cc',
    'ref_counted_unittest.cc',
    'run_all_unittests.cc',
    'sha2_unittest.cc',
    'shared_event_unittest.cc',
    'shared_memory_unittest.cc',
    'singleton_unittest.cc',
    'stack_container_unittest.cc',
    'stats_table_unittest.cc',
    'string_tokenizer_unittest.cc',
    'string_piece_unittest.cc',
    'string_util_unittest.cc',
    'thread_local_storage_unittest.cc',
    'thread_unittest.cc',
    'tracked_objects_test.cc',
    'time_unittest.cc',
    'timer_unittest.cc',
    'values_unittest.cc',
    'win_util_unittest.cc',
    'wmi_util_unittest.cc',

    dll[1],
]

if env['PLATFORM'] == 'win32':
  test_files.extend([
      'sys_string_conversions_win_unittest.cc',
  ])

base_unittests = env_tests.ChromeTestProgram([
    'base_unittests',
    'base_unittests.exp',
    'base_unittests.ilk',
    'base_unittests.lib',
    'base_unittests.pdb'], test_files)


# Install up a level to allow unit test path assumptions to be valid.
installed_base_unittests = env.Install('$TARGET_ROOT', base_unittests)


sconscript_dirs = [
    'gfx/SConscript',
]

SConscript(sconscript_dirs, exports=['env'])


# Setup alias for all base related targets.
env.Alias('base', ['.', installed_base_unittests, '../icudt38.dll'])


env_tests.StaticObject('perftimer.cc')
env_tests.StaticObject('run_all_perftests.cc')
