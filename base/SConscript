# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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

# These base files work on *all* platforms; files that don't work
# cross-platform live below.
input_files = [
    'at_exit.cc',
    'base_paths.cc',
    'base_switches.cc',
    'bzip2_error_handler.cc',
    'command_line.cc',
    'debug_util.cc',
    'file_util.cc',
    'histogram.cc',
    'icu_util.cc',
    'json_reader.cc',
    'json_writer.cc',
    'lazy_instance.cc',
    'lock.cc',
    'logging.cc',
    'md5.cc',
    'memory_debug.cc',
    'message_loop.cc',
    'message_pump_default.cc',
    'non_thread_safe.cc',
    'path_service.cc',
    'pickle.cc',
    'ref_counted.cc',
    'revocable_store.cc',
    'sha2.cc',
    'simple_thread.cc',
    'stats_table.cc',
    'string_escape.cc',
    'string_piece.cc',
    'string_util.cc',
    'string_util_icu.cc',
    'thread.cc',
    'time.cc',
    'time_format.cc',
    'timer.cc',
    'trace_event.cc',
    'tracked.cc',
    'tracked_objects.cc',
    'values.cc',
    'word_iterator.cc',
    'third_party/nspr/prtime.cc',
    'third_party/nss/sha512.cc',
]

if env['PLATFORM'] == 'win32':
  # Some of these aren't really Windows-specific, they're just here until
  # we have the port versions working.
  # TODO: move all these files to either the cross-platform block above or
  # a platform-specific block below.
  input_files.extend([
    'clipboard_util.cc',
    'event_recorder.cc',
    'file_version_info.cc',

    # This group all depends on MessageLoop.
    'idle_timer.cc',
    'object_watcher.cc',
    'shared_event.cc',   # Is this used?
    'watchdog.cc',

    'process.cc',

    'resource_util.cc',  # Uses HMODULE, but may be abstractable.
  ])

if env['PLATFORM'] == 'win32':
  input_files.extend([
      'base_drag_source.cc',
      'base_drop_target.cc',
      'base_paths_win.cc',
      'clipboard_win.cc',
      'condition_variable_win.cc',
      'debug_on_start.cc',
      'debug_util_win.cc',
      'file_util_win.cc',
      'hmac_win.cc',
      'iat_patch.cc',
      'image_util.cc',
      'lock_impl_win.cc',
      'message_pump_win.cc',
      'pe_image.cc',
      'platform_thread_win.cc',
      'process_util_win.cc',
      'registry.cc',
      'shared_memory_win.cc',
      'sys_info_win.cc',
      'sys_string_conversions_win.cc',
      'thread_local_storage_win.cc',
      'thread_local_win.cc',
      'time_win.cc',
      'waitable_event_win.cc',
      'win_util.cc',
      'wmi_util.cc',
      'worker_pool.cc',
  ])

if env['PLATFORM'] in ('darwin', 'posix'):
  input_files.extend([
      'condition_variable_posix.cc',
      'debug_util_posix.cc',
      'file_util_posix.cc',
      'lock_impl_posix.cc',
      'platform_thread_posix.cc',
      'process_util_posix.cc',
      'shared_memory_posix.cc',
      'string16.cc',
      'sys_info_posix.cc',
      'thread_local_storage_posix.cc',
      'thread_local_posix.cc',
      'time_posix.cc',
      'waitable_event_generic.cc',
  ])

if env['PLATFORM'] == 'darwin':
  input_files.extend([
      'base_paths_mac.mm',
      'clipboard_mac.mm',
      'file_util_mac.mm',
      'file_version_info_mac.mm',
      'hmac_mac.cc',
      'platform_thread_mac.mm',
      'sys_string_conversions_mac.cc',
      'worker_pool_mac.mm',
  ])

if env['PLATFORM'] == 'posix':
  input_files.extend([
      'atomicops_internals_x86_gcc.cc',
      'base_paths_linux.cc',
      'file_util_linux.cc',
      'hmac_nss.cc',
      'nss_init.cc',
      'sys_string_conversions_linux.cc',
      'worker_pool.cc',
  ])

env.ChromeStaticLibrary('base', input_files)


