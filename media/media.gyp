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
    'conditions': [
      ['OS!="linux"', {'sources/': [['exclude', '/linux/']]}],
      ['OS!="mac"', {'sources/': [['exclude', '/mac/']]}],
      ['OS!="win"', {'sources/': [['exclude', '/win/']]}],
    ],
  },
  'targets': [
    {
      'target_name': 'media',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'msvs_guid': '6AE76406-B03B-11DD-94B1-80B556D89593',
      'sources': [
        'audio/linux/audio_manager_linux.cc',
        'audio/mac/audio_manager_mac.cc',
        'audio/win/audio_manager_win.h',
        'audio/win/audio_output_win.cc',
        'audio/win/simple_sources_win.cc',
        'audio/win/waveout_output_win.cc',
        'audio/win/waveout_output_win.h',
        'audio/audio_output.h',
        'audio/simple_sources.h',
        'base/buffers.h',
        'base/data_buffer.cc',
        'base/data_buffer.h',
        'base/factory.h',
        'base/filter_host.h',
        'base/filter_host_impl.cc',
        'base/filter_host_impl.h',
        'base/filters.h',
        'base/media_format.cc',
        'base/media_format.h',
        'base/mock_filter_host.h',
        'base/mock_media_filters.h',
        'base/mock_pipeline.h',
        'base/pipeline.h',
        'base/pipeline_impl.cc',
        'base/pipeline_impl.h',
        'base/synchronizer.cc',
        'base/synchronizer.h',
        'filters/audio_renderer_base.cc',
        'filters/audio_renderer_base.h',
        'filters/audio_renderer_impl.cc',
        'filters/audio_renderer_impl.h',
        'filters/file_data_source.cc',
        'filters/file_data_source.h',
        'filters/null_audio_renderer.cc',
        'filters/null_audio_renderer.h',
        'filters/video_renderer_base.cc',
        'filters/video_renderer_base.h',
        'player/player.cc',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
    },
    {
      'target_name': 'media_unittests',
      'type': 'executable',
      'dependencies': [
        'media',
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'audio/win/audio_output_win_unittest.cc',
        'base/data_buffer_unittest.cc',
        'base/pipeline_impl_unittest.cc',
        'base/run_all_unittests.cc',
        'filters/file_data_source_unittest.cc',
        'filters/video_renderer_unittest.cc',
      ],
    },
  ],
}
