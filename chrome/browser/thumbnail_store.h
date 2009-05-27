// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_STORE_H_
#define CHROME_BROWSER_THUMBNAIL_STORE_H_

#include <vector>

#include "base/file_path.h"

class GURL;
class SkBitmap;
struct ThumbnailScore;
namespace base {
class Time;
}

// This storage interface provides storage for the thumbnails used
// by the new_tab_ui.
class ThumbnailStore {
 public:
  ThumbnailStore();
  ~ThumbnailStore();

  // Must be called after creation but before other methods are called.
  // file_path is where a new database should be created or the
  // location of an existing databse.
  // If false is returned, no other methods should be called.
  bool Init(const FilePath& file_path);

  // Stores the given thumbnail and score with the associated url.
  bool SetPageThumbnail(const GURL& url,
                        const SkBitmap& thumbnail,
                        const ThumbnailScore& score,
                        const base::Time& time);

  // Retrieves the thumbnail and score for the given url.
  // Returns false if there is not data for the given url or some other
  // error occurred.
  bool GetPageThumbnail(const GURL& url,
                        SkBitmap* thumbnail,
                        ThumbnailScore* score);

 private:
  // The location of the thumbnail store.
  FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailStore);
};

#endif  // CHROME_BROWSER_THUMBNAIL_STORE_H_
