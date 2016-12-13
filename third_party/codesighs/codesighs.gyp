# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'codesighs',
      'type': 'executable',
      'sources': [
        'codesighs.c',
      ],
    },
    {
      'target_name': 'maptsvdifftool',
      'type': 'executable',
      'sources': [
        'maptsvdifftool.c',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'msmap2tsv',
          'type': 'executable',
          'sources': [
            'msmap2tsv.c',
          ],
          'link_settings': {
            'libraries': [
              '-lDbgHelp.lib',
            ],
          },
        },
        {
          'target_name': 'msdump2symdb',
          'type': 'executable',
          'sources': [
            'msdump2symdb.c',
          ],
        },
      ],
    }, { # else: OS != "windows"
      'targets': [
        {
          'target_name': 'nm2tsv',
          'type': 'executable',
          'sources': [
            'nm2tsv.c',
          ],
        },
      ],
    }],
  ],
}
