# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # .gyp files should set chromium_code to 1 if they build Chromium-specific
    # code, as opposed to external code.  This variable is used to control
    # such things as the set of warnings to enable, and whether warnings are
    # treated as errors.
    'chromium_code%': 0,

    # Variables expected to be overriden on the GYP command line (-D) or by
    # ~/.gyp/include.gypi.

    # Override chromium_mac_pch and set it to 0 to suppress the use of
    # precompiled headers on the Mac.  Prefix header injection may still be
    # used, but prefix headers will not be precompiled.  This is useful when
    # using distcc to distribute a build to compile slaves that don't
    # share the same compiler executable as the system driving the compilation,
    # because precompiled headers rely on pointers into a specific compiler
    # executable's image.  Setting this to 0 is needed to use an experimental
    # Linux-Mac cross compiler distcc farm.
    'chromium_mac_pch%': 1,

    # Override branding to select the desired branding flavor.
    'branding%': 'Chromium',
  },
  'target_defaults': {
    'conditions': [
      ['branding=="Chrome"', {
        'defines': ['GOOGLE_CHROME_BUILD'],
      }, {  # else: branding!="Chrome"
        'defines': ['CHROMIUM_BUILD'],
      }],
    ],
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
    [ 'OS=="linux"', {
      'target_defaults': {
        'asflags': [
          # Needed so that libs with .s files (e.g. libicudata.a)
          # are compatible with the general 32-bit-ness.
          '-32',
        ],
        # All floating-point computations on x87 happens in 80-bit
        # precision.  Because the C and C++ language standards allow
        # the compiler to keep the floating-point values in higher
        # precision than what's specified in the source and doing so
        # is more efficient than constantly rounding up to 64-bit or
        # 32-bit precision as specified in the source, the compiler,
        # especially in the optimized mode, tries very hard to keep
        # values in x87 floating-point stack (in 80-bit precision)
        # as long as possible. This has important side effects, that
        # the real value used in computation may change depending on
        # how the compiler did the optimization - that is, the value
        # kept in 80-bit is different than the value rounded down to
        # 64-bit or 32-bit. There are possible compiler options to make
        # this behavior consistent (e.g. -ffloat-store would keep all
        # floating-values in the memory, thus force them to be rounded
        # to its original precision) but they have significant runtime
        # performance penalty.
        #
        # -mfpmath=sse -msse2 makes the compiler use SSE instructions
        # which keep floating-point values in SSE registers in its
        # native precision (32-bit for single precision, and 64-bit for
        # double precision values). This means the floating-point value
        # used during computation does not change depending on how the
        # compiler optimized the code, since the value is always kept
        # in its specified precision.
        'cflags': [
          '-m32',
          '-pthread',
          '-march=pentium4',
          '-fno-exceptions',
          '-msse2',
          '-mfpmath=sse',
        ],
        'linkflags': [
          '-m32',
          '-pthread',
        ],
        'scons_variable_settings': {
          'LIBPATH': ['$LIB_DIR'],
          # Linking of large files uses lots of RAM, so serialize links
          # using the handy flock command from util-linux.
          'FLOCK_LINK': ['flock', '$DESTINATION_ROOT/linker.lock', '$LINK'],

          # We have several cases where archives depend on each other in
          # a cyclic fashion.  Since the GNU linker does only a single
          # pass over the archives we surround the libraries with
          # --start-group and --end-group (aka -( and -) ). That causes
          # ld to loop over the group until no more undefined symbols
          # are found. In an ideal world we would only make groups from
          # those libraries which we knew to be in cycles. However,
          # that's tough with SCons, so we bodge it by making all the
          # archives a group by redefining the linking command here.
          #
          # TODO:  investigate whether we still have cycles that
          # require --{start,end}-group.  There has been a lot of
          # refactoring since this was first coded, which might have
          # eliminated the circular dependencies.
          'LINKCOM': [['$FLOCK_LINK', '-o', '$TARGET', '$LINKFLAGS', '$SOURCES', '$_LIBDIRFLAGS', '-Wl,--start-group', '$_LIBFLAGS', '-Wl,--end-group']],
          'SHLINKCOM': [['$FLOCK_LINK', '-o', '$TARGET', '$SHLINFLAGS', '$SOURCES', '$_LIBDIRFLAGS', '-Wl,--start-group', '$_LIBFLAGS', '-Wl,--end-group']],
          'IMPLICIT_COMMAND_DEPENDENCIES': 0,
        },
        'scons_import_variables': [
          'CC',
          'CXX',
          'LINK',
        ],
        'scons_propagate_variables': [
          'CC',
          'CCACHE_DIR',
          'CXX',
          'DISTCC_DIR',
          'DISTCC_HOSTS',
          'HOME',
          'LINK',
        ],
      },
    }],
    ['OS=="mac"', {
      'target_defaults': {
        'mac_bundle': 0,
        'xcode_settings': {
          'ALWAYS_SEARCH_USER_PATHS': 'NO',
          'GCC_C_LANGUAGE_STANDARD': 'c99',
          'GCC_CW_ASM_SYNTAX': 'NO',
          'GCC_DYNAMIC_NO_PIC': 'YES',
          'GCC_ENABLE_PASCAL_STRINGS': 'NO',
          'GCC_INLINES_ARE_PRIVATE_EXTERN': 'YES',
          'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',
          'GCC_TREAT_WARNINGS_AS_ERRORS': 'YES',
          'GCC_VERSION': '4.2',
          'GCC_WARN_ABOUT_MISSING_NEWLINE': 'YES',
          'MACOSX_DEPLOYMENT_TARGET': '10.5',
          'PREBINDING': 'NO',
          'SDKROOT': 'macosx10.5',
          'USE_HEADERMAP': 'NO',
          'WARNING_CFLAGS': ['-Wall', '-Wendif-labels'],
          'conditions': [
            ['chromium_mac_pch', {'GCC_PRECOMPILE_PREFIX_HEADER': 'YES'},
                                 {'GCC_PRECOMPILE_PREFIX_HEADER': 'NO'}],
          ],
        },
        'target_conditions': [
          ['_type=="shared_library" or _type=="loadable_module"', {
            'xcode_settings': {'GCC_DYNAMIC_NO_PIC': 'NO'},
          }],
          ['_type!="static_library"', {
            'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-search_paths_first']},
          }],
          ['_mac_bundle', {
            'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
          }],
          ['_type=="executable"', {
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
    'SYMROOT': '<(DEPTH)/xcodebuild',
  },
}
