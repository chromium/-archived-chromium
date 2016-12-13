# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        '../../<(antlrdir)/antlr.gyp:*',
        '../../<(fcolladadir)/fcollada.gyp:*',
        '../../<(jpegdir)/libjpeg.gyp:*',
        '../../<(pngdir)/libpng.gyp:*',
        '../../<(zlibdir)/zlib.gyp:*',
        '../../base/base.gyp:base',
        '../../breakpad/breakpad.gyp:*',
        '../compiler/technique/technique.gyp:*',
        '../converter/converter.gyp:*',
        '../core/core.gyp:*',
        '../import/import.gyp:*',
        '../tests/tests.gyp:*',
        '../plugin/idl/idl.gyp:*',
        '../plugin/plugin.gyp:*',
        '../serializer/serializer.gyp:*',
        '../utils/utils.gyp:*',
      ],
    },
  ],
}
