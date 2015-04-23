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
          'target_name': 'activex_shim',
          'type': '<(library)',
          'msvs_guid': 'F4F4BCAA-EA59-445C-A119-3E6C29647A51',
          'dependencies': [
            '../../base/base.gyp:base',
            '../../third_party/npapi/npapi.gyp:npapi',
            '../../build/temp_gyp/googleurl.gyp:googleurl',
          ],
          'sources': [
            'activex_plugin.cc',
            'activex_plugin.h',
            'activex_shared.cc',
            'activex_shared.h',
            'activex_util.cc',
            'activex_util.h',
            'dispatch_object.cc',
            'dispatch_object.h',
            'ihtmldocument_impl.h',
            'iwebbrowser_impl.h',
            'npn_scripting.cc',
            'npn_scripting.h',
            'npp_impl.cc',
            'npp_impl.h',
            'README',
            'web_activex_container.cc',
            'web_activex_container.h',
            'web_activex_site.cc',
            'web_activex_site.h',
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
