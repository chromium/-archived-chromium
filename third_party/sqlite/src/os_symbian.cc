// Copyright 2008, Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. Neither the name of Google Inc. nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This file contains code that is specific to Symbian.
// Differently from the rest of SQLite, it is implemented in C++ as this is
// the native language of the OS and all interfaces we need to use are C++.
//
// This file follows the Gears code style guidelines.

#ifdef OS_SYMBIAN
#include <coemain.h>
#include <e32math.h>
#include <f32file.h>
#include <utf.h>

extern "C" {
#include "sqliteInt.h"
#include "os_common.h"
}

const TInt kFileLockAttempts = 3;

// The global file system session.
RFs g_fs_session;

static TInt UTF8ToUTF16(const char *in, TDes *out16) {
  assert(in);
  TPtrC8 in_des(reinterpret_cast<const unsigned char*>(in));
  return CnvUtfConverter::ConvertToUnicodeFromUtf8(*out16, in_des);
}

static TInt UTF16ToUTF8(const TDesC16& in16, TDes8 *out8) {
  return CnvUtfConverter::ConvertFromUnicodeToUtf8(*out8, in16);
}

// The SymbianFile structure is a subclass of sqlite3_file* specific to the
// Symbian portability layer.
struct SymbianFile {
  const sqlite3_io_methods *methods;
  RFile handle;              // The file handle
  TUint8 lock_type;          // Type of lock currently held on this file
  TUint16 shared_lock_byte;  // Randomly chosen byte used as a shared lock
};

static SymbianFile* ConvertToSymbianFile(sqlite3_file* const id) {
  assert(id);
  return reinterpret_cast<SymbianFile*>(id);
}

static int SymbianClose(sqlite3_file *id) {
  SymbianFile *file_id = ConvertToSymbianFile(id);
  file_id->handle.Close();
  OpenCounter(-1);
  return SQLITE_OK;
}

static int SymbianRead(sqlite3_file *id,
                       void *buffer,
                       int amount,
                       sqlite3_int64 offset) {
  assert(buffer);
  assert(amount >=0);
  assert(offset >=0);

  SymbianFile* file_id = ConvertToSymbianFile(id);
  TPtr8 dest(static_cast<unsigned char*>(buffer), amount);

  if (KErrNone == file_id->handle.Read(offset, dest, amount)) {
    if (dest.Length() == amount) {
      return SQLITE_OK;
    } else {
      return SQLITE_IOERR_SHORT_READ;
    }
  } else {
    return SQLITE_IOERR;
  }
}

static int SymbianWrite(sqlite3_file *id,
                        const void *buffer,
                        int amount,
                        sqlite3_int64 offset) {
  assert(buffer);
  assert(amount >=0);
  assert(offset >=0);

  SymbianFile *file_id = ConvertToSymbianFile(id);
  TPtrC8 src(static_cast<const unsigned char*>(buffer), amount);
  if (file_id->handle.Write(offset, src) != KErrNone) {
    return SQLITE_IOERR_WRITE;
  }

  return SQLITE_OK;
}

static int SymbianTruncate(sqlite3_file *id, sqlite3_int64 bytes) {
  assert(bytes >=0);

  SymbianFile *file_id = ConvertToSymbianFile(id);
  if (file_id->handle.SetSize(bytes) != KErrNone) {
    return SQLITE_IOERR;
  }
  return SQLITE_OK;
}

static int SymbianSync(sqlite3_file *id, int /*flags*/) {
  SymbianFile *file_id = ConvertToSymbianFile(id);
  if (file_id->handle.Flush() != KErrNone) {
    return SQLITE_IOERR;
  } else {
    return SQLITE_OK;
  }
}

static int SymbianFileSize(sqlite3_file *id, sqlite3_int64 *size) {
  assert(size);

  SymbianFile *file_id = ConvertToSymbianFile(id);
  TInt size_tmp;
  if (file_id->handle.Size(size_tmp) != KErrNone) {
    return SQLITE_IOERR;
  }
  *size = size_tmp;
  return SQLITE_OK;
}

// File lock/unlock functions; see os_win.c for a description
// of the algorithm used.
static int GetReadLock(SymbianFile *file) {
  file->shared_lock_byte = Math::Random() % (SHARED_SIZE - 1);
  return file->handle.Lock(SHARED_FIRST + file->shared_lock_byte, 1);
}

static int UnlockReadLock(SymbianFile *file) {
  return file->handle.UnLock(SHARED_FIRST + file->shared_lock_byte, 1);
}

