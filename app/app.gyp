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
    'sources/': [
      ['exclude', '/(cocoa|gtk|win)/'],
      ['exclude', '_(cocoa|gtk|linux|mac|posix|skia|win|x)\\.(cc|mm?)$'],
      ['exclude', '/(gtk|win|x11)_[^/]*\\.cc$'],
    ],
    'conditions': [
      ['OS=="linux"', {'sources/': [
        ['include', '/gtk/'],
        ['include', '_(gtk|linux|posix|skia|x)\\.cc$'],
        ['include', '/(gtk|x11)_[^/]*\\.cc$'],
      ]}],
      ['OS=="mac"', {'sources/': [
        ['include', '/cocoa/'],
        ['include', '_(cocoa|mac|posix)\\.(cc|mm?)$'],
      ]}, { # else: OS != "mac"
        'sources/': [
          ['exclude', '\\.mm?$'],
        ],
      }],
      ['OS=="win"', {'sources/': [
        ['include', '_(win)\\.cc$'],
        ['include', '/win/'],
        ['include', '/win_[^/]*\\.cc$'],
      ]}],
    ],
  },
  'targets': [
    {
      'target_name': 'app_base',
      'type': '<(library)',
      'msvs_guid': '4631946D-7D5F-44BD-A5A8-504C0A7033BE',
      'dependencies': [
        'app_resources',
        'app_strings',
        '../base/base.gyp:base',
        '../base/base.gyp:base_gfx',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../third_party/icu38/icu38.gyp:icui18n',
        '../third_party/icu38/icu38.gyp:icuuc',
      ],
      'include_dirs': [
        '..',
        '../chrome/third_party/wtl/include',
      ],
      'sources': [
        # All .cc, .h, and .mm files under app/ except for tests.
        'animation.cc',
        'animation.h',
        'app_paths.h',
        'app_paths.cc',
        'app_switches.h',
        'app_switches.cc',
        'drag_drop_types.cc',
        'drag_drop_types.h',
        'gfx/canvas.cc',
        'gfx/canvas.h',
        'gfx/canvas_linux.cc',
        'gfx/canvas_win.cc',
        'gfx/font.h',
        'gfx/font_gtk.cc',
        'gfx/font_mac.mm',
        'gfx/font_skia.cc',
        'gfx/font_win.cc',
        'gfx/color_utils.cc',
        'gfx/color_utils.h',
        'gfx/favicon_size.h',
        'gfx/icon_util.cc',
        'gfx/icon_util.h',
        'gfx/insets.h',
        'gfx/path_gtk.cc',
        'gfx/path_win.cc',
        'gfx/path.h',
        'gfx/text_elider.cc',
        'gfx/text_elider.h',
        'l10n_util.cc',
        'l10n_util.h',
        'l10n_util_posix.cc',
        'l10n_util_win.cc',
        'l10n_util_win.h',
        'message_box_flags.h',
        'os_exchange_data_win.cc',
        'os_exchange_data_gtk.cc',
        'os_exchange_data.h',
        'resource_bundle.cc',
        'resource_bundle.h',
        'resource_bundle_win.cc',
        'resource_bundle_linux.cc',
        'resource_bundle_mac.mm',
        'slide_animation.cc',
        'slide_animation.h',
        'theme_provider.cc',
        'theme_provider.h',
        'throb_animation.cc',
        'throb_animation.h',
        'table_model.cc',
        'table_model.h',
        'table_model_observer.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            # font_gtk.cc uses fontconfig.
            # TODO(evanm): I think this is wrong; it should just use GTK.
            '../build/linux/system.gyp:fontconfig',
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'win_util.cc',
            'win_util.h',
          ],
        }],
        ['OS!="win"', {
          'sources!': [
            'drag_drop_types.cc',
            'drag_drop_types.h',
            'gfx/icon_util.cc',
            'gfx/icon_util.h',
            'os_exchange_data.cc',
          ],
          'conditions': [
            ['toolkit_views==0', {
              # Note: because of gyp predence rules this has to be defined as
              # 'sources/' rather than 'sources!'.
              'sources/': [
                ['exclude', '^os_exchange_data_gtk.cc'],
                ['exclude', '^os_exchange_data.h'],
              ],
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'app_unittests',
      'type': 'executable',
      'msvs_guid': 'B4D59AE8-8D2F-97E1-A8E9-6D2826729530',
      'dependencies': [
        'app_base',
        'app_resources',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu38/icu38.gyp:icui18n',
        '../third_party/icu38/icu38.gyp:icuuc',
        '../third_party/libxml/libxml.gyp:libxml',
      ],
      'sources': [
        'animation_unittest.cc',
        'gfx/font_unittest.cc',
        'gfx/icon_util_unittest.cc',
        'gfx/text_elider_unittest.cc',
        'l10n_util_unittest.cc',
        'os_exchange_data_win_unittest.cc',
        'run_all_unittests.cc',
        'test_suite.h',
        'tree_node_iterator_unittest.cc',
        'win_util_unittest.cc',      
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['OS=="linux"', {
          # TODO: Move these dependencies to platform-neutral once these
          # projects are generated by GYP.
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['OS!="win"', {
          'sources!': [
            'gfx/icon_util_unittest.cc',
            'os_exchange_data_win_unittest.cc',
            'win_util_unittest.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'app_strings',
      'msvs_guid': 'AE9BF4A2-19C5-49D8-BB1A-F28496DD7051',
      'type': 'none',
      'rules': [
        {
          'rule_name': 'grit',
          'extension': 'grd',
          'inputs': [
            '../tools/grit/grit.py',
          ],
          'variables': {
            'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/app',
          },
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/app/grit/<(RULE_INPUT_ROOT).h',
            '<(SHARED_INTERMEDIATE_DIR)/app/<(RULE_INPUT_ROOT)_en-US.pak',
          ],
          'action': ['python', '<@(_inputs)', '-i', '<(RULE_INPUT_PATH)',
            'build', '-o', '<(grit_out_dir)'],
          'message': 'Generating resources from <(RULE_INPUT_PATH)',
        },
      ],
      'sources': [
        # Localizable resources.
        'resources/app_locale_settings.grd',
        'resources/app_strings.grd',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/app',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['../build/win/system.gyp:cygwin'],
        }],
      ],
    },
    {
      'target_name': 'app_resources',
      'type': 'none',
      'msvs_guid': '3FBC4235-3FBD-46DF-AEDC-BADBBA13A095',
      'variables': {
        'grit_path': '../tools/grit/grit.py',
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/app',
      },
      'actions': [
        {
          'action_name': 'app_resources',
          'variables': {
            'input_path': 'resources/app_resources.grd',
          },
          'inputs': [
            '<(input_path)',
          ],
          'outputs': [
            '<(grit_out_dir)/grit/app_resources.h',
            '<(grit_out_dir)/app_resources.pak',
            '<(grit_out_dir)/app_resources.rc',
          ],
          'action': ['python', '<(grit_path)', '-i', '<(input_path)', 'build', '-o', '<(grit_out_dir)'],
          'message': 'Generating resources from <(input_path)',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/app',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['../build/win/system.gyp:cygwin'],
        }],
      ],
    },    
  ],
}
