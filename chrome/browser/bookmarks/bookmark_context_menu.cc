// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_context_menu.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/generated_resources.h"

// TODO(port): Port these files.
#if defined(OS_WIN)
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/browser/views/bookmark_editor_view.h"
#include "chrome/browser/views/bookmark_manager_view.h"
#include "chrome/browser/views/input_window.h"
#include "chrome/views/window.h"
#endif

namespace {

// Returns true if the specified node is of type URL, or has a descendant
// of type URL.
bool NodeHasURLs(BookmarkNode* node) {
  if (node->is_url())
    return true;

  for (int i = 0; i < node->GetChildCount(); ++i) {
    if (NodeHasURLs(node->GetChild(i)))
      return true;
  }
  return false;
}

// EditFolderController -------------------------------------------------------

// EditFolderController manages the editing and/or creation of a folder. If the
// user presses ok, the name change is committed to the model.
//
// EditFolderController deletes itself when the window is closed.
class EditFolderController : public InputWindowDelegate,
                             public BookmarkModelObserver {
 public:
  virtual ~EditFolderController() {
    if (model_)
      model_->RemoveObserver(this);
  }

  static void Show(Profile* profile,
                   gfx::NativeWindow wnd,
                   BookmarkNode* node,
                   bool is_new,
                   bool show_in_manager) {
    // EditFolderController deletes itself when done.
    EditFolderController* controller =
        new EditFolderController(profile, wnd, node, is_new, show_in_manager);
    controller->Show();
  }

 private:
  EditFolderController(Profile* profile,
                       gfx::NativeWindow wnd,
                       BookmarkNode* node,
                       bool is_new,
                       bool show_in_manager)
      : profile_(profile),
        model_(profile->GetBookmarkModel()),
        node_(node),
        is_new_(is_new),
        show_in_manager_(show_in_manager) {
    DCHECK(is_new_ || node);
    window_ = CreateInputWindow(wnd, this);
    model_->AddObserver(this);
  }

  void Show() {
    window_->Show();
  }

  // InputWindowDelegate methods.
  virtual std::wstring GetTextFieldLabel() {
    return l10n_util::GetString(IDS_BOOMARK_BAR_EDIT_FOLDER_LABEL);
  }

  virtual std::wstring GetTextFieldContents() {
    if (is_new_)
      return l10n_util::GetString(IDS_BOOMARK_EDITOR_NEW_FOLDER_NAME);
    return node_->GetTitle();
  }

  virtual bool IsValid(const std::wstring& text) {
    return !text.empty();
  }

  virtual void InputAccepted(const std::wstring& text) {
    if (is_new_) {
      BookmarkNode* node =
          model_->AddGroup(node_, node_->GetChildCount(), text);
      if (show_in_manager_) {
        BookmarkManagerView* manager = BookmarkManagerView::current();
        if (manager && manager->profile() == profile_)
          manager->SelectInTree(node);
      }
    } else {
      model_->SetTitle(node_, text);
    }
  }

  virtual void InputCanceled() {
  }

  virtual void WindowClosing() {
    delete this;
  }

  virtual std::wstring GetWindowTitle() const {
    return is_new_ ?
        l10n_util::GetString(IDS_BOOMARK_FOLDER_EDITOR_WINDOW_TITLE_NEW) :
        l10n_util::GetString(IDS_BOOMARK_FOLDER_EDITOR_WINDOW_TITLE);
  }

  // BookmarkModelObserver methods, all invoke ModelChanged and close the
  // dialog.
  virtual void Loaded(BookmarkModel* model) {}
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) {
    model_->RemoveObserver(this);
    model_ = NULL;
    ModelChanged();
  }

  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 BookmarkNode* old_parent,
                                 int old_index,
                                 BookmarkNode* new_parent,
                                 int new_index) {
    ModelChanged();
  }

  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 BookmarkNode* parent,
                                 int index) {
    ModelChanged();
  }

  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   BookmarkNode* parent,
                                   int index,
                                   BookmarkNode* node) {
    ModelChanged();
  }

  virtual void BookmarkNodeChanged(BookmarkModel* model, BookmarkNode* node) {
    ModelChanged();
  }

  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         BookmarkNode* node) {}

  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             BookmarkNode* node) {
    ModelChanged();
  }

  void ModelChanged() {
    window_->Close();
  }

  Profile* profile_;
  BookmarkModel* model_;
  // If is_new is true, this is the parent to create the new node under.
  // Otherwise this is the node to change the title of.
  BookmarkNode* node_;

  bool is_new_;

  // If is_new_ is true and a new node is created, it is selected in the
  // bookmark manager.
  bool show_in_manager_;
  views::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(EditFolderController);
};

// SelectOnCreationHandler ----------------------------------------------------