static int SymbianLock(sqlite3_file *id, int lock_type) {
  SymbianFile *file = ConvertToSymbianFile(id);
  if (file->lock_type >= lock_type) {
    return SQLITE_OK;
  }

  // Make sure the locking sequence is correct
  assert(file->lock_type != NO_LOCK || lock_type == SHARED_LOCK);
  assert(lock_type != PENDING_LOCK);
  assert(lock_type != RESERVED_LOCK || file->lock_type == SHARED_LOCK);

  // Lock the PENDING_LOCK byte if we need to acquire a PENDING lock or
  // a SHARED lock.  If we are acquiring a SHARED lock, the acquisition of
  // the PENDING_LOCK byte is temporary.
  int new_lock_type = file->lock_type;
  int got_pending_lock = 0;
  int res = KErrNone;
  if (file->lock_type == NO_LOCK ||
         (lock_type == EXCLUSIVE_LOCK && file->lock_type == RESERVED_LOCK)) {
    int count = kFileLockAttempts;
    while (count-- > 0 &&
        (res = file->handle.Lock(PENDING_BYTE, 1)) != KErrNone ) {
      // Try 3 times to get the pending lock.  The pending lock might be
      // held by another reader process who will release it momentarily.
      OSTRACE2("could not get a PENDING lock. cnt=%d\n", cnt);
      User::After(1000);
    }
    got_pending_lock = (res == KErrNone? 1 : 0);
  }

  // Acquire a shared lock
  if (lock_type == SHARED_LOCK && res == KErrNone) {
    assert(file->lock_type == NO_LOCK);
    res = GetReadLock(file);
    if (res == KErrNone) {
      new_lock_type = SHARED_LOCK;
    }
  }

  // Acquire a RESERVED lock
  if (lock_type == RESERVED_LOCK && res == KErrNone) {
    assert(file->lock_type == SHARED_LOCK);
    res = file->handle.Lock(RESERVED_BYTE, 1);
    if (res == KErrNone) {
      new_lock_type = RESERVED_LOCK;
    }
  }

  // Acquire a PENDING lock
  if (lock_type == EXCLUSIVE_LOCK && res == KErrNone) {
    new_lock_type = PENDING_LOCK;
    got_pending_lock = 0;
  }

  // Acquire an EXCLUSIVE lock
  if (lock_type == EXCLUSIVE_LOCK && res == KErrNone) {
    assert(file->lock_type >= SHARED_LOCK);
    res = UnlockReadLock(file);
    OSTRACE2("unreadlock = %d\n", res);
    res = file->handle.Lock(SHARED_FIRST, SHARED_SIZE);
    if (res == KErrNone) {
      new_lock_type = EXCLUSIVE_LOCK;
    } else {
      OSTRACE2("error-code = %d\n", GetLastError());
      GetReadLock(file);
    }
  }

  // If we are holding a PENDING lock that ought to be released, then
  // release it now.
  if (got_pending_lock && lock_type == SHARED_LOCK) {
    file->handle.UnLock(PENDING_BYTE, 1);
  }

  // Update the state of the lock held in the file descriptor, then
  // return the appropriate result code.
  file->lock_type = new_lock_type;
  if (res == KErrNone) {
    return SQLITE_OK;
  } else {
    OSTRACE4("LOCK FAILED %d trying for %d but got %d\n", file->handle,
           lock_type, new_lock_type);
    return SQLITE_BUSY;
  }
}

static int SymbianUnlock(sqlite3_file *id, int lock_type) {
  int type;
  int rc = SQLITE_OK;
  SymbianFile *file = ConvertToSymbianFile(id);
  assert(lock_type <= SHARED_LOCK);
  OSTRACE5("UNLOCK %d to %d was %d(%d)\n", file->handle, lock_type,
          file->lock_type, file->shared_lock_byte);
  type = file->lock_type;
  if (type >= EXCLUSIVE_LOCK) {
    file->handle.UnLock(SHARED_FIRST, SHARED_SIZE);
    if (lock_type == SHARED_LOCK && GetReadLock(file) != KErrNone) {
      // This should never happen.  We should always be able to
      // reacquire the read lock
      rc = SQLITE_IOERR_UNLOCK;
    }
  }
  if (type >= RESERVED_LOCK) {
    file->handle.UnLock(RESERVED_BYTE, 1);
  }
  if (lock_type == NO_LOCK && type >= SHARED_LOCK) {
    UnlockReadLock(file);
  }
  if (type >= PENDING_LOCK) {
    file->handle.UnLock(PENDING_BYTE, 1);
  }
  file->lock_type = lock_type;
  return rc;
}

