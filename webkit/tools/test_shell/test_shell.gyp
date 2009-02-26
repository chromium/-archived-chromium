# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'test_shell_common',
      'type': 'static_library',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../base/base.gyp:base_gfx',
        '../../../net/net.gyp:net',
        '../../../skia/skia.gyp:skia',
        '../../../testing/gtest.gyp:gtest',
        '../../../third_party/npapi/npapi.gyp:npapi',
        '../../webkit.gyp:glue',
        '../../webkit.gyp:webkit',
      ],
      'sources': [
        'mac/DumpRenderTreePasteboard.h',
        'mac/DumpRenderTreePasteboard.m',
        'mac/KeystoneGlue.h',
        'mac/KeystoneGlue.m',
        'mac/test_shell_webview.h',
        'mac/test_shell_webview.mm',
        'mac/test_webview_delegate.mm',
        'mac/webview_host.mm',
        'mac/webwidget_host.mm',
        'drag_delegate.cc',
        'drag_delegate.h',
        'drop_delegate.cc',
        'drop_delegate.h',
        'event_sending_controller.cc',
        'event_sending_controller.h',
        'foreground_helper.h',
        'layout_test_controller.cc',
        'layout_test_controller.h',
        'resource.h',
        'simple_resource_loader_bridge.cc',
        'simple_resource_loader_bridge.h',
        'test_navigation_controller.cc',
        'test_navigation_controller.h',
        'test_shell.cc',
        'test_shell.h',
        'test_shell_gtk.cc',
        'test_shell_mac.mm',
        'test_shell_platform_delegate.h',
        'test_shell_platform_delegate_gtk.cc',
        'test_shell_platform_delegate_mac.mm',
        'test_shell_platform_delegate_win.cc',
        'test_shell_request_context.cc',
        'test_shell_request_context.h',
        'test_shell_switches.cc',
        'test_shell_switches.h',
        'test_shell_win.cc',
        'test_webview_delegate.cc',
        'test_webview_delegate.h',
        'test_webview_delegate_gtk.cc',
        'test_webview_delegate_win.cc',
        'text_input_controller.cc',
        'text_input_controller.h',
        'webview_host.h',
        'webview_host_gtk.cc',
        'webview_host_win.cc',
        'webwidget_host.h',
        'webwidget_host_gtk.cc',
        'webwidget_host_win.cc',
      ],
      'export_dependent_settings': [
        '../../../base/base.gyp:base',
        '../../../net/net.gyp:net',
        '../../webkit.gyp:glue',
        '../../webkit.gyp:webkit',
      ],
      'conditions': [
        ['OS!="linux"', {'sources/': [['exclude', '_gtk\\.cc$']]}],
        ['OS!="mac"', {
          'sources/': [
            ['exclude', 'mac/[^/]*\\.(cc|mm?)$'],
            ['exclude', '_mac\\.(cc|mm?)$'],
          ]
        }],
        ['OS=="win"', {
          'include_dirs': [
            '../../../breakpad/src',
          ],
          'msvs_disabled_warnings': [ 4800 ],
        }, {  # OS!=win
          'sources/': [
            ['exclude', '_win\\.cc$']
          ],
          'sources!': [
            'drag_delegate.cc',
            'drop_delegate.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'test_shell',
      'type': 'application',
      'dependencies': [
        'test_shell_common',
      ],
      'sources': [
        'test_shell_main.cc',
      ],
      'mac_bundle_resources': [
        '../../data/test_shell/',
        'mac/English.lproj/InfoPlist.strings',
        'mac/English.lproj/MainMenu.nib',
        'mac/Info.plist',
        'mac/test_shell.icns',
        'resources/AHEM____.TTF',
      ],
      'mac_bundle_resources!': [
        # TODO(mark): Come up with a fancier way to do this (mac_info_plist?)
        # that automatically sets the correct INFOPLIST_FILE setting and adds
        # the file to a source group.
        'mac/Info.plist',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'mac/Info.plist',
      },
      'conditions': [
        ['OS=="mac"', {'product_name': 'TestShell'}],
      ],
    },
    {
      'target_name': 'test_shell_tests',
      'type': 'executable',
      'dependencies': [
        'test_shell_common',
        '../../../skia/skia.gyp:skia',
        '../../../testing/gtest.gyp:gtest',
      ],
      'sources': [
        '../../../skia/ext/convolver_unittest.cc',
        '../../../skia/ext/platform_canvas_unittest.cc',
        '../../../skia/ext/vector_canvas_unittest.cc',
        '../../glue/bookmarklet_unittest.cc',
        '../../glue/context_menu_unittest.cc',
        '../../glue/cpp_bound_class_unittest.cc',
        '../../glue/cpp_variant_unittest.cc',
        '../../glue/dom_operations_unittest.cc',
        '../../glue/dom_serializer_unittest.cc',
        '../../glue/glue_serialize_unittest.cc',
        '../../glue/iframe_redirect_unittest.cc',
        '../../glue/mimetype_unittest.cc',
        '../../glue/multipart_response_delegate_unittest.cc',
        '../../glue/password_autocomplete_listener_unittest.cc',
        '../../glue/regular_expression_unittest.cc',
        '../../glue/resource_fetcher_unittest.cc',
        '../../glue/unittest_test_server.h',
        '../../glue/webframe_unittest.cc',
        '../../glue/webplugin_impl_unittest.cc',
        '../webcore_unit_tests/BMPImageDecoder_unittest.cpp',
        '../webcore_unit_tests/GKURL_unittest.cpp',
        '../webcore_unit_tests/ICOImageDecoder_unittest.cpp',
        '../webcore_unit_tests/UniscribeHelper_unittest.cpp',
        '../webcore_unit_tests/XBMImageDecoder_unittest.cpp',
        'image_decoder_unittest.cc',
        'image_decoder_unittest.h',
        'keyboard_unittest.cc',
        'layout_test_controller_unittest.cc',
        'node_leak_test.cc',
        'plugin_tests.cc',
        'run_all_tests.cc',
        'test_shell_test.cc',
        'test_shell_test.h',
        'text_input_controller_unittest.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_disabled_warnings': [ 4800 ],
        }, {  # OS!=win
          'sources!': [
            '../../../skia/ext/vector_canvas_unittest.cc',
            '../webcore_unit_tests/UniscribeHelper_unittest.cpp',
            'plugin_tests.cc'
          ],
        }],
      ],
    },
  ],
}
