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
        'libraries': [
          '<!@(python pkg_config_wrapper.py --libs gtk+-2.0)',
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
        'libraries': [
          '<!@(python pkg_config_wrapper.py --libs nss)',
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
        'libraries': [
          '<!@(python pkg_config_wrapper.py --libs freetype2)',
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
        'libraries': [
          '<!@(python pkg_config_wrapper.py --libs fontconfig)',
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
        'libraries': [
          '<!@(python pkg_config_wrapper.py --libs gdk-2.0)',
        ],
      },
    },
  ],
}
