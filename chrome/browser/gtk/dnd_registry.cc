// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dnd_registry.h"

namespace dnd {

GtkTargetList* GetTargetListFromCodeMask(int code_mask) {
  GtkTargetList* targets = gtk_target_list_new(NULL, 0);

  if (code_mask & X_CHROME_TAB) {
    GdkAtom tab_atom = gdk_atom_intern(
        const_cast<char*>("application/x-chrome-tab"), false);
    gtk_target_list_add(targets, tab_atom, GTK_TARGET_SAME_APP,
                        X_CHROME_TAB);
  }

  if (code_mask & X_CHROME_TEXT_PLAIN)
    gtk_target_list_add_text_targets(targets, X_CHROME_TEXT_PLAIN);

  if (code_mask & X_CHROME_TEXT_URI_LIST)
    gtk_target_list_add_uri_targets(targets, X_CHROME_TEXT_URI_LIST);

  if (code_mask & X_CHROME_BOOKMARK_ITEM) {
    GdkAtom bookmark_atom = gdk_atom_intern(
        const_cast<char*>("application/x-chrome-bookmark-item"), false);
    gtk_target_list_add(targets, bookmark_atom, GTK_TARGET_SAME_APP,
                        X_CHROME_BOOKMARK_ITEM);
  }

  return targets;
}

void SetDestTargetListFromCodeMask(GtkWidget* dest, int code_mask) {
  GtkTargetList* targets = GetTargetListFromCodeMask(code_mask);
  gtk_drag_dest_set_target_list(dest, targets);
  gtk_target_list_unref(targets);
}

void SetSourceTargetListFromCodeMask(GtkWidget* source, int code_mask) {
  GtkTargetList* targets = GetTargetListFromCodeMask(code_mask);
  gtk_drag_source_set_target_list(source, targets);
  gtk_target_list_unref(targets);
}

}  // namespace dnd
