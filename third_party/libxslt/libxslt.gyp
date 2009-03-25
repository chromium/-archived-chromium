# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['OS=="linux"', {'os_include': 'linux'}],
      ['OS=="mac"', {'os_include': 'mac'}],
      ['OS=="win"', {'os_include': 'win32'}],
    ],
    'use_system_libxslt%': 0,
  },
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'libxslt',
      'conditions': [
        ['OS=="linux" and use_system_libxslt', {
          'type': 'settings',
          'direct_dependent_settings': {
            'cflags': [
              '<!@(python ../../build/linux/pkg_config_wrapper.py --cflags libxslt)',
            ],
          },
          'link_settings': {
            'libraries': [
              '<!@(python ../../build/linux/pkg_config_wrapper.py --libs libxslt)',
            ],
          },
        }, { # else: OS != "linux" or ! use_system_libxslt
          'type': 'static_library',
          'msvs_guid': 'F9810DE8-CBC3-4605-A7B1-ECA2D5292FD7',
          'sources': [
            'libxslt/attributes.c',
            'libxslt/attributes.h',
            'libxslt/attrvt.c',
            'libxslt/documents.c',
            'libxslt/documents.h',
            'libxslt/extensions.c',
            'libxslt/extensions.h',
            'libxslt/extra.c',
            'libxslt/extra.h',
            'libxslt/functions.c',
            'libxslt/functions.h',
            'libxslt/imports.c',
            'libxslt/imports.h',
            'libxslt/keys.c',
            'libxslt/keys.h',
            'libxslt/libxslt.h',
            'libxslt/namespaces.c',
            'libxslt/namespaces.h',
            'libxslt/numbers.c',
            'libxslt/numbersInternals.h',
            'libxslt/pattern.c',
            'libxslt/pattern.h',
            'libxslt/preproc.c',
            'libxslt/preproc.h',
            'libxslt/security.c',
            'libxslt/security.h',
            'libxslt/templates.c',
            'libxslt/templates.h',
            'libxslt/transform.c',
            'libxslt/transform.h',
            'libxslt/trio.h',
            'libxslt/triodef.h',
            'libxslt/variables.c',
            'libxslt/variables.h',
            'libxslt/win32config.h',
            'libxslt/xslt.c',
            'libxslt/xslt.h',
            'libxslt/xsltconfig.h',
            'libxslt/xsltexports.h',
            'libxslt/xsltInternals.h',
            'libxslt/xsltutils.c',
            'libxslt/xsltutils.h',
            'libxslt/xsltwin32config.h',
            'linux/config.h',
            'mac/config.h',
            # TODO(port): Need a pregenerated win32/config.h?
          ],
          'defines': [
            'LIBXSLT_STATIC',
          ],
          'include_dirs': [
            '<(os_include)',
            '.',
          ],
          'dependencies': [
            '../libxml/libxml.gyp:libxml',
          ],
          'direct_dependent_settings': {
            'defines': [
              'LIBXSLT_STATIC',
            ],
            'include_dirs': [
              '.',
            ],
          },
          'conditions': [
            ['OS!="win"', {'product_name': 'xslt'}],
          ],
        }],
      ],
    },
  ],
}
