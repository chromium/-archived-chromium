# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code%': 0,
  },
  'target_defaults': {
    # TODO(bradnelson): This should really be able to be either:
    #   CHROMIUM_BUILD or GOOGLE_CHROME_BUILD
    'defines': ['CHROMIUM_BUILD'],
    'default_configuration': 'Debug',
    'configurations': {
      'Debug': {
        'conditions': [
          [ 'OS=="mac"', {
            'xcode_settings': {
              'COPY_PHASE_STRIP': 'NO',
              'GCC_OPTIMIZATION_LEVEL': '0',
            }
          }],
          [ 'OS=="win"', {
            'configuration_platform': 'Win32',
            'msvs_configuration_attributes': {
              'OutputDirectory': '$(SolutionDir)$(ConfigurationName)',
              'IntermediateDirectory': '$(OutDir)\\obj\\$(ProjectName)',
              'CharacterSet': '1',
            },
            'msvs_settings': {
              'VCCLCompilerTool': {
                'Optimization': '0',
                'PreprocessorDefinitions': ['_DEBUG'],
                'BasicRuntimeChecks': '3',
                'RuntimeLibrary': '1',
              },
              'VCLinkerTool': {
                'LinkIncremental': '1',
              },
              'VCResourceCompilerTool': {
                'PreprocessorDefinitions': ['_DEBUG'],
              },
            },
          }],
        ],
      },
      'Release': {
        'defines': [
          'NDEBUG',
        ],
        'conditions': [
          [ 'OS=="mac"', {
            'xcode_settings': {
              'DEAD_CODE_STRIPPING': 'YES',
            }
          }],
          [ 'OS=="win"', {
            'configuration_platform': 'Win32',
            'msvs_props': ['release.vsprops'],
          }],
        ],
      },
    },
  },
  'conditions': [
    ['OS=="mac"', {
      'target_defaults': {
        'xcode_settings': {
          'ALWAYS_SEARCH_USER_PATHS': 'NO',
          'GCC_C_LANGUAGE_STANDARD': 'c99',
          'GCC_CW_ASM_SYNTAX': 'NO',
          'GCC_DYNAMIC_NO_PIC': 'YES',
          'GCC_ENABLE_PASCAL_STRINGS': 'NO',
          'GCC_INLINES_ARE_PRIVATE_EXTERN': 'YES',
          'GCC_PRECOMPILE_PREFIX_HEADER': 'YES',
          'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',
          'GCC_TREAT_WARNINGS_AS_ERRORS': 'YES',
          'GCC_VERSION': '4.2',
          'GCC_WARN_ABOUT_MISSING_NEWLINE': 'YES',
          'MACOSX_DEPLOYMENT_TARGET': '10.5',
          'PREBINDING': 'NO',
          'SDKROOT': 'macosx10.5',
          'USE_HEADERMAP': 'NO',
          'WARNING_CFLAGS': ['-Wall', '-Wendif-labels'],
        },
        'target_conditions': [
          ['_type=="shared_library"', {
            'xcode_settings': {'GCC_DYNAMIC_NO_PIC': 'NO'},
          }],
          ['_type!="static_library"', {
            'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-search_paths_first']},
          }],
          ['_type=="application"', {
            'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
          }],
          ['_type=="application" or _type=="executable"', {
            'postbuilds': [
              {
                'variables': {
                  # Define strip_from_xcode in a variable ending in _path so
                  # that gyp understands it's a path and performs proper
                  # relativization during dict merging.
                  'strip_from_xcode_path': 'mac/strip_from_xcode',
                },
                'postbuild_name': 'Strip If Needed',
                'action': ['<(strip_from_xcode_path)'],
              },
            ],
          }],
        ],
      },
    }],
    ['OS=="win"', {
      'target_defaults': {
        'defines': [
          '_WIN32_WINNT=0x0600',
          'WINVER=0x0600',
          'WIN32',
          '_WINDOWS',
          '_HAS_EXCEPTIONS=0',
          'NOMINMAX',
          '_CRT_RAND_S',
          'CERT_CHAIN_PARA_HAS_EXTRA_FIELDS',
          'WIN32_LEAN_AND_MEAN',
          '_SECURE_ATL',
          '_HAS_TR1=0',
        ],
        'include_dirs': [
          '<(DEPTH)/third_party/platformsdk_win2008_6_1/files/Include',
          '$(VSInstallDir)/VC/atlmfc/include',
        ],
        'msvs_cygwin_dirs': ['../third_party/cygwin'],
        'msvs_disabled_warnings': [4503, 4819],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'MinimalRebuild': 'false',
            'ExceptionHandling': '0',
            'BufferSecurityCheck': 'true',
            'EnableFunctionLevelLinking': 'true',
            'RuntimeTypeInfo': 'false',
            'WarningLevel': '3',
            'WarnAsError': 'true',
            'DebugInformationFormat': '3',
          },
          'VCLibrarianTool': {
            'AdditionalOptions': '/ignore:4221',
            'OutputFile': '$(OutDir)\\lib\\$(ProjectName).lib',
            'AdditionalLibraryDirectories':
              '<(DEPTH)/third_party/platformsdk_win2008_6_1/files/Lib',
          },
          'VCLinkerTool': {
            'AdditionalOptions':
              '/safeseh /dynamicbase /ignore:4199 /ignore:4221 /nxcompat',
            'AdditionalDependencies': [
              'wininet.lib',
              'version.lib',
              'msimg32.lib',
              'ws2_32.lib',
              'usp10.lib',
              'psapi.lib',
            ],
            'AdditionalLibraryDirectories':
              '<(DEPTH)/third_party/platformsdk_win2008_6_1/files/Lib',
            'DelayLoadDLLs': 'dwmapi.dll,uxtheme.dll',
            'GenerateDebugInformation': 'true',
            'MapFileName': '$(OutDir)\\$(TargetName).map',
            'ImportLibrary': '$(OutDir)\\lib\\$(TargetName).lib',
            'TargetMachine': '1',
            'FixedBaseAddress': '1',
          },
          'VCMIDLTool': {
            'GenerateStublessProxies': 'true',
            'TypeLibraryName': '$(InputName).tlb',
            'OutputDirectory': '$(IntDir)',
            'HeaderFileName': '$(InputName).h',
            'DLLDataFileName': 'dlldata.c',
            'InterfaceIdentifierFileName': '$(InputName)_i.c',
            'ProxyFileName': '$(InputName)_p.c',
          },
          'VCResourceCompilerTool': {
            'Culture' : '1033',
            'AdditionalIncludeDirectories': '<(DEPTH)',
          },
        },
      },
    }],
    ['chromium_code==0', {
      # This section must follow the other conditon sections above because
      # external_code.gypi expects to be merged into those settings.
      'includes': [
        'external_code.gypi',
      ],
    }],
  ],
  'xcode_settings': {
    # The Xcode generator will look for an xcode_settings section at the root
    # of each dict and use it to apply settings on a file-wide basis.  Most
    # settings should not be here, they should be in target-specific
    # xcode_settings sections, or better yet, should use non-Xcode-specific
    # settings in target dicts.  SYMROOT is a special case, because many other
    # Xcode variables depend on it, including variables such as
    # PROJECT_DERIVED_FILE_DIR.  When a source group corresponding to something
    # like PROJECT_DERIVED_FILE_DIR is added to a project, in order for the
    # files to appear (when present) in the UI as actual files and not red
    # red "missing file" proxies, the correct path to PROJECT_DERIVED_FILE_DIR,
    # and therefore SYMROOT, needs to be set at the project level.
    #
    # xcodebuild_gyp is a temporary name to avoid colliding with the xcodebuild
    # directory used by the non-gyp Xcode build system.  When the gyp-based
    # Xcode build system replaces the older system, this should be changed to
    # simply "xcodebuild" or some other suitable name.
    'SYMROOT': '<(DEPTH)/xcodebuild_gyp',
  },
}