// Used when adding a new bookmark. If a new bookmark is created it is selected
// in the bookmark manager.
class SelectOnCreationHandler : public BookmarkEditorView::Handler {
 public:
  explicit SelectOnCreationHandler(Profile* profile) : profile_(profile) {
  }

  virtual void NodeCreated(BookmarkNode* new_node) {
    BookmarkManagerView* manager = BookmarkManagerView::current();
    if (!manager || manager->profile() != profile_)
      return;  // Manager no longer showing, or showing a different profile.

    manager->SelectInTree(new_node);
  }

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SelectOnCreationHandler);
};

}  // namespace

// BookmarkContextMenu -------------------------------------------

BookmarkContextMenu::BookmarkContextMenu(
    gfx::NativeWindow wnd,
    Profile* profile,
    Browser* browser,
    PageNavigator* navigator,
    BookmarkNode* parent,
    const std::vector<BookmarkNode*>& selection,
    ConfigurationType configuration)
    : wnd_(wnd),
      profile_(profile),
      browser_(browser),
      navigator_(navigator),
      parent_(parent),
      selection_(selection),
      model_(profile->GetBookmarkModel()),
      configuration_(configuration) {
  DCHECK(profile_);
  DCHECK(model_->IsLoaded());
  menu_.reset(new views::MenuItemView(this));
  if (configuration != BOOKMARK_MANAGER_ORGANIZE_MENU) {
    if (selection.size() == 1 && selection[0]->is_url()) {
      menu_->AppendMenuItemWithLabel(
          IDS_BOOMARK_BAR_OPEN_ALL,
          l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_IN_NEW_TAB));
      menu_->AppendMenuItemWithLabel(
          IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW,
          l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_IN_NEW_WINDOW));
      menu_->AppendMenuItemWithLabel(
          IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO,
          l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_INCOGNITO));
    } else {
      menu_->AppendMenuItemWithLabel(
          IDS_BOOMARK_BAR_OPEN_ALL,
          l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_ALL));
      menu_->AppendMenuItemWithLabel(
          IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW,
          l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW));
      menu_->AppendMenuItemWithLabel(
          IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO,
          l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO));
    }
    menu_->AppendSeparator();
  }

  if (selection.size() == 1 && selection[0]->is_folder()) {
    menu_->AppendMenuItemWithLabel(IDS_BOOKMARK_BAR_RENAME_FOLDER,
        l10n_util::GetString(IDS_BOOKMARK_BAR_RENAME_FOLDER));
  } else {
    menu_->AppendMenuItemWithLabel(IDS_BOOKMARK_BAR_EDIT,
                                   l10n_util::GetString(IDS_BOOKMARK_BAR_EDIT));
  }
  menu_->AppendMenuItemWithLabel(
      IDS_BOOKMARK_BAR_REMOVE,
      l10n_util::GetString(IDS_BOOKMARK_BAR_REMOVE));

  if (configuration == BOOKMARK_MANAGER_TABLE ||
      configuration == BOOKMARK_MANAGER_TABLE_OTHER ||
      configuration == BOOKMARK_MANAGER_ORGANIZE_MENU ||
      configuration == BOOKMARK_MANAGER_ORGANIZE_MENU_OTHER) {
    menu_->AppendMenuItemWithLabel(
        IDS_BOOKMARK_MANAGER_SHOW_IN_FOLDER,
        l10n_util::GetString(IDS_BOOKMARK_MANAGER_SHOW_IN_FOLDER));
  }

  if (configuration == BOOKMARK_MANAGER_TABLE ||
      configuration == BOOKMARK_MANAGER_TABLE_OTHER ||
      configuration == BOOKMARK_MANAGER_TREE ||
      configuration == BOOKMARK_MANAGER_ORGANIZE_MENU ||
      configuration == BOOKMARK_MANAGER_ORGANIZE_MENU_OTHER) {
    menu_->AppendSeparator();
    menu_->AppendMenuItemWithLabel(
        IDS_CUT, l10n_util::GetString(IDS_CUT));
    menu_->AppendMenuItemWithLabel(
        IDS_COPY, l10n_util::GetString(IDS_COPY));
    menu_->AppendMenuItemWithLabel(
        IDS_PASTE, l10n_util::GetString(IDS_PASTE));
  }

  if (configuration == BOOKMARK_MANAGER_ORGANIZE_MENU) {
    menu_->AppendSeparator();
    menu_->AppendMenuItemWithLabel(
        IDS_BOOKMARK_MANAGER_SORT,
        l10n_util::GetString(IDS_BOOKMARK_MANAGER_SORT));
  }

  menu_->AppendSeparator();

  menu_->AppendMenuItemWithLabel(
      IDS_BOOMARK_BAR_ADD_NEW_BOOKMARK,
      l10n_util::GetString(IDS_BOOMARK_BAR_ADD_NEW_BOOKMARK));
  menu_->AppendMenuItemWithLabel(
      IDS_BOOMARK_BAR_NEW_FOLDER,
      l10n_util::GetString(IDS_BOOMARK_BAR_NEW_FOLDER));

  if (configuration == BOOKMARK_BAR) {
    menu_->AppendSeparator();
    menu_->AppendMenuItemWithLabel(IDS_BOOKMARK_MANAGER,
                                   l10n_util::GetString(IDS_BOOKMARK_MANAGER));
    menu_->AppendMenuItem(IDS_BOOMARK_BAR_ALWAYS_SHOW,
                          l10n_util::GetString(IDS_BOOMARK_BAR_ALWAYS_SHOW),
                          views::MenuItemView::CHECKBOX);
  }
  model_->AddObserver(this);
}

