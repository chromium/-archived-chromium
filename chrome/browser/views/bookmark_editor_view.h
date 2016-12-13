// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BOOKMARK_EDITOR_VIEW_H_
#define CHROME_BROWSER_VIEWS_BOOKMARK_EDITOR_VIEW_H_

#include <set>

#include "app/tree_node_model.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "views/controls/button/button.h"
#include "views/controls/menu/simple_menu_model.h"
#include "views/controls/textfield/textfield.h"
#include "views/controls/tree/tree_view.h"
#include "views/window/dialog_delegate.h"

namespace views {
class NativeButton;
class Window;
}

class GURL;
class Menu;
class Profile;

// View that allows the user to edit a bookmark/starred URL. The user can
// change the URL, title and where the bookmark appears as well as adding
// new groups and changing the name of other groups.
//
// Edits are applied to the BookmarkModel when the user presses 'OK'.
//
// To use BookmarkEditorView invoke the static show method.

class BookmarkEditorView : public BookmarkEditor,
                           public views::View,
                           public views::ButtonListener,
                           public views::TreeViewController,
                           public views::DialogDelegate,
                           public views::Textfield::Controller,
                           public views::ContextMenuController,
                           public views::SimpleMenuModel::Delegate,
                           public BookmarkModelObserver {
  FRIEND_TEST(BookmarkEditorViewTest, ChangeParent);
  FRIEND_TEST(BookmarkEditorViewTest, ChangeParentAndURL);
  FRIEND_TEST(BookmarkEditorViewTest, ChangeURLToExistingURL);
  FRIEND_TEST(BookmarkEditorViewTest, EditTitleKeepsPosition);
  FRIEND_TEST(BookmarkEditorViewTest, EditURLKeepsPosition);
  FRIEND_TEST(BookmarkEditorViewTest, ModelsMatch);
  FRIEND_TEST(BookmarkEditorViewTest, MoveToNewParent);
  FRIEND_TEST(BookmarkEditorViewTest, NewURL);
  FRIEND_TEST(BookmarkEditorViewTest, ChangeURLNoTree);
  FRIEND_TEST(BookmarkEditorViewTest, ChangeTitleNoTree);
 public:
  BookmarkEditorView(Profile* profile,
                     const BookmarkNode* parent,
                     const BookmarkNode* node,
                     BookmarkEditor::Configuration configuration,
                     BookmarkEditor::Handler* handler);

  virtual ~BookmarkEditorView();

  // DialogDelegate methods:
  virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool Accept();
  virtual bool AreAcceleratorsEnabled(MessageBoxFlags::DialogButton button);
  virtual views::View* GetContentsView();

  // View methods.
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);

  // TreeViewObserver methods.
  virtual void OnTreeViewSelectionChanged(views::TreeView* tree_view);
  virtual bool CanEdit(views::TreeView* tree_view, TreeModelNode* node);

  // Textfield::Controller methods.
  virtual void ContentsChanged(views::Textfield* sender,
                               const std::wstring& new_contents);
  virtual bool HandleKeystroke(views::Textfield* sender,
                               const views::Textfield::Keystroke&) {
    return false;
  }

  // NativeButton.
  virtual void ButtonPressed(views::Button* sender);

  // SimpleMenuModel::Delegate.
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          views::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

  // Creates a Window and adds the BookmarkEditorView to it. When the window is
  // closed the BookmarkEditorView is deleted.
  void Show(HWND parent_hwnd);

  // Closes the dialog.
  void Close();

  // Shows the context menu.
  virtual void ShowContextMenu(View* source,
                               int x,
                               int y,
                               bool is_mouse_gesture);

 private:
  // Type of node in the tree.
  typedef TreeNodeWithValue<int> EditorNode;

  // Model for the TreeView. Trivial subclass that doesn't allow titles with
  // empty strings.
  class EditorTreeModel : public TreeNodeModel<EditorNode> {
   public:
    explicit EditorTreeModel(EditorNode* root)
        : TreeNodeModel<EditorNode>(root) {}

    virtual void SetTitle(TreeModelNode* node,
                          const std::wstring& title) {
      if (!title.empty())
        TreeNodeModel::SetTitle(node, title);
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(EditorTreeModel);
  };

  // Creates the necessary sub-views, configures them, adds them to the layout,
  // and requests the entries to display from the database.
  void Init();

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

  // Expands all the nodes in the tree and selects the parent node of the
  // url we're editing or the most recent parent if the url being editted isn't
  // starred.
  void ExpandAndSelect();

  // Creates a returns the new root node. This invokes CreateNodes to do
  // the real work.
  EditorNode* CreateRootNode();

  // Adds and creates a child node in b_node for all children of bb_node that
  // are groups.
  void CreateNodes(const BookmarkNode* bb_node, EditorNode* b_node);

  // Returns the node with the specified id, or NULL if one can't be found.
  EditorNode* FindNodeWithID(BookmarkEditorView::EditorNode* node, int id);

  // Invokes ApplyEdits with the selected node.
  void ApplyEdits();

  // Applies the edits done by the user. |parent| gives the parent of the URL
  // being edited.
  void ApplyEdits(EditorNode* parent);

  // Recursively adds newly created groups and sets the title of nodes to
  // match the user edited title.
  //
  // bb_node gives the BookmarkNode the edits are to be applied to, with b_node
  // the source of the edits.
  //
  // If b_node == parent_b_node, parent_bb_node is set to bb_node. This is
  // used to determine the new BookmarkNode parent based on the EditorNode
  // parent.
  void ApplyNameChangesAndCreateNewGroups(
      const BookmarkNode* bb_node,
      BookmarkEditorView::EditorNode* b_node,
      BookmarkEditorView::EditorNode* parent_b_node,
      const BookmarkNode** parent_bb_node);

  // Returns the current url the user has input.
  GURL GetInputURL() const;

  // Returns the title the user has input.
  std::wstring GetInputTitle() const;

  // Invoked when the url or title has possibly changed. Updates the background
  // of Textfields and ok button appropriately.
  void UserInputChanged();

  // Creates a new group as a child of the selected node. If no node is
  // selected, the new group is added as a child of the bookmark node. Starts
  // editing on the new gorup as well.
  void NewGroup();

  // Creates a new EditorNode as the last child of parent. The new node is
  // added to the model and returned. This does NOT start editing. This is used
  // internally by NewGroup and broken into a separate method for testing.
  EditorNode* AddNewGroup(EditorNode* parent);

  // Profile the entry is from.
  Profile* profile_;

  // Model driving the TreeView.
  scoped_ptr<EditorTreeModel> tree_model_;

  // Displays star groups.
  views::TreeView* tree_view_;

  // Used to create a new group.
  scoped_ptr<views::NativeButton> new_group_button_;

  // Used for editing the URL.
  views::Textfield url_tf_;

  // Used for editing the title.
  views::Textfield title_tf_;

  // Initial parent to select. Is only used if node_ is NULL.
  const BookmarkNode* parent_;

  // Node being edited. Is NULL if creating a new node.
  const BookmarkNode* node_;

  // The context menu.
  scoped_ptr<views::SimpleMenuModel> context_menu_contents_;
  scoped_ptr<views::Menu2> context_menu_;

  // Mode used to create nodes from.
  BookmarkModel* bb_model_;

  // If true, we're running the menu for the bookmark bar or other bookmarks
  // nodes.
  bool running_menu_for_root_;

  // Is the tree shown?
  bool show_tree_;

  scoped_ptr<BookmarkEditor::Handler> handler_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkEditorView);
};

#endif  // CHROME_BROWSER_VIEWS_BOOKMARK_EDITOR_VIEW_H_
