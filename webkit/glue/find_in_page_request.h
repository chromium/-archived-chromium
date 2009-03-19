// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_FIND_IN_PAGE_REQUEST_H__
#define WEBKIT_GLUE_FIND_IN_PAGE_REQUEST_H__

#include "base/string16.h"

// Parameters for a find in page request.
struct FindInPageRequest {
  // The id for the request, this message is passed in by the caller and
  // returned back with the reply so that the caller can start a new search and
  // choose to ignore packets that belonged to the last search.
  int request_id;

  // The word(s) to find on the page.
  string16 search_string;

  // Whether to search forward or backward within the page.
  bool forward;

  // Whether search should be Case sensitive.
  bool match_case;

  // Whether this operation is first request (Find) or a follow-up (FindNext).
  bool find_next;
};

#endif  // WEBKIT_GLUE_FIND_IN_PAGE_REQUEST_H__
