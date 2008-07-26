// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_VIEWS_BOOKMARK_EDITOR_VIEW_H__
#define CHROME_BROWSER_VIEWS_BOOKMARK_EDITOR_VIEW_H__

#include <set>

#include "chrome/views/tree_node_model.h"
#include "chrome/browser/bookmark_bar_model.h"
#include "chrome/views/checkbox.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/menu.h"
#include "chrome/views/native_button.h"
#include "chrome/views/text_field.h"

namespace ChromeViews {
class Window;
}

class GURL;
class Menu;
class Profile;

// View that allows the user to edit a bookmark/starred URL. The user can
// change the URL, title and where the bookmark appears as well as adding
// new groups and changing the name of other groups.
//
// Edits are applied to the BookmarkBarModel when the user presses 'OK'.
//
// To use BookmarkEditorView invoke the static show method.

class BookmarkEditorView : public ChromeViews::View,
                           public ChromeViews::NativeButton::Listener,
                           public ChromeViews::TreeViewController,
                           public ChromeViews::DialogDelegate,
                           public ChromeViews::TextField::Controller,
                           public ChromeViews::ContextMenuController,
                           public Menu::Delegate,
                           public BookmarkBarModelObserver {
 public:
  // Shows the BookmarkEditorView editing the specified entry.
  static void Show(HWND parent_window,
                   Profile* profile,
                   const GURL& url,
                   const std::wstring& title);

  BookmarkEditorView(Profile* profile,
                     const GURL& url,
                     const std::wstring& title);

  virtual ~BookmarkEditorView();

  // DialogDelegate methods:
  virtual bool IsDialogButtonEnabled(DialogButton button) const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool Accept();
  virtual bool AreAcceleratorsEnabled(DialogButton button);

  // View methods.
  virtual void Layout();
  virtual void GetPreferredSize(CSize *out);
  virtual void DidChangeBounds(const CRect& previous, const CRect& current);
  virtual void ViewHierarchyChanged(bool is_add, ChromeViews::View* parent,
                                    ChromeViews::View* child);

  // TreeViewObserver methods.
  virtual void OnTreeViewSelectionChanged(ChromeViews::TreeView* tree_view);
  virtual bool CanEdit(ChromeViews::TreeView* tree_view,
                       ChromeViews::TreeModelNode* node);

  // TextField::Controller methods.
  virtual void ContentsChanged(ChromeViews::TextField* sender,
                               const std::wstring& new_contents);
  virtual void HandleKeystroke(ChromeViews::TextField* sender,
                               UINT message, TCHAR key, UINT repeat_count,
                               UINT flags) {}

  // NativeButton/CheckBox.
  virtual void ButtonPressed(ChromeViews::NativeButton* sender);

  // Menu::Delegate method.
  virtual void ExecuteCommand(int id);

  // Menu::Delegate method, return false if id is edit and the bookmark node
  // was selected, true otherwise.
  virtual bool IsCommandEnabled(int id) const;

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
  typedef ChromeViews::TreeNodeWithValue<history::UIStarID> BookmarkNode;

  // Model for the TreeView. Trivial subclass that doesn't allow titles with
  // empty strings.
  class BookmarkTreeModel : public ChromeViews::TreeNodeModel<BookmarkNode> {
   public:
    explicit BookmarkTreeModel(BookmarkNode* root)
        : TreeNodeModel<BookmarkNode>(root) {}

    virtual void SetTitle(ChromeViews::TreeModelNode* node,
                          const std::wstring& title) {
      if (!title.empty())
        TreeNodeModel::SetTitle(node, title);
    }

   private:
    DISALLOW_EVIL_CONSTRUCTORS(BookmarkTreeModel);
  };

  // Creates the necessary sub-views, configures them, adds them to the layout,
  // and requests the entries to display from the database.
  void Init();

  // BookmarkBarModel observer methods. Any structural change results in
  // resetting the tree model.
  virtual void Loaded(BookmarkBarModel* model);
  virtual void BookmarkNodeMoved(BookmarkBarModel* model,
                                 BookmarkBarNode* old_parent,
                                 int old_index,
                                 BookmarkBarNode* new_parent,
                                 int new_index);
  virtual void BookmarkNodeAdded(BookmarkBarModel* model,
                                 BookmarkBarNode* parent,
                                 int index);
  virtual void BookmarkNodeRemoved(BookmarkBarModel* model,
                                   BookmarkBarNode* parent,
                                   int index);
  virtual void BookmarkNodeChanged(BookmarkBarModel* model,
                                   BookmarkBarNode* node) {}
  virtual void BookmarkNodeFavIconLoaded(BookmarkBarModel* model,
                                         BookmarkBarNode* node) {}

  // Resets the model of the tree and updates the various buttons appropriately.
  // If first_time is true, Reset is being invoked from the constructor or
  // once the bookmark bar has finished loading.
  void Reset(bool first_time);

  // Expands all the nodes in the tree and selects the parent node of the
  // url we're editing or the most recent parent if the url being editted isn't
  // starred.
  void ExpandAndSelect();

  // Creates a returns the new root node. This invokes CreateNodes to do
  // the real work.
  BookmarkNode* CreateRootNode();

  // Adds and creates a child node in b_node for all children of bb_node that
  // are groups.
  void CreateNodes(BookmarkBarNode* bb_node,
                   BookmarkNode* b_node);

  // Returns the node with the specified id, or NULL if one can't be found.
  BookmarkNode* FindNodeWithID(BookmarkEditorView::BookmarkNode* node,
                               history::UIStarID id);

  // Applies the edits done by the user.
  void ApplyEdits();

  // Recursively adds newly created groups and sets the title of nodes to
  // match the user edited title.
  //
  // bb_node gives the BookmarkBarNode the edits are to be applied to,
  // with b_node the source of the edits.
  //
  // If b_node == parent_b_node, parent_bb_node is set to bb_node. This is
  // used to determine the new BookmarkBarNode parent based on the BookmarkNode
  // parent.
  void ApplyNameChangesAndCreateNewGroups(
      BookmarkBarNode* bb_node,
      BookmarkEditorView::BookmarkNode* b_node,
      BookmarkEditorView::BookmarkNode* parent_b_node,
      BookmarkBarNode** parent_bb_node);

  // Returns the current url the user has input.
  GURL GetInputURL() const;

  // Returns the title the user has input.
  std::wstring GetInputTitle() const;

  // Invoked when the url or title has possibly changed. Updates the background
  // of textfields and ok button appropriately.
  void UserInputChanged();

  // Creates a new group as a child of the selected node. If no node is
  // selected, the new group is added as a child of the bookmark node. Starts
  // editing on the new gorup as well.
  void NewGroup();

  // Profile the entry is from.
  Profile* profile_;

  // Model driving the TreeView.
  scoped_ptr<BookmarkTreeModel> tree_model_;

  // Displays star groups.
  ChromeViews::TreeView tree_view_;

  // Used to create a new group.
  ChromeViews::NativeButton new_group_button_;

  // Used for editing the URL.
  ChromeViews::TextField url_tf_;

  // Used for editing the title.
  ChromeViews::TextField title_tf_;

  // Dialog we're contained in.
  ChromeViews::Window* dialog_;

  // URL we were created with.
  const GURL url_;

  // The context menu.
  scoped_ptr<Menu> context_menu_;

  // Title of the url to display.
  std::wstring title_;

  // Mode used to create nodes from.
  BookmarkBarModel* bb_model_;

  // If true, we're running the menu for the bookmark bar or other bookmarks
  // nodes.
  bool running_menu_for_root_;

  DISALLOW_EVIL_CONSTRUCTORS(BookmarkEditorView);
};

#endif  // CHROME_BROWSER_VIEWS_BOOKMARK_EDITOR_VIEW_H__
