// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmark_bar_context_menu_controller.h"

#include "chrome/browser/bookmarks/bookmark_bar_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/page_navigator.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/browser/views/bookmark_editor_view.h"
#include "chrome/browser/views/input_window.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/views/view_container.h"
#include "chrome/views/window.h"

#include "generated_resources.h"

namespace {

// Returns true if the specified node is of type URL, or has a descendant
// of type URL.
bool NodeHasURLs(BookmarkBarNode* node) {
  if (node->GetType() == history::StarredEntry::URL)
    return true;

  for (int i = 0; i < node->GetChildCount(); ++i) {
    if (NodeHasURLs(node->GetChild(i)))
      return true;
  }
  return false;
}

// Opens a tab/window for node and recursively opens all descendants.
// If open_first_in_new_window is true, the first opened node is opened
// in a new window. navigator indicates the PageNavigator to use for
// new tabs. It is reset if open_first_in_new_window is true.
// opened_url is set to true the first time a new tab is opened.
void OpenAll(BookmarkBarNode* node,
             bool open_first_in_new_window,
             PageNavigator** navigator,
             bool* opened_url) {
  if (node->GetType() == history::StarredEntry::URL) {
    WindowOpenDisposition disposition;
    if (*opened_url)
      disposition = NEW_BACKGROUND_TAB;
    else if (open_first_in_new_window)
      disposition = NEW_WINDOW;
    else  // Open in current window.
      disposition = CURRENT_TAB;
    (*navigator)->OpenURL(node->GetURL(), disposition,
                          PageTransition::AUTO_BOOKMARK);
    if (!*opened_url) {
      *opened_url = true;
      if (open_first_in_new_window || disposition == CURRENT_TAB) {
        // We opened the tab in a new window or in the current tab which
        // likely reset the navigator. Need to reset the page navigator
        // appropriately.
        Browser* new_browser = BrowserList::GetLastActive();
        if (new_browser) {
          TabContents* current_tab = new_browser->GetSelectedTabContents();
          DCHECK(new_browser && current_tab);
          if (new_browser && current_tab)
            *navigator = current_tab;
        }  // else, new_browser == NULL, which happens during testing.
      }
    }
  } else {
    // Group, recurse through children.
    for (int i = 0; i < node->GetChildCount(); ++i) {
      OpenAll(node->GetChild(i), open_first_in_new_window, navigator,
              opened_url);
    }
  }
}

// EditFolderController -------------------------------------------------------

// EditFolderController manages the editing and/or creation of a folder. If the
// user presses ok, the name change is committed to the database.
//
// EditFolderController deletes itself when the window is closed.
//
class EditFolderController : public InputWindowDelegate,
                             public BookmarkBarView::ModelChangedListener {
 public:
  EditFolderController(BookmarkBarView* view,
                       BookmarkBarNode* node,
                       int visual_order,
                       bool is_new)
      : view_(view),
        node_(node),
        visual_order_(visual_order),
        is_new_(is_new) {
    DCHECK(is_new_ || node);
    window_ = CreateInputWindow(view->GetViewContainer()->GetHWND(), this);
    view_->SetModelChangedListener(this);
  }

  void Show() {
    window_->Show();
  }

  virtual void ModelChanged() {
    window_->Close();
  }

 private:
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
    view_->ClearModelChangedListenerIfEquals(this);
    BookmarkBarModel* model = view_->GetProfile()->GetBookmarkBarModel();
    if (is_new_)
      model->AddGroup(node_, visual_order_, text);
    else
      model->SetTitle(node_, text);
  }

  virtual void InputCanceled() {
    view_->ClearModelChangedListenerIfEquals(this);
  }

  virtual void WindowClosing() {
    view_->ClearModelChangedListenerIfEquals(this);
    delete this;
  }

  virtual std::wstring GetWindowTitle() const {
    return is_new_ ?
        l10n_util::GetString(IDS_BOOMARK_FOLDER_EDITOR_WINDOW_TITLE_NEW) :
        l10n_util::GetString(IDS_BOOMARK_FOLDER_EDITOR_WINDOW_TITLE);
  }

