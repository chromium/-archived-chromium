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
      '../../<(cgdir)/include',
      '../../<(gtestdir)',
    ],
  },
  'targets': [
    {
      'target_name': 'o3dImport',
      'type': 'static_library',
      'dependencies': [
        '../../<(antlrdir)/antlr.gyp:antlr3c',
        '../../<(fcolladadir)/fcollada.gyp:fcollada',
        '../../<(jpegdir)/libjpeg.gyp:libjpeg',
        '../../<(pngdir)/libpng.gyp:libpng',
        '../../<(zlibdir)/zlib.gyp:zlib',
        '../compiler/technique/technique.gyp:technique',
      ],
      'sources': [
        'cross/collada_conditioner.cc',
        'cross/collada_conditioner.h',
        'cross/collada.cc',
        'cross/collada.h',
        'cross/collada_zip_archive.cc',
        'cross/collada_zip_archive.h',
        'cross/file_output_stream_processor.cc',
        'cross/file_output_stream_processor.h',
        'cross/gz_compressor.cc',
        'cross/gz_compressor.h',
        'cross/precompile.h',
        'cross/tar_generator.cc',
        'cross/tar_generator.h',
        'cross/tar_processor.h',
        'cross/targz_generator.cc',
        'cross/targz_generator.h',
        'cross/targz_processor.h',
        'cross/zip_archive.cc',
        'cross/zip_archive.h',
      ],
      'conditions' : [
        ['renderer == "d3d9" and OS == "win"',
          {
            'include_dirs': [
              '$(DXSDK_DIR)/Include',
            ],
          }
        ],
        ['OS == "win"',
          {
            'sources': [
              'win/collada_conditioner_win.cc',
            ],
          },
        ],
        ['OS == "mac"',
          {
            'sources': [
              'mac/collada_conditioner_mac.mm',
            ],
            'link_settings': {
              'libraries': [
                '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
              ],
            },
          },
        ],
        ['OS == "linux"',
          {
            'sources': [
              'linux/collada_conditioner_linux.cc',
            ],
          },
        ],
      ],
    },
    {
      'target_name': 'o3dArchive',
      'type': 'static_library',
      'dependencies': [
        '../../<(antlrdir)/antlr.gyp:antlr3c',
        '../../<(fcolladadir)/fcollada.gyp:fcollada',
        '../../<(jpegdir)/libjpeg.gyp:libjpeg',
        '../../<(pngdir)/libpng.gyp:libpng',
        '../../<(zlibdir)/zlib.gyp:zlib',
      ],
      'sources': [
        'cross/archive_processor.cc',
        'cross/archive_processor.h',
        'cross/archive_request.cc',
        'cross/archive_request.h',
        'cross/gz_decompressor.cc',
        'cross/gz_decompressor.h',
        'cross/iarchive_generator.h',
        'cross/memory_buffer.h',
        'cross/memory_stream.cc',
        'cross/memory_stream.h',
        'cross/raw_data.cc',
        'cross/raw_data.h',
        'cross/tar_generator.h',
        'cross/tar_processor.cc',
        'cross/tar_processor.h',
        'cross/targz_generator.h',
        'cross/targz_processor.cc',
        'cross/targz_processor.h',
      ],
      'conditions' : [
        ['renderer == "d3d9" and OS == "win"',
          {
            'include_dirs': [
              '$(DXSDK_DIR)/Include',
            ],
          }
        ],
      ],
    },
    {
      'target_name': 'o3dImportTest',
      'type': 'none',
      'dependencies': [
        'o3dArchive',
        'o3dImport',
        '../../<(antlrdir)/antlr.gyp:antlr3c',
        '../../<(fcolladadir)/fcollada.gyp:fcollada',
        '../../<(jpegdir)/libjpeg.gyp:libjpeg',
        '../../<(pngdir)/libpng.gyp:libpng',
        '../../<(zlibdir)/zlib.gyp:zlib',
      ],
      'direct_dependent_settings': {
        'sources': [
          'cross/gz_compressor_test.cc',
          'cross/gz_decompressor_test.cc',
          'cross/memory_buffer_test.cc',
          'cross/memory_stream_test.cc',
          'cross/raw_data_test.cc',
          'cross/tar_generator_test.cc',
          'cross/tar_processor_test.cc',
          'cross/targz_generator_test.cc',
          'cross/targz_processor_test.cc',
        ],
        'conditions' : [
          ['renderer == "d3d9" and OS == "win"',
            {
              'include_dirs': [
                '$(DXSDK_DIR)/Include',
              ],
            }
          ],
        ],
      },
    },
  ],
}
