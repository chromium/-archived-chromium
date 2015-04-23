// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Some Google related utility functions.

#ifndef CHROME_BROWSER_GOOGLE_UTIL_H__
#define CHROME_BROWSER_GOOGLE_UTIL_H__

class GURL;

namespace google_util {

// Adds the Google locale string to the URL (e.g., hl=en-US).  This does not
// check to see if the param already exists.
GURL AppendGoogleLocaleParam(const GURL& url);

// Adds the Google TLD string to the URL (e.g., sd=com).  This does not
// check to see if the param already exists.
GURL AppendGoogleTLDParam(const GURL& url);

}  // namespace google_util

#endif  // CHROME_BROWSER_GOOGLE_UTIL_H__
