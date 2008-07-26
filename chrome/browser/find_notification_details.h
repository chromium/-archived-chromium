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

#ifndef CHROME_BROWSER_FIND_NOTIFICATION_DETAILS_H__
#define CHROME_BROWSER_FIND_NOTIFICATION_DETAILS_H__

#include "base/basictypes.h"
#include "base/gfx/rect.h"

class FindNotificationDetails {
 public:
  FindNotificationDetails(int request_id,
                          int number_of_matches,
                          const gfx::Rect& selection_rect,
                          int active_match_ordinal,
                          bool final_update)
    : request_id_(request_id),
      number_of_matches_(number_of_matches),
      selection_rect_(selection_rect),
      active_match_ordinal_(active_match_ordinal),
      final_update_(final_update) {}

  ~FindNotificationDetails() {}

  int request_id() const { return request_id_; }

  int number_of_matches() const { return number_of_matches_; }

  gfx::Rect selection_rect() const { return selection_rect_; }

  int active_match_ordinal() const { return active_match_ordinal_; }

  bool final_update() const { return final_update_; }

 private:
  int request_id_;    // The find-in-page request whose results we're returning.
  int number_of_matches_;  // How many matches were found.
  gfx::Rect selection_rect_;  // Where selection occurred (screen coordinate).
  int active_match_ordinal_;  // The ordinal of the currently selected match.
  bool final_update_;  // Whether this is the last Find Result update.

  FindNotificationDetails() {}

  DISALLOW_EVIL_CONSTRUCTORS(FindNotificationDetails);
};

#endif  // CHROME_BROWSER_FIND_NOTIFICATION_DETAILS_H__
