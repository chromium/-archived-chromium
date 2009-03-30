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
        'mock_webclipboard_impl.cc',
        'mock_webclipboard_impl.h',
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
        'test_shell_webkit_init.h',
        'test_webview_delegate.cc',
        'test_webview_delegate.h',
        'test_webview_delegate_gtk.cc',
        'test_webview_delegate_win.cc',
        'test_webworker_helper.cc',
        'test_webworker_helper.h',
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
        ['OS=="linux"', {
          'dependencies': [
            'test_shell_resources',
            'npapi_layout_test_plugin',
            'npapi_test_plugin',
            '../../../build/linux/system.gyp:gtk',
          ],
          # for:  test_shell_gtk.cc
          'cflags': ['-Wno-multichar'],
        }, { # else: OS!=linux
          'sources/': [
            ['exclude', '_gtk\\.cc$']
          ],
        }],
        ['OS!="mac"', {
          'sources/': [
            ['exclude', 'mac/[^/]*\\.(cc|mm?)$'],
            ['exclude', '_mac\\.(cc|mm?)$'],
          ]
        }],
        ['OS=="win"', {
          'msvs_disabled_warnings': [ 4800 ],
          'link_settings': {
            'libraries': [
              '-lcomctl32.lib',
            ],
          },
          'dependencies': [
            '../../../breakpad/breakpad.gyp:breakpad_handler',
            '../../default_plugin/default_plugin.gyp:default_plugin',
          ],
        }, {  # else: OS!=win
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
      'type': 'executable',
      'mac_bundle': 1,
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
        ['OS=="linux"', {
          'dependencies': [
            '../../../build/linux/system.gyp:gtk',
            '../../../net/net.gyp:net_resources',
            '../../webkit.gyp:glue', # for webkit_{resources,strings_en-US}.pak
            'test_shell_resources',
          ],
          'actions': [
            {
              'action_name': 'test_shell_repack',
              'inputs': [
                '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
                '<(SHARED_INTERMEDIATE_DIR)/test_shell/test_shell_resources.pak',
                '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.pak',
                '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.pak',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/test_shell.pak',
              ],
              'action': ['python', '../../../tools/data_pack/repack.py', '<@(_outputs)', '<@(_inputs)'],
            },
          ],
          'scons_depends': [
            ['<(PRODUCT_DIR)/test_shell'],
            ['<(PRODUCT_DIR)/test_shell.pak'],
          ],
        }],
        ['OS=="mac"', {
          'product_name': 'TestShell',
          'variables': {
            'repack_path': '../../../tools/data_pack/repack.py',
          },
          'actions': [
            {
              # TODO(mark): Make this work with more languages than the
              # hardcoded en-US.
              'action_name': 'repack_locale',
              'variables': {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.pak',
                ],
              },
              'inputs': [
                '<(repack_path)',
                '<@(pak_inputs)',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/repack/test_shell.pak',
              ],
              'action': ['python', '<(repack_path)', '<@(_outputs)', '<@(pak_inputs)'],
              'process_outputs_as_mac_bundle_resources': 1,
            },
          ]
        }],
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
        '../../glue/devtools/dom_agent_unittest.cc',
        '../../glue/devtools/devtools_rpc_unittest.cc',
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
        ['OS=="linux"', {
          'dependencies': [
            '../../../build/linux/system.gyp:gtk',
          ],
          'sources!': [
             # TODO(port)
            '../../../skia/ext/platform_canvas_unittest.cc',
            '../../glue/webplugin_impl_unittest.cc',
          ],
        }],
        ['OS=="mac"', {
          # mac tests load the resources from the built test_shell beside the
          # test
          'dependencies': ['test_shell'],
        }],
        ['OS=="win"', {
          'msvs_disabled_warnings': [ 4800 ],
        }, {  # else: OS!=win
          'sources!': [
            '../../../skia/ext/vector_canvas_unittest.cc',
            '../webcore_unit_tests/UniscribeHelper_unittest.cpp',
            'plugin_tests.cc'
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="linux"', {
      'targets': [
        {
          'target_name': 'test_shell_resources',
          'type': 'none',
          'sources': [
            'test_shell_resources.grd',
          ],
          # This was orignally in grit_resources.rules
          # NOTE: this version doesn't mimic the Properties specified there.
          'rules': [
            {
              'rule_name': 'grit',
              'extension': 'grd',
              'inputs': [
                '../../../tools/grit/grit.py',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/test_shell/grit/<(RULE_INPUT_ROOT).h',
                '<(SHARED_INTERMEDIATE_DIR)/test_shell/<(RULE_INPUT_ROOT).pak',
              ],
              'action':
                ['python', '<@(_inputs)', '-i', '<(RULE_INPUT_PATH)', 'build', '-o', '<(SHARED_INTERMEDIATE_DIR)/test_shell'],
              'message': 'Generating resources from <(RULE_INPUT_PATH)',
            },
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/test_shell',
            ],
          },
        },
      ],
    }],
    # TODO:  change this condition to 'OS!="mac"'
    # when Windows is ready for the plugins, too.
    ['OS=="linux"', {
      'targets': [
        {
          'target_name': 'npapi_test_plugin',
          'type': 'loadable_module',
          'product_dir': '<(PRODUCT_DIR)/plugins',
          'dependencies': [
            '../../../base/base.gyp:base',
            '../../../third_party/icu38/icu38.gyp:icuuc',
            '../../../third_party/npapi/npapi.gyp:npapi',
          ],
          'sources': [
            '../../glue/plugins/test/npapi_constants.cc',
            '../../glue/plugins/test/npapi_test.cc',
            '../../glue/plugins/test/plugin_arguments_test.cc',
            '../../glue/plugins/test/plugin_client.cc',
            '../../glue/plugins/test/plugin_delete_plugin_in_stream_test.cc',
            '../../glue/plugins/test/plugin_execute_script_delete_test.cc',
            '../../glue/plugins/test/plugin_get_javascript_url_test.cc',
            '../../glue/plugins/test/plugin_geturl_test.cc',
            '../../glue/plugins/test/plugin_javascript_open_popup.cc',
            '../../glue/plugins/test/plugin_new_fails_test.cc',
            '../../glue/plugins/test/plugin_npobject_lifetime_test.cc',
            '../../glue/plugins/test/plugin_npobject_proxy_test.cc',
            '../../glue/plugins/test/plugin_test.cc',
            '../../glue/plugins/test/plugin_window_size_test.cc',
          ],
          'include_dirs': [
            '../../..',
          ],
          'conditions': [
            ['OS=="linux"', {
              'sources!': [
                # TODO(port):  Port these.
                # plugin_client.cc includes not ported headers.
                # plugin_npobject_lifetime_test.cc has win32-isms
                #   (HWND, CALLBACK).
                # plugin_window_size_test.cc has w32-isms including HWND.
                '../../glue/plugins/test/plugin_execute_script_delete_test.cc',
                '../../glue/plugins/test/plugin_javascript_open_popup.cc',
                '../../glue/plugins/test/plugin_client.cc',
                '../../glue/plugins/test/plugin_npobject_lifetime_test.cc',
                '../../glue/plugins/test/plugin_window_size_test.cc',
              ],
            }],
          ],
        },
        {
          'target_name': 'npapi_layout_test_plugin',
          'type': 'loadable_module',
          'product_dir': '<(PRODUCT_DIR)/plugins',
          'sources': [
            '../npapi_layout_test_plugin/main.cpp',
            '../npapi_layout_test_plugin/PluginObject.cpp',
            '../npapi_layout_test_plugin/TestObject.cpp',
          ],
          'include_dirs': [
            '../../..',
          ],
          'dependencies': [
            '../../../third_party/npapi/npapi.gyp:npapi',
            '../../webkit.gyp:wtf',
          ],
        },
      ],
    }],
  ],
}
