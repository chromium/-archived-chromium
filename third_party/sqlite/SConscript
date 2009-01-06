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
      ],
  )

input_files = [
    'ext/fts2/fts2.c',
    'ext/fts2/fts2.h',
    'ext/fts2/fts2_hash.c',
    'ext/fts2/fts2_hash.h',
    'ext/fts2/fts2_icu.c',
    'ext/fts2/fts2_porter.c',
    'ext/fts2/fts2_tokenizer.c',
    'ext/fts2/fts2_tokenizer.h',
    'ext/fts2/fts2_tokenizer1.c',
    'ext/icu/icu.c',
    'preprocessed/keywordhash.h',
    'preprocessed/opcodes.c',
    'preprocessed/opcodes.h',
    'preprocessed/parse.c',
    'preprocessed/parse.h',
    'preprocessed/sqlite3.h',
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
    'src/func.c',
    'src/global.c',
    'src/hash.c',
    'src/hash.h',
    'src/insert.c',
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
    'src/mutex_w32.c',
    'src/os.c',
    'src/os.h',
    'src/os_common.h',
    'src/os_win.c',
    'src/pager.c',
    'src/pager.h',
    'src/pragma.c',
    'src/prepare.c',
    'src/printf.c',
    'src/random.c',
    'src/select.c',
    'src/shell_icu.c',
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
    'src/vdbeInt.h',
    'src/vdbeapi.c',
    'src/vdbeaux.c',
    'src/vdbeblob.c',
    'src/vdbefifo.c',
    'src/vdbemem.c',
    'src/vtab.c',
    'src/where.c',
]

x=env.ChromeLibrary('sqlite', input_files)
env.Alias('sl', x)

env.ChromeMSVSProject('$SQLITE_DIR/sqlite.vcproj',
                guid='{6EAD4A4B-2BBC-4974-8E45-BB5C16CC2AC9}')

# TODO(tc): There should be a target to build the stand alone sqlite shell.
