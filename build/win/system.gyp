# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../common.gypi',
  ],
  'targets': [
    {
      'target_name': 'cygwin',
      'type': 'none',
      'actions': [
        {
          'action_name': 'setup_mount',
          'msvs_cygwin_shell': 0,
          'inputs': [
            '../../third_party/cygwin/setup_mount.bat',
          ],
          # Visual Studio requires an output file, or else the
          # custom build step won't run.
          'outputs': [
            '../../third_party/cygwin/_always_run_setup_mount.marker',
          ],
          'action': ['<@(_inputs)'],
        },
      ],
    },
  ],
}
