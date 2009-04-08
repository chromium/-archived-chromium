# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'modp_b64',
      'type': '<(library)',
      'msvs_guid': '7100F41F-868D-4E99-80A2-AF8E6574749D',
      'sources': [
        'modp_b64.cc',
        'modp_b64.h',
        'modp_b64_data.h',
      ],
      'include_dirs': [
        '../..',
      ],
    },
  ],
}
