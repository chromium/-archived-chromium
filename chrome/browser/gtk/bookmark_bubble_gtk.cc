// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/bookmark_bubble_gtk.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/info_bubble_gtk.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"

namespace {

// We basically have a singleton, since a bubble is sort of app-modal.  This
// keeps track of the currently open bubble, or NULL if none is open.
BookmarkBubbleGtk* g_bubble = NULL;

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
}

BookmarkBubbleGtk::BookmarkBubbleGtk(GtkWindow* transient_toplevel,
                                     const gfx::Rect& rect,
                                     Profile* profile,
                                     const GURL& url,
                                     bool newly_bookmarked)
    : url_(url),
      profile_(profile),
      content_(NULL),
      name_entry_(NULL),
      folder_combo_(NULL),
      bubble_(NULL),
      newly_bookmarked_(newly_bookmarked),
      apply_edits_(true),
      remove_bookmark_(false) {
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

  // Our content is arrange in 3 rows.  |top| contains a left justified
  // message, and a right justified remove link button.  |table| is the middle
  // portion with the name entry and the folder combo.  |bottom| is the final
  // row with a spacer, and the edit... and close buttons on the right.
  GtkWidget* content = gtk_vbox_new(FALSE, 5);
  GtkWidget* top = gtk_hbox_new(FALSE, 0);

  gtk_misc_set_alignment(GTK_MISC(label), 0, 1);
  gtk_box_pack_start(GTK_BOX(top), label,
                     TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(top), remove_button,
                     FALSE, FALSE, 0);

  // TODO(deanm): We should show the bookmark bar folder along with the top
  // other choices and an entry to go into the bookmark editor.  Since we don't
  // have the editor up yet on Linux, just show the bookmark bar for now.
  BookmarkModel* model = profile_->GetBookmarkModel();
  BookmarkNode* bookmark_bar = model->GetBookmarkBarNode();
  folder_combo_ = gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(folder_combo_),
                            WideToUTF8(bookmark_bar->GetTitle()).c_str());
  // We default to the bookmark bar, so make it the active selection.
  gtk_combo_box_set_active(GTK_COMBO_BOX(folder_combo_), 0);

  // Create the edit entry for updating the bookmark name / title.
  name_entry_ = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(name_entry_), GetTitle().c_str());

  // We use a table to allow the labels to line up with each other, along
  // with the entry and folder combo lining up.
  GtkWidget* table = gtk_table_new(2, 2, FALSE);
  gtk_table_set_row_spacings(GTK_TABLE(table), 5);
  gtk_table_set_col_spacings(GTK_TABLE(table), 10);
  gtk_table_attach_defaults(GTK_TABLE(table),
                            name_label,
                            0, 1, 0, 1);
  gtk_table_attach_defaults(GTK_TABLE(table),
                            name_entry_,
                            1, 2, 0, 1);
  gtk_table_attach_defaults(GTK_TABLE(table),
                            folder_label,
                            0, 1, 1, 2);
  gtk_table_attach_defaults(GTK_TABLE(table),
                            folder_combo_,
                            1, 2, 1, 2);

  GtkWidget* bottom = gtk_hbox_new(FALSE, 0);
  // We want the buttons on the right, so just use an expanding label to fill
  // all of the extra space on the right.
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

  bubble_ = InfoBubbleGtk::Show(transient_toplevel,
                                rect, content, this);
  if (!bubble_) {
    NOTREACHED();
    return;
  }

  g_signal_connect(content, "destroy",
                   G_CALLBACK(&HandleDestroyThunk), this);
  g_signal_connect(name_entry_, "activate",
                   G_CALLBACK(&HandleNameActivateThunk), this);
  g_signal_connect(close_button, "clicked",
                   G_CALLBACK(&HandleCloseButtonThunk), this);
  g_signal_connect(remove_button, "clicked",
                   G_CALLBACK(&HandleRemoveButtonThunk), this);
}

BookmarkBubbleGtk::~BookmarkBubbleGtk() {
  DCHECK(!content_);  // |content_| should have already been destroyed.

  DCHECK(g_bubble);
  g_bubble = NULL;

  if (apply_edits_) {
    ApplyEdits();
  } else if (remove_bookmark_) {
    BookmarkModel* model = profile_->GetBookmarkModel();
    BookmarkNode* node = model->GetMostRecentlyAddedNodeForURL(url_);
    if (node)
      model->Remove(node->GetParent(), node->GetParent()->IndexOfChild(node));
  }
}

gboolean BookmarkBubbleGtk::HandleDestroy() {
  // We are self deleting, we have a destroy signal setup to catch when we
  // destroyed (via the InfoBubble being destroyed), and delete ourself.
  content_ = NULL;  // We are being destroyed.
  delete this;
  return FALSE;  // Propagate.
}

void BookmarkBubbleGtk::HandleNameActivate() {
  bubble_->Close();
}

void BookmarkBubbleGtk::HandleCloseButton() {
  bubble_->Close();
}

void BookmarkBubbleGtk::HandleRemoveButton() {
  UserMetrics::RecordAction(L"BookmarkBubble_Unstar", profile_);

  apply_edits_ = false;
  remove_bookmark_ = true;
  bubble_->Close();
}

void BookmarkBubbleGtk::ApplyEdits() {
  // Set this to make sure we don't attempt to apply edits again.
  apply_edits_ = false;

  BookmarkModel* model = profile_->GetBookmarkModel();
  BookmarkNode* node = model->GetMostRecentlyAddedNodeForURL(url_);
  if (node) {
    // NOTE: Would be nice to save a strlen and use gtk_entry_get_text_length,
    // but it is fairly new and not always in our GTK version.
    const std::wstring new_title(
        UTF8ToWide(gtk_entry_get_text(GTK_ENTRY(name_entry_))));

    if (new_title != node->GetTitle()) {
      model->SetTitle(node, new_title);
      UserMetrics::RecordAction(L"BookmarkBubble_ChangeTitleInBubble",
                                profile_);
    }

// TODO(deanm): We need to get the Bookmark editor running on Linux.
#if 0
    // Last index means 'Choose another folder...'
    if (parent_combobox_->selected_item() <
        parent_model_.GetItemCount(parent_combobox_) - 1) {
      BookmarkNode* new_parent =
          parent_model_.GetNodeAt(parent_combobox_->selected_item());
      if (new_parent != node->GetParent()) {
        UserMetrics::RecordAction(L"BookmarkBubble_ChangeParent", profile_);
        model->Move(node, new_parent, new_parent->GetChildCount());
      }
    }
#endif
  }
}

std::string BookmarkBubbleGtk::GetTitle() {
  BookmarkModel* bookmark_model= profile_->GetBookmarkModel();
  BookmarkNode* node = bookmark_model->GetMostRecentlyAddedNodeForURL(url_);
  if (!node) {
    NOTREACHED();
    return std::string();
  }

  return WideToUTF8(node->GetTitle());
}
