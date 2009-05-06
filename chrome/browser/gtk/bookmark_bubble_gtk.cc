// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/bookmark_bubble_gtk.h"

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "chrome/browser/gtk/info_bubble_gtk.h"

namespace {

// We basically have a singleton, since a bubble is sort of app-modal.  This
// keeps track of the currently open bubble, or NULL if none is open.
BookmarkBubbleGtk* g_bubble = NULL;

// TODO(deanm): Just a temporary state to keep track of the last entry in the
// combo box.  This makes sure we are catching the closing events right and
// saving the state.
gint g_last_active = 0;

}  // namespace

// static
void BookmarkBubbleGtk::Show(const gfx::Rect& rect,
                             Profile* profile,
                             const GURL& url,
                             bool newly_bookmarked) {
  // TODO(deanm): The Views code deals with the possibility of a bubble already
  // being open, and then it just does nothing.  I am not sure how this could
  // happen with the style of our GTK bubble since it has a grab.  I would also
  // think that closing the previous bubble and opening the new one would make
  // more sense, but I guess then you would commit the bubble's changes.
  DCHECK(!g_bubble);
  g_bubble = new BookmarkBubbleGtk(rect, profile, url, newly_bookmarked);
}

void BookmarkBubbleGtk::InfoBubbleClosing(InfoBubbleGtk* info_bubble,
                                          bool closed_by_escape) {
  // Possibly commit any bookmark changes here...
  g_last_active = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_));
}

BookmarkBubbleGtk::BookmarkBubbleGtk(const gfx::Rect& rect,
                                     Profile* profile,
                                     const GURL& url,
                                     bool newly_bookmarked)
    : profile_(profile),
      newly_bookmarked_(newly_bookmarked),
      content_(NULL),
      combo_(NULL) {
  // TODO(deanm): Implement the real bookmark bubble.  For now we just have
  // a placeholder for testing that input and focus works correctly.
  GtkWidget* content = gtk_vbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(content), gtk_label_new("Hej!"), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(content), gtk_entry_new(), TRUE, TRUE, 0);
  // Use a combo box just to make sure popup windows work in the bubble content
  // and we're not fighting with the bubble for the grab.
  combo_ = gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo_), "entry 1");
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo_), "entry 2");
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo_), "entry 3");
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo_), g_last_active);
  gtk_box_pack_start(GTK_BOX(content), combo_, TRUE, TRUE, 0);

  g_signal_connect(content, "destroy",
                   G_CALLBACK(&HandleDestroyThunk), this);

  // TODO(deanm): In the future we might want to hang on to the returned
  // InfoBubble so that we can call Close() on it.
  if (!InfoBubbleGtk::Show(rect, content, this)) {
    NOTREACHED();
  }
}

BookmarkBubbleGtk::~BookmarkBubbleGtk() {
  DCHECK(!content_);  // |content_| should have already been destroyed.

  DCHECK(g_bubble);
  g_bubble = NULL;
}

gboolean BookmarkBubbleGtk::HandleDestroy() {
  // We are self deleting, we have a destroy signal setup to catch when we
  // destroyed (via the InfoBubble being destroyed), and delete ourself.
  content_ = NULL;  // We are being destroyed.
  delete this;
  return FALSE;  // Propagate.
}
