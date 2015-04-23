# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../build/common.gypi',
  ],
  'conditions': [
    [ 'OS=="win"', {
      'targets': [
        {
          'target_name': 'activex_shim_dll',
          'type': 'loadable_module',
          'dependencies': [
            '../../base/base.gyp:base',
            '../../third_party/npapi/npapi.gyp:npapi',
            '../activex_shim/activex_shim.gyp:activex_shim',
          ],
          'product_name': 'npaxshim',
          'msvs_guid': '494E414B-1655-48CE-996D-6413ECFB7829',
          'msvs_settings': {
            'VCLinkerTool': {
              'RegisterOutput': 'false',
            },
          },
          'sources': [
            'activex_shim_dll.cc',
            'activex_shim_dll.def',
            'activex_shim_dll.rc',
            'resource.h',
          ],
          'link_settings': {
            'libraries': [
              '-lurlmon.lib',
            ],
          },
        },
      ],
    }],
  ],
}
