// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/importing_progress_view.h"

#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/label.h"
#include "chrome/views/throbber.h"
#include "chrome/views/window.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

////////////////////////////////////////////////////////////////////////////////
// ImportingProgressView, public:

ImportingProgressView::ImportingProgressView(const std::wstring& source_name,
                                             int16 items,
                                             ImporterHost* coordinator,
                                             ImportObserver* observer,
                                             HWND parent_window,
                                             bool bookmarks_import)
    : state_bookmarks_(new views::CheckmarkThrobber),
      state_searches_(new views::CheckmarkThrobber),
      state_passwords_(new views::CheckmarkThrobber),
      state_history_(new views::CheckmarkThrobber),
      state_cookies_(new views::CheckmarkThrobber),
      label_bookmarks_(new views::Label(
          l10n_util::GetString(IDS_IMPORT_PROGRESS_STATUS_BOOKMARKS))),
      label_searches_(new views::Label(
          l10n_util::GetString(IDS_IMPORT_PROGRESS_STATUS_SEARCH))),
      label_passwords_(new views::Label(
          l10n_util::GetString(IDS_IMPORT_PROGRESS_STATUS_PASSWORDS))),
      label_history_(new views::Label(
          l10n_util::GetString(IDS_IMPORT_PROGRESS_STATUS_HISTORY))),
      label_cookies_(new views::Label(
          l10n_util::GetString(IDS_IMPORT_PROGRESS_STATUS_COOKIES))),
      parent_window_(parent_window),
      coordinator_(coordinator),
      import_observer_(observer),
      items_(items),
      importing_(true),
      bookmarks_import_(bookmarks_import) {
  std::wstring info_text = bookmarks_import ?
      l10n_util::GetString(IDS_IMPORT_BOOKMARKS) :
      l10n_util::GetStringF(IDS_IMPORT_PROGRESS_INFO, source_name);
  label_info_ = new views::Label(info_text);
  coordinator_->SetObserver(this);
  label_info_->SetMultiLine(true);
  label_info_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label_bookmarks_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label_searches_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label_passwords_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label_history_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label_cookies_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  // These are scoped pointers, so we don't need the parent to delete them.
  state_bookmarks_->SetParentOwned(false);
  state_searches_->SetParentOwned(false);
  state_passwords_->SetParentOwned(false);
  state_history_->SetParentOwned(false);
  state_cookies_->SetParentOwned(false);
  label_bookmarks_->SetParentOwned(false);
  label_searches_->SetParentOwned(false);
  label_passwords_->SetParentOwned(false);
  label_history_->SetParentOwned(false);
  label_cookies_->SetParentOwned(false);
}

ImportingProgressView::~ImportingProgressView() {
  RemoveChildView(state_bookmarks_.get());
  RemoveChildView(state_searches_.get());
  RemoveChildView(state_passwords_.get());
  RemoveChildView(state_history_.get());
  RemoveChildView(state_cookies_.get());
  RemoveChildView(label_bookmarks_.get());
  RemoveChildView(label_searches_.get());
  RemoveChildView(label_passwords_.get());
  RemoveChildView(label_history_.get());
  RemoveChildView(label_cookies_.get());

  if (importing_) {
    // We're being deleted while importing, clean up state so that the importer
    // doesn't have a reference to us and cancel the import. We can get here
    // if our parent window is closed, which closes our window and deletes us.
    importing_ = false;
    coordinator_->SetObserver(NULL);
    coordinator_->Cancel();
    if (import_observer_)
      import_observer_->ImportComplete();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ImportingProgressView, ImporterObserver implementation:

void ImportingProgressView::ImportItemStarted(ImportItem item) {
  DCHECK(items_ & item);
  switch (item) {
    case FAVORITES:
      state_bookmarks_->Start();
      break;
    case SEARCH_ENGINES:
      state_searches_->Start();
      break;
    case PASSWORDS:
      state_passwords_->Start();
      break;
    case HISTORY:
      state_history_->Start();
      break;
    case COOKIES:
      state_cookies_->Start();
      break;
  }
}

void ImportingProgressView::ImportItemEnded(ImportItem item) {
  DCHECK(items_ & item);
  switch (item) {
    case FAVORITES:
      state_bookmarks_->Stop();
      state_bookmarks_->SetChecked(true);
      break;
    case SEARCH_ENGINES:
      state_searches_->Stop();
      state_searches_->SetChecked(true);
      break;
    case PASSWORDS:
      state_passwords_->Stop();
      state_passwords_->SetChecked(true);
      break;
    case HISTORY:
      state_history_->Stop();
      state_history_->SetChecked(true);
      break;
    case COOKIES:
      state_cookies_->Stop();
      state_cookies_->SetChecked(true);
      break;
  }
}

void ImportingProgressView::ImportStarted() {
  importing_ = true;
}

void ImportingProgressView::ImportEnded() {
  // This can happen because:
  // - the import completed successfully.
  // - the import was canceled by the user.
  // - the user chose to skip the import because they didn't want to shut down
  //   Firefox.
  // In every case, we need to close the UI now.
  importing_ = false;
  coordinator_->SetObserver(NULL);
  window()->Close();
  if (import_observer_)
    import_observer_->ImportComplete();
}

////////////////////////////////////////////////////////////////////////////////
// ImportingProgressView, views::View overrides:

gfx::Size ImportingProgressView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_IMPORTPROGRESS_DIALOG_WIDTH_CHARS,
      IDS_IMPORTPROGRESS_DIALOG_HEIGHT_LINES));
}