  virtual ChromeViews::View* GetContentsView() {
    return view_;
  }

  BookmarkBarView* view_;

  // If is_new is true, this is the parent to create the new node under.
  // Otherwise this is the node to change the title of.
  BookmarkBarNode* node_;

  int visual_order_;
  bool is_new_;
  ChromeViews::Window* window_;

  DISALLOW_EVIL_CONSTRUCTORS(EditFolderController);
};

}  // namespace

// BookmarkBarContextMenuController -------------------------------------------

const int BookmarkBarContextMenuController::always_show_command_id = 1;
const int BookmarkBarContextMenuController::open_bookmark_id = 2;
const int BookmarkBarContextMenuController::open_bookmark_in_new_window_id = 3;
const int BookmarkBarContextMenuController::open_bookmark_in_new_tab_id = 4;
const int BookmarkBarContextMenuController::open_all_bookmarks_id = 5;
const int
    BookmarkBarContextMenuController::open_all_bookmarks_in_new_window_id = 6;
const int BookmarkBarContextMenuController::edit_bookmark_id = 7;
const int BookmarkBarContextMenuController::delete_bookmark_id = 8;
const int BookmarkBarContextMenuController::add_bookmark_id = 9;
const int BookmarkBarContextMenuController::new_folder_id = 10;

BookmarkBarContextMenuController::BookmarkBarContextMenuController(
    BookmarkBarView* view,
    BookmarkBarNode* node)
    : view_(view),
      node_(node),
      menu_(this) {
  if (node->GetType() == history::StarredEntry::URL) {
    menu_.AppendMenuItemWithLabel(
        open_bookmark_id,
        l10n_util::GetString(IDS_BOOMARK_BAR_OPEN));
    menu_.AppendMenuItemWithLabel(
        open_bookmark_in_new_tab_id,
        l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_IN_NEW_TAB));
    menu_.AppendMenuItemWithLabel(
        open_bookmark_in_new_window_id,
        l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_IN_NEW_WINDOW));
  } else {
    menu_.AppendMenuItemWithLabel(
        open_all_bookmarks_id,
        l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_ALL));
    menu_.AppendMenuItemWithLabel(
        open_all_bookmarks_in_new_window_id,
        l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW));
  }
  menu_.AppendSeparator();

  if (node->GetParent() !=
      view->GetProfile()->GetBookmarkBarModel()->root_node()) {
    menu_.AppendMenuItemWithLabel(edit_bookmark_id,
                                  l10n_util::GetString(IDS_BOOKMARK_BAR_EDIT));
    menu_.AppendMenuItemWithLabel(
        delete_bookmark_id,
        l10n_util::GetString(IDS_BOOKMARK_BAR_REMOVE));
  }

  menu_.AppendMenuItemWithLabel(
      add_bookmark_id,
      l10n_util::GetString(IDS_BOOMARK_BAR_ADD_NEW_BOOKMARK));
  menu_.AppendMenuItemWithLabel(
      new_folder_id,
      l10n_util::GetString(IDS_BOOMARK_BAR_NEW_FOLDER));
  menu_.AppendSeparator();
  menu_.AppendMenuItem(always_show_command_id,
                       l10n_util::GetString(IDS_BOOMARK_BAR_ALWAYS_SHOW),
                       ChromeViews::MenuItemView::CHECKBOX);
}

void BookmarkBarContextMenuController::RunMenuAt(int x, int y) {
  // Record the current ModelChangedListener. It will be non-null when we're
  // used as the context menu for another menu.
  ModelChangedListener* last_listener = view_->GetModelChangedListener();

  view_->SetModelChangedListener(this);

  // width/height don't matter here.
  menu_.RunMenuAt(view_->GetViewContainer()->GetHWND(), gfx::Rect(x, y, 0, 0),
                  ChromeViews::MenuItemView::TOPLEFT, false);

  if (view_->GetModelChangedListener() == this)
    view_->SetModelChangedListener(last_listener);
}

