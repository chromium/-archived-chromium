# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../common.gypi',
  ],
  'targets': [
    {
      'target_name': 'lastchange',
      'type': 'none',
      'variables': {
        'lastchange_out_path': '<(SHARED_INTERMEDIATE_DIR)/build/LASTCHANGE',
      },
      'actions': [
        {
          'action_name': 'lastchange',
          'inputs': [
            './lastchange.py',
          ],
          'outputs': [
            '<(lastchange_out_path)',
            '<(lastchange_out_path).always',
          ],
          'action': [
            'python', '<@(_inputs)', '-o', '<(lastchange_out_path)',
          ],
          'message': 'Extracting last change to <(lastchange_out_path)'
        },
      ],
    },
  ]
}
