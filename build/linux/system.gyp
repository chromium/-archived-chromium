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
          '<!@(pkg-config --cflags gtk+-2.0)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(pkg-config --libs-only-L --libs-only-other gtk+-2.0)',
        ],
        'libraries': [
          '<!@(pkg-config --libs-only-l gtk+-2.0)',
        ],
      },
    },
    {
      'target_name': 'nss',
      'type': 'settings',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(pkg-config --cflags nss)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(pkg-config --libs-only-L --libs-only-other nss)',
        ],
        'libraries': [
          '<!@(pkg-config --libs-only-l nss)',
        ],
      },
    },
    {
      'target_name': 'freetype2',
      'type': 'settings',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(pkg-config --cflags freetype2)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(pkg-config --libs-only-L --libs-only-other freetype2)',
        ],
        'libraries': [
          '<!@(pkg-config --libs-only-l freetype2)',
        ],
      },
    },
    {
      'target_name': 'fontconfig',
      'type': 'settings',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(pkg-config --cflags fontconfig)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(pkg-config --libs-only-L --libs-only-other fontconfig)',
        ],
        'libraries': [
          '<!@(pkg-config --libs-only-l fontconfig)',
        ],
      },
    },
    {
      'target_name': 'gdk',
      'type': 'settings',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(pkg-config --cflags gdk-2.0)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(pkg-config --libs-only-L --libs-only-other gdk-2.0)',
        ],
        'libraries': [
          '<!@(pkg-config --libs-only-l gdk-2.0)',
        ],
      },
    },
    {
      'target_name': 'gconf',
      'type': 'settings',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(pkg-config --cflags gconf-2.0)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(pkg-config --libs-only-L --libs-only-other gconf-2.0)',
        ],
        'libraries': [
          '<!@(pkg-config --libs-only-l gconf-2.0)',
        ],
      },
    },
    {
      'target_name': 'gthread',
      'type': 'settings',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(pkg-config --cflags gthread-2.0)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(pkg-config --libs-only-L --libs-only-other gthread-2.0)',
        ],
        'libraries': [
          '<!@(pkg-config --libs-only-l gthread-2.0)',
        ],
      },
    },
    {
      'target_name': 'x11',
      'type': 'settings',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(pkg-config --cflags x11)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(pkg-config --libs-only-L --libs-only-other x11)',
        ],
        'libraries': [
          '<!@(pkg-config --libs-only-l x11)',
        ],
      },
    },
# TODO(evanm): temporarily disabled while we figure out whether to depend
# on gnome-keyring etc.
# http://code.google.com/p/chromium/issues/detail?id=12351
#     {
#       'target_name': 'gnome-keyring',
#       'type': 'settings',
#       'direct_dependent_settings': {
#         'cflags': [
#           '<!@(pkg-config --cflags gnome-keyring-1)',
#         ],
#       },
#       'link_settings': {
#         'ldflags': [
#           '<!@(pkg-config --libs-only-L --libs-only-other gnome-keyring-1)',
#         ],
#         'libraries': [
#           '<!@(pkg-config --libs-only-l gnome-keyring-1)',
#         ],
#       },
#     },
#     {
#       'target_name': 'dbus-glib',
#       'type': 'settings',
#       'direct_dependent_settings': {
#         'cflags': [
#           '<!@(pkg-config --cflags dbus-glib-1)',
#         ],
#       },
#       'link_settings': {
#         'ldflags': [
#           '<!@(pkg-config --libs-only-L --libs-only-other dbus-glib-1)',
#         ],
#         'libraries': [
#           '<!@(pkg-config --libs-only-l dbus-glib-1)',
#         ],
#       },
#     },
  ],
}
