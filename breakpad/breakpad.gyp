# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This only builds the Mac version of Breakpad for now.

{
  'includes': [
    '../build/common.gypi',
  ],
  'target_defaults': {
    'include_dirs': [
      'src/',
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
        'src/client/mac/sender/English.lproj/Breakpad.nib',
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
}
