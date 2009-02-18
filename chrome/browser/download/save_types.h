// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_SAVE_TYPES_H__
#define CHROME_BROWSER_DOWNLOAD_SAVE_TYPES_H__

#include <vector>
#include <utility>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "googleurl/src/gurl.h"

typedef std::vector<std::pair<int, FilePath> > FinalNameList;
typedef std::vector<int> SaveIDList;

// This structure is used to handle and deliver some info
// when processing each save item job.
struct SaveFileCreateInfo {
  enum SaveFileSource {
    // This type indicates the save item that need to be retrieved
    // from the network.
    SAVE_FILE_FROM_NET = 0,
    // This type indicates the save item that need to be retrieved
    // from serializing DOM.
    SAVE_FILE_FROM_DOM,
    // This type indicates the save item that need to be retrieved
    // through local file system.
    SAVE_FILE_FROM_FILE
  };

  SaveFileCreateInfo(const FilePath& path,
                     const GURL& url,
                     SaveFileSource save_source,
                     int32 save_id)
      : path(path),
        url(url),
        save_id(save_id),
        render_process_id(-1),
        render_view_id(-1),
        request_id(-1),
        total_bytes(0),
        save_source(save_source) {
  }

  SaveFileCreateInfo() : save_id(-1) {}

  // SaveItem fields.
  // The local file path of saved file.
  FilePath path;
  // Original URL of the saved resource.
  GURL url;
  // Final URL of the saved resource since some URL might be redirected.
  GURL final_url;
  // The unique identifier for saving job, assigned at creation by
  // the SaveFileManager for its internal record keeping.
  int save_id;
  // IDs for looking up the tab we are associated with.
  int render_process_id;
  int render_view_id;
  // Handle for informing the ResourceDispatcherHost of a UI based cancel.
  int request_id;
  // Disposition info from HTTP response.
  std::string content_disposition;
  // Total bytes of saved file.
  int64 total_bytes;
  // Source type of saved file.
  SaveFileSource save_source;
};

#endif  // CHROME_BROWSER_DOWNLOAD_SAVE_TYPES_H__