BookmarkContextMenu::~BookmarkContextMenu() {
  if (model_)
    model_->RemoveObserver(this);
}

void BookmarkContextMenu::RunMenuAt(int x, int y) {
  if (!model_->IsLoaded()) {
    NOTREACHED();
    return;
  }
  // width/height don't matter here.
  views::MenuItemView::AnchorPosition anchor =
      (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) ?
      views::MenuItemView::TOPRIGHT : views::MenuItemView::TOPLEFT;
  menu_->RunMenuAt(wnd_, gfx::Rect(x, y, 0, 0), anchor, true);
}

void BookmarkContextMenu::ExecuteCommand(int id) {
  switch (id) {
    case IDS_BOOMARK_BAR_OPEN_ALL:
    case IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO:
    case IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW: {
      PageNavigator* navigator = browser_ ?
          browser_->GetSelectedTabContents() : navigator_;
      WindowOpenDisposition initial_disposition;
      if (id == IDS_BOOMARK_BAR_OPEN_ALL) {
        initial_disposition = NEW_FOREGROUND_TAB;
        UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_OpenAll",
                                  profile_);
      } else if (id == IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW) {
        initial_disposition = NEW_WINDOW;
        UserMetrics::RecordAction(
            L"BookmarkBar_ContextMenu_OpenAllInNewWindow", profile_);
      } else {
        initial_disposition = OFF_THE_RECORD;
        UserMetrics::RecordAction(
            L"BookmarkBar_ContextMenu_OpenAllIncognito", profile_);
      }

      bookmark_utils::OpenAll(wnd_, profile_, navigator, selection_,
                              initial_disposition);
      break;
    }

    case IDS_BOOKMARK_BAR_RENAME_FOLDER:
    case IDS_BOOKMARK_BAR_EDIT:
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_Edit", profile_);

      if (selection_.size() != 1) {
        NOTREACHED();
        return;
      }

      if (selection_[0]->is_url()) {
        BookmarkEditorView::Configuration editor_config;
        if (configuration_ == BOOKMARK_BAR)
          editor_config = BookmarkEditorView::SHOW_TREE;
        else
          editor_config = BookmarkEditorView::NO_TREE;
        BookmarkEditorView::Show(wnd_, profile_, NULL, selection_[0],
                                 editor_config, NULL);
      } else {
        EditFolderController::Show(profile_, wnd_, selection_[0], false,
                                   false);
      }
      break;

    case IDS_BOOKMARK_BAR_REMOVE: {
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_Remove", profile_);
      BookmarkModel* model = RemoveModelObserver();

      for (size_t i = 0; i < selection_.size(); ++i) {
        model->Remove(selection_[i]->GetParent(),
                      selection_[i]->GetParent()->IndexOfChild(selection_[i]));
      }
      selection_.clear();
      break;
    }

    case IDS_BOOMARK_BAR_ADD_NEW_BOOKMARK: {
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_Add", profile_);

      BookmarkEditorView::Configuration editor_config;
      BookmarkEditorView::Handler* handler = NULL;
      if (configuration_ == BOOKMARK_BAR) {
        editor_config = BookmarkEditorView::SHOW_TREE;
      } else {
        editor_config = BookmarkEditorView::NO_TREE;
        // This is owned by the BookmarkEditorView.
        handler = new SelectOnCreationHandler(profile_);
      }
      BookmarkEditorView::Show(wnd_, profile_, GetParentForNewNodes(), NULL,
                               editor_config, handler);
      break;
    }

    case IDS_BOOMARK_BAR_NEW_FOLDER: {
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_NewFolder",
                                profile_);

      EditFolderController::Show(profile_, wnd_, GetParentForNewNodes(),
                                 true, (configuration_ != BOOKMARK_BAR));
      break;
    }

    case IDS_BOOMARK_BAR_ALWAYS_SHOW:
      BookmarkBarView::ToggleWhenVisible(profile_);
      break;

    case IDS_BOOKMARK_MANAGER_SHOW_IN_FOLDER:
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_ShowInFolder",
                                profile_);

      if (selection_.size() != 1) {
        NOTREACHED();
        return;
      }

      if (BookmarkManagerView::current())
        BookmarkManagerView::current()->SelectInTree(selection_[0]);
      break;

    case IDS_BOOKMARK_MANAGER:
      UserMetrics::RecordAction(L"ShowBookmarkManager", profile_);
      BookmarkManagerView::Show(profile_);
      break;

    case IDS_BOOKMARK_MANAGER_SORT:
      UserMetrics::RecordAction(L"BookmarkManager_Sort", profile_);
      model_->SortChildren(parent_);
      break;

    case IDS_COPY:
    case IDS_CUT:
      bookmark_utils::CopyToClipboard(profile_->GetBookmarkModel(),
                                      selection_, id == IDS_CUT);
      break;

    case IDS_PASTE: {
      // Always paste to parent.
      if (!parent_)
        return;

      int index = (selection_.size() == 1) ?
          parent_->IndexOfChild(selection_[0]) : -1;
      if (index != -1)
        index++;
      bookmark_utils::PasteFromClipboard(profile_->GetBookmarkModel(),
                                         parent_, index);
      break;
    }

    default:
      NOTREACHED();
  }
}

