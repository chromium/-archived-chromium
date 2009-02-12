// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/bookmark_bubble_view.h"

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/bookmark_editor_view.h"
#include "chrome/browser/views/info_bubble.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/button.h"
#include "chrome/views/checkbox.h"
#include "chrome/views/native_button.h"
#include "chrome/views/text_field.h"
#include "generated_resources.h"

using views::ComboBox;
using views::ColumnSet;
using views::GridLayout;
using views::Label;
using views::Link;
using views::NativeButton;
using views::View;

// Color of the title.
static const SkColor kTitleColor = SkColorSetRGB(6, 45, 117);

// Padding between "Title:" and the actual title.
static const int kTitlePadding = 4;

// Minimum width for the fields - they will push out the size of the bubble if
// necessary. This should be big enough so that the field pushes the right side
// of the bubble far enough so that the edit button's left edge is to the right
// of the field's left edge.
static const int kMinimumFieldSize = 180;

// Max number of most recently used folders.
static const size_t kMaxMRUFolders = 5;

// Bubble close image.
static SkBitmap* kCloseImage = NULL;

// RecentlyUsedFoldersModel ---------------------------------------------------

BookmarkBubbleView::RecentlyUsedFoldersModel::RecentlyUsedFoldersModel(
    BookmarkModel* bb_model, BookmarkNode* node)
      // Use + 2 to account for bookmark bar and other node.
      : nodes_(bookmark_utils::GetMostRecentlyModifiedGroups(
            bb_model, kMaxMRUFolders + 2)),
      node_parent_index_(0) {
  // TODO(sky): bug 1173415 add a separator in the combobox here.

  // We special case the placement of these, so remove them from the list, then
  // fix up the order.
  RemoveNode(bb_model->GetBookmarkBarNode());
  RemoveNode(bb_model->other_node());
  RemoveNode(node->GetParent());

  // Make the parent the first item, unless it's the bookmark bar or other node.
  if (node->GetParent() != bb_model->GetBookmarkBarNode() &&
      node->GetParent() != bb_model->other_node()) {
    nodes_.insert(nodes_.begin(), node->GetParent());
  }

  // Make sure we only have kMaxMRUFolders in the first chunk.
  if (nodes_.size() > kMaxMRUFolders)
    nodes_.erase(nodes_.begin() + kMaxMRUFolders, nodes_.end());

  // And put the bookmark bar and other nodes at the end of the list.
  nodes_.push_back(bb_model->GetBookmarkBarNode());
  nodes_.push_back(bb_model->other_node());

  node_parent_index_ = static_cast<int>(
      find(nodes_.begin(), nodes_.end(), node->GetParent()) - nodes_.begin());
}

int BookmarkBubbleView::RecentlyUsedFoldersModel::GetItemCount(
    ComboBox* source) {
  return static_cast<int>(nodes_.size() + 1);
}

std::wstring BookmarkBubbleView::RecentlyUsedFoldersModel::GetItemAt(
    ComboBox* source, int index) {
  if (index == nodes_.size())
    return l10n_util::GetString(IDS_BOOMARK_BUBBLE_CHOOSER_ANOTHER_FOLDER);
  return nodes_[index]->GetTitle();
}

BookmarkNode* BookmarkBubbleView::RecentlyUsedFoldersModel::GetNodeAt(
    int index) {
  return nodes_[index];
}

void BookmarkBubbleView::RecentlyUsedFoldersModel::RemoveNode(
    BookmarkNode* node) {
  std::vector<BookmarkNode*>::iterator i =
      find(nodes_.begin(), nodes_.end(), node);
  if (i != nodes_.end())
    nodes_.erase(i);
}

// BookmarkBubbleView ---------------------------------------------------------

// static
void BookmarkBubbleView::Show(HWND parent,
                              const gfx::Rect& bounds,
                              InfoBubbleDelegate* delegate,
                              Profile* profile,
                              const GURL& url,
                              bool newly_bookmarked) {
  BookmarkBubbleView* view = new BookmarkBubbleView(delegate, profile, url,
                                                    newly_bookmarked);
  InfoBubble::Show(parent, bounds, view, view);
  GURL url_ptr(url);
  NotificationService::current()->Notify(
      NotificationType::BOOKMARK_BUBBLE_SHOWN,
      Source<Profile>(profile->GetOriginalProfile()),
      Details<GURL>(&url_ptr));
  view->BubbleShown();
}

