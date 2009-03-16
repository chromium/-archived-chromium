# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

Import('env')

env = env.Clone()

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
    'preprocessed/opcodes.c',
    'preprocessed/opcodes.h',
    'src/os.c',
    'src/os.h',
    'src/os_common.h',
    'src/os_unix.c',
    'src/pager.c',
    'src/pager.h',
    'preprocessed/parse.c',
    'preprocessed/parse.h',
    'src/pragma.c',
    'src/prepare.c',
    'src/printf.c',
    'src/random.c',
    'src/select.c',
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

sqlite_lib = env.ChromeLibrary('sqlite', input_files)

# Build a stand alone sqlite shell so we can inspect the sqlite files.
env_shell = env.Clone()

shell_input_files = [
    'src/shell.c',
    'src/shell_icu_linux.c',
]

env_shell.Append(
    LIBS = [
        'icu',
        'sqlite',
        'stdc++',
    ]
)

env_shell.ChromeProgram('sqlite_shell', shell_input_files)
