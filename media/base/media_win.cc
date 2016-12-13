// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"

#include <windows.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"

namespace media {

namespace {

enum FFmpegDLLKeys {
  FILE_LIBAVCODEC,       // full path to libavcodec media decoding library.
  FILE_LIBAVFORMAT,      // full path to libavformat media parsing library.
  FILE_LIBAVUTIL,        // full path to libavutil media utility library.
};

// Retrieves the DLLName for the given key.
FilePath::CharType* GetDLLName(FFmpegDLLKeys dll_key) {
  // TODO(ajwong): Do we want to lock to a specific ffmpeg version?
  switch (dll_key) {
    case FILE_LIBAVCODEC:
      return FILE_PATH_LITERAL("avcodec-52.dll");
    case FILE_LIBAVFORMAT:
      return FILE_PATH_LITERAL("avformat-52.dll");
    case FILE_LIBAVUTIL:
      return FILE_PATH_LITERAL("avutil-50.dll");
    default:
      LOG(DFATAL) << "Invalid DLL key requested: " << dll_key;
      return FILE_PATH_LITERAL("");
  }
}

}  // namespace

// Attempts to initialize the media library (loading DLLs, DSOs, etc.).
// Returns true if everything was successfully initialized, false otherwise.
bool InitializeMediaLibrary(const FilePath& base_path) {
  FFmpegDLLKeys path_keys[] = {
    media::FILE_LIBAVCODEC,
    media::FILE_LIBAVFORMAT,
    media::FILE_LIBAVUTIL
  };
  HMODULE libs[arraysize(path_keys)] = {NULL};
  for (size_t i = 0; i < arraysize(path_keys); ++i) {
    FilePath path = base_path.Append(GetDLLName(path_keys[i]));
    libs[i] = LoadLibrary(path.value().c_str());
    if (!libs[i])
      break;
  }

  // Check that we loaded all libraries successfully.  We only need to check the
  // last array element because the loop above will break without initializing
  // it on any prior error.
  if (libs[arraysize(libs)-1])
    return true;

  // Free any loaded libraries if we weren't successful.
  for (size_t i = 0; i < arraysize(libs) && libs[i] != NULL; ++i) {
    FreeLibrary(libs[i]);
    libs[i] = NULL;  // Just to be safe.
  }
  return false;
}

}  // namespace media
