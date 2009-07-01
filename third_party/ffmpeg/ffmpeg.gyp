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
      'variables': {
        'generate_stubs_script': 'generate_stubs.py',
        'sig_files': [
          'avcodec-52.sigs',
          'avformat-52.sigs',
          'avutil-50.sigs',
        ],
        'extra_header': 'ffmpeg_stub_headers.fragment',
      },
      'target_name': 'ffmpeg',
      'msvs_guid': 'D7A94F58-576A-45D9-A45F-EB87C63ABBB0',
      'dependencies': [
        'ffmpeg_binaries',
      ],
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
        '<@(sig_files)',
        '<(extra_header)'
      ],
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
      'conditions': [
        ['OS=="win"',
          {
            'variables': {
              'outfile_type': 'windows_lib',
              'output_dir': '<(PRODUCT_DIR)/lib',
              'intermediate_dir': '<(INTERMEDIATE_DIR)',
            },
            'type': 'none',
            'sources!': [
              '<(extra_header)',
            ],
            'direct_dependent_settings': {
              'include_dirs': [
                'include/win',
              ],
              'link_settings': {
                'libraries': [
                  '<(output_dir)/avcodec-52.lib',
                  '<(output_dir)/avformat-52.lib',
                  '<(output_dir)/avutil-50.lib',
                ],
                'msvs_settings': {
                  'VCLinkerTool': {
                    'DelayLoadDLLs': [
                      'avcodec-52.dll',
                      'avformat-52.dll',
                      'avutil-50.dll',
                    ],
                  },
                },
              },
            },
            'rules': [
              {
                'rule_name': 'generate_libs',
                'extension': 'sigs',
                'inputs': [
                  '<(generate_stubs_script)',
                  '<@(sig_files)',
                ],
                'outputs': [
                  '<(output_dir)/<(RULE_INPUT_ROOT).lib',
                ],
                'action': ['python', '<(generate_stubs_script)',
                           '-i', '<(intermediate_dir)',
                           '-o', '<(output_dir)',
                           '-t', '<(outfile_type)',
                           '<@(RULE_INPUT_PATH)',
                ],
                'message': 'Generating FFmpeg import libraries.',
              },
            ],
          }, {  # else OS!="win"
            'variables': {
              'outfile_type': 'posix_stubs',
              'stubs_filename_root': 'ffmpeg_stubs',
              'project_path': 'third_party/ffmpeg',
              'intermediate_dir': '<(INTERMEDIATE_DIR)',
              'output_root': '<(SHARED_INTERMEDIATE_DIR)/ffmpeg',
            },
            'type': '<(library)',
            'include_dirs': [
              'include',
              '<(output_root)',
              '../..',  # The chromium 'src' directory.
            ],
            'direct_dependent_settings': {
              'defines': [
                '__STDC_CONSTANT_MACROS',  # FFmpeg uses INT64_C.
              ],
              'include_dirs': [
                '<(output_root)',
                '../..',  # The chromium 'src' directory.
              ],
            },
            'actions': [
              {
                'action_name': 'generate_stubs',
                'inputs': [
                  '<(generate_stubs_script)',
                  '<(extra_header)',
                  '<@(sig_files)',
                ],
                'outputs': [
                  '<(intermediate_dir)/<(stubs_filename_root).cc',
                  '<(output_root)/<(project_path)/<(stubs_filename_root).h',
                ],
                'action': ['python',
                           '<(generate_stubs_script)',
                           '-i', '<(intermediate_dir)',
                           '-o', '<(output_root)/<(project_path)',
                           '-t', '<(outfile_type)',
                           '-e', '<(extra_header)',
                           '-s', '<(stubs_filename_root)',
                           '-p', '<(project_path)',
                           '<@(_inputs)',
                ],
                'message': 'Generating FFmpeg stubs for dynamic loading.',
                'process_outputs_as_sources': 1,
              },
            ],
          }
        ],
      ],
    },
    {
      'target_name': 'ffmpeg_binaries',
      'type': 'none',
      'msvs_guid': '4E4070E1-EFD9-4EF1-8634-3960956F6F10',
      'conditions': [
        ['OS=="win"', {
          'variables': {
            'source_files': [
              'binaries/avcodec-52.dll',
              'binaries/avformat-52.dll',
              'binaries/avutil-50.dll',
              'binaries/pthreadGC2.dll',
            ],
            'output_files': [
              '<(PRODUCT_DIR)/avcodec-52.dll',
              '<(PRODUCT_DIR)/avformat-52.dll',
              '<(PRODUCT_DIR)/avutil-50.dll',
              '<(PRODUCT_DIR)/pthreadGC2.dll',
            ],
          },
          'dependencies': ['../../build/win/system.gyp:cygwin'],
        }], ['OS=="linux"', {
          'variables': {
            'source_files': [
              'binaries/libavcodec.so.52',
              'binaries/libavformat.so.52',
              'binaries/libavutil.so.50',
            ],
            'output_files': [
              '<(PRODUCT_DIR)/libavcodec.so.52',
              '<(PRODUCT_DIR)/libavformat.so.52',
              '<(PRODUCT_DIR)/libavutil.so.50',
            ],
          },
        }], ['OS=="mac"', {
          'variables': {
            'source_files': [
              'binaries/libavcodec.52.dylib',
              'binaries/libavformat.52.dylib',
              'binaries/libavutil.50.dylib',
            ],
            'output_files': [
              '<(PRODUCT_DIR)/libavcodec.52.dylib',
              '<(PRODUCT_DIR)/libavformat.52.dylib',
              '<(PRODUCT_DIR)/libavutil.50.dylib',
            ],
          },
        }],
      ],
      'sources': [
      ],
      'actions': [
        {
          'action_name': 'copy_binaries',
          'inputs': [
            'copy_binaries.sh',
          ],
          'outputs': [
            '<@(output_files)',
          ],
          'action': [
            './copy_binaries.sh',
            '<@(source_files)',
            '<(PRODUCT_DIR)/'
          ],
          'message': 'Copying FFmpeg binaries...',
        },
      ],
    },
  ],
}
