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
      ],
  )

input_files = [
    'alter.c',
    'analyze.c',
    'attach.c',
    'auth.c',
    'btree.c',
    'build.c',
    'callback.c',
    'complete.c',
    'date.c',
    'delete.c',
    'expr.c',
    'fts2.c',
    'fts2_hash.c',
    'fts2_icu.c',
    'fts2_porter.c',
    'fts2_tokenizer.c',
    'fts2_tokenizer1.c',
    'func.c',
    'hash.c',
    'icu.c',
    'insert.c',
    'legacy.c',
    'loadext.c',
    'main.c',
    'malloc.c',
    'opcodes.c',
    'os.c',
    'os_unix.c',
    'os_win.c',
    'pager.c',
    'parse.c',
    'pragma.c',
    'prepare.c',
    'printf.c',
    'random.c',
    'select.c',
    'table.c',
    'tokenize.c',
    'trigger.c',
    'update.c',
    'utf.c',
    'util.c',
    'vacuum.c',
    'vdbe.c',
    'vdbeapi.c',
    'vdbeaux.c',
    'vdbefifo.c',
    'vdbemem.c',
    'vtab.c',
    'where.c',
]

env.ChromeStaticLibrary('sqlite', input_files)

env.ChromeMSVSProject('$SQLITE_DIR/sqlite.vcproj',
                guid='{6EAD4A4B-2BBC-4974-8E45-BB5C16CC2AC9}')

# TODO(tc): There should be a target to build the stand alone sqlite shell.
