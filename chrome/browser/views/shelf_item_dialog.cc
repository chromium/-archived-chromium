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

#include "chrome/browser/views/shelf_item_dialog.h"

#include "base/gfx/png_decoder.h"
#include "base/string_util.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/standard_layout.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/url_fixer_upper.h"
#include "chrome/common/gfx/url_elider.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/stl_util-inl.h"
#include "chrome/views/background.h"
#include "chrome/views/focus_manager.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/label.h"
#include "chrome/views/text_field.h"
#include "generated_resources.h"
#include "net/base/net_util.h"

using ChromeViews::ColumnSet;
using ChromeViews::GridLayout;

// Preferred height of the table.
static const int kTableWidth = 300;

// The default favicon.
static SkBitmap* default_fav_icon = NULL;

////////////////////////////////////////////////////////////////////////////////
//
// A table model to represent the list of URLS that we the user might want to
// bookmark.
//
////////////////////////////////////////////////////////////////////////////////

// How long we query entry points for.
static const int kPossibleURLTimeScope = 30;

class PossibleURLModel : public ChromeViews::TableModel {
 public:
  PossibleURLModel() : profile_(NULL) {
    if (!default_fav_icon) {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      default_fav_icon = rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
    }
  }

  virtual ~PossibleURLModel() {
  }

  void Reload(Profile *profile) {
    profile_ = profile;
    consumer_.CancelAllRequests();
    HistoryService* hs =
        profile->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (hs) {
      history::QueryOptions options;
      options.end_time = Time::Now();
      options.begin_time =
          options.end_time - TimeDelta::FromDays(kPossibleURLTimeScope);
      options.most_recent_visit_only = true;
      options.max_count = 50;

      hs->QueryHistory(std::wstring(), options, &consumer_,
          NewCallback(this, &PossibleURLModel::OnHistoryQueryComplete));
    }
  }

  void OnHistoryQueryComplete(HistoryService::Handle h,
                              history::QueryResults* result) {
    results_.Swap(result);

    // The old version of this code would filter out all but the most recent
    // visit to each host, plus all typed URLs and AUTO_BOOKMARK transitions. I
    // think this dialog has a lot of work, and I'm not sure those old
    // conditions are correct (the results look about equal quality for my
    // history with and without those conditions), so I'm not spending time
    // re-implementing them here. They used to be implemented in the history
    // service, but I think they should be implemented here because that was
    // pretty specific behavior that shouldn't be generally exposed.

    fav_icon_map_.clear();
    if (observer_)
      observer_->OnModelChanged();
  }

  virtual int RowCount() {
    return static_cast<int>(results_.size());
  }

  const GURL& GetURL(int row) {
    if (row < 0 || row >= RowCount()) {
      NOTREACHED();
      return GURL::EmptyGURL();
    }
    return results_[row].url();
  }

  const std::wstring& GetTitle(int row) {
    if (row < 0 || row >= RowCount()) {
      NOTREACHED();
      return EmptyWString();
    }
    return results_[row].title();
  }

  virtual std::wstring GetText(int row, int col_id) {
    if (row < 0 || row >= RowCount()) {
      NOTREACHED();
      return std::wstring();
    }

    if (col_id == IDS_ASI_PAGE_COLUMN)
      return GetTitle(row);

    // TODO(brettw): this should probably pass the GURL up so the URL elider
    // can be used at a higher level when we know the width.
    return gfx::ElideUrl(GetURL(row), ChromeFont(), 0, profile_ ?
        profile_->GetPrefs()->GetString(prefs::kAcceptLanguages) :
        std::wstring());
  }

  virtual SkBitmap GetIcon(int row) {
    if (row < 0 || row >= RowCount()) {
      NOTREACHED();
      return *default_fav_icon;
    }

    const history::URLResult& result = results_[row];
    FavIconMap::iterator i = fav_icon_map_.find(result.id());
    if (i != fav_icon_map_.end()) {
      if (!i->second.isNull())
        return i->second;
    } else if (profile_) {
      HistoryService* hs =
          profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
      if (hs) {
        CancelableRequestProvider::Handle h =
            hs->GetFavIconForURL(
                result.url(), &consumer_,
                NewCallback(this, &PossibleURLModel::OnFavIconAvailable));
        consumer_.SetClientData(hs, h, result.id());
      }
    }
    return *default_fav_icon;
  }

  virtual void OnFavIconAvailable(
      HistoryService::Handle h,
      bool fav_icon_available,
      scoped_refptr<RefCountedBytes> data,
      bool expired,
      GURL icon_url) {
    if (profile_) {
      HistoryService* hs =
          profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
      history::URLID pid = consumer_.GetClientData(hs, h);
      if (pid) {
        SkBitmap bm;
        if (fav_icon_available) {
          // The decoder will leave our bitmap empty on error.
          PNGDecoder::Decode(&data->data, &bm);
        }

        // Store the bitmap. We store it even if it is empty to make sure we
        // don't query it again.
        fav_icon_map_[pid] = bm;
        if (!bm.isNull() && observer_) {
          for (size_t i = 0; i < results_.size(); ++i) {
            if (results_[i].id() == pid)
              observer_->OnItemsChanged(static_cast<int>(i), 1);
          }
        }
      }
    }
  }

