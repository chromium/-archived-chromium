# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'google_update',
      'type': 'static_library',
      'sources': [
        'google_update_idl.idl',
        '<(INTERMEDIATE_DIR)/google_update_idl_i.c',
        '<(INTERMEDIATE_DIR)/google_update_idl_p.c',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          # Bit of a hack to work around the built in vstudio rule.
          '<(INTERMEDIATE_DIR)/../google_update',
        ],
      },
    },
  ],
}
