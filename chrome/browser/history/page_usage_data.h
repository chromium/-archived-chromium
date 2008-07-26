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

#ifndef CHROME_BROWSER_HISTORY_PAGE_USAGE_DATA_H__
#define CHROME_BROWSER_HISTORY_PAGE_USAGE_DATA_H__

#include "SkBitmap.h"

#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_types.h"
#include "googleurl/src/gurl.h"

/////////////////////////////////////////////////////////////////////////////
//
// PageUsageData
//
// A per domain usage data structure to compute and manage most visited
// pages.
//
// See History::QueryPageUsageSince()
//
/////////////////////////////////////////////////////////////////////////////
class PageUsageData {
 public:
  PageUsageData(history::URLID id)
      : id_(id),
        thumbnail_(NULL),
        thumbnail_set_(false),
        thumbnail_pending_(false),
        favicon_(NULL),
        favicon_set_(false),
        favicon_pending_(false),
        score_(0.0) {
  }

  virtual ~PageUsageData() {
    delete thumbnail_;
    delete favicon_;
  }

  // Return the url ID
  history::URLID GetID() const {
    return id_;
  }

  void SetURL(const GURL& url) {
    url_ = url;
  }

  const GURL& GetURL() const {
    return url_;
  }

  void SetTitle(const std::wstring& s) {
    title_ = s;
  }

  const std::wstring& GetTitle() const {
    return title_;
  }

  void SetScore(double v) {
    score_ = v;
  }

  double GetScore() const {
    return score_;
  }

  void SetThumbnailMissing() {
    thumbnail_set_ = true;
  }

  void SetThumbnail(SkBitmap* img) {
    if (thumbnail_ && thumbnail_ != img)
      delete thumbnail_;

    thumbnail_ = img;
    thumbnail_set_ = true;
  }

  bool HasThumbnail() const {
    return thumbnail_set_;
  }

  const SkBitmap* GetThumbnail() const {
    return thumbnail_;
  }

  bool thumbnail_pending() const {
    return thumbnail_pending_;
  }

  void set_thumbnail_pending(bool pending) {
    thumbnail_pending_ = pending;
  }

  void SetFavIconMissing() {
    favicon_set_ = true;
  }

  void SetFavIcon(SkBitmap* img) {
    if (favicon_ && favicon_ != img)
      delete favicon_;
    favicon_ = img;
    favicon_set_ = true;
  }

  bool HasFavIcon() const {
    return favicon_set_;
  }

  bool favicon_pending() const {
    return favicon_pending_;
  }

  void set_favicon_pending(bool pending) {
    favicon_pending_ = pending;
  }

  const SkBitmap* GetFavIcon() const {
    return favicon_;
  }

  // Sort predicate to sort instances by score (high to low)
  static bool Predicate(const PageUsageData* dud1,
                        const PageUsageData* dud2);

 private:
  history::URLID id_;
  GURL url_;
  std::wstring title_;

  SkBitmap* thumbnail_;
  bool thumbnail_set_;
  // Whether we have an outstanding request for the thumbnail.
  bool thumbnail_pending_;

  SkBitmap* favicon_;
  bool favicon_set_;
  // Whether we have an outstanding request for the favicon.
  bool favicon_pending_;

  double score_;
};

#endif  // CHROME_BROWSER_HISTORY_PAGE_USAGE_DATA_H__
