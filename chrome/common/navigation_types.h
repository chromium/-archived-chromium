// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NAVIGATION_TYPES_H__
#define CHROME_COMMON_NAVIGATION_TYPES_H__

// An enum for the type of navigation. This is used when calling
// the NotifyDidNavigate method on the active TabContents
enum NavigationType {
  // This is a new navigation resulting in a new entry in the session history
  NAVIGATION_NEW = 0,
  // Back or forward navigation within the session history
  NAVIGATION_BACK_FORWARD = 1,
  // This navigation simply replaces the URL of an existing entry in the
  // seesion history
  NAVIGATION_REPLACE = 2,
};

#endif  // CHROME_COMMON_NAVIGATION_TYPES_H__

