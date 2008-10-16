// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmark_bar_context_menu_controller.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/page_navigator.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/browser/views/bookmark_editor_view.h"
#include "chrome/browser/views/input_window.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/views/container.h"
#include "chrome/views/window.h"

#include "chromium_strings.h"
#include "generated_resources.h"

namespace {

// Number of bookmarks we'll open before prompting the user to see if they
// really want to open all.
const int kNumURLsBeforePrompting = 15;

// Returns true if the specified node is of type URL, or has a descendant
// of type URL.
bool NodeHasURLs(BookmarkNode* node) {
  if (node->GetType() == history::StarredEntry::URL)
    return true;

  for (int i = 0; i < node->GetChildCount(); ++i) {
    if (NodeHasURLs(node->GetChild(i)))
      return true;
  }
  return false;
}

// Implementation of OpenAll. Opens all nodes of type URL and recurses for
// groups.
void OpenAllImpl(BookmarkNode* node,
                 WindowOpenDisposition initial_disposition,
                 PageNavigator** navigator,
                 bool* opened_url) {
  if (node->GetType() == history::StarredEntry::URL) {
    WindowOpenDisposition disposition;
    if (*opened_url)
      disposition = NEW_BACKGROUND_TAB;
    else
      disposition = initial_disposition;
    (*navigator)->OpenURL(node->GetURL(), disposition,
                          PageTransition::AUTO_BOOKMARK);
    if (!*opened_url) {
      *opened_url = true;
      // We opened the first URL which may have opened a new window or clobbered
      // the current page, reset the navigator just to be sure.
      Browser* new_browser = BrowserList::GetLastActive();
      if (new_browser) {
        TabContents* current_tab = new_browser->GetSelectedTabContents();
        DCHECK(new_browser && current_tab);
        if (new_browser && current_tab)
          *navigator = current_tab;
      }  // else, new_browser == NULL, which happens during testing.
    }
  } else {
    // Group, recurse through children.
    for (int i = 0; i < node->GetChildCount(); ++i) {
      OpenAllImpl(node->GetChild(i), initial_disposition, navigator,
                  opened_url);
    }
  }
}

// Returns the number of descendants of node that are of type url.
int DescendantURLCount(BookmarkNode* node) {
  int result = 0;
  for (int i = 0; i < node->GetChildCount(); ++i) {
    BookmarkNode* child = node->GetChild(i);
    if (child->is_url())
      result++;
    else
      result += DescendantURLCount(child);
  }
  return result;
}

bool ShouldOpenAll(HWND parent, BookmarkNode* node) {
  int descendant_count = DescendantURLCount(node);
  if (descendant_count < kNumURLsBeforePrompting)
    return true;

  std::wstring message =
      l10n_util::GetStringF(IDS_BOOKMARK_BAR_SHOULD_OPEN_ALL,
                            IntToWString(descendant_count));
  return (MessageBox(parent, message.c_str(),
                     l10n_util::GetString(IDS_PRODUCT_NAME).c_str(),
                     MB_YESNO | MB_ICONWARNING | MB_TOPMOST) == IDYES);
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
                       BookmarkNode* node,
                       int visual_order,
                       bool is_new)
      : view_(view),
        node_(node),
        visual_order_(visual_order),
        is_new_(is_new) {
    DCHECK(is_new_ || node);
    window_ = CreateInputWindow(view->GetContainer()->GetHWND(), this);
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
    BookmarkModel* model = view_->GetProfile()->GetBookmarkModel();
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
  BookmarkNode* node_;

  int visual_order_;
  bool is_new_;
  ChromeViews::Window* window_;

  DISALLOW_EVIL_CONSTRUCTORS(EditFolderController);
};

}  // namespace

// BookmarkBarContextMenuController -------------------------------------------

