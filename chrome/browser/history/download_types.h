// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Download creation struct used for querying the history service.

#ifndef CHROME_BROWSER_DOWNLOAD_TYPES_H__
#define CHROME_BROWSER_DOWNLOAD_TYPES_H__

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/time.h"

// Used for informing the download database of a new download, where we don't
// want to pass DownloadItems between threads. The history service also uses a
// vector of these structs for passing us the state of all downloads at
// initialization time (see DownloadQueryInfo below).
struct DownloadCreateInfo {
  DownloadCreateInfo(const std::wstring& path,
                     const std::wstring& url,
                     Time start_time,
                     int64 received_bytes,
                     int64 total_bytes,
                     int32 state,
                     int32 download_id)
      : path(path),
        url(url),
        suggested_path_exists(false),
        start_time(start_time),
        received_bytes(received_bytes),
        total_bytes(total_bytes),
        state(state),
        download_id(download_id),
        render_process_id(-1),
        render_view_id(-1),
        request_id(-1),
        db_handle(0),
        save_as(false) {
  }

  DownloadCreateInfo() : download_id(-1) {}

  // DownloadItem fields
  std::wstring path;
  std::wstring url;
  std::wstring suggested_path;
  bool suggested_path_exists;
  Time start_time;
  int64 received_bytes;
  int64 total_bytes;
  int32 state;
  int32 download_id;
  int render_process_id;
  int render_view_id;
  int request_id;
  int64 db_handle;
  std::string content_disposition;
  std::string mime_type;
  bool save_as;
};

#endif  // CHROME_BROWSER_DOWNLOAD_TYPES_H__
