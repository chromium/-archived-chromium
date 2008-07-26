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

#include "chrome/browser/history_tab_ui.h"

#include "base/string_util.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/history_model.h"
#include "chrome/browser/history_view.h"
#include "chrome/browser/user_metrics.h"
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

ChromeViews::View* HistoryTabUI::GetView() {
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