BookmarkBubbleView::~BookmarkBubbleView() {
  if (apply_edits_) {
    ApplyEdits();
  } else if (remove_bookmark_) {
    BookmarkModel* model = profile_->GetBookmarkModel();
    BookmarkNode* node = model->GetMostRecentlyAddedNodeForURL(url_);
    if (node)
      model->Remove(node->GetParent(), node->GetParent()->IndexOfChild(node));
  }
}

void BookmarkBubbleView::DidChangeBounds(const gfx::Rect& previous,
                                         const gfx::Rect& current) {
  Layout();
}

void BookmarkBubbleView::BubbleShown() {
  DCHECK(GetWidget());
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManager(GetWidget()->GetHWND());
  focus_manager->RegisterAccelerator(
      views::Accelerator(VK_RETURN, false, false, false), this);

  title_tf_->RequestFocus();
  title_tf_->SelectAll();
}

bool BookmarkBubbleView::AcceleratorPressed(
    const views::Accelerator& accelerator) {
  if (accelerator.GetKeyCode() != VK_RETURN)
    return false;
  if (edit_button_->HasFocus())
    ButtonPressed(edit_button_);
  else
    ButtonPressed(close_button_);
  return true;
}

BookmarkBubbleView::BookmarkBubbleView(InfoBubbleDelegate* delegate,
                                       Profile* profile,
                                       const GURL& url,
                                       bool newly_bookmarked)
    : delegate_(delegate),
      profile_(profile),
      url_(url),
      newly_bookmarked_(newly_bookmarked),
      parent_model_(
          profile_->GetBookmarkModel(),
          profile_->GetBookmarkModel()->GetMostRecentlyAddedNodeForURL(url)),
      remove_bookmark_(false),
      apply_edits_(true) {
  Init();
}

void BookmarkBubbleView::Init() {
  if (!kCloseImage) {
    kCloseImage = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFO_BUBBLE_CLOSE);
  }

  remove_link_ = new Link(l10n_util::GetString(
      IDS_BOOMARK_BUBBLE_REMOVE_BOOKMARK));
  remove_link_->SetController(this);

  edit_button_ = new NativeButton(
      l10n_util::GetString(IDS_BOOMARK_BUBBLE_OPTIONS));
  edit_button_->SetListener(this);

  close_button_ = new NativeButton(l10n_util::GetString(IDS_CLOSE), true);
  close_button_->SetListener(this);

  parent_combobox_ = new ComboBox(&parent_model_);
  parent_combobox_->SetSelectedItem(parent_model_.node_parent_index());
  parent_combobox_->SetListener(this);

  Label* title_label = new Label(l10n_util::GetString(
      newly_bookmarked_ ? IDS_BOOMARK_BUBBLE_PAGE_BOOKMARKED :
                          IDS_BOOMARK_BUBBLE_PAGE_BOOKMARK));
  title_label->SetFont(
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont));
  title_label->SetColor(kTitleColor);

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  ColumnSet* cs = layout->AddColumnSet(0);

  // Top (title) row.
  cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0, GridLayout::USE_PREF,
                0, 0);
  cs->AddPaddingColumn(1, kUnrelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0, GridLayout::USE_PREF,		
                0, 0);

  // Middle (input field) rows.
  cs = layout->AddColumnSet(2);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                GridLayout::USE_PREF, 0, kMinimumFieldSize);

  // Bottom (buttons) row.
  cs = layout->AddColumnSet(3);
  cs->AddPaddingColumn(1, kRelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::TRAILING, 0,
                GridLayout::USE_PREF, 0, 0);
  // We subtract 2 to account for the natural button padding, and
  // to bring the separation visually in line with the row separation
  // height.
  cs->AddPaddingColumn(0, kRelatedButtonHSpacing - 2);
  cs->AddColumn(GridLayout::LEADING, GridLayout::TRAILING, 0,
                GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(title_label);
  layout->AddView(remove_link_);

  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, 2);
  layout->AddView(
      new Label(l10n_util::GetString(IDS_BOOMARK_BUBBLE_TITLE_TEXT)));
  title_tf_ = new views::TextField();
  title_tf_->SetText(GetTitle());
  layout->AddView(title_tf_);

  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);

  layout->StartRow(0, 2);
  layout->AddView(
      new Label(l10n_util::GetString(IDS_BOOMARK_BUBBLE_FOLDER_TEXT)));
  layout->AddView(parent_combobox_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);

  layout->StartRow(0, 3);
  layout->AddView(edit_button_);
  layout->AddView(close_button_);
}