// static
void BookmarkBarContextMenuController::OpenAll(
    HWND parent,
    PageNavigator* navigator,
    BookmarkNode* node,
    WindowOpenDisposition initial_disposition) {
  if (!ShouldOpenAll(parent, node))
    return;

  PageNavigator* nav = navigator;
  bool opened_url = false;
  OpenAllImpl(node, initial_disposition, &nav, &opened_url);
}

BookmarkBarContextMenuController::BookmarkBarContextMenuController(
    BookmarkBarView* view,
    BookmarkNode* node)
    : view_(view),
      node_(node),
      menu_(this) {
  if (node->GetType() == history::StarredEntry::URL) {
    menu_.AppendMenuItemWithLabel(
        IDS_BOOMARK_BAR_OPEN_IN_NEW_TAB,
        l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_IN_NEW_TAB));
    menu_.AppendMenuItemWithLabel(
        IDS_BOOMARK_BAR_OPEN_IN_NEW_WINDOW,
        l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_IN_NEW_WINDOW));
    menu_.AppendMenuItemWithLabel(
        IDS_BOOMARK_BAR_OPEN_INCOGNITO,
        l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_INCOGNITO));
  } else {
    menu_.AppendMenuItemWithLabel(
        IDS_BOOMARK_BAR_OPEN_ALL,
        l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_ALL));
    menu_.AppendMenuItemWithLabel(
        IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW,
        l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW));
    menu_.AppendMenuItemWithLabel(
        IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO,
        l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO));
  }
  menu_.AppendSeparator();

  if (node->GetParent() !=
      view->GetProfile()->GetBookmarkModel()->root_node()) {
    menu_.AppendMenuItemWithLabel(IDS_BOOKMARK_BAR_EDIT,
                                  l10n_util::GetString(IDS_BOOKMARK_BAR_EDIT));
    menu_.AppendMenuItemWithLabel(
        IDS_BOOKMARK_BAR_REMOVE,
        l10n_util::GetString(IDS_BOOKMARK_BAR_REMOVE));
  }

  menu_.AppendMenuItemWithLabel(
      IDS_BOOMARK_BAR_ADD_NEW_BOOKMARK,
      l10n_util::GetString(IDS_BOOMARK_BAR_ADD_NEW_BOOKMARK));
  menu_.AppendMenuItemWithLabel(
      IDS_BOOMARK_BAR_NEW_FOLDER,
      l10n_util::GetString(IDS_BOOMARK_BAR_NEW_FOLDER));
  menu_.AppendSeparator();
  menu_.AppendMenuItem(IDS_BOOMARK_BAR_ALWAYS_SHOW,
                       l10n_util::GetString(IDS_BOOMARK_BAR_ALWAYS_SHOW),
                       ChromeViews::MenuItemView::CHECKBOX);
}

void BookmarkBarContextMenuController::RunMenuAt(int x, int y) {
  // Record the current ModelChangedListener. It will be non-null when we're
  // used as the context menu for another menu.
  ModelChangedListener* last_listener = view_->GetModelChangedListener();

  view_->SetModelChangedListener(this);

  // width/height don't matter here.
  menu_.RunMenuAt(view_->GetContainer()->GetHWND(), gfx::Rect(x, y, 0, 0),
                  ChromeViews::MenuItemView::TOPLEFT, true);

  if (view_->GetModelChangedListener() == this)
    view_->SetModelChangedListener(last_listener);
}

void BookmarkBarContextMenuController::ModelChanged() {
  menu_.Cancel();
}

