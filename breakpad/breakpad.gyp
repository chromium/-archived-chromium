# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../build/common.gypi',
  ],
  'conditions': [
    [ 'OS=="mac"', {
      'target_defaults': {
        'include_dirs': [
          'src',
        ],
        'configurations': {
          'Debug': {
            'defines': [
              # This is needed for GTMLogger to work correctly.
              'DEBUG',
            ],
          },
        },
      },
      'targets': [
        {
          'target_name': 'breakpad_utilities',
          'type': '<(library)',
          'sources': [
            'src/common/convert_UTF.c',
            'src/client/mac/handler/dynamic_images.cc',
            'src/common/mac/file_id.cc',
            'src/common/mac/MachIPC.mm',
            'src/common/mac/macho_id.cc',
            'src/common/mac/macho_utilities.cc',
            'src/common/mac/macho_walker.cc',
            'src/client/minidump_file_writer.cc',
            'src/client/mac/handler/minidump_generator.cc',
            'src/common/mac/SimpleStringDictionary.mm',
            'src/common/string_conversion.cc',
            'src/common/mac/string_utilities.cc',
          ],
          'link_settings': {
            'libraries': ['$(SDKROOT)/usr/lib/libcrypto.dylib'],
          }
        },
        {
          'target_name': 'crash_inspector',
          'type': 'executable',
          'dependencies': [
            'breakpad_utilities',
          ],
          'sources': [
            'src/client/mac/crash_generation/Inspector.mm',
            'src/client/mac/crash_generation/InspectorMain.mm',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          }
        },
        {
          'target_name': 'crash_report_sender',
          'type': 'executable',
          'mac_bundle': 1,
          'sources': [
            'src/common/mac/HTTPMultipartUpload.m',
            'src/client/mac/sender/crash_report_sender.m',
            'src/common/mac/GTMLogger.m',
          ],
          'mac_bundle_resources': [
            'src/client/mac/sender/English.lproj/Localizable.strings',
            'src/client/mac/sender/crash_report_sender.icns',
            'src/client/mac/sender/Breakpad.nib',
            'src/client/mac/sender/crash_report_sender-Info.plist',
          ],
          'mac_bundle_resources!': [
             'src/client/mac/sender/crash_report_sender-Info.plist',
          ],
          'xcode_settings': {
             'INFOPLIST_FILE': 'src/client/mac/sender/crash_report_sender-Info.plist',
          },
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/SystemConfiguration.framework',
            ],
          }
        },
        {
          'target_name': 'dump_syms',
          'type': 'executable',
          'include_dirs': [
            'src/common/mac',
          ],
          'dependencies': [
            'breakpad_utilities',
          ],
          'sources': [
            'src/common/mac/dwarf/bytereader.cc',
            'src/common/mac/dwarf/dwarf2reader.cc',
            'src/common/mac/dwarf/functioninfo.cc',
            'src/common/mac/dump_syms.mm',
            'src/tools/mac/dump_syms/dump_syms_tool.mm',
          ],
          'xcode_settings': {
            # The DWARF utilities require -funsigned-char.
            'GCC_CHAR_IS_UNSIGNED_CHAR': 'YES',
          },
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          }
        },
        {
          'target_name': 'symupload',
          'type': 'executable',
          'include_dirs': [
            'src/common/mac',
          ],
          'sources': [
            'src/common/mac/HTTPMultipartUpload.m',
            'src/tools/mac/symupload/symupload.m',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          }
        },
        {
          'target_name': 'breakpad',
          'type': '<(library)',
          'dependencies': [
            'breakpad_utilities',
            'crash_inspector',
            'crash_report_sender',
          ],
          'sources': [
            'src/client/mac/handler/protected_memory_allocator.cc',
            'src/client/mac/handler/exception_handler.cc',
            'src/client/mac/Framework/Breakpad.mm',
            'src/client/mac/Framework/OnDemandServer.mm',
          ],
        },
      ],
    }],
    [ 'OS=="win"', {
      'targets': [
        {
          'target_name': 'breakpad_handler',
          'type': '<(library)',
          'msvs_guid': 'B55CA863-B374-4BAF-95AC-539E4FA4C90C',
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
          'link_settings': {
            'libraries': [
              '-lurlmon.lib',
            ],
          },
          'direct_dependent_settings': {
            'include_dirs': [
              'src',
            ],
          },
        },
        {
          'target_name': 'breakpad_sender',
          'type': '<(library)',
          'msvs_guid': '9946A048-043B-4F8F-9E07-9297B204714C',
          'sources': [
            'src/client/windows/sender/crash_report_sender.cc',
            'src/common/windows/http_upload.cc',
            'src/client/windows/sender/crash_report_sender.h',
            'src/common/windows/http_upload.h',
          ],
          'include_dirs': [
            'src',
          ],
          'link_settings': {
            'libraries': [
              '-lurlmon.lib',
            ],
          },
          'direct_dependent_settings': {
            'include_dirs': [
              'src',
            ],
          },
        },
      ],
    }],
    [ 'OS=="linux"', {
      'conditions': [
        # Tools needed for archiving official build symbols.
        ['branding=="Chrome"', {
          'targets': [
            {
              'target_name': 'symupload',
              'type': 'executable',

              # This uses the system libcurl, so don't use the default 32-bit
              # compile flags when building on a 64-bit machine.
              'variables': {
                'host_arch': '<!(uname -m)',
              },
              'conditions': [
                ['host_arch=="x86_64"', {
                  'cflags!': ['-m32', '-march=pentium4', '-msse2',
                              '-mfpmath=sse'],
                  'ldflags!': ['-m32'],
                  'cflags': ['-O2'],
                }],
              ],

              'sources': [
                'src/tools/linux/symupload/sym_upload.cc',
                'src/common/linux/http_upload.cc',
              ],
              'include_dirs': [
                'src',
              ],
              'link_settings': {
                'libraries': [
                  '-ldl',
                ],
              },
            },
            {
              'target_name': 'dump_syms',
              'type': 'executable',

              'sources': [
                'linux/dump_syms.cc',
                'linux/dump_symbols.cc',
                'linux/dump_symbols.h',
                'linux/file_id.cc',
                'linux/file_id.h',
              ],

              'include_dirs': [
                'src',
                '..',
              ],
            },
          ],
        }],
      ],
      'targets': [
        {
          'target_name': 'breakpad_client',
          'type': '<(library)',

          'sources': [
            'linux/exception_handler.cc',
            'linux/linux_dumper.cc',
            'linux/minidump_writer.cc',
            'src/common/linux/guid_creator.cc',
            'src/common/string_conversion.cc',
            'src/common/convert_UTF.c',

            # TODO(agl): unfork this file
            'linux/minidump_file_writer.cc',
          ],

          'include_dirs': [
            'src',
            '..',
            '.',
          ],
        },
        {
          'target_name': 'breakpad_unittests',
          'type': 'executable',
          'dependencies': [
            '../testing/gtest.gyp:gtest',
            '../testing/gtest.gyp:gtestmain',
            'breakpad_client',
          ],

          'sources': [
            'linux/directory_reader_unittest.cc',
            'linux/exception_handler_unittest.cc',
            'linux/line_reader_unittest.cc',
            'linux/linux_dumper_unittest.cc',
            'linux/linux_libc_support_unittest.cc',
            'linux/memory_unittest.cc',
            'linux/minidump_writer_unittest.cc',
          ],

          'include_dirs': [
            'src',
            '..',
            '.',
          ],
        },
        {
          'target_name': 'generate_test_dump',
          'type': 'executable',

          'sources': [
            'linux/generate-test-dump.cc',
          ],

          'dependencies': [
            'breakpad_client',
          ],

          'include_dirs': [
            '..',
          ],
        },
        {
          'target_name': 'minidump_2_core',
          'type': 'executable',

          'sources': [
            'linux/minidump-2-core.cc',
          ],

          'include_dirs': [
            'src',
            '..',
          ],
        },
      ],
    }],
  ],
}
