// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/bookmark_editor_view.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/url_fixer_upper.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/background.h"
#include "chrome/views/focus_manager.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/label.h"
#include "chrome/views/window.h"
#include "googleurl/src/gurl.h"

#include "chromium_strings.h"
#include "generated_resources.h"

using ChromeViews::ColumnSet;
using ChromeViews::GridLayout;
using ChromeViews::Label;
using ChromeViews::NativeButton;
using ChromeViews::TextField;
using ChromeViews::TreeNode;

// Background color of text field when URL is invalid.
static const SkColor kErrorColor = SkColorSetRGB(0xFF, 0xBC, 0xBC);

// Preferred width of the tree.
static const int kTreeWidth = 300;

// ID for various children.
static const int kNewGroupButtonID             = 1002;

// static
void BookmarkEditorView::Show(HWND parent_hwnd,
                              Profile* profile,
                              BookmarkNode* parent,
                              BookmarkNode* node) {
  DCHECK(profile);
  BookmarkEditorView* editor = new BookmarkEditorView(profile, parent, node);
  editor->Show(parent_hwnd);
}

BookmarkEditorView::BookmarkEditorView(Profile* profile,
                                       BookmarkNode* parent,
                                       BookmarkNode* node)
    : profile_(profile),
#pragma warning(suppress: 4355)  // Okay to pass "this" here.
      new_group_button_(
          l10n_util::GetString(IDS_BOOMARK_EDITOR_NEW_FOLDER_BUTTON)),
      parent_(parent),
      node_(node),
      running_menu_for_root_(false) {
  DCHECK(profile);
  Init();
}

BookmarkEditorView::~BookmarkEditorView() {
  bb_model_->RemoveObserver(this);
}

bool BookmarkEditorView::IsDialogButtonEnabled(DialogButton button) const {
  if (button == DIALOGBUTTON_OK) {
    const GURL url(GetInputURL());
    return bb_model_->IsLoaded() && url.is_valid();
  }
  return true;
}
bool BookmarkEditorView::IsModal() const {
  return true;
}

std::wstring BookmarkEditorView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_BOOMARK_EDITOR_TITLE);
}

bool BookmarkEditorView::Accept() {
  if (!IsDialogButtonEnabled(DIALOGBUTTON_OK)) {
    // The url is invalid, focus the url field.
    url_tf_.SelectAll();
    url_tf_.RequestFocus();
    return false;
  }
  // Otherwise save changes and close the dialog box.
  ApplyEdits();
  return true;
}

bool BookmarkEditorView::AreAcceleratorsEnabled(DialogButton button) {
  return !tree_view_.GetEditingNode();
}

ChromeViews::View* BookmarkEditorView::GetContentsView() {
  return this;
}

void BookmarkEditorView::Layout() {
  // Let the grid layout manager lay out most of the dialog...
  GetLayoutManager()->Layout(this);

  // Manually lay out the New Folder button in the same row as the OK/Cancel
  // buttons...
  CRect parent_bounds;
  GetParent()->GetLocalBounds(&parent_bounds, false);
  CSize prefsize;
  new_group_button_.GetPreferredSize(&prefsize);
  int button_y = parent_bounds.bottom - prefsize.cy - kButtonVEdgeMargin;
  new_group_button_.SetBounds(kPanelHorizMargin, button_y, prefsize.cx,
                              prefsize.cy);
}

void BookmarkEditorView::GetPreferredSize(CSize *out) {
  DCHECK(out);
  *out = ChromeViews::Window::GetLocalizedContentsSize(
      IDS_EDITBOOKMARK_DIALOG_WIDTH_CHARS,
      IDS_EDITBOOKMARK_DIALOG_HEIGHT_LINES).ToSIZE();
}

void BookmarkEditorView::DidChangeBounds(const CRect& previous,
                                         const CRect& current) {
  Layout();
}

void BookmarkEditorView::ViewHierarchyChanged(bool is_add,
                                              ChromeViews::View* parent,
                                              ChromeViews::View* child) {
  if (child == this) {
    // Add and remove the New Folder button from the ClientView's hierarchy.
    if (is_add) {
      parent->AddChildView(&new_group_button_);
    } else {
      parent->RemoveChildView(&new_group_button_);
    }
  }
}

void BookmarkEditorView::OnTreeViewSelectionChanged(
    ChromeViews::TreeView* tree_view) {
}

