{
  'variables': {
    'version_py': '../../chrome/tools/build/version.py',
    'version_path': '../../chrome/VERSION',
    'lastchange_path': '<(SHARED_INTERMEDIATE_DIR)/build/LASTCHANGE',
    # 'branding_dir' is set in the 'conditions' section at the bottom.
  },
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'installer_util',
      'conditions': [
        ['OS=="linux"', {
          'type': 'none',
          # Add these files to the build output so the build archives will be
          # "hermetic" for packaging. This is only for branding="Chrome" since
          # we only create packages for official builds.
          'conditions': [
            ['branding=="Chrome"', {
              'variables': {
                'lib32_dir': '<!(if uname -m | egrep -q "x86_64"; then echo lib32; else echo lib; fi)',
              },
              'copies': [
                # Copy tools for generating packages from the build archive.
                {
                  'destination': '<(PRODUCT_DIR)/installer/',
                  'files': [
                    'linux/internal/build_from_archive.sh',
                  ]
                },
                {
                  'destination': '<(PRODUCT_DIR)/installer/debian/',
                  'files': [
                    'linux/internal/debian/changelog.template',
                    'linux/internal/debian/postinst',
                    'linux/internal/debian/postrm',
                    'linux/internal/debian/prerm',
                    'linux/internal/debian/build.sh',
                    'linux/internal/debian/control.template',
                  ]
                },
                {
                  'destination': '<(PRODUCT_DIR)/installer/common/',
                  'files': [
                    'linux/internal/common/apt.include',
                    'linux/internal/common/repo.cron',
                    'linux/internal/common/wrapper',
                    'linux/internal/common/updater',
                    'linux/internal/common/desktop.template',
                    'linux/internal/common/google-chrome/google-chrome.info',
                  ]
                },
                # System libs needed for 64-bit package building.
                {
                  'destination': '<(PRODUCT_DIR)/installer/lib32/',
                  'files': [
                    '/usr/<(lib32_dir)/libsqlite3.so.0',
                    '/usr/<(lib32_dir)/libsqlite3.so.0.8.6',
                    '/usr/<(lib32_dir)/libnspr4.so.0d',
                    '/usr/<(lib32_dir)/libplds4.so.0d',
                    '/usr/<(lib32_dir)/libplc4.so.0d',
                    '/usr/<(lib32_dir)/libssl3.so.1d',
                    '/usr/<(lib32_dir)/libnss3.so.1d',
                    '/usr/<(lib32_dir)/libsmime3.so.1d',
                    '/usr/<(lib32_dir)/libnssutil3.so.1d',
                    '/usr/<(lib32_dir)/nss/libfreebl3.so',
                    '/usr/<(lib32_dir)/nss/libsoftokn3.chk',
                    '/usr/<(lib32_dir)/nss/libsoftokn3.so',
                    '/usr/<(lib32_dir)/nss/libnssckbi.so',
                    '/usr/<(lib32_dir)/nss/libnssdbm3.so',
                    '/usr/<(lib32_dir)/nss/libfreebl3.chk',
                  ],
                },
                # Additional theme resources needed for package building.
                {
                  'destination': '<(PRODUCT_DIR)/installer/theme/',
                  'files': [
                    '<(branding_dir)/product_logo_16.png',
                    '<(branding_dir)/product_logo_32.png',
                    '<(branding_dir)/product_logo_48.png',
                    '<(branding_dir)/product_logo_256.png',
                    '<(branding_dir)/BRANDING',
                  ],
                },
              ],
              'actions': [
                {
                  'action_name': 'save_build_info',
                  'inputs': [
                    '<(branding_dir)/BRANDING',
                    '<(version_path)',
                    '<(lastchange_path)',
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/installer/version.txt',
                  ],
                  # Just output the default version info variables.
                  'action': [
                    'python', '<(version_py)',
                    '-f', '<(branding_dir)/BRANDING',
                    '-f', '<(version_path)',
                    '-f', '<(lastchange_path)',
                    '-o', '<@(_outputs)'
                  ],
                },
              ],
            }],
          ],
        }],
        ['OS=="win"', {
          'type': '<(library)',
          'msvs_guid': 'EFBB1436-A63F-4CD8-9E99-B89226E782EC',
          'dependencies': [
            'installer_util_strings',
            '../chrome.gyp:common',
            '../chrome.gyp:chrome_resources',
            '../chrome.gyp:chrome_strings',
            '../../third_party/icu38/icu38.gyp:icui18n',
            '../../third_party/icu38/icu38.gyp:icuuc',
            '../../third_party/libxml/libxml.gyp:libxml',
            '../../third_party/lzma_sdk/lzma_sdk.gyp:lzma_sdk',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'util/browser_distribution.cc',
            'util/browser_distribution.h',
            'util/compat_checks.cc',
            'util/compat_checks.h',
            'util/copy_tree_work_item.cc',
            'util/copy_tree_work_item.h',
            'util/create_dir_work_item.cc',
            'util/create_dir_work_item.h',
            'util/create_reg_key_work_item.cc',
            'util/create_reg_key_work_item.h',
            'util/delete_reg_value_work_item.cc',
            'util/delete_reg_value_work_item.h',
            'util/delete_tree_work_item.cc',
            'util/delete_tree_work_item.h',
            'util/google_chrome_distribution.cc',
            'util/google_chrome_distribution.h',
            'util/google_update_constants.cc',
            'util/google_update_constants.h',
            'util/google_update_settings.cc',
            'util/google_update_settings.h',
            'util/helper.cc',
            'util/helper.h',
            'util/html_dialog.h',
            'util/html_dialog_impl.cc',
            'util/install_util.cc',
            'util/install_util.h',
            'util/l10n_string_util.cc',
            'util/l10n_string_util.h',
            'util/logging_installer.cc',
            'util/logging_installer.h',
            'util/lzma_util.cc',
            'util/lzma_util.h',
            'util/master_preferences.cc',
            'util/master_preferences.h',
            'util/move_tree_work_item.cc',
            'util/move_tree_work_item.h',
            'util/self_reg_work_item.cc',
            'util/self_reg_work_item.h',
            'util/set_reg_value_work_item.cc',
            'util/set_reg_value_work_item.h',
            'util/shell_util.cc',
            'util/shell_util.h',
            'util/util_constants.cc',
            'util/util_constants.h',
            'util/version.cc',
            'util/version.h',
            'util/work_item.cc',
            'util/work_item.h',
            'util/work_item_list.cc',
            'util/work_item_list.h',
            '../common/json_value_serializer.cc',
            '../common/pref_names.cc',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'gcapi_dll',
          'type': 'loadable_module',
          'msvs_guid': 'B802A2FE-E4E2-4F5A-905A-D5128875C954',
          'dependencies': [
            '../../google_update/google_update.gyp:google_update',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'gcapi/gcapi.cc',
            'gcapi/gcapi.h',
          ],
        },
        {
          'target_name': 'gcapi_lib',
          'type': 'static_library',
          'msvs_guid': 'CD2FD73A-6AAB-4886-B887-760D18E8B635',
          'dependencies': [
            '../../google_update/google_update.gyp:google_update',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'gcapi/gcapi.cc',
            'gcapi/gcapi.h',
          ],
        },
        {
          'target_name': 'gcapi_test',
          'type': 'executable',
          'msvs_guid': 'B64B396B-8EF1-4B6B-A07E-48D40EB961AB',
          'dependencies': [
            'gcapi_dll',
            'gcapi_lib',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'gcapi/gcapi_test.cc',
            'gcapi/gcapi_test.rc',
            'gcapi/resource.h',
          ],
        },
        {
          'target_name': 'installer_util_unittests',
          'type': 'executable',
          'msvs_guid': '903F8C1E-537A-4C9E-97BE-075147CBE769',
          'dependencies': [
            'installer_util',
            'installer_util_strings',
            '../../base/base.gyp:base',
            '../../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'setup/compat_checks_unittest.cc',
            'setup/setup_constants.cc',
            'util/copy_tree_work_item_unittest.cc',
            'util/create_dir_work_item_unittest.cc',
            'util/create_reg_key_work_item_unittest.cc',
            'util/delete_reg_value_work_item_unittest.cc',
            'util/delete_tree_work_item_unittest.cc',
            'util/google_chrome_distribution_unittest.cc',
            'util/helper_unittest.cc',
            'util/installer_util_unittests.rc',
            'util/installer_util_unittests_resource.h',
            'util/move_tree_work_item_unittest.cc',
            'util/run_all_unittests.cc',
            'util/set_reg_value_work_item_unittest.cc',
            'util/work_item_list_unittest.cc',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': '$(ProjectDir)\\mini_installer\\mini_installer.exe.manifest',
            },
          },
        },
        {
          'target_name': 'installer_util_strings',
          'type': 'none',
          'msvs_guid': '0026A376-C4F1-4575-A1BA-578C69F07013',
          'actions': [
            {
              # TODO(sgk):  Clean this up so that we pass in the
              # file names to the script instead of having it hard-code
              # matching path names internally.
              'action_name': 'installer_util_strings',
              'inputs': [
                'util/prebuild/create_string_rc.py',
                '../app/generated_resources.grd',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/installer_util_strings/installer_util_strings.rc',
                '<(SHARED_INTERMEDIATE_DIR)/installer_util_strings/installer_util_strings.h',
              ],
              'action': [
                # The create_string_rc.py script requires the checked-in
                # python.exe that has google modules installed, and
                # a PYTHONPATH pointing to grit so it can import FP.
                # TODO:  clean this up
                'set PYTHONPATH=../../tools/grit/grit/extern', '&&',
                '../../third_party/python_24/python.exe',
                'util/prebuild/create_string_rc.py',
                '<(SHARED_INTERMEDIATE_DIR)/installer_util_strings'
              ],
              'msvs_cygwin_shell': 0,
            },
          ],
          'direct_dependent_settings': {
            'include_dirs': [
               '<(SHARED_INTERMEDIATE_DIR)/installer_util_strings',
            ],
          },
        },
        {
          'target_name': 'mini_installer_test',
          'type': 'executable',
          'msvs_guid': '4B6E199A-034A-49BD-AB93-458DD37E45B1',
          'dependencies': [
            'installer_util',
            '../../base/base.gyp:base',
            '../../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'setup/setup_constants.cc',
            'util/run_all_unittests.cc',
            '../test/mini_installer_test/chrome_mini_installer.cc',
            '../test/mini_installer_test/chrome_mini_installer.h',
            '../test/mini_installer_test/mini_installer_test_constants.cc',
            '../test/mini_installer_test/mini_installer_test_constants.h',
            '../test/mini_installer_test/test.cc',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': '$(ProjectDir)\\mini_installer\\mini_installer.exe.manifest',
            },
          },
        },
        {
          'target_name': 'setup',
          'type': 'executable',
          'msvs_guid': '21C76E6E-8B38-44D6-8148-B589C13B9554',
          'dependencies': [
            'installer_util',
            'installer_util_strings',
            '../../build/util/build_util.gyp:lastchange',
            '../../build/win/system.gyp:cygwin',
            '../../courgette/courgette.gyp:courgette_lib',
            '../../third_party/bspatch/bspatch.gyp:bspatch',
          ],
          'include_dirs': [
            '../..',
            '<(INTERMEDIATE_DIR)',
            '<(SHARED_INTERMEDIATE_DIR)/setup',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/setup',
            ],
          },
          'sources': [
            'mini_installer/chrome.release',
            'setup/install.cc',
            'setup/install.h',
            'setup/setup_main.cc',
            'setup/setup.ico',
            'setup/setup.rc',
            'setup/setup_constants.cc',
            'setup/setup_constants.h',
            'setup/setup_exe_version.rc.version',
            'setup/setup_resource.h',
            'setup/setup_util.cc',
            'setup/setup_util.h',
            'setup/uninstall.cc',
            'setup/uninstall.h',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',     # Set /SUBSYSTEM:WINDOWS
            },
            'VCManifestTool': {
              'AdditionalManifestFiles': '$(ProjectDir)\\setup\\setup.exe.manifest',
            },
          },
          'rules': [
            {
              'rule_name': 'setup_version',
              'extension': 'version',
              'variables': {
                'version_py': '../../chrome/tools/build/version.py',
                'template_input_path': 'setup/setup_exe_version.rc.version',
              },
              'inputs': [
                '<(template_input_path)',
                '<(version_path)',
                '<(lastchange_path)',
                '<(branding_dir)/BRANDING',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/setup/setup_exe_version.rc',
              ],
              'action': [
                'python', '<(version_py)',
                '-f', '<(version_path)',
                '-f', '<(lastchange_path)',
                '-f', '<(branding_dir)/BRANDING',
                '<(template_input_path)',
                '<@(_outputs)',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Generating version information'
            },
            {
              'rule_name': 'server_dlls',
              'extension': 'release',
              'variables': {
                'scan_server_dlls_py' : '../tools/build/win/scan_server_dlls.py',
              },
              'inputs': [
                '<scan_server_dlls_py)',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/registered_dlls.h',
              ],
              'action': [
                'python',
                '<(scan_server_dlls_py)',
                '--output_dir=<(INTERMEDIATE_DIR)',
                '--input_file=<(RULE_INPUT_PATH)',
                '--header_output_dir=<(INTERMEDIATE_DIR)',
                # TODO(sgk):  may just use environment variables
                #'--distribution=$(CHROMIUM_BUILD)',
                '--distribution=_google_chrome',
              ],
            },
          ],
          # TODO(mark):  <(branding_dir) should be defined by the
          # global condition block at the bottom of the file, but
          # this doesn't work due to the following issue:
          #
          #   http://code.google.com/p/gyp/issues/detail?id=22
          #
          # Remove this block once the above issue is fixed.
          'conditions': [
            [ 'branding == "Chrome"', {
              'variables': {
                 'branding_dir': '../app/theme/google_chrome',
              },
            }, { # else branding!="Chrome"
              'variables': {
                 'branding_dir': '../app/theme/chromium',
              },
            }],
          ],
        },
        {
          'target_name': 'setup_unittests',
          'type': 'executable',
          'msvs_guid': 'C0AE4E06-F023-460F-BC14-6302CEAC51F8',
          'dependencies': [
            'installer_util',
            '../../base/base.gyp:base',
            '../../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'setup/run_all_unittests.cc',
            'setup/setup_util.cc',
            'setup/setup_util_unittest.cc',
          ],
        },
      ],
    }],
    [ 'branding == "Chrome"', {
      'variables': {
         'branding_dir': '../app/theme/google_chrome',
      },
    }, { # else branding!="Chrome"
      'variables': {
         'branding_dir': '../app/theme/chromium',
      },
    }],
  ],
}
