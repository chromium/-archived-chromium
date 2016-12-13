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
    'include_dirs': [
      '..',
      '../..',
      '../../<(gtestdir)',
    ],
    'defines': [
      'O3D_PLUGIN_NAME="<!(python version_info.py --name)"',
      'O3D_PLUGIN_DESCRIPTION="<!(python version_info.py --description)"',
      'O3D_PLUGIN_MIME_TYPE="<!(python version_info.py --mimetype)"',
    ],
  },

  'targets': [
    {
      'target_name': 'npo3dautoplugin',
      'type': 'shared_library',
      'dependencies': [
        '../../<(antlrdir)/antlr.gyp:antlr3c',
        '../../<(jpegdir)/libjpeg.gyp:libjpeg',
        '../../<(pngdir)/libpng.gyp:libpng',
        '../../<(zlibdir)/zlib.gyp:zlib',
        '../../base/base.gyp:base',
        '../../skia/skia.gyp:skia',
        '../../v8/tools/gyp/v8.gyp:v8',
        '../compiler/technique/technique.gyp:technique',
        '../core/core.gyp:o3dCore',
        '../core/core.gyp:o3dCorePlatform',
        '../import/import.gyp:o3dArchive',
        '../import/import.gyp:o3dImport',
        '../serializer/serializer.gyp:o3dSerializer',
        '../utils/utils.gyp:o3dUtils',
        '../build/nacl.gyp:build_nacl',
        'idl/idl.gyp:o3dPluginIdl',
      ],
      'sources': [
        'cross/async_loading.cc',
        'cross/async_loading.h',
        'cross/blacklist.cc',
        'cross/config.h',
        'cross/config_common.cc',
        'cross/download_stream.h',
        'cross/main.cc',
        'cross/main.h',
        'cross/marshaling_utils.h',
        'cross/np_v8_bridge.cc',
        'cross/np_v8_bridge.h',
        'cross/out_of_memory.cc',
        'cross/out_of_memory.h',
        'cross/plugin_logging.h',
        'cross/plugin_main.h',
        'cross/stream_manager.cc',
        'cross/stream_manager.h',
      ],
      'conditions' : [
        ['OS != "linux"',
          {
            'dependencies': [
              '../statsreport/statsreport.gyp:o3dStatsReport',
              'add_version',
              'o3dPluginLogging',
            ],
          },
        ],
        ['OS == "mac"',
          {
            'sources': [
              'mac/config_mac.mm',
              'mac/main_mac.mm',
              'mac/o3d_plugin.r',
              'mac/plugin_logging-mac.mm',
              'mac/plugin_mac.h',
              'mac/plugin_mac.mm',
            ],
            'link_settings': {
              'libraries': [
                '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
              ],
            },
          },
        ],
        ['OS == "win"',
          {
            'dependencies': [
              '../breakpad/breakpad.gyp:o3dBreakpad',
            ],
            'sources': [
              'win/config.cc',
              'win/logger_main.cc',
              'win/main_win.cc',
              'win/o3dPlugin.def',
              'win/o3dPlugin.rc',
              'win/plugin_logging-win32.cc',
              'win/resource.h',
              'win/update_lock.cc',
              'win/update_lock.h',
            ],
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'rpcrt4.lib',
                ],
              },
            },
          },
        ],
        ['OS == "win" and renderer == "d3d9"',
          {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  '"$(DXSDK_DIR)/Lib/x86/DxErr9.lib"',
                  '"$(DXSDK_DIR)/Lib/x86/d3dx9.lib"',
                  'd3d9.lib',
                ],
              },
            },
          },
        ],
      ],
    },
  ],
  'conditions': [
    ['OS != "linux"',
      {
        'targets': [
          {
            'target_name': 'o3dPluginLogging',
            'type': 'static_library',
            'conditions': [
              ['OS=="win"',
                {
                  'sources': [
                    'win/plugin_metrics-win32.cc',
                    'win/plugin_logging-win32.cc',
                  ],
                },
              ],
              ['OS=="mac"',
                {
                  'sources': [
                    'mac/plugin_metrics-mac.cc',
                    'mac/plugin_logging-mac.mm',
                  ],
                },
              ],
            ],
          },
          {
            'target_name': 'add_version',
            'type': 'none',
            'actions': [
              {
                'action_name': 'add_version',
                'inputs': [
                  'version_info.py',
                ],
                'conditions': [
                  ['OS=="win"',
                    {
                      'inputs': [
                        'win/o3dPlugin.rc_template',
                      ],
                      'outputs': [
                        'win/o3dPlugin.rc'
                      ],
                      'action': ['python',
                        'version_info.py',
                        'win/o3dPlugin.rc_template',
                      'win/o3dPlugin.rc'],
                    },
                  ],
                  ['OS=="mac"',
                    {
                      'inputs': [
                        'mac/processed/Info.plist',
                      ],
                      'outputs': [
                        'mac/Info.plist',
                      ],
                      'action': ['python',
                        'version_info.py',
                        'mac/processed/Info.plist',
                      'mac/Info.plist'],
                    },
                  ],
                ],
              },
            ],
          },
        ],
      },
    ],
  ],
}