  virtual void SetObserver(ChromeViews::TableModelObserver* observer) {
    observer_ = observer;
  }

 private:
  // The current profile.
  Profile* profile_;

  // Our observer.
  ChromeViews::TableModelObserver* observer_;

  // Our consumer for favicon requests.
  CancelableRequestConsumerT<history::URLID, NULL> consumer_;

  // The results provided by the history service.
  history::QueryResults results_;

  // Map URLID -> Favicon. If we queried for a favicon and there is none, that
  // URL will have an entry, and the bitmap will be empty.
  typedef std::map<history::URLID, SkBitmap> FavIconMap;
  FavIconMap fav_icon_map_;

  DISALLOW_EVIL_CONSTRUCTORS(PossibleURLModel);
};

////////////////////////////////////////////////////////////////////////////////
//
// ShelfItemDialog implementation
//
////////////////////////////////////////////////////////////////////////////////
ShelfItemDialog::ShelfItemDialog(ShelfItemDialogDelegate* delegate,
                                 Profile* profile,
                                 bool show_title)
    : profile_(profile),
      expected_title_handle_(0),
      delegate_(delegate) {
  DCHECK(profile_);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  url_table_model_.reset(new PossibleURLModel());

  ChromeViews::TableColumn col1(IDS_ASI_PAGE_COLUMN,
                                ChromeViews::TableColumn::LEFT, -1,
                                50);
  ChromeViews::TableColumn col2(IDS_ASI_URL_COLUMN,
                                ChromeViews::TableColumn::LEFT, -1,
                                50);
  std::vector<ChromeViews::TableColumn> cols;
  cols.push_back(col1);
  cols.push_back(col2);

  url_table_ = new ChromeViews::TableView(url_table_model_.get(),
                                          cols,
                                          ChromeViews::ICON_AND_TEXT,
                                          true,
                                          true,
                                          true);
  url_table_->SetObserver(this);

  // Yummy layout code.
  const int labels_column_set_id = 0;
  const int single_column_view_set_id = 1;
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
                        GridLayout::FIXED, kTableWidth, 0);

  if (show_title) {
    layout->StartRow(0, labels_column_set_id);
    ChromeViews::Label* title_label = new ChromeViews::Label();
    title_label->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
    title_label->SetText(l10n_util::GetString(IDS_ASI_TITLE_LABEL));
    layout->AddView(title_label);

    title_field_ = new ChromeViews::TextField();
    title_field_->SetController(this);
    layout->AddView(title_field_);

    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  } else {
    title_field_ = NULL;
  }

  layout->StartRow(0, labels_column_set_id);
  ChromeViews::Label* url_label = new ChromeViews::Label();
  url_label->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
  url_label->SetText(l10n_util::GetString(IDS_ASI_URL));
  layout->AddView(url_label);

  url_field_ = new ChromeViews::TextField();
  url_field_->SetController(this);
  layout->AddView(url_field_);

  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  ChromeViews::Label* description_label = new ChromeViews::Label();
  description_label->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
  description_label->SetText(l10n_util::GetString(IDS_ASI_DESCRIPTION));
  description_label->SetFont(
      description_label->GetFont().DeriveFont(0, ChromeFont::BOLD));
  layout->AddView(description_label);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(1, single_column_view_set_id);
  layout->AddView(url_table_);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  AddAccelerator(ChromeViews::Accelerator(VK_ESCAPE, false, false, false));
  AddAccelerator(ChromeViews::Accelerator(VK_RETURN, false, false, false));
}

ShelfItemDialog::~ShelfItemDialog() {
  url_table_->SetModel(NULL);
}

void ShelfItemDialog::Show(HWND parent) {
  DCHECK(!window());
  ChromeViews::Window::CreateChromeWindow(parent, gfx::Rect(), this)->Show();
  if (title_field_) {
    title_field_->SetText(l10n_util::GetString(IDS_ASI_DEFAULT_TITLE));
    title_field_->SelectAll();
    title_field_->RequestFocus();
  } else {
    url_field_->SelectAll();
    url_field_->RequestFocus();
  }
  url_table_model_->Reload(profile_);
}

void ShelfItemDialog::Close() {
  DCHECK(window());
  window()->Close();
}

std::wstring ShelfItemDialog::GetWindowTitle() const {
  return l10n_util::GetString(IDS_ASI_ADD_TITLE);
}

bool ShelfItemDialog::IsModal() const {
  return true;
}

void ShelfItemDialog::WindowClosing() {
}

