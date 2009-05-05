// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/bookmark_editor_gtk.h"

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/gfx/gtk_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/gtk/bookmark_tree_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/l10n_util.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

namespace {

// Background color of text field when URL is invalid.
const GdkColor kErrorColor = GDK_COLOR_RGB(0xFF, 0xBC, 0xBC);

// Preferred width of the tree.
static const int kTreeWidth = 300;
static const int kTreeHeight = 150;

}  // namespace

// static
void BookmarkEditor::Show(gfx::NativeWindow parent_hwnd,
                          Profile* profile,
                          BookmarkNode* parent,
                          BookmarkNode* node,
                          Configuration configuration,
                          Handler* handler) {
  DCHECK(profile);
  BookmarkEditorGtk* editor =
      new BookmarkEditorGtk(parent_hwnd, profile, parent, node, configuration,
                            handler);
  editor->Show();
}

BookmarkEditorGtk::BookmarkEditorGtk(
    GtkWindow* window,
    Profile* profile,
    BookmarkNode* parent,
    BookmarkNode* node,
    BookmarkEditor::Configuration configuration,
    BookmarkEditor::Handler* handler)
    : profile_(profile),
      dialog_(NULL),
      parent_(parent),
      node_(node),
      running_menu_for_root_(false),
      show_tree_(configuration == SHOW_TREE),
      handler_(handler) {
  DCHECK(profile);
  Init(window);
}

BookmarkEditorGtk::~BookmarkEditorGtk() {
  // The tree model is deleted before the view. Reset the model otherwise the
  // tree will reference a deleted model.

  bb_model_->RemoveObserver(this);
}

void BookmarkEditorGtk::Init(GtkWindow* parent_window) {
  bb_model_ = profile_->GetBookmarkModel();
  DCHECK(bb_model_);
  bb_model_->AddObserver(this);

  // TODO(erg): Redo this entire class as a normal GtkWindow with it's modality
  // manually set to TRUE because using the stock GtkDialog class gives me
  // almost no control over the buttons on the bottom.
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_BOOMARK_EDITOR_TITLE).c_str(),
      parent_window,
      GTK_DIALOG_MODAL,
      NULL);

  if (show_tree_) {
    // We want the New Folder button to not automatically dismiss the dialog so
    // we have to do that manually. gtk_dialog_add_button() always makes the
    // button dismiss the dialog box. This isn't 100% accurate to what I want;
    // see above about redoing this as a GtkWindow.
    GtkWidget* action_area = GTK_DIALOG(dialog_)->action_area;
    new_folder_button_ = gtk_button_new_with_label("New Folder");
    g_signal_connect(new_folder_button_, "clicked",
                     G_CALLBACK(OnNewFolderClicked), this);
    gtk_box_pack_start(GTK_BOX(action_area), new_folder_button_,
                       FALSE, FALSE, 0);
  }

  close_button_ = gtk_dialog_add_button(GTK_DIALOG(dialog_),
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_REJECT);
  ok_button_ = gtk_dialog_add_button(GTK_DIALOG(dialog_),
                                     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_ACCEPT);

  // The GTK dialog content area layout (overview)
  //
  // +- GtkVBox |content_area| --------------------------------------+
  // |+- GtkTable |table| ------------------------------------------+|
  // ||+- GtkLabel ------+ +- GtkEntry |name_entry_| --------------+||
  // |||                 | |                                       |||
  // ||+-----------------+ +---------------------------------------+||
  // ||+- GtkLabel ------+ +- GtkEntry |url_entry_| ---------------+||
  // |||                 | |                                       |||
  // ||+-----------------+ +---------------------------------------+||
  // |+-------------------------------------------------------------+|
  // |+- GtkScrollWindow |scroll_window| ---------------------------+|
  // ||+- GtkTreeView |tree_view_| --------------------------------+||
  // |||+- GtkTreeViewColumn |name_column| -----------------------+|||
  // ||||                                                         ||||
  // ||||                                                         ||||
  // ||||                                                         ||||
  // ||||                                                         ||||
  // |||+---------------------------------------------------------+|||
  // ||+-----------------------------------------------------------+||
  // |+-------------------------------------------------------------+|
  // +---------------------------------------------------------------+
  GtkWidget* content_area = GTK_DIALOG(dialog_)->vbox;
  gtk_container_set_border_width(GTK_CONTAINER(content_area), 12);
  GtkWidget* table = gtk_table_new(2, 2, FALSE);

  GtkWidget* label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_BOOMARK_EDITOR_NAME_LABEL).c_str());
  gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(label),
                   0, 1, 0, 1,
                   (GtkAttachOptions)(GTK_SHRINK),
                   (GtkAttachOptions)(GTK_SHRINK),
                   12, 0);
  name_entry_ = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(name_entry_),
                     node_ ? WideToUTF8(node_->GetTitle()).c_str() : "");
  g_signal_connect(G_OBJECT(name_entry_), "changed",
                   G_CALLBACK(OnEntryChanged), this);
  g_object_set(G_OBJECT(name_entry_), "activates-default", TRUE, NULL);
  gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(name_entry_),
                   1, 2, 0, 1,
                   (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                   (GtkAttachOptions)(GTK_FILL),
                   0, 0);

  label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_BOOMARK_EDITOR_URL_LABEL).c_str());
  gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(label),
                   0, 1, 1, 2,
                   (GtkAttachOptions)(GTK_SHRINK),
                   (GtkAttachOptions)(GTK_SHRINK),
                   12, 0);
  url_entry_ = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(url_entry_),
                     node_ ? node_->GetURL().spec().c_str() : "");
  g_signal_connect(G_OBJECT(url_entry_), "changed",
                   G_CALLBACK(OnEntryChanged), this);
  g_object_set(G_OBJECT(url_entry_), "activates-default", TRUE, NULL);
  gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(url_entry_),
                   1, 2, 1, 2,
                   (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                   (GtkAttachOptions)(GTK_FILL),
                   0, 0);

  gtk_box_pack_start(GTK_BOX(content_area), table, FALSE, FALSE, 0);

  if (show_tree_) {
    GtkTreeIter selected_iter;
    int selected_id = node_ ? node_->GetParent()->id() : 0;
    bookmark_utils::BuildTreeStoreFrom(bb_model_, selected_id, &tree_store_,
                                       &selected_iter);

    // TODO(erg): Figure out how to place icons here.
    GtkTreeViewColumn* name_column =
        gtk_tree_view_column_new_with_attributes(
            "Folder", gtk_cell_renderer_text_new(), "text", 0, NULL);

    tree_view_ = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tree_store_));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view_), FALSE);
    gtk_tree_view_insert_column(GTK_TREE_VIEW(tree_view_), name_column, -1);
    gtk_widget_set_size_request(tree_view_, kTreeWidth, kTreeHeight);

    tree_selection_ = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view_));

    if (selected_id) {
      GtkTreePath* path = gtk_tree_model_get_path(GTK_TREE_MODEL(tree_store_),
                                                  &selected_iter);
      gtk_tree_view_expand_to_path(GTK_TREE_VIEW(tree_view_), path);
      gtk_tree_selection_select_path(tree_selection_, path);
      gtk_tree_path_free(path);
    }

    GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll_window), tree_view_);
    gtk_box_pack_start(GTK_BOX(content_area), scroll_window, TRUE, TRUE, 12);
  }

  g_signal_connect(dialog_, "response",
                   G_CALLBACK(OnResponse), this);
  g_signal_connect(dialog_, "delete-event",
                   G_CALLBACK(OnWindowDeleteEvent), this);
  g_signal_connect(dialog_, "destroy",
                   G_CALLBACK(OnWindowDestroy), this);
}

