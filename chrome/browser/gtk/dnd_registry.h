// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_DND_REGISTRY_H_
#define CHROME_BROWSER_GTK_DND_REGISTRY_H_

#include <gtk/gtk.h>

// We wrap all of these constants in a namespace to prevent pollution.
namespace dnd {

// Registry of all internal int codes for drag and drop.
//
// These ids need to be unique app wide. Simply adding a GtkTargetEntry with an
// id of 0 should be an error and it will conflict with X_CHROME_TAB below.
enum {
  X_CHROME_TAB = 0,
  X_CHROME_TEXT_PLAIN = 1 << 0,
  X_CHROME_TEXT_URI_LIST = 1 << 1,
  X_CHROME_BOOKMARK_ITEM = 1 << 2,
};

// Creates a target list from the given mask. The mask should be an OR of
// X_CHROME_* values. The target list is returned with ref count 1; the caller
// is responsible for unreffing it when it is no longer needed.
GtkTargetList* GetTargetListFromCodeMask(int code_mask);

// Set the drag target list for |dest| or |source| with the target list that
// corresponds to |code_mask|.
void SetDestTargetListFromCodeMask(GtkWidget* dest, int code_mask);
void SetSourceTargetListFromCodeMask(GtkWidget* source, int code_mask);

}  // namespace dnd

#endif  // CHROME_BROWSER_GTK_DND_REGISTRY_H_
