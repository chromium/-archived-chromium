// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download creation struct used for querying the history service.

#ifndef CHROME_BROWSER_DOWNLOAD_TYPES_H__
#define CHROME_BROWSER_DOWNLOAD_TYPES_H__

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/time.h"

// Used for informing the download database of a new download, where we don't
// want to pass DownloadItems between threads. The history service also uses a
// vector of these structs for passing us the state of all downloads at
// initialization time (see DownloadQueryInfo below).
struct DownloadCreateInfo {
  DownloadCreateInfo(const FilePath& path,
                     const std::wstring& url,
                     base::Time start_time,
                     int64 received_bytes,
                     int64 total_bytes,
                     int32 state,
                     int32 download_id)
      : path(path),
        url(url),
        path_uniquifier(0),
        start_time(start_time),
        received_bytes(received_bytes),
        total_bytes(total_bytes),
        state(state),
        download_id(download_id),
        render_process_id(-1),
        render_view_id(-1),
        request_id(-1),
        db_handle(0),
        save_as(false),
        is_dangerous(false) {
  }

  DownloadCreateInfo() : download_id(-1) {}

  // DownloadItem fields
  FilePath path;
  std::wstring url;
  FilePath suggested_path;
  // A number that should be added to the suggested path to make it unique.
  // 0 means no number should be appended.  Not actually stored in the db.
  int path_uniquifier;
  base::Time start_time;
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
  // Whether this download is potentially dangerous (ex: exe, dll, ...).
  bool is_dangerous;
  // The original name for a dangerous download.
  FilePath original_name;
};

#endif  // CHROME_BROWSER_DOWNLOAD_TYPES_H__