void BookmarkEditorGtk::Show() {
  // Manually call our OnEntryChanged handler to set the initial state.
  OnEntryChanged(NULL, this);

  gtk_widget_show_all(dialog_);
}

void BookmarkEditorGtk::Close() {
  // Under the model that we've inherited from Windows, dialogs can receive
  // more than one Close() call inside the current message loop event.
  if (dialog_) {
    gtk_widget_destroy(dialog_);
    dialog_ = NULL;
  }
}

void BookmarkEditorGtk::BookmarkNodeMoved(BookmarkModel* model,
                                          BookmarkNode* old_parent,
                                          int old_index,
                                          BookmarkNode* new_parent,
                                          int new_index) {
  Reset();
}

void BookmarkEditorGtk::BookmarkNodeAdded(BookmarkModel* model,
                                          BookmarkNode* parent,
                                          int index) {
  Reset();
}

void BookmarkEditorGtk::BookmarkNodeRemoved(BookmarkModel* model,
                                            BookmarkNode* parent,
                                            int index,
                                            BookmarkNode* node) {
  if ((node_ && node_->HasAncestor(node)) ||
      (parent_ && parent_->HasAncestor(node))) {
    // The node, or its parent was removed. Close the dialog.
    Close();
  } else {
    Reset();
  }
}

void BookmarkEditorGtk::BookmarkNodeChildrenReordered(BookmarkModel* model,
                                                      BookmarkNode* node) {
  Reset();
}

void BookmarkEditorGtk::Reset() {
  // TODO(erg): The windows implementation tries to be smart. For now, just
  // close the window.
  Close();
}

GURL BookmarkEditorGtk::GetInputURL() const {
  std::wstring input = URLFixerUpper::FixupURL(
      UTF8ToWide(gtk_entry_get_text(GTK_ENTRY(url_entry_))), L"");
  return GURL(WideToUTF8(input));
}

