# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'target_defaults': {
    'conditions': [
      ['OS!="linux"', {'sources/': [['exclude', '/linux/']]}],
      ['OS!="mac"', {'sources/': [['exclude', '/mac/']]}],
      ['OS!="win"', {'sources/': [['exclude', '/win/']]}],
    ],
  },
  'targets': [
    {
      'target_name': 'ffmpeg',
      'type': 'none',
      'msvs_guid': 'D7A94F58-576A-45D9-A45F-EB87C63ABBB0',
      'sources': [
        'include/libavcodec/avcodec.h',
        'include/libavcodec/opt.h',
        'include/libavcodec/vdpau.h',
        'include/libavcodec/xvmc.h',
        'include/libavformat/avformat.h',
        'include/libavformat/avio.h',
        'include/libavutil/adler32.h',
        'include/libavutil/avstring.h',
        'include/libavutil/avutil.h',
        'include/libavutil/base64.h',
        'include/libavutil/common.h',
        'include/libavutil/crc.h',
        'include/libavutil/fifo.h',
        'include/libavutil/intfloat_readwrite.h',
        'include/libavutil/log.h',
        'include/libavutil/lzo.h',
        'include/libavutil/mathematics.h',
        'include/libavutil/md5.h',
        'include/libavutil/mem.h',
        'include/libavutil/pixfmt.h',
        'include/libavutil/rational.h',
        'include/libavutil/sha1.h',
        'include/win/inttypes.h',
        'include/win/stdint.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'avcodec-52.def',
            'avformat-52.def',
            'avutil-50.def',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'include/win',
            ],
            'link_settings': {
              'libraries': [
                '<(PRODUCT_DIR)/lib/avcodec-52.lib',
                '<(PRODUCT_DIR)/lib/avformat-52.lib',
                '<(PRODUCT_DIR)/lib/avutil-50.lib',
              ],
            },
          },
          'dependencies': ['../../build/win/system.gyp:cygwin'],
          'rules': [
            {
              'rule_name': 'generate_libs',
              'extension': 'def',
              'inputs': [
                'generate_libs.py',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/lib/<(RULE_INPUT_ROOT).lib',
              ],
              'variables': {
                'def_files': [
                  'avcodec-52.def',
                  'avformat-52.def',
                  'avutil-50.def',
                ],
              },
              'action': ['python', '<@(_inputs)', '-o', '<(PRODUCT_DIR)/lib', '<@(RULE_INPUT_PATH)'],
              'message': 'Generating import libraries',
            },
          ],
        }],
      ],
    },
  ],
}
