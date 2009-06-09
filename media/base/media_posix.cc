// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"

#include <string>

#include <dlfcn.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "third_party/ffmpeg/ffmpeg_stubs.h"

namespace tp_ffmpeg = third_party_ffmpeg;

namespace media {

namespace {

// Retrieves the DSOName for the given key.
std::string GetDSOName(tp_ffmpeg::StubModules stub_key) {
  // TODO(ajwong): Do we want to lock to a specific ffmpeg version?
  // TODO(port): These library names are incorrect for mac.  We need .dynlib
  // suffixes.
  switch (stub_key) {
    case tp_ffmpeg::kModuleAvcodec52:
      return FILE_PATH_LITERAL("libavcodec.so.52");
    case tp_ffmpeg::kModuleAvformat52:
      return FILE_PATH_LITERAL("libavformat.so.52");
    case tp_ffmpeg::kModuleAvutil50:
      return FILE_PATH_LITERAL("libavutil.so.50");
    default:
      LOG(DFATAL) << "Invalid stub module requested: " << stub_key;
      return FILE_PATH_LITERAL("");
  }
}

}  // namespace

// Attempts to initialize the media library (loading DSOs, etc.).
// Returns true if everything was successfully initialized, false otherwise.
bool InitializeMediaLibrary(const FilePath& module_dir) {
  // TODO(ajwong): We need error resolution.
  tp_ffmpeg::StubPathMap paths;
  for (int i = 0; i < static_cast<int>(tp_ffmpeg::kNumStubModules); ++i) {
    tp_ffmpeg::StubModules module = static_cast<tp_ffmpeg::StubModules>(i);
    FilePath path = module_dir.Append(GetDSOName(module));
    paths[module].push_back(path.value());
  }

  return tp_ffmpeg::InitializeStubs(paths);
}

}  // namespace media