env_tests.Prepend(
    CPPPATH = [
        '$GTEST_DIR/include',
        '$GTEST_DIR',
        '$SKIA_DIR/include',
        '$SKIA_DIR/include/corecg',
        '$SKIA_DIR/platform',
        '$ZLIB_DIR',
        '$LIBPNG_DIR',
        '$ICU38_DIR/public/common',
        '$ICU38_DIR/public/i18n',
        '..',
    ],
    CPPDEFINES = [
        'UNIT_TEST',
        'PNG_USER_CONFIG',
        'CHROME_PNG_WRITE_SUPPORT',
        'U_STATIC_IMPLEMENTATION',
        'GOOGLE_CHROME_BUILD',
    ],
    LIBS = [
        'base',
        'base_gfx',
        'gtest',
        'icuuc',
        'libpng',
        'skia',
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
      ],
      CPPDEFINES = [
        '_WIN32_WINNT=0x0600',
        'WINVER=0x0600',
        '_HAS_EXCEPTIONS=0',
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

# These test files work on *all* platforms; tests that don't work
# cross-platform live below.
test_files = [
    'at_exit_unittest.cc',
    'atomicops_unittest.cc',
    'command_line_unittest.cc',
    'condition_variable_unittest.cc',
    'file_util_unittest.cc',
    'hmac_unittest.cc',
    'histogram_unittest.cc',
    'json_reader_unittest.cc',
    'json_writer_unittest.cc',
    'lazy_instance_unittest.cc',
    'linked_ptr_unittest.cc',
    'message_loop_unittest.cc',
    'observer_list_unittest.cc',
    'path_service_unittest.cc',
    'pickle_unittest.cc',
    'pr_time_unittest.cc',
    'ref_counted_unittest.cc',
    'run_all_unittests.cc',
    'scoped_ptr_unittest.cc',
    'sha2_unittest.cc',
    'shared_memory_unittest.cc',
    'simple_thread_unittest.cc',
    'singleton_unittest.cc',
    'stack_container_unittest.cc',
    'string_escape_unittest.cc',
    'string_piece_unittest.cc',
    'string_tokenizer_unittest.cc',
    'string_util_unittest.cc',
    'sys_info_unittest.cc',
    'thread_local_unittest.cc',
    'thread_local_storage_unittest.cc',
    'thread_unittest.cc',
    'time_unittest.cc',
    'timer_unittest.cc',
    'tracked_objects_unittest.cc',
    'tuple_unittest.cc',
    'values_unittest.cc',
    'waitable_event_unittest.cc',
    'word_iterator_unittest.cc',
    'worker_pool_unittest.cc',
    'gfx/convolver_unittest.cc',
    'gfx/image_operations_unittest.cc',
    'gfx/png_codec_unittest.cc',
    'gfx/rect_unittest.cc',
]

if env['PLATFORM'] == 'win32':
  # These tests aren't really Windows-specific, they're just here until
  # we have the port versions working.
  env_tests.ChromeTestProgram('debug_message', ['debug_message.cc'])

  test_files.extend([
    'clipboard_unittest.cc',
    'idletimer_unittest.cc',
    'process_util_unittest.cc',
    'shared_event_unittest.cc',
    'stats_table_unittest.cc',
    'watchdog_unittest.cc',
    'gfx/native_theme_unittest.cc',
    'gfx/uniscribe_unittest.cc',
    'gfx/vector_canvas_unittest.cc',
  ])

if env['PLATFORM'] == 'win32':
  # Windows-specific tests.
  test_files.extend([
    'file_version_info_unittest.cc',
    'object_watcher_unittest.cc',
    'pe_image_unittest.cc',
    'sys_string_conversions_win_unittest.cc',
    'time_unittest_win.cc',
    'win_util_unittest.cc',
    'wmi_util_unittest.cc',
    ])

if env['PLATFORM'] == 'darwin':
  test_files.extend([
      'platform_test_mac.mm',
  ])

base_unittests = env_tests.ChromeTestProgram('base_unittests', test_files)

# Install up a level to allow unit test path assumptions to be valid.
installed_base_unittests = env.Install('$TARGET_ROOT', base_unittests)


sconscript_dirs = [
    'gfx/SConscript',
]

SConscript(sconscript_dirs, exports=['env'])


env.Alias('base', ['.', installed_base_unittests])

# TODO(sgk) should this be moved into base.lib like everything else?  This will
# require updating a bunch of other SConscripts which link directly against
# this generated object file.
env_tests.StaticObject('perftimer.cc')

# Since run_all_perftests supplies a main, we cannot have it in base.lib
env_tests.StaticObject('run_all_perftests.cc')