void BookmarkBarContextMenuController::ExecuteCommand(int id) {
  Profile* profile = view_->GetProfile();

  switch (id) {
    case IDS_BOOMARK_BAR_OPEN_INCOGNITO:
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_OpenInIncognito",
                                profile);

      view_->GetPageNavigator()->OpenURL(node_->GetURL(), OFF_THE_RECORD,
                                         PageTransition::AUTO_BOOKMARK);
      break;

    case IDS_BOOMARK_BAR_OPEN_IN_NEW_WINDOW:
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_OpenInNewWindow",
                                profile);

      view_->GetPageNavigator()->OpenURL(node_->GetURL(), NEW_WINDOW,
                                         PageTransition::AUTO_BOOKMARK);
      break;

    case IDS_BOOMARK_BAR_OPEN_IN_NEW_TAB:
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_OpenInNewTab",
                                profile);

      view_->GetPageNavigator()->OpenURL(node_->GetURL(), NEW_FOREGROUND_TAB,
                                         PageTransition::AUTO_BOOKMARK);
      break;

    case IDS_BOOMARK_BAR_OPEN_ALL:
    case IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO:
    case IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW: {
      WindowOpenDisposition initial_disposition;
      if (id == IDS_BOOMARK_BAR_OPEN_ALL) {
        initial_disposition = CURRENT_TAB;
        UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_OpenAll",
                                  profile);
      } else if (id == IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW) {
        initial_disposition = NEW_WINDOW;
        UserMetrics::RecordAction(
            L"BookmarkBar_ContextMenu_OpenAllInNewWindow", profile);
      } else {
        initial_disposition = OFF_THE_RECORD;
        UserMetrics::RecordAction(
            L"BookmarkBar_ContextMenu_OpenAllIncognito", profile);
      }

      // GetContainer is NULL during testing.
      HWND parent_hwnd = view_->GetContainer() ?
          view_->GetContainer()->GetHWND() : 0;

      OpenAll(parent_hwnd, view_->GetPageNavigator(), node_,
              initial_disposition);
      break;
    }

    case IDS_BOOKMARK_BAR_EDIT:
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_Edit", profile);

      if (node_->GetType() == history::StarredEntry::URL) {
        BookmarkEditorView::Show(view_->GetContainer()->GetHWND(),
                                 view_->GetProfile(), NULL, node_);
      } else {
        // Controller deletes itself when done.
        EditFolderController* controller = new EditFolderController(
            view_, node_, -1, false);
        controller->Show();
      }
      break;

    case IDS_BOOKMARK_BAR_REMOVE: {
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_Remove", profile);

      view_->GetModel()->Remove(node_->GetParent(),
                                node_->GetParent()->IndexOfChild(node_));
      break;
    }

    case IDS_BOOMARK_BAR_ADD_NEW_BOOKMARK: {
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_Add", profile);

      BookmarkEditorView::Show(view_->GetContainer()->GetHWND(),
                               view_->GetProfile(), node_, NULL);
      break;
    }

    case IDS_BOOMARK_BAR_NEW_FOLDER: {
      UserMetrics::RecordAction(L"BookmarkBar_ContextMenu_NewFolder",
                                profile);

      int visual_order;
      BookmarkNode* parent =
          GetParentAndVisualOrderForNewNode(&visual_order);
      GetParentAndVisualOrderForNewNode(&visual_order);
      // Controller deletes itself when done.
      EditFolderController* controller =
          new EditFolderController(view_, parent, visual_order, true);
      controller->Show();
      break;
    }

    case IDS_BOOMARK_BAR_ALWAYS_SHOW:
      view_->ToggleWhenVisible();
      break;

    default:
      NOTREACHED();
  }
}

bool BookmarkBarContextMenuController::IsItemChecked(int id) const {
  DCHECK(id == IDS_BOOMARK_BAR_ALWAYS_SHOW);
  return view_->GetProfile()->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

bool BookmarkBarContextMenuController::IsCommandEnabled(int id) const {
  switch (id) {
    case IDS_BOOMARK_BAR_OPEN_INCOGNITO:
      return !view_->GetProfile()->IsOffTheRecord();

    case IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO:
      return NodeHasURLs(node_) && !view_->GetProfile()->IsOffTheRecord();

    case IDS_BOOMARK_BAR_OPEN_ALL:
    case IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW:
      return NodeHasURLs(node_);
  }
  return true;
}

// Returns the parent node and visual_order to use when adding new
// bookmarks/folders.
BookmarkNode* BookmarkBarContextMenuController::
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