static int SymbianCheckReservedLock(sqlite3_file *id, int *result) {
  int rc;
  SymbianFile *file = ConvertToSymbianFile(id);
  if (file->lock_type >= RESERVED_LOCK) {
    rc = 1;
    OSTRACE3("TEST WR-LOCK %d %d (local)\n", pFile->h, rc);
  } else {
    rc = file->handle.Lock(RESERVED_BYTE, 1);
    if (rc == KErrNone) {
      file->handle.UnLock(RESERVED_BYTE, 1);
    }
    rc = !rc;
    OSTRACE3("TEST WR-LOCK %d %d (remote)\n", file->handle, rc);
  }
  *result = rc;
  return SQLITE_OK;
}

static int SymbianFileControl(sqlite3_file */*id*/,
                              int /*op*/,
                              void */*arg*/) {
  return SQLITE_OK;
}

static int SymbianSectorSize(sqlite3_file */*id*/) {
  return SQLITE_DEFAULT_SECTOR_SIZE;
}

static int SymbianDeviceCharacteristics(sqlite3_file */*id*/) {
  return 0;
}

/*
** This vector defines all the methods that can operate on a
** sqlite3_file for Symbian.
*/
static const sqlite3_io_methods SymbianIoMethod = {
  1,    // iVersion
  SymbianClose,
  SymbianRead,
  SymbianWrite,
  SymbianTruncate,
  SymbianSync,
  SymbianFileSize,
  SymbianLock,
  SymbianUnlock,
  SymbianCheckReservedLock,
  SymbianFileControl,
  SymbianSectorSize,
  SymbianDeviceCharacteristics
};

// ============================================================================
// vfs methods begin here
// ============================================================================
static int SymbianOpen(sqlite3_vfs */*vfs*/,
                       const char *name,
                       sqlite3_file *id,
                       int flags,
                       int *out_flags) {
  TUint desired_access;
  TUint share_mode;
  TInt err = KErrNone;
  TFileName name_utf16;
  SymbianFile *file = ConvertToSymbianFile(id);

  if (out_flags) {
    *out_flags = flags;
  }

  // if the name is NULL we have to open a temporary file.
  if (!name) {
    TPath private_path;
    TFileName file_name;
    if (g_fs_session.PrivatePath(private_path) != KErrNone) {
      return SQLITE_CANTOPEN;
    }
    if (file->handle.Temp(g_fs_session,
                          private_path,
                          file_name,
                          EFileWrite) !=
        KErrNone) {
      return SQLITE_CANTOPEN;
    }
    file->methods = &SymbianIoMethod;
    file->lock_type = NO_LOCK;
    file->shared_lock_byte = 0;
    OpenCounter(+1);
    return SQLITE_OK;
  }

  if (UTF8ToUTF16(name, &name_utf16) != KErrNone)
    return SQLITE_CANTOPEN;

  if (flags & SQLITE_OPEN_READWRITE) {
    desired_access = EFileWrite;
  } else {
    desired_access = EFileRead;
  }
  if (flags & SQLITE_OPEN_MAIN_DB) {
    share_mode = EFileShareReadersOrWriters;
  } else {
    share_mode = 0;
  }

  if (flags & SQLITE_OPEN_CREATE) {
    err = file->handle.Create(g_fs_session,
                              name_utf16,
                              desired_access | share_mode);
    if (err != KErrNone && err != KErrAlreadyExists) {
      return SQLITE_CANTOPEN;
    }
  }

  if (err != KErrNone) {
    err = file->handle.Open(g_fs_session,
                            name_utf16,
                            desired_access | share_mode);
    if (err != KErrNone && flags & SQLITE_OPEN_READWRITE) {
      if (out_flags) {
        *out_flags = (flags | SQLITE_OPEN_READONLY) & ~SQLITE_OPEN_READWRITE;
      }
      desired_access = EFileRead;
      err = file->handle.Open(g_fs_session,
                              name_utf16,
                              desired_access | share_mode);
    }
    if (err != KErrNone) {
      return SQLITE_CANTOPEN;
    }
  }
  file->methods = &SymbianIoMethod;
  file->lock_type = NO_LOCK;
  file->shared_lock_byte = 0;
  OpenCounter(+1);
  return SQLITE_OK;
}

static int SymbianDelete(sqlite3_vfs */*vfs*/,
                         const char *file_name,
                         int /*sync_dir*/) {
  assert(file_name);
  TFileName file_name_utf16;

  if (UTF8ToUTF16(file_name, &file_name_utf16) != KErrNone) {
    return SQLITE_ERROR;
  }

  TInt result = g_fs_session.Delete(file_name_utf16);
  return (result == KErrNone || result == KErrPathNotFound)?
         SQLITE_OK : SQLITE_IOERR_DELETE;
}

