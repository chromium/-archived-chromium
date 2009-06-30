# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'test_shell_windows_resource_files': [
      'resources/test_shell.rc',
      'resources/pan_east.cur',
      'resources/pan_middle.cur',
      'resources/pan_north.cur',
      'resources/pan_north_east.cur',
      'resources/pan_north_west.cur',
      'resources/pan_south.cur',
      'resources/pan_south_east.cur',
      'resources/pan_south_west.cur',
      'resources/pan_west.cur',
      'resources/small.ico',
      'resources/test_shell.ico',
      'resource.h',
    ],
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'test_shell_common',
      'type': '<(library)',
      'dependencies': [
        'npapi_layout_test_plugin',
        '../../../base/base.gyp:base',
        '../../../base/base.gyp:base_gfx',
        '../../../media/media.gyp:media',
        '../../../net/net.gyp:net',
        '../../../skia/skia.gyp:skia',
        '../../../testing/gtest.gyp:gtest',
        '../../../third_party/npapi/npapi.gyp:npapi',
        '../../webkit.gyp:glue',
        '../../webkit.gyp:webkit',
        '../../webkit.gyp:inspector_resources',
      ],
      'msvs_guid': '77C32787-1B96-CB84-B905-7F170629F0AC',
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
        'simple_clipboard_impl.cc',
        'simple_resource_loader_bridge.cc',
        'simple_resource_loader_bridge.h',
        'test_navigation_controller.cc',
        'test_navigation_controller.h',
        'test_shell.cc',
        'test_shell.h',
        'test_shell_gtk.cc',
        'test_shell_x11.cc',
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
        ['OS!="win"', {
          'dependencies': [
            'npapi_test_plugin',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            'test_shell_resources',
            '../../../build/linux/system.gyp:gtk',
            '../../../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
          # for:  test_shell_gtk.cc
          'cflags': ['-Wno-multichar'],
          # See below TODO in the Windows branch.
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/plugins',
              'files': ['<(PRODUCT_DIR)/libnpapi_layout_test_plugin.so'],
            },
          ],
        }, { # else: OS!=linux
          'sources/': [
            ['exclude', '_gtk\\.cc$'],
            ['exclude', '_x11\\.cc$'],
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
          'include_dirs': [
            '.',
          ],
          'dependencies': [
            '../../../breakpad/breakpad.gyp:breakpad_handler',
            '../../default_plugin/default_plugin.gyp:default_plugin',
          ],
          # TODO(bradnelson):  
          # This should really be done in the 'npapi_layout_test_plugin'
          # target, but the current VS generator handles 'copies'
          # settings as AdditionalDependencies, which means that
          # when it's over there, it tries to do the copy *before*
          # the file is built, instead of after.  We work around this
          # by attaching the copy here, since it depends on that
          # target.
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/plugins',
              'files': ['<(PRODUCT_DIR)/npapi_layout_test_plugin.dll'],
            },
          ],
        }, {  # else: OS!=win
          'sources/': [
            ['exclude', '_win\\.cc$'],
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
      'msvs_guid': 'FA39524D-3067-4141-888D-28A86C66F2B9',
      'dependencies': [
        'test_shell_common',
        '../../../third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
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
        ['OS=="win"', {
          'resource_include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit',
          ],
          'sources': [
            '<@(test_shell_windows_resource_files)',
            # TODO:  It would be nice to have these pulled in
            # automatically from direct_dependent_settings in
            # their various targets (net.gyp:net_resources, etc.),
            # but that causes errors in other targets when
            # resulting .res files get referenced multiple times.
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.rc',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../../../build/linux/system.gyp:gtk',
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
                '<(INTERMEDIATE_DIR)/repack/test_shell.pak',
              ],
              'action': ['python', '../../../tools/data_pack/repack.py', '<@(_outputs)', '<@(_inputs)'],
            },
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': ['<(INTERMEDIATE_DIR)/repack/test_shell.pak'],
            },
          ],
        }],
        ['OS=="mac"', {
          'product_name': 'TestShell',
          'dependencies': ['layout_test_helper'],
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
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/TestShell.app/Contents/PlugIns/',
              'files': [
                '<(PRODUCT_DIR)/TestNetscapePlugIn.plugin/',
              ],
            },
          ],
        }, { # OS != "mac"
          'dependencies': [
            '../../../net/net.gyp:net_resources',
            '../../webkit.gyp:webkit_resources',
            '../../webkit.gyp:webkit_strings',
          ]
        }],
      ],
    },
    {
      'target_name': 'npapi_layout_test_plugin',
      'type': 'loadable_module',
      'mac_bundle': 1,
      'product_dir': '<(PRODUCT_DIR)',
      'msvs_guid': 'BE6D5659-A8D5-4890-A42C-090DD10EF62C',
      'sources': [
        '../npapi_layout_test_plugin/PluginObject.cpp',
        '../npapi_layout_test_plugin/TestObject.cpp',
        '../npapi_layout_test_plugin/main.cpp',
        '../npapi_layout_test_plugin/npapi_layout_test_plugin.def',
        '../npapi_layout_test_plugin/npapi_layout_test_plugin.rc',
      ],
      'include_dirs': [
        '../../..',
      ],
      'dependencies': [
        '../../../third_party/npapi/npapi.gyp:npapi',
        '../../webkit.gyp:wtf',
      ],
      'msvs_disabled_warnings': [ 4996 ],
      'mac_bundle_resources': [
        '../npapi_layout_test_plugin/Info.r',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': '../npapi_layout_test_plugin/Info.plist',
      },
      'conditions': [
        ['OS!="win"', {
          'sources!': [
            '../npapi_layout_test_plugin/npapi_layout_test_plugin.def',
            '../npapi_layout_test_plugin/npapi_layout_test_plugin.rc',
          ],
        # TODO(bradnelson):
        # This copy should really live here, as a post-build step,
        # but it's currently being implemented via
        # AdditionalDependencies, which tries to do the copy before
        # the file is built...
        #
        #}, { # OS == "win"
        #  # The old VS build would explicitly copy the .dll into the
        #  # plugins subdirectory like this.  It might be possible to
        #  # use the 'product_dir' setting to build directly into
        #  # plugins/ (as is done on Linux), but we'd need to verify
        #  # that nothing breaks first.
        #  'copies': [
        #    {
        #      'destination': '<(PRODUCT_DIR)/plugins',
        #      'files': ['<(PRODUCT_DIR)/npapi_layout_test_plugin.dll'],
        #    },
        #  ],
        }],
        ['OS=="mac"', {
          'product_name': 'TestNetscapePlugIn',
          'product_extension': 'plugin',
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'npapi_test_plugin',
      'type': 'loadable_module',
      'mac_bundle': 1,
      'product_dir': '<(PRODUCT_DIR)',
      'msvs_guid': '0D04AEC1-6B68-492C-BCCF-808DFD69ABC6',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../third_party/icu38/icu38.gyp:icuuc',
        '../../../third_party/npapi/npapi.gyp:npapi',
      ],
      'sources': [
        '../../glue/plugins/test/npapi_constants.cc',
        '../../glue/plugins/test/npapi_constants.h',
        '../../glue/plugins/test/npapi_test.cc',
        '../../glue/plugins/test/npapi_test.def',
        '../../glue/plugins/test/npapi_test.rc',
        '../../glue/plugins/test/plugin_arguments_test.cc',
        '../../glue/plugins/test/plugin_arguments_test.h',
        '../../glue/plugins/test/plugin_client.cc',
        '../../glue/plugins/test/plugin_client.h',
        '../../glue/plugins/test/plugin_create_instance_in_paint.cc',
        '../../glue/plugins/test/plugin_create_instance_in_paint.h',
        '../../glue/plugins/test/plugin_delete_plugin_in_stream_test.cc',
        '../../glue/plugins/test/plugin_delete_plugin_in_stream_test.h',
        '../../glue/plugins/test/plugin_get_javascript_url_test.cc',
        '../../glue/plugins/test/plugin_get_javascript_url_test.h',
        '../../glue/plugins/test/plugin_geturl_test.cc',
        '../../glue/plugins/test/plugin_geturl_test.h',
        '../../glue/plugins/test/plugin_javascript_open_popup.cc',
        '../../glue/plugins/test/plugin_javascript_open_popup.h',
        '../../glue/plugins/test/plugin_new_fails_test.cc',
        '../../glue/plugins/test/plugin_new_fails_test.h',
        '../../glue/plugins/test/plugin_npobject_lifetime_test.cc',
        '../../glue/plugins/test/plugin_npobject_lifetime_test.h',
        '../../glue/plugins/test/plugin_npobject_proxy_test.cc',
        '../../glue/plugins/test/plugin_windowed_test.cc',
        '../../glue/plugins/test/plugin_windowed_test.h',
        '../../glue/plugins/test/plugin_npobject_proxy_test.h',
        '../../glue/plugins/test/plugin_private_test.cc',
        '../../glue/plugins/test/plugin_private_test.h',
        '../../glue/plugins/test/plugin_test.cc',
        '../../glue/plugins/test/plugin_test.h',
        '../../glue/plugins/test/plugin_window_size_test.cc',
        '../../glue/plugins/test/plugin_window_size_test.h',
        '../../glue/plugins/test/plugin_windowless_test.cc',
        '../../glue/plugins/test/plugin_windowless_test.h',
        '../../glue/plugins/test/resource.h',
      ],
      'include_dirs': [
        '../../..',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': '../../glue/plugins/test/Info.plist',
      },
      'conditions': [
        ['OS!="win"', {
          'sources!': [
            # TODO(port):  Port these.

            # plugin_npobject_lifetime_test.cc has win32-isms
            #   (HWND, CALLBACK).
            '../../glue/plugins/test/plugin_npobject_lifetime_test.cc',

            # The windowed/windowless APIs are necessarily
            # platform-specific.
            '../../glue/plugins/test/plugin_window_size_test.cc',
            '../../glue/plugins/test/plugin_windowed_test.cc',
            '../../glue/plugins/test/plugin_windowless_test.cc',

            # Seems windows specific.
            '../../glue/plugins/test/plugin_create_instance_in_paint.cc',
            '../../glue/plugins/test/plugin_create_instance_in_paint.h',

            # windows-specific resources
            '../../glue/plugins/test/npapi_test.def',
            '../../glue/plugins/test/npapi_test.rc',
          ],
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'test_shell_tests',
      'type': 'executable',
      'msvs_guid': 'E6766F81-1FCD-4CD7-BC16-E36964A14867',
      'dependencies': [
        'test_shell_common',
        '../../../skia/skia.gyp:skia',
        '../../../testing/gtest.gyp:gtest',
      ],
      'sources': [
        '../../../skia/ext/convolver_unittest.cc',
        '../../../skia/ext/image_operations_unittest.cc',
        '../../../skia/ext/skia_utils_unittest.cc',
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
        '../../glue/media/media_resource_loader_bridge_factory_unittest.cc',
        '../../glue/mimetype_unittest.cc',
        '../../glue/multipart_response_delegate_unittest.cc',
        '../../glue/password_autocomplete_listener_unittest.cc',
        '../../glue/regular_expression_unittest.cc',
        '../../glue/resource_fetcher_unittest.cc',
        '../../glue/unittest_test_server.h',
        '../../glue/webcursor_unittest.cc',
        '../../glue/webframe_unittest.cc',
        '../../glue/webplugin_impl_unittest.cc',
        '../webcore_unit_tests/BMPImageDecoder_unittest.cpp',
        '../webcore_unit_tests/GKURL_unittest.cpp',
        '../webcore_unit_tests/ICOImageDecoder_unittest.cpp',
        '../webcore_unit_tests/UniscribeHelper_unittest.cpp',
        '../webcore_unit_tests/XBMImageDecoder_unittest.cpp',
        '../webcore_unit_tests/TransparencyWin_unittest.cpp',
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
          'resource_include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit',
          ],
          'sources': [ '<@(test_shell_windows_resource_files)' ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            # Linux tests use the built test_shell beside the test
            'test_shell',
            '../../../build/linux/system.gyp:gtk',
          ],
          'sources!': [
             # TODO(port)
            '../../../skia/ext/platform_canvas_unittest.cc',
          ],
        }],
        ['OS=="mac"', {
          # mac tests load the resources from the built test_shell beside the
          # test
          'dependencies': ['test_shell'],
          # TODO(port)
          # disable plugin tests until we re-enable plugins for chromium.app
          'sources!': [
            'plugin_tests.cc',

            # Disable the image decoder tests because we use CoreGraphics
            # code on mac and these tests are for the Skia image-decoders.
            '../webcore_unit_tests/BMPImageDecoder_unittest.cpp',
            '../webcore_unit_tests/ICOImageDecoder_unittest.cpp',
            '../webcore_unit_tests/XBMImageDecoder_unittest.cpp',
            'image_decoder_unittest.cc',
            'image_decoder_unittest.h',
          ],
          'sources': [
            '../../../skia/ext/skia_utils_mac_unittest.mm',
          ],
        }],
        ['OS=="win"', {
          'msvs_disabled_warnings': [ 4800 ],
        }, {  # else: OS!=win
          'sources!': [
            '../../../skia/ext/image_operations_unittest.cc',
            '../../../skia/ext/skia_utils_unittest.cc',
            '../../../skia/ext/vector_canvas_unittest.cc',
            '../webcore_unit_tests/UniscribeHelper_unittest.cpp',
            '../webcore_unit_tests/TransparencyWin_unittest.cpp',
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
          'actions': [
            {
              'action_name': 'test_shell_resources',
              'variables': {
                'grit_path': '../../../tools/grit/grit.py',
                'input_path': './test_shell_resources.grd',
                'out_dir': '<(SHARED_INTERMEDIATE_DIR)/test_shell',
              },
              'inputs': [
                '<(input_path)',
              ],
              'outputs': [
                '<(out_dir)/grit/test_shell_resources.h',
                '<(out_dir)/test_shell_resources.pak',
              ],
              'action': ['python', '<(grit_path)', '-i', '<(input_path)', 'build', '-o', '<(out_dir)'],
              'message': 'Generating resources from <(input_path)',
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
    # Need to do the same for Linux. We build a separate dylib/dll/so
    # so V8 can have a second set of global variables and run workers.
    # Normally, workers run in a separate process.
    ['OS!="linux"', {
      'targets': [
        {
          'target_name': 'test_worker',
          'type': 'shared_library',
          'xcode_settings': {
             'EXPORTED_SYMBOLS_FILE': 'test_worker/test_worker.exp',
          },
          'msvs_guid': '3E03D462-780D-4C4D-B22E-5E095E6CF110',
          'msvs_disabled_warnings': [4800],
          'dependencies': [
            '../../../base/base.gyp:base',
            '../../../base/base.gyp:base_gfx',
            '../../../net/net.gyp:net',
            '../../../skia/skia.gyp:skia',
            '../../../testing/gtest.gyp:gtest',
            '../../../third_party/npapi/npapi.gyp:npapi',
            '../../webkit.gyp:glue',
            '../../webkit.gyp:webkit',
            # For simple_clipboard_impl.cc
            'test_shell_common',
          ],
          'sources': [
            'test_worker/test_webworker.cc',
            'test_worker/test_webworker.h',
            'test_worker/test_worker_main.cc',
            'test_worker/test_worker.def',
          ],
          'conditions': [
            ['OS!="win"', {
              'sources!' : ['test_worker/test_worker.def'],
            }],
          ],
        },
      ],
    }],
    ['OS=="mac"', {
      'targets': [
        {
          # Helper application that manages the color sync profile on mac
          # for the test shells run by the layout tests.
          'target_name': 'layout_test_helper',
          'type': 'executable',
          'sources': [
            'mac/layout_test_helper.mm',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
            ],
          },
        },
      ]
    }],
  ],
}
