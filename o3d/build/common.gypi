# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'variables': {
    'antlrdir': 'third_party/antlr3',
    'breakpaddir': 'breakpad/src',
    'fcolladadir': 'third_party/fcollada/files',
    'gtestdir': 'testing/gtest/include',
    'jpegdir': 'third_party/libjpeg',
    'nacldir': 'third_party/native_client/googleclient',
    'nixysadir': 'third_party/nixysa/files',
    'npapidir': 'third_party/npapi/files',
    'pngdir': 'third_party/libpng',
    'skiadir': 'third_party/skia/include',
    'zlibdir': 'third_party/zlib',
  },
  'target_defaults': {
    'defines': [
      'GYP_BUILD',  # Needed to make a change in base/types.h conditional.
    ],
  },
  'conditions' : [
    ['OS == "win"',
      {
        'variables': {
          'renderer': 'd3d9',
          'cgdir': 'third_party/cg/files/win',
          'LIBRARY_SUFFIX': '.lib',
          'CONFIGURATION': '$(ConfigurationName)',
        },
        'target_defaults': {
          'defines': [
            '_CRT_SECURE_NO_WARNINGS',
            'RENDERER_D3D9',
            'OS_WIN',
            'NACL_WINDOWS',
          ],
          'msvs_disabled_warnings': [4355],
          'msvs_settings': {
            'VCCLCompilerTool': {
              'AdditionalOptions': '/MP',
            },
          },
        },
      },
    ],
    ['OS == "mac"',
      {
        'variables': {
          'renderer': 'gl',
          'cgdir': 'third_party/cg/files/mac',
          'LIBRARY_SUFFIX': '.a',
          'CONFIGURATION': '$(CONFIGURATION)',
        },
        'target_defaults': {
          'defines': [
            'RENDERER_GL',
            'OS_MACOSX',
          ],
        },
      },
    ],
    ['OS == "linux"',
      {
        'variables': {
          'renderer': 'gl',
          'cgdir': 'third_party/cg/files/linux',
          'LIBRARY_SUFFIX': '.a',
          'CONFIGURATION': '$(CONFIGURATION)',
        },
        'target_defaults': {
          'defines': [
            'RENDERER_GL',
            'OS_LINUX',
          ],
        },
      },
    ],
  ],
}
