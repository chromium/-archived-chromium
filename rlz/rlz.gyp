# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../build/common.gypi',
  ],
  'conditions': [
    [ 'OS == "win"', {
      'targets': [
        {
          'target_name': 'rlz',
          'type': 'none',
          'msvs_guid': 'BF4F447B-72B5-4059-BE1B-F94337B1F385',
          'conditions': [
            ['branding == "Chrome"', {
              'copies': [
                {
                  'destination': '<(PRODUCT_DIR)',
                  'files': [
                    'binaries/rlz.dll',
                    'binaries/rlz_dll.pdb',
                  ],
                },
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
