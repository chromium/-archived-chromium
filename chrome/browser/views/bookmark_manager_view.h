// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BOOKMARK_MANAGER_VIEW_H_
#define CHROME_BROWSER_VIEWS_BOOKMARK_MANAGER_VIEW_H_

#include "base/ref_counted.h"
#include "base/task.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/browser/views/bookmark_context_menu.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/controls/table/table_view_observer.h"
#include "views/controls/textfield/textfield.h"
#include "views/controls/tree/tree_view.h"
#include "views/view.h"
#include "views/window/window_delegate.h"
#include "webkit/glue/window_open_disposition.h"

class BookmarkFolderTreeModel;
class BookmarkFolderTreeView;
class BookmarkTableModel;
class BookmarkTableView;
class Profile;

namespace views {
class SingleSplitView;
}
// A view that lets the user manage their bookmarks. The bookmark manager
// shows a tree on the left with a table on the right. The tree shows the
// folder nodes and the table the contents of the selected tree node. The
// tree is a BookmarkFolderTreeView and the table a BookmarkTableView. A
// text field is also provided that allows the user to search the contents
// of the bookmarks.
class BookmarkManagerView : public views::View,
                            public views::WindowDelegate,
                            public views::TreeViewController,
                            public views::TableViewObserver,
                            public views::Textfield::Controller,
                            public BookmarkModelObserver,
                            public views::ContextMenuController,
                            public views::ViewMenuDelegate,
                            public views::MenuDelegate,
                            public SelectFileDialog::Listener {
 public:
  enum CutCopyPasteType {
    CUT,
    COPY,
    PASTE,
    NONE
  };

  explicit BookmarkManagerView(Profile* profile);
  virtual ~BookmarkManagerView();

  // Shows the bookmark manager. Only one bookmark manager exists.
  static void Show(Profile* profile);

  // Returns the current manager, or NULL if the manager is not showing.
  static BookmarkManagerView* current();

  // Selects the specified node in the tree. If node is a URL it's parent is
  // selected and node is selected in the table.
  void SelectInTree(const BookmarkNode* node);

  // Expands all the children of the selected folder.
  void ExpandAll(const BookmarkNode* node);

  // Returns the selected folder in the tree, which may be null.
  const BookmarkNode* GetSelectedFolder();

  // Returns the selection of the table.
  std::vector<const BookmarkNode*> GetSelectedTableNodes();

  virtual void PaintBackground(gfx::Canvas* canvas);

  virtual gfx::Size GetPreferredSize();

  // WindowDelegate methods.
  virtual bool CanResize() const { return true; }
  virtual bool CanMaximize() const { return true; }
  virtual std::wstring GetWindowTitle() const;
  virtual std::wstring GetWindowName() const;
  virtual View* GetContentsView() { return this; }
  // TODO(sky): implement these when we have an icon.
  //virtual SkBitmap GetWindowIcon();
  //virtual bool ShouldShowWindowIcon() const { return true; }
  virtual void WindowClosing();

  Profile* profile() const { return profile_; }

 protected:
  // View methods.
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

 private:
  // TableViewObserver methods.
  virtual void OnSelectionChanged() {}
  // Overriden to open the selected table nodes in the current browser.
  virtual void OnDoubleClick();
  virtual void OnMiddleClick();
  virtual void OnTableViewDelete(views::TableView* table);
  virtual void OnKeyDown(unsigned short virtual_keycode);

  // TreeViewController methods.
  virtual void OnTreeViewSelectionChanged(views::TreeView* tree_view);
  virtual void OnTreeViewKeyDown(unsigned short virtual_keycode);

  // BookmarkModelObserver. We're only installed as an observer until the
  // bookmarks are loaded.
  virtual void Loaded(BookmarkModel* model);
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) {}
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) {}
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) {}
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int index) {}
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) {}
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) {}
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) {}
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node) {}

  // Textfield::Controller methods.
  // Starts a timer to search for the search text.
  virtual void ContentsChanged(views::Textfield* sender,
                               const std::wstring& new_contents);
  // If return has been pressed this performs an immediate search.
  virtual bool HandleKeystroke(views::Textfield* sender,
                               const views::Textfield::Keystroke& key);

  // ContextMenuController.
  virtual void ShowContextMenu(views::View* source,
                               int x,
                               int y,
                               bool is_mouse_gesture);

  // ViewMenuDelegate.
  virtual void RunMenu(views::View* source, const gfx::Point& pt, HWND hwnd);

  // MenuDelegate.
  virtual void ExecuteCommand(int id);

  // SelectFileDialog::Listener.
  virtual void FileSelected(const FilePath& path,
                            int index, void* params);
  virtual void FileSelectionCanceled(void* params);

  // Creates the table model to use when searching. This returns NULL if there
  // is no search text.
  BookmarkTableModel* CreateSearchTableModel();

  // Sets the model of the table and its parent node. If |is_search| is true,
  // it means the table is showing search results.
  void SetTableModel(BookmarkTableModel* new_table_model,
                     const BookmarkNode* parent_node,
                     bool is_search);

  // Sets the table's model to the results of CreateSearchTableModel and selects
  // the search node in the tree.
  void PerformSearch();

  // Invoked prior to showing. If the BookmarkModel is loaded this invokes
  // LoadedImpl.
  void PrepareForShow();

  // Invoked when we're parented and the BookmarkModel is loaded. Sets the
  // models of the tree/table appropriately and expands the necessary nodes.
  void LoadedImpl();

  // Returns the BookmarkModel.
  BookmarkModel* GetBookmarkModel() const;

  // Shows the menu. This is invoked to show the context menu for table/tree
  // as well as to show the menu from the organize button.
  void ShowMenu(HWND host,
                int x,
                int y,
                BookmarkContextMenuController::ConfigurationType config);

  // Invoked to handle cut/copy/paste from the table or tree. If |from_table|
  // is true the source is the table.
  void OnCutCopyPaste(CutCopyPasteType type, bool from_table);

  // Shows the tools menu.
  void ShowToolsMenu(HWND host, int x, int y);

  // Shows the import/export file chooser. These invoke
  // FileSelected/FileSelectionCanceled when done.
  void ShowImportBookmarksFileChooser();
  void ShowExportBookmarksFileChooser();

  Profile* profile_;
  BookmarkTableView* table_view_;
  BookmarkFolderTreeView* tree_view_;
  scoped_ptr<BookmarkTableModel> table_model_;
  scoped_ptr<BookmarkFolderTreeModel> tree_model_;
  views::Textfield* search_tf_;
  views::SingleSplitView* split_view_;

  // Import/export file dialog.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // Factory used for delaying search.
  ScopedRunnableMethodFactory<BookmarkManagerView> search_factory_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkManagerView);
};

#endif  // CHROME_BROWSER_VIEWS_BOOKMARK_MANAGER_VIEW_H_
