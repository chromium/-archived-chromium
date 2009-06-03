// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/bookmark_bubble_gtk.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/info_bubble_gtk.h"
#include "grit/generated_resources.h"

namespace {

// We basically have a singleton, since a bubble is sort of app-modal.  This
// keeps track of the currently open bubble, or NULL if none is open.
BookmarkBubbleGtk* g_bubble = NULL;

// TODO(deanm): Just a temporary state to keep track of the last entry in the
// combo box.  This makes sure we are catching the closing events right and
// saving the state.
gint g_last_active = 0;

void HandleCloseButton(GtkWidget* widget,
                       gpointer user_data) {
  reinterpret_cast<InfoBubbleGtk*>(user_data)->Close();
}

}  // namespace

// static
void BookmarkBubbleGtk::Show(GtkWindow* transient_toplevel,
                             const gfx::Rect& rect,
                             Profile* profile,
                             const GURL& url,
                             bool newly_bookmarked) {
  // TODO(deanm): The Views code deals with the possibility of a bubble already
  // being open, and then it just does nothing.  I am not sure how this could
  // happen with the style of our GTK bubble since it has a grab.  I would also
  // think that closing the previous bubble and opening the new one would make
  // more sense, but I guess then you would commit the bubble's changes.
  DCHECK(!g_bubble);
  g_bubble = new BookmarkBubbleGtk(transient_toplevel, rect, profile,
                                   url, newly_bookmarked);
}

void BookmarkBubbleGtk::InfoBubbleClosing(InfoBubbleGtk* info_bubble,
                                          bool closed_by_escape) {
  // Possibly commit any bookmark changes here...
  g_last_active = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_));
}

BookmarkBubbleGtk::BookmarkBubbleGtk(GtkWindow* transient_toplevel,
                                     const gfx::Rect& rect,
                                     Profile* profile,
                                     const GURL& url,
                                     bool newly_bookmarked)
    : profile_(profile),
      newly_bookmarked_(newly_bookmarked),
      content_(NULL),
      combo_(NULL) {
  GtkWidget* label = gtk_label_new(l10n_util::GetStringUTF8(
      newly_bookmarked_ ? IDS_BOOMARK_BUBBLE_PAGE_BOOKMARKED :
                          IDS_BOOMARK_BUBBLE_PAGE_BOOKMARK).c_str());
  GtkWidget* remove_button = gtk_chrome_link_button_new(
      l10n_util::GetStringUTF8(IDS_BOOMARK_BUBBLE_REMOVE_BOOKMARK).c_str());
  GtkWidget* name_label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_BOOMARK_BUBBLE_TITLE_TEXT).c_str());
  GtkWidget* folder_label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_BOOMARK_BUBBLE_FOLDER_TEXT).c_str());
  GtkWidget* edit_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_BOOMARK_BUBBLE_OPTIONS).c_str());
  GtkWidget* close_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_CLOSE).c_str());

  GtkWidget* content = gtk_vbox_new(FALSE, 5);
  GtkWidget* top = gtk_hbox_new(FALSE, 0);

  gtk_misc_set_alignment(GTK_MISC(label), 0, 1);
  gtk_box_pack_start(GTK_BOX(top), label,
                     TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(top), remove_button,
                     FALSE, FALSE, 0);

  // Use a combo box just to make sure popup windows work in the bubble content
  // and we're not fighting with the bubble for the grab.
  combo_ = gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo_), "entry 1");
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo_), "entry 2");
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo_), "entry 3");
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo_), g_last_active);

  GtkWidget* name_entry = gtk_entry_new();

  GtkWidget* table = gtk_table_new(2, 2, FALSE);
  gtk_table_set_row_spacings(GTK_TABLE(table), 5);
  gtk_table_set_col_spacings(GTK_TABLE(table), 10);
  gtk_table_attach_defaults(GTK_TABLE(table),
                            name_label,
                            0, 1, 0, 1);
  gtk_table_attach_defaults(GTK_TABLE(table),
                            name_entry,
                            1, 2, 0, 1);
  gtk_table_attach_defaults(GTK_TABLE(table),
                            folder_label,
                            0, 1, 1, 2);
  gtk_table_attach_defaults(GTK_TABLE(table),
                            combo_,
                            1, 2, 1, 2);

  GtkWidget* bottom = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(bottom), gtk_label_new(""),
                     TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(bottom), edit_button,
                     FALSE, FALSE, 4);
  gtk_box_pack_start(GTK_BOX(bottom), close_button,
                     FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(content), top, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(content), table, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(content), bottom, TRUE, TRUE, 0);
  // We want the focus to start on the entry, not on the remove button.
  gtk_container_set_focus_child(GTK_CONTAINER(content), table);

  g_signal_connect(content, "destroy",
                   G_CALLBACK(&HandleDestroyThunk), this);

  // TODO(deanm): In the future we might want to hang on to the returned
  // InfoBubble so that we can call Close() on it.
  InfoBubbleGtk* bubble = InfoBubbleGtk::Show(transient_toplevel,
                                              rect, content, this);
  if (!bubble) {
    NOTREACHED();
  }

  g_signal_connect(close_button, "clicked",
                   G_CALLBACK(&HandleCloseButton), bubble);
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