void BookmarkBarContextMenuController::ModelChanged() {
  menu_.Cancel();
}

void BookmarkBarContextMenuController::ExecuteCommand(int id) {
  Profile* profile = view_->GetProfile();

  switch (id) {
    case open_bookmark_id:
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_Open", profile);

      view_->GetPageNavigator()->OpenURL(node_->GetURL(), CURRENT_TAB,
                                         PageTransition::AUTO_BOOKMARK);
      break;

    case open_bookmark_in_new_window_id:
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_OpenInNewWindow",
                                profile);

      view_->GetPageNavigator()->OpenURL(node_->GetURL(), NEW_WINDOW,
                                         PageTransition::AUTO_BOOKMARK);
      break;

    case open_bookmark_in_new_tab_id:
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_OpenInNewTab",
                                profile);

      view_->GetPageNavigator()->OpenURL(node_->GetURL(), NEW_FOREGROUND_TAB,
                                         PageTransition::AUTO_BOOKMARK);
      break;

    case open_all_bookmarks_id:
    case open_all_bookmarks_in_new_window_id: {
      if (id == open_all_bookmarks_id) {
        UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_OpenAll",
                                  profile);
      } else {
        UserMetrics::RecordAction(
            L"BookmarkBar_ContextMenu_OpenAllInNewWindow", profile);
      }

      BookmarkBarNode* node = node_;
      PageNavigator* navigator = view_->GetPageNavigator();
      bool opened_url = false;
      OpenAll(node, (id == open_all_bookmarks_in_new_window_id), &navigator,
              &opened_url);
      break;
    }

    case edit_bookmark_id:
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_Edit", profile);

      if (node_->GetType() == history::StarredEntry::URL) {
        BookmarkEditorView::Show(view_->GetViewContainer()->GetHWND(),
                                 view_->GetProfile(), node_->GetURL(),
                                 node_->GetTitle());
      } else {
        // Controller deletes itself when done.
        EditFolderController* controller = new EditFolderController(
            view_, node_, -1, false);
        controller->Show();
      }
      break;

    case delete_bookmark_id: {
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_Remove", profile);

      view_->GetModel()->Remove(node_->GetParent(),
                                node_->GetParent()->IndexOfChild(node_));
      break;
    }

    case add_bookmark_id: {
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_Add", profile);

      BookmarkEditorView::Show(view_->GetViewContainer()->GetHWND(),
                               view_->GetProfile(), GURL(), std::wstring());
      break;
    }

    case new_folder_id: {
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_NewFolder",
                                profile);

      int visual_order;
      BookmarkBarNode* parent =
          GetParentAndVisualOrderForNewNode(&visual_order);
      GetParentAndVisualOrderForNewNode(&visual_order);
      // Controller deletes itself when done.
      EditFolderController* controller =
          new EditFolderController(view_, parent, visual_order, true);
      controller->Show();
      break;
    }

    case always_show_command_id:
      view_->ToggleWhenVisible();
      break;

    default:
      NOTREACHED();
  }
}

bool BookmarkBarContextMenuController::IsItemChecked(int id) const {
  DCHECK(id == always_show_command_id);
  return view_->GetProfile()->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

bool BookmarkBarContextMenuController::IsCommandEnabled(int id) const {
  if (id == open_all_bookmarks_id || id == open_all_bookmarks_in_new_window_id)
    return NodeHasURLs(node_);

  return true;
}

// Returns the parent node and visual_order to use when adding new
// bookmarks/folders.
BookmarkBarNode* BookmarkBarContextMenuController::
    GetParentAndVisualOrderForNewNode(int* visual_order) {
  if (node_->GetType() != history::StarredEntry::URL) {
    // Adding to a group always adds to the end.
    *visual_order = node_->GetChildCount();
    return node_;
  } else {
    DCHECK(node_->GetParent());
    *visual_order = node_->GetParent()->IndexOfChild(node_) + 1;
    return node_->GetParent();
  }
}

