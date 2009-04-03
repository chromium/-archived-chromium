# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'gtk',
      'type': 'settings',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(python pkg_config_wrapper.py --cflags gtk+-2.0)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(python pkg_config_wrapper.py --libs-only-L --libs-only-other gtk+-2.0)',
        ],
        'libraries': [
          '<!@(python pkg_config_wrapper.py --libs-only-l gtk+-2.0)',
        ],
      },
    },
    {
      'target_name': 'nss',
      'type': 'settings',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(python pkg_config_wrapper.py --cflags nss)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(python pkg_config_wrapper.py --libs-only-L --libs-only-other nss)',
        ],
        'libraries': [
          '<!@(python pkg_config_wrapper.py --libs-only-l nss)',
        ],
      },
    },
    {
      'target_name': 'freetype2',
      'type': 'settings',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(python pkg_config_wrapper.py --cflags freetype2)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(python pkg_config_wrapper.py --libs-only-L --libs-only-other freetype2)',
        ],
        'libraries': [
          '<!@(python pkg_config_wrapper.py --libs-only-l freetype2)',
        ],
      },
    },
    {
      'target_name': 'fontconfig',
      'type': 'settings',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(python pkg_config_wrapper.py --cflags fontconfig)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(python pkg_config_wrapper.py --libs-only-L --libs-only-other fontconfig)',
        ],
        'libraries': [
          '<!@(python pkg_config_wrapper.py --libs-only-l fontconfig)',
        ],
      },
    },
    {
      'target_name': 'gdk',
      'type': 'settings',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(python pkg_config_wrapper.py --cflags gdk-2.0)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(python pkg_config_wrapper.py --libs-only-L --libs-only-other gdk-2.0)',
        ],
        'libraries': [
          '<!@(python pkg_config_wrapper.py --libs-only-l gdk-2.0)',
        ],
      },
    },
  ],
}