void ImportingProgressView::ViewHierarchyChanged(bool is_add,
                                                 views::View* parent,
                                                 views::View* child) {
  if (is_add && child == this)
    InitControlLayout();
}

////////////////////////////////////////////////////////////////////////////////
// ImportingProgressView, views::DialogDelegate implementation:

int ImportingProgressView::GetDialogButtons() const {
  return DIALOGBUTTON_CANCEL;
}

std::wstring ImportingProgressView::GetDialogButtonLabel(
    DialogButton button) const {
  DCHECK(button == DIALOGBUTTON_CANCEL);
  return l10n_util::GetString(IDS_IMPORT_PROGRESS_STATUS_CANCEL);
}

bool ImportingProgressView::IsModal() const {
  return parent_window_ != NULL;
}

std::wstring ImportingProgressView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_IMPORT_PROGRESS_TITLE);
}

bool ImportingProgressView::Cancel() {
  // When the user cancels the import, we need to tell the coordinator to stop
  // importing and return false so that the window lives long enough to receive
  // ImportEnded, which will close the window. Closing the window results in
  // another call to this function and at that point we must return true to
  // allow the window to close.
  if (!importing_)
    return true;  // We have received ImportEnded, so we can close.

  // Cancel the import and wait for further instructions.
  coordinator_->Cancel();
  return false;
}

views::View* ImportingProgressView::GetContentsView() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// ImportingProgressView, private:

void ImportingProgressView::InitControlLayout() {
  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  gfx::Size ps = state_history_->GetPreferredSize();

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  if (bookmarks_import_) {
    column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                          GridLayout::FIXED, ps.width(), 0);
    column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  }
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  const int double_column_view_set_id = 1;
  column_set = layout->AddColumnSet(double_column_view_set_id);
  column_set->AddPaddingColumn(0, kUnrelatedControlLargeHorizontalSpacing);
  column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                        GridLayout::FIXED, ps.width(), 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kUnrelatedControlLargeHorizontalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  if (bookmarks_import_)
    layout->AddView(state_bookmarks_.get());
  layout->AddView(label_info_);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  if (items_ & FAVORITES && !bookmarks_import_) {
    layout->StartRow(0, double_column_view_set_id);
    layout->AddView(state_bookmarks_.get());
    layout->AddView(label_bookmarks_.get());
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }
  if (items_ & SEARCH_ENGINES) {
    layout->StartRow(0, double_column_view_set_id);
    layout->AddView(state_searches_.get());
    layout->AddView(label_searches_.get());
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }
  if (items_ & PASSWORDS) {
    layout->StartRow(0, double_column_view_set_id);
    layout->AddView(state_passwords_.get());
    layout->AddView(label_passwords_.get());
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }
  if (items_ & HISTORY) {
    layout->StartRow(0, double_column_view_set_id);
    layout->AddView(state_history_.get());
    layout->AddView(label_history_.get());
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }
  if (items_ & COOKIES) {
    layout->StartRow(0, double_column_view_set_id);
    layout->AddView(state_cookies_.get());
    layout->AddView(label_cookies_.get());
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }
}

////////////////////////////////////////////////////////////////////////////////
// StartImportingWithUI

void StartImportingWithUI(HWND parent_window,
                          int16 items,
                          ImporterHost* coordinator,
                          const ProfileInfo& source_profile,
                          Profile* target_profile,
                          ImportObserver* observer,
                          bool first_run) {
  DCHECK(items != 0);
  ImportingProgressView* v = new ImportingProgressView(
      source_profile.description, items, coordinator, observer, parent_window,
      source_profile.browser_type == BOOKMARKS_HTML);
  views::Window* window =
    views::Window::CreateChromeWindow(parent_window, gfx::Rect(), v);

  // In headless mode it means that we don't show the progress window, but it
  // still need it to exist. No user interaction will be required.
  if (!coordinator->is_headless())
    window->Show();

  coordinator->StartImportSettings(source_profile, items,
                                   new ProfileWriter(target_profile),
                                   first_run);
}