std::wstring ShelfItemDialog::GetDialogButtonLabel(DialogButton button) const {
  if (button == DialogDelegate::DIALOGBUTTON_OK)
    return l10n_util::GetString(IDS_ASI_ADD);
  return std::wstring();
}

void ShelfItemDialog::OnURLInfoAvailable(
    HistoryService::Handle handle,
    bool success,
    const history::URLRow* info,
    history::VisitVector* unused) {
  if (handle != expected_title_handle_)
    return;

  std::wstring s;
  if (success)
    s = info->title();
  if (s.empty())
    s = l10n_util::GetString(IDS_ASI_DEFAULT_TITLE);

  if (title_field_) {
    // expected_title_handle_ is reset if the title textfield is edited so we
    // can safely set the value.
    title_field_->SetText(s);
    title_field_->SelectAll();
  }
  expected_title_handle_ = 0;
}

void ShelfItemDialog::InitiateTitleAutoFill(const GURL& url) {
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!hs)
    return;

  if (expected_title_handle_)
    hs->CancelRequest(expected_title_handle_);

  expected_title_handle_ = hs->QueryURL(url, false, &history_consumer_,
      NewCallback(this, &ShelfItemDialog::OnURLInfoAvailable));
}

void ShelfItemDialog::ContentsChanged(ChromeViews::TextField* sender,
                                      const std::wstring& new_contents) {
  // If the user has edited the title field we no longer want to autofill it
  // so we reset the expected handle to an impossible value.
  if (sender == title_field_)
    expected_title_handle_ = 0;
  GetDialogClientView()->UpdateDialogButtons();
}

bool ShelfItemDialog::Accept() {
  if (!IsDialogButtonEnabled(DIALOGBUTTON_OK)) {
    if (!GetInputURL().is_valid())
      url_field_->RequestFocus();
    else if (title_field_)
      title_field_->RequestFocus();
    return false;
  }
  PerformModelChange();
  return true;
}

bool ShelfItemDialog::IsDialogButtonEnabled(DialogButton button) const {
  if (button == DIALOGBUTTON_OK)
    return GetInputURL().is_valid();
  return true;
}

ChromeViews::View* ShelfItemDialog::GetContentsView() {
  return this;
}

void ShelfItemDialog::PerformModelChange() {
  DCHECK(delegate_);
  GURL url(GetInputURL());
  const std::wstring title =
      title_field_ ? title_field_->GetText() : std::wstring();
  delegate_->AddBookmark(this, title, url);
}

void ShelfItemDialog::GetPreferredSize(CSize *out) {
  DCHECK(out);
  *out = ChromeViews::Window::GetLocalizedContentsSize(
      IDS_SHELFITEM_DIALOG_WIDTH_CHARS,
      IDS_SHELFITEM_DIALOG_HEIGHT_LINES).ToSIZE();
}

bool ShelfItemDialog::AcceleratorPressed(
    const ChromeViews::Accelerator& accelerator) {
  if (accelerator.GetKeyCode() == VK_ESCAPE) {
    window()->Close();
  } else if (accelerator.GetKeyCode() == VK_RETURN) {
    ChromeViews::FocusManager* fm = ChromeViews::FocusManager::GetFocusManager(
        GetViewContainer()->GetHWND());
    if (fm->GetFocusedView() == url_table_) {
      // Return on table behaves like a double click.
      OnDoubleClick();
    } else if (fm->GetFocusedView()== url_field_) {
      // Return on the url field accepts the input if url is valid. If the URL
      // is invalid, focus is left on the url field.
      if (GetInputURL().is_valid()) {
        PerformModelChange();
        if (window())
          window()->Close();
      } else {
        url_field_->SelectAll();
      }
    } else if (title_field_ && fm->GetFocusedView() == title_field_) {
      url_field_->SelectAll();
      url_field_->RequestFocus();
    }
  }
  return true;
}

void ShelfItemDialog::DidChangeBounds(const CRect& previous,
                                      const CRect& current) {
  Layout();
}

void ShelfItemDialog::OnSelectionChanged() {
  int selection = url_table_->FirstSelectedRow();
  if (selection >= 0 && selection < url_table_model_->RowCount()) {
    url_field_->SetText(
        UTF8ToWide(url_table_model_->GetURL(selection).spec()));
    if (title_field_)
      title_field_->SetText(url_table_model_->GetTitle(selection));
    GetDialogClientView()->UpdateDialogButtons();
  }
}

void ShelfItemDialog::OnDoubleClick() {
  int selection = url_table_->FirstSelectedRow();
  if (selection >= 0 && selection < url_table_model_->RowCount()) {
    OnSelectionChanged();
    PerformModelChange();
    if (window())
      window()->Close();
  }
}

GURL ShelfItemDialog::GetInputURL() const {
  return GURL(URLFixerUpper::FixupURL(url_field_->GetText(), L""));
}
