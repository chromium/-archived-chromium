// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BOOKMARK_EDITOR_GTK_H_
#define CHROME_BROWSER_GTK_BOOKMARK_EDITOR_GTK_H_

#include <gtk/gtk.h>

#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_model.h"

// GTK version of the bookmark editor dialog.
class BookmarkEditorGtk : public BookmarkEditor,
                          public BookmarkModelObserver {
  FRIEND_TEST(BookmarkEditorGtkTest, ChangeParent);
  FRIEND_TEST(BookmarkEditorGtkTest, ChangeParentAndURL);
  FRIEND_TEST(BookmarkEditorGtkTest, ChangeURLToExistingURL);
  FRIEND_TEST(BookmarkEditorGtkTest, EditTitleKeepsPosition);
  FRIEND_TEST(BookmarkEditorGtkTest, EditURLKeepsPosition);
  FRIEND_TEST(BookmarkEditorGtkTest, ModelsMatch);
  FRIEND_TEST(BookmarkEditorGtkTest, MoveToNewParent);
  FRIEND_TEST(BookmarkEditorGtkTest, NewURL);
  FRIEND_TEST(BookmarkEditorGtkTest, ChangeURLNoTree);
  FRIEND_TEST(BookmarkEditorGtkTest, ChangeTitleNoTree);
 public:
  BookmarkEditorGtk(GtkWindow* window,
                    Profile* profile,
                    const BookmarkNode* parent,
                    const BookmarkNode* node,
                    BookmarkEditor::Configuration configuration,
                    BookmarkEditor::Handler* handler);

  virtual ~BookmarkEditorGtk();

  void Show();
  void Close();

 private:
  void Init(GtkWindow* parent_window);

  // BookmarkModel observer methods. Any structural change results in
  // resetting the tree model.
  virtual void Loaded(BookmarkModel* model) { }
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index);
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index);
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int index,
                                   const BookmarkNode* node);
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) {}
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node);
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node) {}

  // Resets the model of the tree and updates the various buttons appropriately.
  void Reset();

  // Returns the current url the user has input.
  GURL GetInputURL() const;

  // Returns the title the user has input.
  std::wstring GetInputTitle() const;

  // Invokes ApplyEdits with the selected node.
  //
  // TODO(erg): This was copied from the windows version. Both should be
  // cleaned up so that we don't overload ApplyEdits.
  void ApplyEdits();

  // Applies the edits done by the user. |selected_parent| gives the parent of
  // the URL being edited.
  void ApplyEdits(GtkTreeIter* selected_parent);

  // Adds a new group parented on |parent| and sets |child| to point to this
  // new group.
  void AddNewGroup(GtkTreeIter* parent, GtkTreeIter* child);

  static void OnResponse(GtkDialog* dialog, int response_id,
                         BookmarkEditorGtk* window);

  static gboolean OnWindowDeleteEvent(GtkWidget* widget,
                                      GdkEvent* event,
                                      BookmarkEditorGtk* dialog);

  static void OnWindowDestroy(GtkWidget* widget, BookmarkEditorGtk* dialog);
  static void OnEntryChanged(GtkEditable* entry, BookmarkEditorGtk* dialog);

  static void OnNewFolderClicked(GtkWidget* button, BookmarkEditorGtk* dialog);

  // Profile the entry is from.
  Profile* profile_;

  // The dialog to display on screen.
  GtkWidget* dialog_;
  GtkWidget* name_entry_;
  GtkWidget* url_entry_;
  GtkWidget* tree_view_;

  // Helper object that manages the currently selected item in |tree_view_|.
  GtkTreeSelection* tree_selection_;

  // Our local copy of the bookmark data that we make from the BookmarkModel
  // that we can modify as much as we want and still discard when the user
  // clicks Cancel.
  GtkTreeStore* tree_store_;

  // TODO(erg): BookmarkEditorView has an EditorTreeModel object here; convert
  // that into a GObject that implements the interface GtkTreeModel.

  // Initial parent to select. Is only used if node_ is NULL.
  const BookmarkNode* parent_;

  // Node being edited. Is NULL if creating a new node.
  const BookmarkNode* node_;

  // Mode used to create nodes from.
  BookmarkModel* bb_model_;

  // If true, we're running the menu for the bookmark bar or other bookmarks
  // nodes.
  bool running_menu_for_root_;

  // Is the tree shown?
  bool show_tree_;

  scoped_ptr<BookmarkEditor::Handler> handler_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkEditorGtk);
};

#endif  // CHROME_BROWSER_GTK_BOOKMARK_EDITOR_GTK_H_
