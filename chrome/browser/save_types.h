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

#ifndef CHROME_BROWSER_SAVE_TYPES_H__
#define CHROME_BROWSER_SAVE_TYPES_H__

#include <vector>
#include <utility>

#include "base/basictypes.h"

typedef std::vector<std::pair<int, std::wstring> > FinalNameList;
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

  SaveFileCreateInfo(const std::wstring& path,
                     const std::wstring& url,
                     SaveFileSource save_source,
                     int32 save_id)
      : path(path),
        url(url),
        save_id(save_id),
        save_source(save_source),
        render_process_id(-1),
        render_view_id(-1),
        request_id(-1),
        total_bytes(0) {
  }

  SaveFileCreateInfo() : save_id(-1) {}

  // SaveItem fields.
  // The local file path of saved file.
  std::wstring path;
  // Original URL of the saved resource.
  std::wstring url;
  // Final URL of the saved resource since some URL might be redirected.
  std::wstring final_url;
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

#endif  // CHROME_BROWSER_SAVE_TYPES_H__
