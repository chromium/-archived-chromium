// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_tab_ui.h"

#include "base/string_util.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/history_model.h"
#include "chrome/browser/history_view.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/checkbox.h"
#include "net/base/escape.h"

#include "generated_resources.h"

// State key used to identify search text.
static const wchar_t kSearchTextKey[] = L"st";

// State key used for whether the search is for starred pages only.
static const wchar_t kStarredOnlyKey[] = L"starred_only";

// HistoryTabUIFactory ------------------------------------------------------

class HistoryTabUIFactory : public NativeUIFactory {
 public:
  HistoryTabUIFactory() {}
  virtual ~HistoryTabUIFactory() {}

  virtual NativeUI* CreateNativeUIForURL(const GURL& url,
                                         NativeUIContents* contents) {
    HistoryTabUI* tab_ui = new HistoryTabUI(contents);
    tab_ui->Init();
    return tab_ui;
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(HistoryTabUIFactory);
};

// HistoryTabUI -------------------------------------------------------------

HistoryTabUI::HistoryTabUI(NativeUIContents* contents)
#pragma warning(suppress: 4355)  // Okay to pass "this" here.
    : searchable_container_(this),
      contents_(contents) {
}

void HistoryTabUI::Init() {
  model_ = CreateModel();
  searchable_container_.SetContents(CreateHistoryView());
}

const std::wstring HistoryTabUI::GetTitle() const {
  return l10n_util::GetString(IDS_HISTORY_TITLE);
}

const int HistoryTabUI::GetFavIconID() const {
  return IDR_HISTORY_FAVICON;
}

const int HistoryTabUI::GetSectionIconID() const {
  return IDR_HISTORY_SECTION;
}

const std::wstring HistoryTabUI::GetSearchButtonText() const {
  return l10n_util::GetString(IDS_HISTORY_SEARCH_BUTTON);
}

views::View* HistoryTabUI::GetView() {
  return &searchable_container_;
}

void HistoryTabUI::WillBecomeVisible(NativeUIContents* parent) {
  UserMetrics::RecordAction(L"Destination_History", parent->profile());
}

void HistoryTabUI::WillBecomeInvisible(NativeUIContents* parent) {
}

void HistoryTabUI::Navigate(const PageState& state) {
  std::wstring search_text;
  state.GetProperty(kSearchTextKey, &search_text);
  // Make sure a query starts on navigation, that way if history has changed
  // since we last issued the query we'll show the right thing.
  if (model_->GetSearchText() == search_text)
    model_->Refresh();
  else
    model_->SetSearchText(search_text);
  searchable_container_.GetSearchField()->SetText(search_text);

  ChangedModel();
}

bool HistoryTabUI::SetInitialFocus() {
  searchable_container_.GetSearchField()->RequestFocus();
  return true;
}

// static
GURL HistoryTabUI::GetURL() {
  std::string spec(NativeUIContents::GetScheme());
  spec.append("://history");
  return GURL(spec);
}

// static
NativeUIFactory* HistoryTabUI::GetNativeUIFactory() {
  return new HistoryTabUIFactory();
}

// static
const GURL HistoryTabUI::GetHistoryURLWithSearchText(
    const std::wstring& text) {
  return GURL(GetURL().spec() + "/params?" + WideToUTF8(kSearchTextKey) + "=" +
              EscapeQueryParamValue(WideToUTF8(text)));
}

BaseHistoryModel* HistoryTabUI::CreateModel() {
  return new HistoryModel(contents_->profile(), std::wstring());
}

HistoryView* HistoryTabUI::CreateHistoryView() {
  return new HistoryView(&searchable_container_, model_, contents_);
}

void HistoryTabUI::ChangedModel() {
  HistoryView* history_view =
      static_cast<HistoryView*>(searchable_container_.GetContents());
  history_view->SetShowDeleteControls(model_->GetSearchText().empty());
  if (!model_->GetSearchText().empty())
    UserMetrics::RecordAction(L"History_Search", contents_->profile());
}

void HistoryTabUI::DoSearch(const std::wstring& text) {
  if (model_->GetSearchText() == text)
    return;

  model_->SetSearchText(text);

  // Update the page state.
  PageState* page_state = contents_->page_state().Copy();
  page_state->SetProperty(kSearchTextKey, text);
  contents_->SetPageState(page_state);

  ChangedModel();
}

