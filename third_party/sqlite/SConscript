# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

Import('env')

env = env.Clone(
)

env.Prepend(
    CPPPATH = [
        '$ICU38_DIR/public/common',
        '$ICU38_DIR/public/i18n',
        '$CHROME_SRC_DIR',
        'preprocessed',
        'src',
    ],
)

env.Append(
    CPPDEFINES = [
        'U_STATIC_IMPLEMENTATION',

        'SQLITE_ENABLE_FTS2',
        'SQLITE_ENABLE_BROKEN_FTS2',
        'SQLITE_ENABLE_ICU',
        'SQLITE_SECURE_DELETE',
        'THREADSAFE',
        ['_HAS_EXCEPTIONS', '0'],
    ],
)

if env.Bit('windows'):
  env.Append(
      CCFLAGS = [
          '/TC',

          '/wd4800',
          '/wd4244',
          '/wd4018',
          '/wd4146',
          '/wd4996',
      ],
  )

input_files = ChromeFileList([
    'src/alter.c',
    'src/analyze.c',
    'src/attach.c',
    'src/auth.c',
    'src/bitvec.c',
    'src/btmutex.c',
    'src/btree.c',
    'src/btree.h',
    'src/btreeInt.h',
    'src/build.c',
    'src/callback.c',
    'src/complete.c',
    'src/date.c',
    'src/delete.c',
    'src/expr.c',
    'src/fault.c',
    'ext/fts2/fts2.c',
    'ext/fts2/fts2.h',
    'ext/fts2/fts2_hash.c',
    'ext/fts2/fts2_hash.h',
    'ext/fts2/fts2_icu.c',
    'ext/fts2/fts2_porter.c',
    'ext/fts2/fts2_tokenizer.c',
    'ext/fts2/fts2_tokenizer.h',
    'ext/fts2/fts2_tokenizer1.c',
    'src/func.c',
    'src/global.c',
    'src/hash.c',
    'src/hash.h',
    'ext/icu/icu.c',
    'src/insert.c',
    'preprocessed/keywordhash.h',
    'src/legacy.c',
    'src/loadext.c',
    'src/main.c',
    'src/malloc.c',
    'src/mem1.c',
    'src/mem2.c',
    'src/mem3.c',
    'src/mem4.c',
    'src/mem5.c',
    'src/mem6.c',
    'src/mutex.c',
    'src/mutex.h',
    'src/mutex_unix.c',
    'src/mutex_w32.c',
    'preprocessed/opcodes.c',
    'preprocessed/opcodes.h',
    'src/os.c',
    'src/os.h',
    'src/os_common.h',
    'src/os_unix.c',
    'src/os_win.c',
    'src/pager.c',
    'src/pager.h',
    'preprocessed/parse.c',
    'preprocessed/parse.h',
    'src/pragma.c',
    'src/prepare.c',
    'src/printf.c',
    'src/random.c',
    'src/select.c',
    'src/shell.c',
    'src/shell_icu.c',
    'preprocessed/sqlite3.h',
    'src/sqlite3ext.h',
    'src/sqliteInt.h',
    'src/sqliteLimit.h',
    'src/status.c',
    'src/table.c',
    'src/tokenize.c',
    'src/trigger.c',
    'src/update.c',
    'src/utf.c',
    'src/util.c',
    'src/vacuum.c',
    'src/vdbe.c',
    'src/vdbe.h',
    'src/vdbeapi.c',
    'src/vdbeaux.c',
    'src/vdbeblob.c',
    'src/vdbefifo.c',
    'src/vdbeInt.h',
    'src/vdbemem.c',
    'src/vtab.c',
    'src/where.c',
])

if not env.Bit('windows'):
  input_files.Remove(
      'src/mutex_w32.c',
      'src/os_win.c',
      'src/shell.c',
      'src/shell_icu.c',
  )

if not env.Bit('linux'):
  input_files.Remove(
      'src/mutex_unix.c',
      'src/os_unix.c',
  )

env.ChromeLibrary('sqlite', input_files)

p = env.ChromeMSVSProject('sqlite.vcproj',
                          dest=('$CHROME_SRC_DIR/third_party/' +
                                    'sqlite/sqlite.vcproj'),
                          dependencies = [
                              '$ICU38_DIR/build/icu.vcproj',
                          ],
                          guid='{6EAD4A4B-2BBC-4974-8E45-BB5C16CC2AC9}',
                          keyword='Win32Proj',
                          # TODO(sgk):  when we can intuit the hierarchy
                          # from the built targets.
                          #buildtargets=TODO,
                          files=input_files,
                          relative_path_prefix='./')


