// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/bookmark_bubble_gtk.h"

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "chrome/browser/gtk/info_bubble_gtk.h"

// static
void BookmarkBubbleGtk::Show(const gfx::Rect& rect,
                             Profile* profile,
                             const GURL& url,
                             bool newly_bookmarked) {
  // TODO(deanm): Implement the real bookmark bubble.  For now we just have
  // a placeholder for testing that input and focus works correctly.
  GtkWidget* content = gtk_vbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(content), gtk_label_new("Hej!"), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(content), gtk_entry_new(), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(content), gtk_entry_new(), TRUE, TRUE, 0);
  InfoBubbleGtk* bubble = InfoBubbleGtk::Show(rect, content);
  DCHECK(bubble);
}