bool BookmarkEditorView::CanEdit(ChromeViews::TreeView* tree_view,
                                 ChromeViews::TreeModelNode* node) {
  // Only allow editting of children of the bookmark bar node and other node.
  EditorNode* bb_node = tree_model_->AsNode(node);
  return (bb_node->GetParent() && bb_node->GetParent()->GetParent());
}

void BookmarkEditorView::ContentsChanged(TextField* sender,
                                         const std::wstring& new_contents) {
  UserInputChanged();
}

void BookmarkEditorView::ButtonPressed(NativeButton* sender) {
  DCHECK(sender);
  switch (sender->GetID()) {
    case kNewGroupButtonID:
      NewGroup();
      break;

    default:
      NOTREACHED();
  }
}

void BookmarkEditorView::ExecuteCommand(int id) {
  DCHECK(tree_view_.GetSelectedNode());
  if (id == IDS_EDIT) {
    tree_view_.StartEditing(tree_view_.GetSelectedNode());
  } else {
    DCHECK(id == IDS_BOOMARK_EDITOR_NEW_FOLDER_MENU_ITEM);
    NewGroup();
  }
}

bool BookmarkEditorView::IsCommandEnabled(int id) const {
  return (id != IDS_EDIT || !running_menu_for_root_);
}

void BookmarkEditorView::Show(HWND parent_hwnd) {
  ChromeViews::Window::CreateChromeWindow(parent_hwnd, gfx::Rect(), this);
  UserInputChanged();
  if (bb_model_->IsLoaded())
    ExpandAndSelect();
  window()->Show();
  // Select all the text in the name textfield.
  title_tf_.SelectAll();
  // Give focus to the name textfield.
  title_tf_.RequestFocus();
}

void BookmarkEditorView::Close() {
  DCHECK(window());
  window()->Close();
}

void BookmarkEditorView::ShowContextMenu(View* source,
                                         int x,
                                         int y,
                                         bool is_mouse_gesture) {
  DCHECK(source == &tree_view_);
  if (!tree_view_.GetSelectedNode())
    return;
  running_menu_for_root_ =
      (tree_model_->GetParent(tree_view_.GetSelectedNode()) ==
       tree_model_->GetRoot());
  context_menu_.reset(new Menu(this, Menu::TOPLEFT,
                               GetViewContainer()->GetHWND()));
  context_menu_->AppendMenuItemWithLabel(IDS_EDIT,
      l10n_util::GetString(IDS_EDIT));
  context_menu_->AppendMenuItemWithLabel(
      IDS_BOOMARK_EDITOR_NEW_FOLDER_MENU_ITEM,
      l10n_util::GetString(IDS_BOOMARK_EDITOR_NEW_FOLDER_MENU_ITEM));
  context_menu_->RunMenuAt(x, y);
}

void BookmarkEditorView::Init() {
  tree_view_.SetContextMenuController(this);
  bb_model_ = profile_->GetBookmarkModel();
  DCHECK(bb_model_);
  bb_model_->AddObserver(this);

  tree_view_.SetRootShown(false);
  // Tell View not to delete all Views declared by value.
  tree_view_.SetParentOwned(false);
  new_group_button_.SetParentOwned(false);
  url_tf_.SetParentOwned(false);
  title_tf_.SetParentOwned(false);

  new_group_button_.SetEnabled(false);
  new_group_button_.SetListener(this);
  new_group_button_.SetID(kNewGroupButtonID);

  title_tf_.SetText(node_ ? node_->GetTitle() : std::wstring());
  title_tf_.SetController(this);

  url_tf_.SetText(node_ ? UTF8ToWide(node_->GetURL().spec()) : std::wstring());
  url_tf_.SetController(this);

  // Yummy layout code.
  const int labels_column_set_id = 0;
  const int single_column_view_set_id = 1;
  const int buttons_column_set_id = 2;
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);
  ColumnSet* column_set = layout->AddColumnSet(labels_column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::FIXED, kTreeWidth, 0);

  column_set = layout->AddColumnSet(buttons_column_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(1, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->LinkColumnSizes(0, 2, 4, -1);

  layout->StartRow(0, labels_column_set_id);
  layout->AddView(
      new Label(l10n_util::GetString(IDS_BOOMARK_EDITOR_NAME_LABEL)));
  layout->AddView(&title_tf_);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, labels_column_set_id);
  layout->AddView(
      new Label(l10n_util::GetString(IDS_BOOMARK_EDITOR_URL_LABEL)));
  layout->AddView(&url_tf_);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(1, single_column_view_set_id);
  layout->AddView(&tree_view_);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  if (bb_model_->IsLoaded())
    Reset();
}

