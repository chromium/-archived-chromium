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
          '<!@(python ../build/linux/pkg_config_wrapper.py --cflags gtk+-2.0)',
        ],
      },
      'link_settings': {
        'libraries': [
          '<!@(python ../build/linux/pkg_config_wrapper.py --libs gtk+-2.0)',
        ],
      },
    },
    {
      'target_name': 'nss',
      'type': 'settings',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(python ../build/linux/pkg_config_wrapper.py --cflags nss)',
        ],
      },
      'link_settings': {
        'libraries': [
          '<!@(python ../build/linux/pkg_config_wrapper.py --libs nss)',
        ],
      },
    },
    {
      'target_name': 'pangoft2',
      'type': 'settings',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(python ../build/linux/pkg_config_wrapper.py --cflags pangoft2)',
        ],
      },
      'link_settings': {
        'libraries': [
          '<!@(python ../build/linux/pkg_config_wrapper.py --libs pangoft2)',
        ],
      },
    },
  ],
}
