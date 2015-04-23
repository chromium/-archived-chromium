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
  'conditions': [
    [ 'OS == "win"', {
      'targets': [
        {
          'target_name': 'gears',
          'type': 'none',
          'msvs_guid': 'D703D7A0-EDC1-4FE6-9E22-56154155B24E',
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                'binaries/gears.dll',
                'binaries/gears.pdb',
              ],
            },
          ],
        },
      ],
    }],
  ],
}