p.AddConfig('Debug|Win32',
            InheritedPropertySheets=[
                '$(SolutionDir)../build/common.vsprops',
                '$(SolutionDir)../build/debug.vsprops',
                './sqlite.vsprops',
                '$(SolutionDir)../build/external_code.vsprops',
                '$(SolutionDir)../third_party/icu38/build/using_icu.vsprops',
            ],
            ConfigurationType='4',
            tools=[
                'VCPreBuildEventTool',
                'VCCustomBuildTool',
                'VCXMLDataGeneratorTool',
                'VCWebServiceProxyGeneratorTool',
                'VCMIDLTool',
                MSVSTool('VCCLCompilerTool',
                         AdditionalIncludeDirectories='"$(SolutionDir)..\\third_party\\sqlite\\src";"$(SolutionDir)..\\third_party\\sqlite\\preprocessed"'),
                'VCManagedResourceCompilerTool',
                'VCResourceCompilerTool',
                'VCPreLinkEventTool',
                'VCLibrarianTool',
                'VCALinkTool',
                'VCXDCMakeTool',
                'VCBscMakeTool',
                'VCFxCopTool',
                'VCPostBuildEventTool',
            ])

p.AddConfig('Release|Win32',
            InheritedPropertySheets=[
                '$(SolutionDir)../build/common.vsprops',
                '$(SolutionDir)../build/release.vsprops',
                './sqlite.vsprops',
                '$(SolutionDir)../build/external_code.vsprops',
                '$(SolutionDir)../third_party/icu38/build/using_icu.vsprops',
            ],
            ConfigurationType='4',
            tools=[
                'VCPreBuildEventTool',
                'VCCustomBuildTool',
                'VCXMLDataGeneratorTool',
                'VCWebServiceProxyGeneratorTool',
                'VCMIDLTool',
                MSVSTool('VCCLCompilerTool',
                         AdditionalIncludeDirectories=[
                           '"$(SolutionDir)../third_party/sqlite/src"',
                           '"$(SolutionDir)../third_party/sqlite/preprocessed"',
                        ]),
                'VCManagedResourceCompilerTool',
                'VCResourceCompilerTool',
                'VCPreLinkEventTool',
                'VCLibrarianTool',
                'VCALinkTool',
                'VCXDCMakeTool',
                'VCBscMakeTool',
                'VCFxCopTool',
                'VCPostBuildEventTool',
            ])

p.AddConfig('Shell|Win32',
            InheritedPropertySheets=[
                '$(SolutionDir)../build/common.vsprops',
                './sqlite.vsprops',
                '$(SolutionDir)../build/release.vsprops',
                '$(SolutionDir)../build/external_code.vsprops',
                '$(SolutionDir)../third_party/icu38/build/using_icu.vsprops',
            ],
            ConfigurationType='1',
            tools=[
                'VCPreBuildEventTool',
                'VCCustomBuildTool',
                'VCXMLDataGeneratorTool',
                'VCWebServiceProxyGeneratorTool',
                'VCMIDLTool',
                MSVSTool('VCCLCompilerTool',
                         AdditionalIncludeDirectories=[
                           '"$(SolutionDir)../third_party/sqlite/src"',
                           '"$(SolutionDir)../third_party/sqlite/preprocessed"',
                        ]),
                'VCManagedResourceCompilerTool',
                'VCResourceCompilerTool',
                'VCPreLinkEventTool',
                'VCLinkerTool',
                'VCALinkTool',
                'VCManifestTool',
                'VCXDCMakeTool',
                'VCBscMakeTool',
                'VCFxCopTool',
                'VCAppVerifierTool',
                'VCWebDeploymentTool',
                'VCPostBuildEventTool',
            ])

p.AddFileConfig('./src/shell.c',
                'Debug|Win32',
                ExcludedFromBuild='true',
                tools=[
                    'VCCLCompilerTool',
                ])

p.AddFileConfig('./src/shell.c',
                'Release|Win32',
                ExcludedFromBuild='true',
                tools=[
                    'VCCLCompilerTool',
                ])

# TODO(tc): There should be a target to build the stand alone sqlite shell.
