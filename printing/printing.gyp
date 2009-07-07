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
      'target_name': 'printing',
      'type': '<(library)',
      'dependencies': [
        '../app/app.gyp:app_base',  # Only required for Font support
        '../base/base.gyp:base',
        '../base/base.gyp:base_gfx',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../skia/skia.gyp:skia',
        '../third_party/icu38/icu38.gyp:icui18n',
        '../third_party/icu38/icu38.gyp:icuuc',
      ],
      'msvs_guid': '9E5416B9-B91B-4029-93F4-102C1AD5CAF4',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'emf_win.cc',
        'emf_win.h',
        'native_metafile.h',
        'page_number.cc',
        'page_number.h',
        'page_overlays.cc',
        'page_overlays.h',
        'page_range.cc',
        'page_range.h',
        'page_setup.cc',
        'page_setup.h',
        'print_settings.cc',
        'print_settings.h',
        'printed_document.cc',
        'printed_document.h',
        'printed_page.cc',
        'printed_page.h',
        'printed_pages_source.h',
        'printing_context.h',
        'printing_context_linux.cc',
        'printing_context_mac.cc',        
        'printing_context_win.cc',
        'units.cc',
        'units.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'conditions': [
        ['OS!="linux"', {'sources/': [['exclude', '_linux\\.cc$']]}],
        ['OS!="mac"', {'sources/': [['exclude', '_mac\\.(cc|mm?)$']]}],
        ['OS!="win"', {
          'sources/': [
            ['exclude', '_win\\.cc$'],
            ['exclude', 
              'printed_document.cc',
            ]
          ]  
        }, {  # else: OS=="win"
          'sources/': [['exclude', '_posix\\.cc$']]
        }],
      ],
    },
    {
      'target_name': 'printing_unittests',
      'type': 'executable',
      'msvs_guid': '8B2EE5D9-41BC-4AA2-A401-2DC143A05D2E',
      'dependencies': [
        'printing',
        '../testing/gtest.gyp:gtest',
        '../testing/gtest.gyp:gtestmain',
      ],
      'sources': [
        'emf_win_unittest.cc',
        'printing_test.h',
        'page_number_unittest.cc',
        'page_overlays_unittest.cc',
        'page_range_unittest.cc',
        'page_setup_unittest.cc',
        'printing_context_win_unittest.cc',
        'units_unittest.cc',
      ],
      'conditions': [
        ['OS!="linux"', {'sources/': [['exclude', '_linux_unittest\\.cc$']]}],
        ['OS!="mac"', {'sources/': [['exclude', '_mac_unittest\\.(cc|mm?)$']]}],
        ['OS!="win"', {
          'sources/': [
            ['exclude', '_win_unittest\\.cc$'],

            # Most of the printing functionailty is Windows only for now.
            ['exclude', '.*'],
            ['include', 'page_range_unittest.cc'],
            ['include', 'page_setup_unittest.cc'],            
            ['include', 'units_unittest.cc'],
          ]
        }, {  # else: OS=="win"
          'sources/': [['exclude', '_posix_unittest\\.cc$']]
        }],
      ],
    },
  ],
}