static int SymbianAccess(sqlite3_vfs */*vfs*/,
                         const char *file_name,
                         int flags,
                         int *result) {
  assert(file_name);
  TEntry entry;
  TFileName file_name_utf16;

  if (UTF8ToUTF16(file_name, &file_name_utf16) != KErrNone) {
    return SQLITE_ERROR;
  }

  if (g_fs_session.Entry(file_name_utf16, entry) != KErrNone) {
    *result = 0;
    return SQLITE_OK;
  }

  switch (flags) {
    case SQLITE_ACCESS_READ:
    case SQLITE_ACCESS_EXISTS:
      *result = !entry.IsDir();
      break;
    case SQLITE_ACCESS_READWRITE:
      *result = !entry.IsDir() && !entry.IsReadOnly();
      break;
    default:
      return SQLITE_ERROR;
  }

  return SQLITE_OK;
}

static int SymbianFullPathname(sqlite3_vfs */*vfs*/,
                               const char *relative,
                               int full_len,
                               char *full) {
  assert(relative);
  assert(full);

  TParse parse;
  TPath relative_utf16;
  TPath base_path;
  TPtr8 full_utf8(reinterpret_cast<unsigned char*>(full), full_len);

  g_fs_session.PrivatePath(base_path);

  if (UTF8ToUTF16(relative, &relative_utf16) != KErrNone) {
    return SQLITE_ERROR;
  }

  if (parse.Set(relative_utf16, &base_path, NULL) != KErrNone) {
    return SQLITE_ERROR;
  }

  TDesC full_utf16(parse.FullName());
  if (UTF16ToUTF8(relative_utf16, &full_utf8) != KErrNone) {
    return SQLITE_ERROR;
  }

  full_utf8.PtrZ();
  return SQLITE_OK;
}

static int SymbianRandomness(sqlite3_vfs */*vfs*/, int buf_len, char *buffer) {
  assert(buffer);
  TInt64 seed = User::TickCount();
  for (TInt i = 0; i < buf_len; i++) {
    buffer[i] = Math::Rand(seed) % 255;
  }
  return SQLITE_OK;
}

static int SymbianSleep(sqlite3_vfs */*vfs*/, int microsec) {
  User::After(microsec);
  return SQLITE_OK;
}

int SymbianCurrentTime(sqlite3_vfs */*vfs*/, double *now) {
  _LIT(kEpoch, "19700101:000000.000000");
  assert(now);
  TTime time;
  TTime epoch_time(kEpoch);
  TTimeIntervalSeconds interval;

  time.HomeTime();
  // calculate seconds elapsed since 1-1-1970
  time.SecondsFrom(epoch_time, interval);

  // Julian date @ 1-1-1970 = 2440587.5
  // seconds per day = 86400.0
  *now = interval.Int()/86400.0 + 2440587.5;
  return SQLITE_OK;
}

static int SymbianGetLastError(sqlite3_vfs */*vfs*/,
                               int /*buf_len*/,
                               char */*buf*/) {
  assert(buf[0] == '\0');
  return 0;
}

// Interfaces for opening a shared library, finding entry points
// within the shared library, and closing the shared library.
// TODO(marcogelmi): implement.
#define SymbianDlOpen  0
#define SymbianDlError 0
#define SymbianDlSym   0
#define SymbianDlClose 0

// Initialize and deinitialize the operating system interface.
int sqlite3_os_init(void) {
  static sqlite3_vfs symbian_vfs = {
    1,                     // iVersion
    sizeof(SymbianFile),   // szOsFile
    KMaxPath,              // mxPathname
    0,                     // pNext
    "symbian",             // name
    0,                     // pAppData

    SymbianOpen,           // xOpen
    SymbianDelete,         // xDelete
    SymbianAccess,         // xAccess
    SymbianFullPathname,   // xFullPathname
    SymbianDlOpen,         // xDlOpen
    SymbianDlError,        // xDlError
    SymbianDlSym,          // xDlSym
    SymbianDlClose,        // xDlClose
    SymbianRandomness,     // xRandomness
    SymbianSleep,          // xSleep
    SymbianCurrentTime,    // xCurrentTime
    SymbianGetLastError    // xGetLastError
  };

  if (g_fs_session.Connect() != KErrNone) {
    return SQLITE_ERROR;
  }

  if (g_fs_session.ShareAuto() != KErrNone) {
    g_fs_session.Close();
    return SQLITE_ERROR;
  }

  sqlite3_vfs_register(&symbian_vfs, 1);
  return SQLITE_OK;
}

int sqlite3_os_end(void) {
  g_fs_session.Close();
  return SQLITE_OK;
}

#endif /* OS_SYMBIAN*/