void BookmarkEditorView::BookmarkNodeMoved(BookmarkModel* model,
                                           BookmarkNode* old_parent,
                                           int old_index,
                                           BookmarkNode* new_parent,
                                           int new_index) {
  Reset();
}

void BookmarkEditorView::BookmarkNodeAdded(BookmarkModel* model,
                                           BookmarkNode* parent,
                                           int index) {
  Reset();
}

void BookmarkEditorView::BookmarkNodeRemoved(BookmarkModel* model,
                                             BookmarkNode* parent,
                                             int index) {
  if ((node_ && !node_->GetParent()) || (parent_ && !parent_->GetParent())) {
    // The node, or its parent was removed. Close the dialog.
    window()->Close();
  } else {
    Reset();
  }
}

void BookmarkEditorView::Reset() {
  // Do this first, otherwise when we invoke SetModel with the real one
  // tree_view will try to invoke something on the model we just deleted.
  tree_view_.SetModel(NULL);

  EditorNode* root_node = CreateRootNode();
  tree_model_.reset(new EditorTreeModel(root_node));

  tree_view_.SetModel(tree_model_.get());
  tree_view_.SetController(this);

  new_group_button_.SetEnabled(true);

  context_menu_.reset();

  if (GetParent()) {
    ExpandAndSelect();
    UserInputChanged();
  } else if (GetParent()) {
    tree_view_.ExpandAll();
  }
}
GURL BookmarkEditorView::GetInputURL() const {
  std::wstring input = URLFixerUpper::FixupURL(url_tf_.GetText(), L"");
  return GURL(input);
}

std::wstring BookmarkEditorView::GetInputTitle() const {
  return title_tf_.GetText();
}

void BookmarkEditorView::UserInputChanged() {
  const GURL url(GetInputURL());
  if (!url.is_valid())
    url_tf_.SetBackgroundColor(kErrorColor);
  else
    url_tf_.SetDefaultBackgroundColor();
  GetDialogClientView()->UpdateDialogButtons();
}

void BookmarkEditorView::NewGroup() {
  // Create a new entry parented to the selected item, or the bookmark
  // bar if nothing is selected.
  EditorNode* parent = tree_model_->AsNode(tree_view_.GetSelectedNode());
  if (!parent) {
    NOTREACHED();
    return;
  }

  tree_view_.StartEditing(AddNewGroup(parent));
}

BookmarkEditorView::EditorNode* BookmarkEditorView::AddNewGroup(
    EditorNode* parent) {
  EditorNode* new_node = new EditorNode();
  new_node->SetTitle(l10n_util::GetString(IDS_BOOMARK_EDITOR_NEW_FOLDER_NAME));
  new_node->value = 0;
  // new_node is now owned by parent.
  tree_model_->Add(parent, parent->GetChildCount(), new_node);
  return new_node;
}

void BookmarkEditorView::ExpandAndSelect() {
  tree_view_.ExpandAll();

  BookmarkNode* to_select = node_ ? node_->GetParent() : parent_;
  int group_id_to_select = to_select->id();
  DCHECK(group_id_to_select);  // GetMostRecentParent should never return NULL.
  EditorNode* b_node =
      FindNodeWithID(tree_model_->GetRoot(), group_id_to_select);
  if (!b_node)
    b_node = tree_model_->GetRoot()->GetChild(0);  // Bookmark bar node.

  tree_view_.SetSelectedNode(b_node);
}

BookmarkEditorView::EditorNode* BookmarkEditorView::CreateRootNode() {
  EditorNode* root_node = new EditorNode(std::wstring(), 0);
  BookmarkNode* bb_root_node = bb_model_->root_node();
  CreateNodes(bb_root_node, root_node);
  DCHECK(root_node->GetChildCount() == 2);
  DCHECK(bb_root_node->GetChild(0)->GetType() ==
         history::StarredEntry::BOOKMARK_BAR);
  DCHECK(bb_root_node->GetChild(1)->GetType() == history::StarredEntry::OTHER);
  return root_node;
}