std::wstring BookmarkEditorGtk::GetInputTitle() const {
  return UTF8ToWide(gtk_entry_get_text(GTK_ENTRY(name_entry_)));
}

void BookmarkEditorGtk::ApplyEdits() {
  DCHECK(bb_model_->IsLoaded());

  GtkTreeIter currently_selected_iter;
  if (show_tree_) {
    if (!gtk_tree_selection_get_selected(tree_selection_, NULL,
                                         &currently_selected_iter)) {
      NOTREACHED() << "Something should always be selected";
      return;
    }
  }

  ApplyEdits(&currently_selected_iter);
}

void BookmarkEditorGtk::ApplyEdits(GtkTreeIter* selected_parent) {
  // We're going to apply edits to the bookmark bar model, which will call us
  // back. Normally when a structural edit occurs we reset the tree model.
  // We don't want to do that here, so we remove ourselves as an observer.
  bb_model_->RemoveObserver(this);

  GURL new_url(GetInputURL());
  std::wstring new_title(GetInputTitle());

  if (!show_tree_) {
    bookmark_utils::ApplyEditsWithNoGroupChange(
        bb_model_, parent_, node_, new_title, new_url, handler_.get());
    return;
  }

  // Create the new groups and update the titles.
  BookmarkNode* new_parent = bookmark_utils::CommitTreeStoreDifferencesBetween(
      bb_model_, tree_store_, selected_parent);

  if (!new_parent) {
    // Bookmarks must be parented.
    NOTREACHED();
    return;
  }

  bookmark_utils::ApplyEditsWithPossibleGroupChange(
      bb_model_, new_parent, node_, new_title, new_url, handler_.get());
}

void BookmarkEditorGtk::AddNewGroup(GtkTreeIter* parent, GtkTreeIter* child) {
  gtk_tree_store_append(tree_store_, child, parent);
  gtk_tree_store_set(
      tree_store_, child,
      0, l10n_util::GetStringUTF8(IDS_BOOMARK_EDITOR_NEW_FOLDER_NAME).c_str(),
      1, 0,
      -1);
}

// static
void BookmarkEditorGtk::OnResponse(GtkDialog* dialog, int response_id,
                                   BookmarkEditorGtk* window) {
  if (response_id == GTK_RESPONSE_ACCEPT) {
    window->ApplyEdits();
  }

  window->Close();
}

// static
gboolean BookmarkEditorGtk::OnWindowDeleteEvent(
    GtkWidget* widget,
    GdkEvent* event,
    BookmarkEditorGtk* dialog) {
  dialog->Close();

  // Return true to prevent the gtk dialog from being destroyed. Close will
  // destroy it for us and the default gtk_dialog_delete_event_handler() will
  // force the destruction without us being able to stop it.
  return TRUE;
}

// static
void BookmarkEditorGtk::OnWindowDestroy(GtkWidget* widget,
                                        BookmarkEditorGtk* dialog) {
  MessageLoop::current()->DeleteSoon(FROM_HERE, dialog);
}

// static
void BookmarkEditorGtk::OnEntryChanged(GtkEditable* entry,
                                       BookmarkEditorGtk* dialog) {
  const GURL url(dialog->GetInputURL());
  if (!url.is_valid()) {
    gtk_widget_modify_base(dialog->url_entry_, GTK_STATE_NORMAL, &kErrorColor);
    gtk_widget_set_sensitive(GTK_WIDGET(dialog->ok_button_), false);
  } else {
    gtk_widget_modify_base(dialog->url_entry_, GTK_STATE_NORMAL, NULL);
    gtk_widget_set_sensitive(GTK_WIDGET(dialog->ok_button_), true);
  }
}

// static
void BookmarkEditorGtk::OnNewFolderClicked(GtkWidget* button,
                                           BookmarkEditorGtk* dialog) {
  // TODO(erg): Make the inserted item here editable and edit it. If that's
  // impossible (it's probably possible), fall back on the folder editor.
  GtkTreeIter iter;
  if (!gtk_tree_selection_get_selected(dialog->tree_selection_,
                                       NULL,
                                       &iter)) {
    NOTREACHED() << "Something should always be selected";
    return;
  }

  GtkTreeIter new_item_iter;
  dialog->AddNewGroup(&iter, &new_item_iter);

  GtkTreePath* path = gtk_tree_model_get_path(
      GTK_TREE_MODEL(dialog->tree_store_), &new_item_iter);
  gtk_tree_view_expand_to_path(GTK_TREE_VIEW(dialog->tree_view_), path);
  gtk_tree_selection_select_path(dialog->tree_selection_, path);
  gtk_tree_path_free(path);
}
