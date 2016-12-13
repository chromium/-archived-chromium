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
  'target_defaults': {
    'include_dirs': [
      '..',
      '../..',
      '../../<(gtestdir)',
    ],
  },
  'targets': [],
  'conditions': [
    ['OS!="linux"',
      {
        'targets': [
          {
            'target_name': 'o3dStatsReport',
            'type': 'static_library',
            'sources': [
              'aggregator-mac.h',
              'aggregator-mac.mm',
              'aggregator-win32.cc',
              'aggregator-win32.h',
              'aggregator.cc',
              'aggregator.h',
              'common/const_product.h',
              'common/highres_timer-linux.cc',
              'common/highres_timer-linux.h',
              'common/highres_timer-mac.cc',
              'common/highres_timer-mac.h',
              'common/highres_timer-win32.cc',
              'common/highres_timer-win32.h',
              'common/highres_timer.h',
              'const-mac.h',
              'const-mac.mm',
              'const-win32.cc',
              'const-win32.h',
              'const_server.h',
              'formatter.cc',
              'formatter.h',
              'lock.h',
              'metrics.cc',
              'metrics.h',
              'persistent_iterator-win32.cc',
              'persistent_iterator-win32.h',
              'uploader-mac.mm',
              'uploader-win32.cc',
              'uploader.h',
              'uploader_aggregation-mac.mm',
              'uploader_aggregation-win32.cc',
              'util-win32.h',
            ],
            'conditions': [
              ['OS != "win"',
                {
                  'sources/': [
                    ['exclude', '(-win32)\.(cc|h)$'],
                  ],
                },
              ],
              ['OS != "linux"',
                {
                  'sources/': [
                    ['exclude', '(-linux)\.(cc|h)$'],
                  ],
                },
              ],
              ['OS != "mac"',
                {
                  'sources/': [
                    ['exclude', '(-mac)\.(cc|mm|h)$'],
                  ],
                },
              ],
            ],
          },
          {
            'target_name': 'o3dStatsReportTest',
            'type': 'none',
            'dependencies': [
              'o3dStatsReport',
            ],
            'direct_dependent_settings': {
              'sources': [
                'aggregator-win32_unittest.cc',
                'aggregator-win32_unittest.h',
                'aggregator_unittest.cc',
                'aggregator_unittest.h',
                'common/highres_timer_unittest.cc',
                'formatter_unittest.cc',
                'metrics_unittest.cc',
                'persistent_iterator-win32_unittest.cc',
              ],
              'conditions': [
                ['OS != "win"',
                  {
                    'sources/': [
                      ['exclude', '(-win32)_unittest\.(cc|h)$'],
                    ],
                  },
                ],
                ['OS != "mac"',
                  {
                    'sources/': [
                      ['exclude', '(-mac)_unittest\.(cc|mm|h)$'],
                    ],
                  },
                ],
              ],
            },
          },
        ],
      },
    ],
  ],
}
