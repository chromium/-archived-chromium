# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'tld_cleanup',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../build/temp_gyp/googleurl.gyp:googleurl',
      ],
      'sources': [
        'tld_cleanup.cc',
      ],
    },
  ],
}
