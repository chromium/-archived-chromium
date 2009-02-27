# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(mark): Upstream this file to v8.
{
  'variables': {
    'chromium_code': 1,
    'base_source_files': [
      '../../v8/src/third_party/dtoa/dtoa.c',
      '../../v8/src/accessors.cc',
      '../../v8/src/accessors.h',
      '../../v8/src/allocation.cc',
      '../../v8/src/allocation.h',
      '../../v8/src/api.cc',
      '../../v8/src/api.h',
      '../../v8/src/apiutils.h',
      '../../v8/src/arguments.h',
      '../../v8/src/assembler-arm-inl.h',
      '../../v8/src/assembler-arm.cc',
      '../../v8/src/assembler-arm.h',
      '../../v8/src/assembler-ia32-inl.h',
      '../../v8/src/assembler-ia32.cc',
      '../../v8/src/assembler-ia32.h',
      '../../v8/src/assembler.cc',
      '../../v8/src/assembler.h',
      '../../v8/src/ast.cc',
      '../../v8/src/ast.h',
      '../../v8/src/bootstrapper.cc',
      '../../v8/src/bootstrapper.h',
      '../../v8/src/builtins-arm.cc',
      '../../v8/src/builtins-ia32.cc',
      '../../v8/src/builtins.cc',
      '../../v8/src/builtins.h',
      '../../v8/src/bytecodes-irregexp.h',
      '../../v8/src/char-predicates-inl.h',
      '../../v8/src/char-predicates.h',
      '../../v8/src/checks.cc',
      '../../v8/src/checks.h',
      '../../v8/src/code-stubs.cc',
      '../../v8/src/code-stubs.h',
      '../../v8/src/code.h',
      '../../v8/src/codegen-arm.cc',
      '../../v8/src/codegen-arm.h',
      '../../v8/src/codegen-ia32.cc',
      '../../v8/src/codegen-ia32.h',
      '../../v8/src/codegen-inl.h',
      '../../v8/src/codegen.cc',
      '../../v8/src/codegen.h',
      '../../v8/src/compilation-cache.cc',
      '../../v8/src/compilation-cache.h',
      '../../v8/src/compiler.cc',
      '../../v8/src/compiler.h',
      '../../v8/src/constants-arm.h',
      '../../v8/src/contexts.cc',
      '../../v8/src/contexts.h',
      '../../v8/src/conversions-inl.h',
      '../../v8/src/conversions.cc',
      '../../v8/src/conversions.h',
      '../../v8/src/counters.cc',
      '../../v8/src/counters.h',
      '../../v8/src/cpu-arm.cc',
      '../../v8/src/cpu-ia32.cc',
      '../../v8/src/cpu.h',
      '../../v8/src/dateparser.cc',
      '../../v8/src/dateparser.h',
      '../../v8/src/debug-arm.cc',
      '../../v8/src/debug-ia32.cc',
      '../../v8/src/debug.cc',
      '../../v8/src/debug.h',
      '../../v8/src/disasm-arm.cc',
      '../../v8/src/disasm-ia32.cc',
      '../../v8/src/disasm.h',
      '../../v8/src/disassembler.cc',
      '../../v8/src/disassembler.h',
      '../../v8/src/dtoa-config.c',
      '../../v8/src/execution.cc',
      '../../v8/src/execution.h',
      '../../v8/src/factory.cc',
      '../../v8/src/factory.h',
      '../../v8/src/flag-definitions.h',
      '../../v8/src/flags.cc',
      '../../v8/src/flags.h',
      '../../v8/src/frames-arm.cc',
      '../../v8/src/frames-arm.h',
      '../../v8/src/frames-ia32.cc',
      '../../v8/src/frames-ia32.h',
      '../../v8/src/frames-inl.h',
      '../../v8/src/frames.cc',
      '../../v8/src/frames.h',
      '../../v8/src/global-handles.cc',
      '../../v8/src/global-handles.h',
      '../../v8/src/globals.h',
      '../../v8/src/handles-inl.h',
      '../../v8/src/handles.cc',
      '../../v8/src/handles.h',
      '../../v8/src/hashmap.cc',
      '../../v8/src/hashmap.h',
      '../../v8/src/heap-inl.h',
      '../../v8/src/heap.cc',
      '../../v8/src/heap.h',
      '../../v8/src/ic-arm.cc',
      '../../v8/src/ic-ia32.cc',
      '../../v8/src/ic-inl.h',
      '../../v8/src/ic.cc',
      '../../v8/src/ic.h',
      '../../v8/src/interpreter-irregexp.cc',
      '../../v8/src/interpreter-irregexp.h',
      '../../v8/src/jsregexp-inl.h',
      '../../v8/src/jsregexp.cc',
      '../../v8/src/jsregexp.h',
      '../../v8/src/list-inl.h',
      '../../v8/src/list.h',
      '../../v8/src/log.cc',
      '../../v8/src/log.h',
      '../../v8/src/macro-assembler-arm.cc',
      '../../v8/src/macro-assembler-arm.h',
      '../../v8/src/macro-assembler-ia32.cc',
      '../../v8/src/macro-assembler-ia32.h',
      '../../v8/src/macro-assembler.h',
      '../../v8/src/mark-compact.cc',
      '../../v8/src/mark-compact.h',
      '../../v8/src/memory.h',
      '../../v8/src/messages.cc',
      '../../v8/src/messages.h',
      '../../v8/src/natives.h',
      '../../v8/src/objects-debug.cc',
      '../../v8/src/objects-inl.h',
      '../../v8/src/objects.cc',
      '../../v8/src/objects.h',
      '../../v8/src/parser.cc',
      '../../v8/src/parser.h',
      '../../v8/src/platform-freebsd.cc',
      '../../v8/src/platform-linux.cc',
      '../../v8/src/platform-macos.cc',
      '../../v8/src/platform-nullos.cc',
      '../../v8/src/platform-win32.cc',
      '../../v8/src/platform.h',
      '../../v8/src/prettyprinter.cc',
      '../../v8/src/prettyprinter.h',
      '../../v8/src/property.cc',
      '../../v8/src/property.h',
      '../../v8/src/regexp-macro-assembler-arm.cc',
      '../../v8/src/regexp-macro-assembler-arm.h',
      '../../v8/src/regexp-macro-assembler-ia32.cc',
      '../../v8/src/regexp-macro-assembler-ia32.h',
      '../../v8/src/regexp-macro-assembler-irregexp-inl.h',
      '../../v8/src/regexp-macro-assembler-irregexp.cc',
      '../../v8/src/regexp-macro-assembler-irregexp.h',
      '../../v8/src/regexp-macro-assembler-tracer.cc',
      '../../v8/src/regexp-macro-assembler-tracer.h',
      '../../v8/src/regexp-macro-assembler.cc',
      '../../v8/src/regexp-macro-assembler.h',
      '../../v8/src/regexp-stack.cc',
      '../../v8/src/regexp-stack.h',
      '../../v8/src/rewriter.cc',
      '../../v8/src/rewriter.h',
      '../../v8/src/runtime.cc',
      '../../v8/src/runtime.h',
      '../../v8/src/scanner.cc',
      '../../v8/src/scanner.h',
      '../../v8/src/scopeinfo.cc',
      '../../v8/src/scopeinfo.h',
      '../../v8/src/scopes.cc',
      '../../v8/src/scopes.h',
      '../../v8/src/serialize.cc',
      '../../v8/src/serialize.h',
      '../../v8/src/shell.h',
      '../../v8/src/simulator-arm.cc',
      '../../v8/src/smart-pointer.h',
      '../../v8/src/snapshot-common.cc',
      '../../v8/src/snapshot.h',
      '../../v8/src/spaces-inl.h',
      '../../v8/src/spaces.cc',
      '../../v8/src/spaces.h',
      '../../v8/src/string-stream.cc',
      '../../v8/src/string-stream.h',
      '../../v8/src/stub-cache-arm.cc',
      '../../v8/src/stub-cache-ia32.cc',
      '../../v8/src/stub-cache.cc',
      '../../v8/src/stub-cache.h',
      '../../v8/src/token.cc',
      '../../v8/src/token.h',
      '../../v8/src/top.cc',
      '../../v8/src/top.h',
      '../../v8/src/unicode-inl.h',
      '../../v8/src/unicode.cc',
      '../../v8/src/unicode.h',
      '../../v8/src/usage-analyzer.cc',
      '../../v8/src/usage-analyzer.h',
      '../../v8/src/utils.cc',
      '../../v8/src/utils.h',
      '../../v8/src/v8-counters.cc',
      '../../v8/src/v8-counters.h',
      '../../v8/src/v8.cc',
      '../../v8/src/v8.h',
      '../../v8/src/v8threads.cc',
      '../../v8/src/v8threads.h',
      '../../v8/src/variables.cc',
      '../../v8/src/variables.h',
      '../../v8/src/zone-inl.h',
      '../../v8/src/zone.cc',
      '../../v8/src/zone.h',
    ],
    'not_base_source_files': [
      # These files are #included by others and are not meant to be compiled
      # directly.
      '../../v8/src/third_party/dtoa/dtoa.c',
    ],
    'd8_source_files': [
      '../../v8/src/d8-debug.cc',
      '../../v8/src/d8-readline.cc',
      '../../v8/src/d8.cc',
    ],
  },
  'includes': [
    '../common.gypi',
  ],
  'target_defaults': {
    'configurations': {
      'Debug': {
        'defines': [
          'DEBUG',
          'ENABLE_DISASSEMBLER',
          'ENABLE_LOGGING_AND_PROFILING',
        ],
      },
    },
    'xcode_settings': {
      'GCC_ENABLE_CPP_EXCEPTIONS': 'NO',
      'GCC_ENABLE_CPP_RTTI': 'NO',
    },
  },
  'targets': [
    # Targets that apply to any architecture.
    {
      'target_name': 'js2c',
      'type': 'none',
      'variables': {
        'library_files': [
          '../../v8/src/runtime.js',
          '../../v8/src/v8natives.js',
          '../../v8/src/array.js',
          '../../v8/src/string.js',
          '../../v8/src/uri.js',
          '../../v8/src/math.js',
          '../../v8/src/messages.js',
          '../../v8/src/apinatives.js',
          '../../v8/src/debug-delay.js',
          '../../v8/src/mirror-delay.js',
          '../../v8/src/date-delay.js',
          '../../v8/src/regexp-delay.js',
          '../../v8/src/macros.py',
        ],
      },
      'actions': [
        {
          'action_name': 'js2c',
          'inputs': [
            '../../v8/tools/js2c.py',
            '<@(library_files)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/v8/libraries.cc',
            '<(SHARED_INTERMEDIATE_DIR)/v8/libraries-empty.cc',
          ],
          'action': ['python', '../../v8/tools/js2c.py', '<@(_outputs)', 'CORE', '<@(library_files)'],
          # TODO(sgk):  figure out how to get gyp and SCons to play nice here.
          'conditions': [
            ['OS=="linux"', {'action=': ['python', '${SOURCES[0]}', '${TARGETS}', 'CORE', '${SOURCES[1:]}'],}],
          ],
        },
      ],
    },
    {
      'target_name': 'd8_js2c',
      'type': 'none',
      'variables': {
        'library_files': [
          '../../v8/src/d8.js',
          '../../v8/src/macros.py',
        ],
      },
      'actions': [
        {
          'action_name': 'js2c',
          'inputs': [
            '../../v8/tools/js2c.py',
            '<@(library_files)',
          ],
          'extra_inputs': [
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/v8/d8-js.cc',
            '<(SHARED_INTERMEDIATE_DIR)/v8/d8-js-empty.cc',
          ],
          'action': ['python', '../../v8/tools/js2c.py', '<@(_outputs)', 'D8', '<@(library_files)'],
          # TODO(sgk):  figure out how to get gyp and SCons to play nice here.
          'conditions': [
            ['OS=="linux"', {'action=': ['python', '${SOURCES[0]}', '${TARGETS}', 'D8', '${SOURCES[1:]}'],}],
          ],
        },
      ],
    },

    # Targets to build v8 for the native architecture (ia32).
    {
      'target_name': 'v8_base',
      'type': 'static_library',
      'include_dirs': [
        '../../v8/src',
      ],
      'sources': [
        '<@(base_source_files)',
      ],
      'sources!': [
        '<@(not_base_source_files)',
      ],
      'sources/': [
        ['exclude', '-arm\\.cc$'],
        ['exclude', 'src/platform-.*\\.cc$' ],
      ],
      'conditions': [
        ['OS=="linux"', {'sources/': [['include', 'src/platform-linux\\.cc$']]}],
        ['OS=="mac"', {'sources/': [['include', 'src/platform-macos\\.cc$']]}],
        ['OS=="win"', {
          'sources/': [['include', 'src/platform-win32\\.cc$']],
          # 4355, 4800 came from common.vsprops
          # 4018, 4244 were a per file config on dtoa-config.c
          # TODO: It's probably possible and desirable to stop disabling the
          # dtoa-specific warnings by modifying dtoa as was done in Chromium
          # r9255.  Refer to that revision for details.
          'msvs_disabled_warnings': [4355, 4800, 4018, 4244],
          'all_dependent_settings':  {
            'msvs_system_libraries': [ 'winmm.lib' ],
          },
        }],
      ],
    },
    {
      'target_name': 'v8_nosnapshot',
      'type': 'static_library',
      'dependencies': [
        'js2c',
        'v8_base',
      ],
      'include_dirs': [
        '../../v8/src',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/v8/libraries.cc',
        '../../v8/src/snapshot-empty.cc',
      ],
    },
    {
      'target_name': 'mksnapshot',
      'type': 'executable',
      'dependencies': [
        'v8_nosnapshot',
      ],
      'sources': [
        '../../v8/src/mksnapshot.cc',
      ],
    },
    {
      'target_name': 'v8',
      'type': 'static_library',
      'dependencies': [
        'js2c',
        'mksnapshot',
        'v8_base',
      ],
      'actions': [
        {
          'action_name': 'mksnapshot',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)mksnapshot<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/snapshot.cc',
          ],
          'action': ['<@(_inputs)', '<@(_outputs)'],
        },
      ],
      'include_dirs': [
        '../../v8/src',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/v8/libraries-empty.cc',
        '<(INTERMEDIATE_DIR)/snapshot.cc',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../../v8/include',
        ],
      },
    },
    {
      'target_name': 'v8_shell',
      'type': 'executable',
      'dependencies': [
        'v8',
      ],
      'sources': [
        '../../v8/samples/shell.cc',
      ],
      'conditions': [
        [ 'OS=="win"', {
          # This could be gotten by not setting chromium_code, if that's OK.
          'defines': ['_CRT_SECURE_NO_WARNINGS'],
        }],
      ],
    },
    {
      'target_name': 'd8',
      'type': 'executable',
      'dependencies': [
        'd8_js2c',
        'v8',
      ],
      'include_dirs': [
        '../../v8/src',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/v8/d8-js.cc',
        '<@(d8_source_files)',
      ],
      'conditions': [
        [ 'OS=="linux"', {
          'link_settings': { 'libraries': [ '-lreadline' ] },
        }],
        [ 'OS=="mac"', {
          'link_settings': { 'libraries': [
            '$(SDKROOT)/usr/lib/libreadline.dylib'
          ]},
        }],
        [ 'OS=="win"', {
          'sources!': [ '../../v8/src/d8-readline.cc' ],
        }],
      ],
    },

    # ARM targets, to test ARM code generation.  These use an ARM simulator
    # (src/simulator-arm.cc).  The ARM targets are not snapshot-enabled.
    {
      'target_name': 'v8_arm',
      'type': 'static_library',
      'dependencies': [
        'js2c',
      ],
      'defines': [
        'ARM',
      ],
      'include_dirs': [
        '../../v8/src',
      ],
      'sources': [
        '<@(base_source_files)',
        '<(SHARED_INTERMEDIATE_DIR)/v8/libraries.cc',
        '../../v8/src/snapshot-empty.cc',
      ],
      'sources!': [
        '<@(not_base_source_files)',
      ],
      'sources/': [
        ['exclude', '-ia32\\.cc$'],
        ['exclude', 'src/platform-.*\\.cc$' ],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../../v8/include',
        ],
      },
      'conditions': [
        ['OS=="linux"', {'sources/': [['include', 'src/platform-linux\\.cc$']]}],
        ['OS=="mac"', {'sources/': [['include', 'src/platform-macos\\.cc$']]}],
        ['OS=="win"', {
          'sources/': [['include', 'src/platform-win32\\.cc$']],
          # 4355, 4800 came from common.vsprops
          # 4018, 4244 were a per file config on dtoa-config.c
          # TODO: It's probably possible and desirable to stop disabling the
          # dtoa-specific warnings by modifying dtoa as was done in Chromium
          # r9255.  Refer to that revision for details.
          'msvs_disabled_warnings': [4355, 4800, 4018, 4244],
        }],
      ],
    },
    {
      'target_name': 'v8_shell_arm',
      'type': 'executable',
      'dependencies': [
        'v8_arm',
      ],
      'defines': [
        'ARM',
      ],
      'sources': [
        '../../v8/samples/shell.cc',
      ],
      'conditions': [
        [ 'OS=="win"', {
          # This could be gotten by not setting chromium_code, if that's OK.
          'defines': ['_CRT_SECURE_NO_WARNINGS'],
        }],
      ],
    },
    {
      'target_name': 'd8_arm',
      'type': 'executable',
      'dependencies': [
        'd8_js2c',
        'v8_arm',
      ],
      'defines': [
        'ARM',
      ],
      'include_dirs': [
        '../../v8/src',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/v8/d8-js.cc',
        '<@(d8_source_files)',
      ],
      'conditions': [
        [ 'OS=="linux"', {
          'link_settings': { 'libraries': [ '-lreadline' ] },
        }],
        [ 'OS=="mac"', {
          'link_settings': { 'libraries': [
            '$(SDKROOT)/usr/lib/libreadline.dylib'
          ]},
        }],
        [ 'OS=="win"', {
          'sources!': [ '../../v8/src/d8-readline.cc' ],
        }],
      ],
    },
  ],
}
