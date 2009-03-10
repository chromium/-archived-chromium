// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/lzma_util.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"

extern "C" {
#include "third_party/lzma_sdk/Archive/7z/7zExtract.h"
#include "third_party/lzma_sdk/Archive/7z/7zIn.h"
#include "third_party/lzma_sdk/7zCrc.h"
}


namespace installer {

typedef struct _CFileInStream {
  ISzInStream InStream;
  HANDLE File;
} CFileInStream;


size_t LzmaReadFile(HANDLE file, void *data, size_t size) {
  if (size == 0)
    return 0;

  size_t processedSize = 0;
  do {
    DWORD processedLoc = 0;
    BOOL res = ReadFile(file, data, (DWORD) size, &processedLoc, NULL);
    data = (void *)((unsigned char *) data + processedLoc);
    size -= processedLoc;
    processedSize += processedLoc;
    if (!res || processedLoc == 0)
      break;
  } while (size > 0);

  return processedSize;
}

SZ_RESULT SzFileSeekImp(void *object, CFileSize pos) {
  CFileInStream *s = (CFileInStream *) object;
  LARGE_INTEGER value;
  value.LowPart = (DWORD) pos;
  value.HighPart = (LONG) ((UInt64) pos >> 32);
  value.LowPart = SetFilePointer(s->File, value.LowPart, &value.HighPart,
                                 FILE_BEGIN);
  if (value.LowPart == 0xFFFFFFFF) {
    if(GetLastError() != NO_ERROR) {
      return SZE_FAIL;
    }
  }
  return SZ_OK;
}

SZ_RESULT SzFileReadImp(void *object, void **buffer,
                        size_t maxRequiredSize, size_t *processedSize) {
  const int kBufferSize = 1 << 12;
  static Byte g_Buffer[kBufferSize];
  if (maxRequiredSize > kBufferSize) {
    maxRequiredSize = kBufferSize;
  }

  CFileInStream *s = (CFileInStream *) object;
  size_t processedSizeLoc;
  processedSizeLoc = LzmaReadFile(s->File, g_Buffer, maxRequiredSize);
  *buffer = g_Buffer;
  if (processedSize != 0) {
    *processedSize = processedSizeLoc;
  }
  return SZ_OK;
}


LzmaUtil::LzmaUtil() : archive_handle_(NULL) {}

LzmaUtil::~LzmaUtil() {
  if (archive_handle_) CloseArchive();
}

DWORD LzmaUtil::OpenArchive(const std::wstring& archivePath) {
  DWORD ret = NO_ERROR;
  archive_handle_ = CreateFile(archivePath.c_str(), GENERIC_READ,
      FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (archive_handle_ == INVALID_HANDLE_VALUE) {
    ret = GetLastError();
  }
  return ret;
}

// Unpacks the archive to the given location.
DWORD LzmaUtil::UnPack(const std::wstring& location) {
  CFileInStream archiveStream;
  ISzAlloc allocImp;
  ISzAlloc allocTempImp;
  CArchiveDatabaseEx db;
  DWORD ret = NO_ERROR;

  archiveStream.File = archive_handle_;
  archiveStream.InStream.Read = SzFileReadImp;
  archiveStream.InStream.Seek = SzFileSeekImp;
  allocImp.Alloc = SzAlloc;
  allocImp.Free = SzFree;
  allocTempImp.Alloc = SzAllocTemp;
  allocTempImp.Free = SzFreeTemp;

  CrcGenerateTable();
  SzArDbExInit(&db);
  if ((ret = SzArchiveOpen(&archiveStream.InStream, &db,
                           &allocImp, &allocTempImp)) != SZ_OK) {
    return ret;
  }

  Byte *outBuffer = 0; // it must be 0 before first call for each new archive
  UInt32 blockIndex = 0xFFFFFFFF; // can have any value if outBuffer = 0
  size_t outBufferSize = 0;  // can have any value if outBuffer = 0
  for (unsigned int i = 0; i < db.Database.NumFiles; i++) {
    DWORD written;
    size_t offset;
    size_t outSizeProcessed;
    CFileItem *f = db.Database.Files + i;

    if ((ret = SzExtract(&archiveStream.InStream, &db, i, &blockIndex,
                         &outBuffer, &outBufferSize, &offset, &outSizeProcessed,
                         &allocImp, &allocTempImp)) != SZ_OK) {
      break;
    }

    // Append location to the file path in archive, to get full path.
    std::wstring wfileName(location);
    file_util::AppendToPath(&wfileName, UTF8ToWide(f->Name));

    // If archive entry is directory create it and move on to the next entry.
    if (f->IsDirectory) {
      file_util::CreateDirectory(wfileName);
      continue;
    }

    HANDLE hFile;
    std::wstring directory = file_util::GetDirectoryFromPath(wfileName);
    file_util::CreateDirectory(directory);

    hFile = CreateFile(wfileName.c_str(), GENERIC_WRITE, 0, NULL,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)  {
      ret = GetLastError();
      break;
    }

    WriteFile(hFile, outBuffer + offset, (DWORD) outSizeProcessed,
              &written, NULL);
    if (written != outSizeProcessed) {
      ret = GetLastError();
      CloseHandle(hFile);
      break;
    }

    if (f->IsLastWriteTimeDefined) {
      if (!SetFileTime(hFile, NULL, NULL,
                       (const FILETIME *)&(f->LastWriteTime))) {
        ret = GetLastError();
        CloseHandle(hFile);
        break;
      }
    }
    if (!CloseHandle(hFile)) {
      ret = GetLastError();
      break;
    }
  }  // for loop

  allocImp.Free(outBuffer);
  SzArDbExFree(&db, allocImp.Free);
  return ret;
}

void LzmaUtil::CloseArchive() {
  CloseHandle(archive_handle_);
  archive_handle_ = NULL;
}

}  // namespace installer
