// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WINDOW_OPEN_DISPOSITION_H__
#define WEBKIT_GLUE_WINDOW_OPEN_DISPOSITION_H__

enum WindowOpenDisposition {
  SUPPRESS_OPEN,
  CURRENT_TAB,
  NEW_FOREGROUND_TAB,
  NEW_BACKGROUND_TAB,
  NEW_POPUP,
  NEW_WINDOW,
  SAVE_TO_DISK,
  OFF_THE_RECORD,
  IGNORE_ACTION
};

#endif  // WEBKIT_GLUE_WINDOW_OPEN_DISPOSITION_H__
