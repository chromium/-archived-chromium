# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'conditions': [
      ['OS!="win"', {
        'all_gyps%': 1,
      },{  # else OS=="win"
        'all_gyps%': 0,
      }],
    ],
  },
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:*',
        '../net/net.gyp:*',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../tools/gtk_clipboard_dump/gtk_clipboard_dump.gyp:*',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '../sandbox/sandbox.gyp:*',
          ],
        }],
        ['all_gyps', {
          'dependencies': [
            '../chrome/chrome.gyp:*',
            '../media/media.gyp:*',
            '../printing/printing.gyp:*',
            '../sdch/sdch.gyp:*',
            '../skia/skia.gyp:*',
            '../testing/gtest.gyp:*',
            '../third_party/bzip2/bzip2.gyp:*',
            '../third_party/icu38/icu38.gyp:*',
            '../third_party/libevent/libevent.gyp:*',
            '../third_party/libjpeg/libjpeg.gyp:*',
            '../third_party/libpng/libpng.gyp:*',
            '../third_party/libxml/libxml.gyp:*',
            '../third_party/libxslt/libxslt.gyp:*',
            '../third_party/modp_b64/modp_b64.gyp:*',
            '../third_party/npapi/npapi.gyp:*',
            '../third_party/sqlite/sqlite.gyp:*',
            '../third_party/zlib/zlib.gyp:*',
            '../webkit/tools/test_shell/test_shell.gyp:*',
            '../webkit/webkit.gyp:*',
            'temp_gyp/googleurl.gyp:*',
            'temp_gyp/v8.gyp:*',
          ],
        }],
      ],
    },
  ],
}

