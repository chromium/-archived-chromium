// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/bookmark_editor_gtk.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/gfx/gtk_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/gtk/bookmark_tree_model.h"
#include "chrome/browser/gtk/bookmark_utils_gtk.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/gtk_util.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

namespace {

// Background color of text field when URL is invalid.
const GdkColor kErrorColor = GDK_COLOR_RGB(0xFF, 0xBC, 0xBC);

// Preferred initial dimensions, in pixels, of the folder tree.
static const int kTreeWidth = 300;
static const int kTreeHeight = 150;

}  // namespace

// static
void BookmarkEditor::Show(gfx::NativeView parent_hwnd,
                          Profile* profile,
                          const BookmarkNode* parent,
                          const BookmarkNode* node,
                          Configuration configuration,
                          Handler* handler) {
  DCHECK(profile);
  BookmarkEditorGtk* editor =
      new BookmarkEditorGtk(GTK_WINDOW(gtk_widget_get_toplevel(parent_hwnd)),
                            profile, parent, node, configuration, handler);
  editor->Show();
}

BookmarkEditorGtk::BookmarkEditorGtk(
    GtkWindow* window,
    Profile* profile,
    const BookmarkNode* parent,
    const BookmarkNode* node,
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

  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_BOOMARK_EDITOR_TITLE).c_str(),
      parent_window,
      GTK_DIALOG_MODAL,
      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
      NULL);
  gtk_dialog_set_has_separator(GTK_DIALOG(dialog_), FALSE);

  if (show_tree_) {
    GtkWidget* action_area = GTK_DIALOG(dialog_)->action_area;
    GtkWidget* new_folder_button = gtk_button_new_with_label(
        l10n_util::GetStringUTF8(IDS_BOOMARK_EDITOR_NEW_FOLDER_BUTTON).c_str());
    g_signal_connect(new_folder_button, "clicked",
                     G_CALLBACK(OnNewFolderClicked), this);
    gtk_container_add(GTK_CONTAINER(action_area), new_folder_button);
    gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(action_area),
                                       new_folder_button, TRUE);
  }

  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_ACCEPT);

  // The GTK dialog content area layout (overview)
  //
  // +- GtkVBox |vbox| ----------------------------------------------+
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
  gtk_box_set_spacing(GTK_BOX(content_area), gtk_util::kContentAreaSpacing);

  GtkWidget* vbox = gtk_vbox_new(FALSE, 12);

  name_entry_ = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(name_entry_),
                     node_ ? WideToUTF8(node_->GetTitle()).c_str() : "");
  g_signal_connect(G_OBJECT(name_entry_), "changed",
                   G_CALLBACK(OnEntryChanged), this);
  gtk_entry_set_activates_default(GTK_ENTRY(name_entry_), TRUE);

  url_entry_ = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(url_entry_),
                     node_ ? node_->GetURL().spec().c_str() : "");
  g_signal_connect(G_OBJECT(url_entry_), "changed",
                   G_CALLBACK(OnEntryChanged), this);
  gtk_entry_set_activates_default(GTK_ENTRY(url_entry_), TRUE);

  GtkWidget* table = gtk_util::CreateLabeledControlsGroup(
      l10n_util::GetStringUTF8(IDS_BOOMARK_EDITOR_NAME_LABEL).c_str(),
      name_entry_,
      l10n_util::GetStringUTF8(IDS_BOOMARK_EDITOR_URL_LABEL).c_str(),
      url_entry_,
      NULL);

  gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

  if (show_tree_) {
    GtkTreeIter selected_iter;
    int selected_id = node_ ? node_->GetParent()->id() : 0;
    tree_store_ = bookmark_utils::MakeFolderTreeStore();
    bookmark_utils::AddToTreeStore(bb_model_, selected_id,
                                   tree_store_, &selected_iter);

    GtkTreeViewColumn* icon_column =
        gtk_tree_view_column_new_with_attributes(
            "", gtk_cell_renderer_pixbuf_new(), "pixbuf",
             bookmark_utils::FOLDER_ICON, NULL);
    GtkTreeViewColumn* name_column =
        gtk_tree_view_column_new_with_attributes(
            "", gtk_cell_renderer_text_new(), "text",
             bookmark_utils::FOLDER_NAME, NULL);

    tree_view_ = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tree_store_));
    // Let |tree_view| own the store.
    g_object_unref(tree_store_);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view_), FALSE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view_), icon_column);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view_), name_column);
    gtk_widget_set_size_request(tree_view_, kTreeWidth, kTreeHeight);

    tree_selection_ = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view_));

    GtkTreePath* path = NULL;
    if (selected_id) {
      path = gtk_tree_model_get_path(GTK_TREE_MODEL(tree_store_),
                                     &selected_iter);
    } else {
      // We don't have a selected parent (Probably because we're making a new
      // bookmark). Select the first item in the list.
      path = gtk_tree_path_new_from_string("0");
    }

    gtk_tree_view_expand_to_path(GTK_TREE_VIEW(tree_view_), path);
    gtk_tree_selection_select_path(tree_selection_, path);
    gtk_tree_path_free(path);

    GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                    GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                        GTK_SHADOW_ETCHED_IN);
    gtk_container_add(GTK_CONTAINER(scroll_window), tree_view_);

    gtk_box_pack_start(GTK_BOX(vbox), scroll_window, TRUE, TRUE, 0);
  }

  gtk_box_pack_start(GTK_BOX(content_area), vbox, TRUE, TRUE, 0);

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
                                          const BookmarkNode* old_parent,
                                          int old_index,
                                          const BookmarkNode* new_parent,
                                          int new_index) {
  Reset();
}

void BookmarkEditorGtk::BookmarkNodeAdded(BookmarkModel* model,
                                          const BookmarkNode* parent,
                                          int index) {
  Reset();
}

void BookmarkEditorGtk::BookmarkNodeRemoved(BookmarkModel* model,
                                            const BookmarkNode* parent,
                                            int index,
                                            const BookmarkNode* node) {
  if ((node_ && node_->HasAncestor(node)) ||
      (parent_ && parent_->HasAncestor(node))) {
    // The node, or its parent was removed. Close the dialog.
    Close();
  } else {
    Reset();
  }
}

void BookmarkEditorGtk::BookmarkNodeChildrenReordered(
    BookmarkModel* model, const BookmarkNode* node) {
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
  const BookmarkNode* new_parent =
      bookmark_utils::CommitTreeStoreDifferencesBetween(
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
      bookmark_utils::FOLDER_ICON,
      bookmark_utils::GetFolderIcon(),
      bookmark_utils::FOLDER_NAME,
      l10n_util::GetStringUTF8(IDS_BOOMARK_EDITOR_NEW_FOLDER_NAME).c_str(),
      bookmark_utils::ITEM_ID, 0,
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
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog->dialog_),
                                      GTK_RESPONSE_ACCEPT, FALSE);
  } else {
    gtk_widget_modify_base(dialog->url_entry_, GTK_STATE_NORMAL, NULL);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog->dialog_),
                                      GTK_RESPONSE_ACCEPT, TRUE);
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
