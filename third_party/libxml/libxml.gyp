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
    'use_system_libxml%': 0,
  },
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'libxml',
      'conditions': [
        ['OS=="linux" and use_system_libxml', {
          'type': 'settings',
          'direct_dependent_settings': {
            'cflags': [
              '<!@(python ../../build/linux/pkg_config_wrapper.py --cflags libxml-2.0)',
            ],
          },
          'link_settings': {
            'libraries': [
              '<!@(python ../../build/linux/pkg_config_wrapper.py --libs libxml-2.0)',
            ],
          },
        }, { # else: OS != "linux" or ! use_system_libxml
          'type': '<(library)',
          'msvs_guid': 'F9810DE8-CBC3-4605-A7B1-ECA2D5292FD7',
          'sources': [
            'include/libxml/c14n.h',
            'include/libxml/catalog.h',
            'include/libxml/chvalid.h',
            'include/libxml/debugXML.h',
            'include/libxml/dict.h',
            'include/libxml/DOCBparser.h',
            'include/libxml/encoding.h',
            'include/libxml/entities.h',
            'include/libxml/globals.h',
            'include/libxml/hash.h',
            'include/libxml/HTMLparser.h',
            'include/libxml/HTMLtree.h',
            'include/libxml/list.h',
            'include/libxml/nanoftp.h',
            'include/libxml/nanohttp.h',
            'include/libxml/parser.h',
            'include/libxml/parserInternals.h',
            'include/libxml/pattern.h',
            'include/libxml/relaxng.h',
            'include/libxml/SAX.h',
            'include/libxml/SAX2.h',
            'include/libxml/schemasInternals.h',
            'include/libxml/schematron.h',
            'include/libxml/threads.h',
            'include/libxml/tree.h',
            'include/libxml/uri.h',
            'include/libxml/valid.h',
            'include/libxml/xinclude.h',
            'include/libxml/xlink.h',
            'include/libxml/xmlautomata.h',
            'include/libxml/xmlerror.h',
            'include/libxml/xmlexports.h',
            'include/libxml/xmlIO.h',
            'include/libxml/xmlmemory.h',
            'include/libxml/xmlmodule.h',
            'include/libxml/xmlreader.h',
            'include/libxml/xmlregexp.h',
            'include/libxml/xmlsave.h',
            'include/libxml/xmlschemas.h',
            'include/libxml/xmlschemastypes.h',
            'include/libxml/xmlstring.h',
            'include/libxml/xmlunicode.h',
            'include/libxml/xmlwriter.h',
            'include/libxml/xpath.h',
            'include/libxml/xpathInternals.h',
            'include/libxml/xpointer.h',
            'include/win32config.h',
            'include/wsockcompat.h',
            'linux/config.h',
            'linux/include/libxml/xmlversion.h',
            'mac/config.h',
            'mac/include/libxml/xmlversion.h',
            'win32/config.h',
            'win32/include/libxml/xmlversion.h',
            'acconfig.h',
            'c14n.c',
            'catalog.c',
            'chvalid.c',
            'debugXML.c',
            'dict.c',
            'DOCBparser.c',
            'elfgcchack.h',
            'encoding.c',
            'entities.c',
            'error.c',
            'globals.c',
            'hash.c',
            'HTMLparser.c',
            'HTMLtree.c',
            'legacy.c',
            'libxml.h',
            'list.c',
            'nanoftp.c',
            'nanohttp.c',
            'parser.c',
            'parserInternals.c',
            'pattern.c',
            'relaxng.c',
            'SAX.c',
            'SAX2.c',
            'schematron.c',
            'threads.c',
            'tree.c',
            #'trio.c',
            #'trio.h',
            #'triodef.h',
            #'trionan.c',
            #'trionan.h',
            #'triop.h',
            #'triostr.c',
            #'triostr.h',
            'uri.c',
            'valid.c',
            'xinclude.c',
            'xlink.c',
            'xmlIO.c',
            'xmlmemory.c',
            'xmlmodule.c',
            'xmlreader.c',
            'xmlregexp.c',
            'xmlsave.c',
            'xmlschemas.c',
            'xmlschemastypes.c',
            'xmlstring.c',
            'xmlunicode.c',
            'xmlwriter.c',
            'xpath.c',
            'xpointer.c',
          ],
          'defines': [
            'LIBXML_STATIC',
          ],
          'include_dirs': [
            '<(os_include)',
            '<(os_include)/include',
            'include',
          ],
          'dependencies': [
            '../icu38/icu38.gyp:icuuc',
            '../zlib/zlib.gyp:zlib',
          ],
          'export_dependent_settings': [
            '../icu38/icu38.gyp:icuuc',
          ],
          'direct_dependent_settings': {
            'defines': [
              'LIBXML_STATIC',
            ],
            'include_dirs': [
              '<(os_include)/include',
              'include',
            ],
          },
          'conditions': [
            ['OS=="mac"', {'defines': ['_REENTRANT']}],
            ['OS=="win"', {
              'product_name': 'libxml2',
            }, {  # else: OS!="win"
              'product_name': 'xml2',
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'xmlcatalog',
      'type': 'executable',
      'sources': [
        'xmlcatalog.c',
      ],
      'include_dirs': [
        '<(os_include)',
      ],
      'dependencies': [
        'libxml',
      ],
      'conditions': [
        ['OS=="linux"', {
          'link_settings': {
            'libraries': [
              '-ldl',
              '-lm',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'xmllint',
      'type': 'executable',
      'sources': [
        'xmllint.c',
      ],
      'include_dirs': [
        '<(os_include)',
      ],
      'dependencies': [
        'libxml',
        '../icu38/icu38.gyp:icuuc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'link_settings': {
            'libraries': [
              '-ldl',
              '-lm',
            ],
          },
        }],
      ],
    },
  ],
}
