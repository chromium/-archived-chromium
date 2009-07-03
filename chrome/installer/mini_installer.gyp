{
  'variables': {
    'version_py': '../../chrome/tools/build/version.py',
    'version_path': '../../chrome/VERSION',
    'lastchange_path': '<(SHARED_INTERMEDIATE_DIR)/build/LASTCHANGE',
    # 'branding_dir' is set in the 'conditions' section at the bottom.
    'msvs_use_common_release': 0,
  },
  'includes': [
    '../../build/common.gypi',
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'mini_installer',
          'type': 'executable',
          'msvs_guid': '24A5AC7C-280B-4899-9153-6BA570A081E7',
          'dependencies': [
            '../chrome.gyp:chrome',
            '../chrome.gyp:chrome_dll',
            '../chrome.gyp:default_extensions',
            '../../testing/gtest.gyp:gtest',
            'installer.gyp:setup',
          ],
          'include_dirs': [
            '../..',
            '<(PRODUCT_DIR)',
            '<(INTERMEDIATE_DIR)',
          ],
          'sources': [
            'mini_installer/chrome.release',
            'mini_installer/mini_installer.cc',
            'mini_installer/mini_installer.h',
            'mini_installer/mini_installer.ico',
            'mini_installer/mini_installer.rc',
            'mini_installer/mini_installer_exe_version.rc.version',
            'mini_installer/mini_installer_resource.h',
            'mini_installer/pe_resource.cc',
            'mini_installer/pe_resource.h',
          ],
          'msvs_settings': {
            'VCCLCompilerTool': {
              'EnableIntrinsicFunctions': 'true',
              'BufferSecurityCheck': 'false',
            },
            'VCLinkerTool': {
              'AdditionalOptions':
                '/safeseh:no /dynamicbase:no /ignore:4199 /ignore:4221 /nxcompat',
              'RandomizedBaseAddress': '1',
              'DataExecutionPrevention': '0',
              'AdditionalLibraryDirectories':
                ['<(DEPTH)/third_party/platformsdk_win2008_6_1/files/Lib;<(PRODUCT_DIR)/lib'],
              # TODO(bradnelson): change when vsprops are all in gyp.
              'DelayLoadDLLs': [],
              'EntryPointSymbol': 'MainEntryPoint',
              'GenerateMapFile': 'true',
              'IgnoreAllDefaultLibraries': 'true',
              'OptimizeForWindows98': '1',
              'SubSystem': '2',     # Set /SUBSYSTEM:WINDOWS
            },
          },
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'BasicRuntimeChecks': '0',
                },
                # TODO(bradnelson): Add these back to common configuration
                # when vsprops goes away.
                'VCLinkerTool': {
                  'AdditionalDependencies': [
                    '"$(VCInstallDir)crt\\src\\intel\\mt_lib\\memset.obj"',
                    '"$(VCInstallDir)crt\\src\\intel\\mt_lib\\P4_memset.obj"',
                    'shlwapi.lib',
                  ],
                },
              },
            },
            'Release': {
              'msvs_props': ['mini_installer/mini_installer_release.vsprops'],
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'BasicRuntimeChecks': '0',
                },
              },
            },
          },
          'rules': [
            {
              'rule_name': 'mini_installer_version',
              'extension': 'version',
              'variables': {
                'template_input_path': 'mini_installer/mini_installer_exe_version.rc.version',
              },
              'inputs': [
                '<(template_input_path)',
                '<(version_path)',
                '<(lastchange_path)',
                '<(branding_dir)/BRANDING',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/mini_installer_exe_version.rc',
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
              'rule_name': 'installer_archive',
              'extension': 'release',
              'variables': {
                'create_installer_archive_py_path':
                  '../tools/build/win/create_installer_archive.py',
              },
              'inputs': [
                '<(create_installer_archive_py_path)',
                '<(PRODUCT_DIR)/chrome.exe',
                '<(PRODUCT_DIR)/chrome.dll',
                '<(PRODUCT_DIR)/locales/en-US.dll',
                '<(PRODUCT_DIR)/icudt38.dll',
              ],
              'outputs': [
                'xxx.out',
                '<(PRODUCT_DIR)/<(RULE_INPUT_NAME).7z',
                '<(PRODUCT_DIR)/<(RULE_INPUT_NAME).packed.7z',
                '<(PRODUCT_DIR)/setup.ex_',
                '<(PRODUCT_DIR)/packed_files.txt',
              ],
              'action': [
                'python',
                '<(create_installer_archive_py_path)',
                '--output_dir=<(PRODUCT_DIR)',
                '--input_file=<(RULE_INPUT_PATH)',
                # TODO(sgk):  may just use environment variables
                #'--distribution=$(CHROMIUM_BUILD)',
                '--distribution=_google_chrome',
              ],
              'message': 'Create installer archive'
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
