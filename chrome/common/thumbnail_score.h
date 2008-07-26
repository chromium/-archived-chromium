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

#ifndef CHROME_BROWSER_COMMON_THUMBNAIL_SCORE_H__
#define CHROME_BROWSER_COMMON_THUMBNAIL_SCORE_H__

#include "base/time.h"

// A set of metadata about a Thumbnail.
struct ThumbnailScore {
  // Initializes the ThumbnailScore to the absolute worst possible
  // values except for time, which is set to Now().
  ThumbnailScore();

  // Builds a ThumbnailScore with the passed in values, and sets the
  // thumbnail generation time to Now().
  ThumbnailScore(double score, bool clipping, bool top);

  // Builds a ThumbnailScore with the passed in values.
  ThumbnailScore(double score, bool clipping, bool top,
                 const Time& time);
  ~ThumbnailScore();

  // Tests for equivalence between two ThumbnailScore objects.
  bool Equals(const ThumbnailScore& rhs) const;

  // How "boring" a thumbnail is. The boring score is the 0,1 ranged
  // percentage of pixels that are the most common luma. Higher boring
  // scores indicate that a higher percentage of a bitmap are all the
  // same brightness (most likely the same color).
  double boring_score;

  // Whether the thumbnail was taken with height greater then
  // width. In cases where we don't have |good_clipping|, the
  // thumbnails are either clipped from the horizontal center of the
  // window, or are otherwise weirdly stretched.
  bool good_clipping;

  // Whether this thumbnail was taken while the renderer was
  // displaying the top of the page. Most pages are more recognizable
  // by their headers then by a set of random text half way down the
  // page; i.e. most MediaWiki sites would be indistinguishable by
  // thumbnails with |at_top| set to false.
  bool at_top;

  // Record the time when a thumbnail was taken. This is used to make
  // sure thumbnails are kept fresh.
  Time time_at_snapshot;

  // How bad a thumbnail needs to be before we completely ignore it.
  static const double kThumbnailMaximumBoringness;

  // Time before we take a worse thumbnail (subject to
  // kThumbnailMaximumBoringness) over what's currently in the database
  // for freshness.
  static const TimeDelta kUpdateThumbnailTime;

  // Penalty of how much more boring a thumbnail should be per hour.
  static const double kThumbnailDegradePerHour;
};

// Checks whether we should replace one thumbnail with another.
bool ShouldReplaceThumbnailWith(const ThumbnailScore& current,
                                const ThumbnailScore& replacement);

#endif  // CHROME_BROWSER_COMMON_THUMBNAIL_SCORE_H__
