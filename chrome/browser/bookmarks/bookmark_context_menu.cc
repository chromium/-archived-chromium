// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_context_menu.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/page_navigator.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/browser/views/bookmark_editor_view.h"
#include "chrome/browser/views/bookmark_manager_view.h"
#include "chrome/browser/views/input_window.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/views/container.h"
#include "chrome/views/window.h"

#include "generated_resources.h"

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
                   HWND hwnd,
                   BookmarkNode* node,
                   bool is_new) {
    // EditFolderController deletes itself when done.
    EditFolderController* controller =
        new EditFolderController(profile, hwnd, node, is_new);
    controller->Show();
  }

 private:
  EditFolderController(Profile* profile,
                       HWND hwnd,
                       BookmarkNode* node,
                       bool is_new)
      : profile_(profile),
        model_(profile->GetBookmarkModel()),
        node_(node),
        is_new_(is_new) {
    DCHECK(is_new_ || node);
    window_ = CreateInputWindow(hwnd, this);
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
    if (is_new_)
      model_->AddGroup(node_, node_->GetChildCount(), text);
    else
      model_->SetTitle(node_, text);
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

  void ModelChanged() {
    window_->Close();
  }

  Profile* profile_;
  BookmarkModel* model_;
  // If is_new is true, this is the parent to create the new node under.
  // Otherwise this is the node to change the title of.
  BookmarkNode* node_;

  bool is_new_;
  views::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(EditFolderController);
};

}  // namespace

// BookmarkContextMenu -------------------------------------------

BookmarkContextMenu::BookmarkContextMenu(
    HWND hwnd,
    Profile* profile,
    Browser* browser,
    PageNavigator* navigator,
    BookmarkNode* parent,
    const std::vector<BookmarkNode*>& selection,
    ConfigurationType configuration)
    : hwnd_(hwnd),
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

  menu_->AppendMenuItemWithLabel(IDS_BOOKMARK_BAR_EDIT,
                                l10n_util::GetString(IDS_BOOKMARK_BAR_EDIT));
  menu_->AppendMenuItemWithLabel(
      IDS_BOOKMARK_BAR_REMOVE,
      l10n_util::GetString(IDS_BOOKMARK_BAR_REMOVE));
 
  if (configuration != BOOKMARK_BAR) {
    menu_->AppendMenuItemWithLabel(
        IDS_BOOKMARK_MANAGER_SHOW_IN_FOLDER,
        l10n_util::GetString(IDS_BOOKMARK_MANAGER_SHOW_IN_FOLDER));
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
  menu_->RunMenuAt(hwnd_, gfx::Rect(x, y, 0, 0), views::MenuItemView::TOPLEFT,
                  true);
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

      bookmark_utils::OpenAll(hwnd_, profile_, navigator, selection_,
                              initial_disposition);
      break;
    }

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
        BookmarkEditorView::Show(hwnd_, profile_, NULL, selection_[0],
                                 editor_config);
      } else {
        EditFolderController::Show(profile_, hwnd_, selection_[0], false);
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
      if (configuration_ == BOOKMARK_BAR)
        editor_config = BookmarkEditorView::SHOW_TREE;
      else
        editor_config = BookmarkEditorView::NO_TREE;
      BookmarkEditorView::Show(hwnd_, profile_, GetParentForNewNodes(), NULL,
                               editor_config);
      break;
    }

    case IDS_BOOMARK_BAR_NEW_FOLDER: {
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_NewFolder",
                                profile_);

      EditFolderController::Show(profile_, hwnd_, GetParentForNewNodes(),
                                 true);
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

    case IDS_BOOKMARK_BAR_EDIT:
      return selection_.size() == 1 && !is_root_node;

    case IDS_BOOKMARK_BAR_REMOVE:
      return !selection_.empty() && !is_root_node;

    case IDS_BOOKMARK_MANAGER_SHOW_IN_FOLDER:
      return configuration_ == BOOKMARK_MANAGER_TABLE &&
             selection_.size() == 1;

    case IDS_BOOMARK_BAR_NEW_FOLDER:
    case IDS_BOOMARK_BAR_ADD_NEW_BOOKMARK:
      return GetParentForNewNodes() != NULL;
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
