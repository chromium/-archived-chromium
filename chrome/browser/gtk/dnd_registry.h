// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_DND_REGISTRY_H_
#define CHROME_BROWSER_GTK_DND_REGISTRY_H_

// We wrap all of these constants in a namespace to prevent pollution.
namespace dnd {

// Registry of all internal int codes for drag and drop.
//
// These ids need to be unique app wide. Simply adding a GtkTargetEntry with an
// id of 0 should be an error and it will conflict with X_CHROME_TAB below.
enum {
  // Tab DND items:
  X_CHROME_TAB = 0,

  // Tabstrip DND items:
  X_CHROME_STRING,
  X_CHROME_TEXT_PLAIN,
  X_CHROME_TEXT_URI_LIST,

  // Bookmark DND items:
  X_CHROME_BOOKMARK_ITEM
};

};

#endif  // CHROME_BROWSER_GTK_DND_REGISTRY_H_