std::wstring BookmarkBubbleView::GetTitle() {
  BookmarkModel* bookmark_model= profile_->GetBookmarkModel();
  BookmarkNode* node = bookmark_model->GetMostRecentlyAddedNodeForURL(url_);
  if (node)
    return node->GetTitle();
  else
    NOTREACHED();
  return std::wstring();
}

void BookmarkBubbleView::ButtonPressed(views::NativeButton* sender) {		
  if (sender == edit_button_) {
    UserMetrics::RecordAction(L"BookmarkBubble_Edit", profile_);
    ShowEditor();
  } else {
    DCHECK(sender == close_button_);
    Close();
  }
  // WARNING: we've most likely been deleted when CloseWindow returns.		
}

void BookmarkBubbleView::LinkActivated(Link* source, int event_flags) {
  DCHECK(source == remove_link_);
  UserMetrics::RecordAction(L"BookmarkBubble_Unstar", profile_);

  // Set this so we remove the bookmark after the window closes.
  remove_bookmark_ = true;
  apply_edits_ = false;

  Close();
}

void BookmarkBubbleView::ItemChanged(ComboBox* combo_box,
                                     int prev_index,
                                     int new_index) {
  if (new_index + 1 == parent_model_.GetItemCount(parent_combobox_)) {
    UserMetrics::RecordAction(L"BookmarkBubble_EditFromCombobox", profile_);

    ShowEditor();
    return;
  }
}

void BookmarkBubbleView::InfoBubbleClosing(InfoBubble* info_bubble,
                                           bool closed_by_escape) {
  if (closed_by_escape) {
    remove_bookmark_ = newly_bookmarked_;
    apply_edits_ = false;
  }

  if (delegate_)
    delegate_->InfoBubbleClosing(info_bubble, closed_by_escape);
  NotificationService::current()->Notify(
      NotificationType::BOOKMARK_BUBBLE_HIDDEN,
      Source<Profile>(profile_->GetOriginalProfile()),
      NotificationService::NoDetails());
}

bool BookmarkBubbleView::CloseOnEscape() {
  return delegate_ ? delegate_->CloseOnEscape() : true;
}

void BookmarkBubbleView::Close() {
  static_cast<InfoBubble*>(GetWidget())->Close();
}

void BookmarkBubbleView::ShowEditor() {
  BookmarkNode* node =
      profile_->GetBookmarkModel()->GetMostRecentlyAddedNodeForURL(url_);

  // Commit any edits now.
  ApplyEdits();

  // Parent the editor to our root ancestor (not the root we're in, as that
  // is the info bubble and will close shortly).
  HWND parent = GetAncestor(GetWidget()->GetHWND(), GA_ROOTOWNER);

  // We're about to show the bookmark editor. When the bookmark editor closes
  // we want the browser to become active. WidgetWin::Hide() does a hide in
  // a such way that activation isn't changed, which means when we close
  // Windows gets confused as to who it should give active status to. We
  // explicitly hide the bookmark bubble window in such a way that activation
  // status changes. That way, when the editor closes, activation is properly
  // restored to the browser.
  ShowWindow(GetWidget()->GetHWND(), SW_HIDE);

  // Even though we just hid the window, we need to invoke Close to schedule
  // the delete and all that.
  Close();

  if (node)
    BookmarkEditorView::Show(parent, profile_, NULL, node,
                             BookmarkEditorView::SHOW_TREE, NULL);
}

void BookmarkBubbleView::ApplyEdits() {
  // Set this to make sure we don't attempt to apply edits again.
  apply_edits_ = false;

  BookmarkModel* model = profile_->GetBookmarkModel();
  BookmarkNode* node = model->GetMostRecentlyAddedNodeForURL(url_);
  if (node) {
    const std::wstring new_title = title_tf_->GetText();
    if (new_title != node->GetTitle()) {
      model->SetTitle(node, new_title);
      UserMetrics::RecordAction(L"BookmarkBubble_ChangeTitleInBubble",
                                profile_);
    }
    // Last index means 'Choose another folder...'
    if (parent_combobox_->GetSelectedItem() <
        parent_model_.GetItemCount(parent_combobox_) - 1) {
      BookmarkNode* new_parent =
          parent_model_.GetNodeAt(parent_combobox_->GetSelectedItem());
      if (new_parent != node->GetParent()) {
        UserMetrics::RecordAction(L"BookmarkBubble_ChangeParent", profile_);
        model->Move(node, new_parent, new_parent->GetChildCount());
      }
    }
  }
}