void BookmarkEditorView::CreateNodes(BookmarkNode* bb_node,
                                     BookmarkEditorView::EditorNode* b_node) {
  for (int i = 0; i < bb_node->GetChildCount(); ++i) {
    BookmarkNode* child_bb_node = bb_node->GetChild(i);
    if (child_bb_node->is_folder()) {
      EditorNode* new_b_node = new EditorNode(child_bb_node->GetTitle(),
                                                  child_bb_node->id());
      b_node->Add(b_node->GetChildCount(), new_b_node);
      CreateNodes(child_bb_node, new_b_node);
    }
  }
}

BookmarkEditorView::EditorNode* BookmarkEditorView::FindNodeWithID(
    BookmarkEditorView::EditorNode* node,
    int id) {
  if (node->value == id)
    return node;
  for (int i = 0; i < node->GetChildCount(); ++i) {
    EditorNode* result = FindNodeWithID(node->GetChild(i), id);
    if (result)
      return result;
  }
  return NULL;
}

void BookmarkEditorView::ApplyEdits() {
  DCHECK(bb_model_->IsLoaded());

  if (!tree_view_.GetSelectedNode()) {
    NOTREACHED();
    return;
  }
  ApplyEdits(tree_model_->AsNode(tree_view_.GetSelectedNode()));
}

void BookmarkEditorView::ApplyEdits(EditorNode* parent) {
  DCHECK(parent);

  // We're going to apply edits to the bookmark bar model, which will call us
  // back. Normally when a structural edit occurs we reset the tree model.
  // We don't want to do that here, so we remove ourselves as an observer.
  bb_model_->RemoveObserver(this);

  GURL new_url(GetInputURL());
  std::wstring new_title(GetInputTitle());

  BookmarkNode* old_parent = node_ ? node_->GetParent() : NULL;
  const int old_index = old_parent ? old_parent->IndexOfChild(node_) : -1;

  // Create the new groups and update the titles.
  BookmarkNode* new_parent = NULL;
  ApplyNameChangesAndCreateNewGroups(
      bb_model_->root_node(), tree_model_->GetRoot(), parent, &new_parent);

  if (!new_parent) {
    // Bookmarks must be parented.
    NOTREACHED();
    return;
  }

  if (node_) {
    Time date_added = node_->date_added();
    if (new_parent == node_->GetParent()) {
      // The parent is the same.
      if (new_url != node_->GetURL()) {
        bb_model_->Remove(old_parent, old_index);
        BookmarkNode* new_node =
            bb_model_->AddURL(old_parent, old_index, new_title, new_url);
        new_node->date_added_ = date_added;
      } else {
        bb_model_->SetTitle(node_, new_title);
      }
    } else if (new_url != node_->GetURL()) {
      // The parent and URL changed.
      bb_model_->Remove(old_parent, old_index);
      BookmarkNode* new_node =
          bb_model_->AddURL(new_parent, new_parent->GetChildCount(), new_title,
                            new_url);
      new_node->date_added_ = date_added;
    } else {
      // The parent and title changed. Move the node and change the title.
      bb_model_->Move(node_, new_parent, new_parent->GetChildCount());
      bb_model_->SetTitle(node_, new_title);
    }
  } else {
    // We're adding a new URL.
    bb_model_->AddURL(new_parent, new_parent->GetChildCount(), new_title,
                      new_url);
  }
}

void BookmarkEditorView::ApplyNameChangesAndCreateNewGroups(
    BookmarkNode* bb_node,
    BookmarkEditorView::EditorNode* b_node,
    BookmarkEditorView::EditorNode* parent_b_node,
    BookmarkNode** parent_bb_node) {
  if (parent_b_node == b_node)
    *parent_bb_node = bb_node;
  for (int i = 0; i < b_node->GetChildCount(); ++i) {
    EditorNode* child_b_node = b_node->GetChild(i);
    BookmarkNode* child_bb_node = NULL;
    if (child_b_node->value == 0) {
      // New group.
      child_bb_node = bb_model_->AddGroup(bb_node,
          bb_node->GetChildCount(), child_b_node->GetTitle());
    } else {
      // Existing node, reset the title (BBModel ignores changes if the title
      // is the same).
      for (int j = 0; j < bb_node->GetChildCount(); ++j) {
        BookmarkNode* node = bb_node->GetChild(j);
        if (node->is_folder() && node->id() == child_b_node->value) {
          child_bb_node = node;
          break;
        }
      }
      DCHECK(child_bb_node);
      bb_model_->SetTitle(child_bb_node, child_b_node->GetTitle());
    }
    ApplyNameChangesAndCreateNewGroups(child_bb_node, child_b_node,
                                       parent_b_node, parent_bb_node);
  }
}