bool BookmarkContextMenu::IsItemChecked(int id) const {
  DCHECK(id == IDS_BOOMARK_BAR_ALWAYS_SHOW);
  return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

bool BookmarkContextMenu::IsCommandEnabled(int id) const {
  bool is_root_node =
      (selection_.size() == 1 &&
       selection_[0]->GetParent() == model_->root_node());
  switch (id) {
    case IDS_BOOMARK_BAR_OPEN_INCOGNITO:
      return !profile_->IsOffTheRecord();

    case IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO:
      return HasURLs() && !profile_->IsOffTheRecord();

    case IDS_BOOMARK_BAR_OPEN_ALL:
    case IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW:
      return HasURLs();

    case IDS_BOOKMARK_BAR_RENAME_FOLDER:
    case IDS_BOOKMARK_BAR_EDIT:
      return selection_.size() == 1 && !is_root_node;

    case IDS_BOOKMARK_BAR_REMOVE:
      return !selection_.empty() && !is_root_node;

    case IDS_BOOKMARK_MANAGER_SHOW_IN_FOLDER:
      return (configuration_ == BOOKMARK_MANAGER_TABLE_OTHER ||
              configuration_ == BOOKMARK_MANAGER_ORGANIZE_MENU_OTHER) &&
             selection_.size() == 1;

    case IDS_BOOKMARK_MANAGER_SORT:
      return parent_ && parent_ != model_->root_node();

    case IDS_BOOMARK_BAR_NEW_FOLDER:
    case IDS_BOOMARK_BAR_ADD_NEW_BOOKMARK:
      return GetParentForNewNodes() != NULL;

    case IDS_COPY:
    case IDS_CUT:
      return selection_.size() > 0 && !is_root_node;

    case IDS_PASTE:
      // Always paste to parent.
      return bookmark_utils::CanPasteFromClipboard(parent_);
  }
  return true;
}

void BookmarkContextMenu::BookmarkModelBeingDeleted(BookmarkModel* model) {
  ModelChanged();
}

void BookmarkContextMenu::BookmarkNodeMoved(BookmarkModel* model,
                                            BookmarkNode* old_parent,
                                            int old_index,
                                            BookmarkNode* new_parent,
                                            int new_index) {
  ModelChanged();
}

void BookmarkContextMenu::BookmarkNodeAdded(BookmarkModel* model,
                                            BookmarkNode* parent,
                                            int index) {
  ModelChanged();
}

void BookmarkContextMenu::BookmarkNodeRemoved(BookmarkModel* model,
                                              BookmarkNode* parent,
                                              int index,
                                              BookmarkNode* node) {
  ModelChanged();
}

void BookmarkContextMenu::BookmarkNodeChanged(BookmarkModel* model,
                                              BookmarkNode* node) {
  ModelChanged();
}

void BookmarkContextMenu::BookmarkNodeChildrenReordered(BookmarkModel* model,
                                                        BookmarkNode* node) {
  ModelChanged();
}

void BookmarkContextMenu::ModelChanged() {
  menu_->Cancel();
}

BookmarkModel* BookmarkContextMenu::RemoveModelObserver() {
  BookmarkModel* model = model_;
  model_->RemoveObserver(this);
  model_ = NULL;
  return model;
}

bool BookmarkContextMenu::HasURLs() const {
  for (size_t i = 0; i < selection_.size(); ++i) {
    if (NodeHasURLs(selection_[i]))
      return true;
  }
  return false;
}

BookmarkNode* BookmarkContextMenu::GetParentForNewNodes() const {
  return (selection_.size() == 1 && selection_[0]->is_folder()) ?
      selection_[0] : parent_;
}
