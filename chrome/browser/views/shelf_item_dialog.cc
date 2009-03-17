// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/shelf_item_dialog.h"

#include "base/gfx/png_decoder.h"
#include "base/string_util.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/gfx/text_elider.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/stl_util-inl.h"
#include "chrome/views/background.h"
#include "chrome/views/controls/label.h"
#include "chrome/views/controls/text_field.h"
#include "chrome/views/focus/focus_manager.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/widget/widget.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"

using base::Time;
using base::TimeDelta;
using views::ColumnSet;
using views::GridLayout;

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

class PossibleURLModel : public views::TableModel {
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
    results_.resize(result->size());
    std::wstring languages = profile_
        ? profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)
        : std::wstring();
    for (size_t i = 0; i < result->size(); ++i) {
      results_[i].url = (*result)[i].url();
      results_[i].index = i;
      results_[i].display_url =
          gfx::SortedDisplayURL((*result)[i].url(), languages);
      results_[i].title = (*result)[i].title();
    }

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
    return results_[row].url;
  }

  const std::wstring& GetTitle(int row) {
    if (row < 0 || row >= RowCount()) {
      NOTREACHED();
      return EmptyWString();
    }
    return results_[row].title;
  }

  virtual std::wstring GetText(int row, int col_id) {
    if (row < 0 || row >= RowCount()) {
      NOTREACHED();
      return std::wstring();
    }

    if (col_id == IDS_ASI_PAGE_COLUMN) {
      const std::wstring& title = GetTitle(row);
      // TODO(xji): Consider adding a special case if the title text is a URL,
      // since those should always have LTR directionality. Please refer to
      // http://crbug.com/6726 for more information.
      std::wstring localized_title;
      if (l10n_util::AdjustStringForLocaleDirection(title, &localized_title))
        return localized_title;
      return title;
    }

    // TODO(brettw): this should probably pass the GURL up so the URL elider
    // can be used at a higher level when we know the width.
    const std::wstring& url = results_[row].display_url.display_url();
    if (l10n_util::GetTextDirection() == l10n_util::LEFT_TO_RIGHT)
      return url;
    // Force URL to be LTR.
    std::wstring localized_url = url;
    l10n_util::WrapStringWithLTRFormatting(&localized_url);
    return localized_url;
  }

  virtual SkBitmap GetIcon(int row) {
    if (row < 0 || row >= RowCount()) {
      NOTREACHED();
      return *default_fav_icon;
    }

    Result& result = results_[row];
    FavIconMap::iterator i = fav_icon_map_.find(result.index);
    if (i != fav_icon_map_.end()) {
      // We already requested the favicon, return it.
      if (!i->second.isNull())
        return i->second;
    } else if (profile_) {
      HistoryService* hs =
          profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
      if (hs) {
        CancelableRequestProvider::Handle h =
            hs->GetFavIconForURL(
                result.url, &consumer_,
                NewCallback(this, &PossibleURLModel::OnFavIconAvailable));
        consumer_.SetClientData(hs, h, result.index);
        // Add an entry to the map so that we don't attempt to request the
        // favicon again.
        fav_icon_map_[result.index] = SkBitmap();
      }
    }
    return *default_fav_icon;
  }

  virtual int CompareValues(int row1, int row2, int column_id) {
    if (column_id == IDS_ASI_URL_COLUMN) {
      return results_[row1].display_url.Compare(
          results_[row2].display_url, GetCollator());
    }
    return TableModel::CompareValues(row1, row2, column_id);
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
      size_t index = consumer_.GetClientData(hs, h);
      if (fav_icon_available) {
        // The decoder will leave our bitmap empty on error.
        PNGDecoder::Decode(&data->data, &(fav_icon_map_[index]));

        // Notify the observer.
        if (!fav_icon_map_[index].isNull() && observer_)
          observer_->OnItemsChanged(static_cast<int>(index), 1);
      }
    }
  }

  virtual void SetObserver(views::TableModelObserver* observer) {
    observer_ = observer;
  }

 private:
  // Contains the data needed to show a result.
  struct Result {
    Result() : index(0) {}

    GURL url;
    // Index of this Result in results_. This is used as the key into
    // fav_icon_map_ to lookup the favicon for the url, as well as the index
    // into results_ when the favicon is received.
    size_t index;
    gfx::SortedDisplayURL display_url;
    std::wstring title;
  };

  // The current profile.
  Profile* profile_;

  // Our observer.
  views::TableModelObserver* observer_;

  // Our consumer for favicon requests.
  CancelableRequestConsumerT<size_t, NULL> consumer_;

  // The results we're showing.
  std::vector<Result> results_;

  // Map Result::index -> Favicon.
  typedef std::map<size_t, SkBitmap> FavIconMap;
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

  views::TableColumn col1(IDS_ASI_PAGE_COLUMN, views::TableColumn::LEFT, -1,
                          50);
  col1.sortable = true;
  views::TableColumn col2(IDS_ASI_URL_COLUMN, views::TableColumn::LEFT, -1,
                          50);
  col2.sortable = true;
  std::vector<views::TableColumn> cols;
  cols.push_back(col1);
  cols.push_back(col2);

  url_table_ = new views::TableView(url_table_model_.get(), cols,
                                    views::ICON_AND_TEXT, true, true,
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
    views::Label* title_label = new views::Label();
    title_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    title_label->SetText(l10n_util::GetString(IDS_ASI_TITLE_LABEL));
    layout->AddView(title_label);

    title_field_ = new views::TextField();
    title_field_->SetController(this);
    layout->AddView(title_field_);

    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  } else {
    title_field_ = NULL;
  }

  layout->StartRow(0, labels_column_set_id);
  views::Label* url_label = new views::Label();
  url_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  url_label->SetText(l10n_util::GetString(IDS_ASI_URL));
  layout->AddView(url_label);

  url_field_ = new views::TextField();
  url_field_->SetController(this);
  layout->AddView(url_field_);

  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  views::Label* description_label = new views::Label();
  description_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  description_label->SetText(l10n_util::GetString(IDS_ASI_DESCRIPTION));
  description_label->SetFont(
      description_label->GetFont().DeriveFont(0, ChromeFont::BOLD));
  layout->AddView(description_label);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(1, single_column_view_set_id);
  layout->AddView(url_table_);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  AddAccelerator(views::Accelerator(VK_ESCAPE, false, false, false));
  AddAccelerator(views::Accelerator(VK_RETURN, false, false, false));
}

ShelfItemDialog::~ShelfItemDialog() {
  url_table_->SetModel(NULL);
}

void ShelfItemDialog::Show(HWND parent) {
  DCHECK(!window());
  views::Window::CreateChromeWindow(parent, gfx::Rect(), this)->Show();
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

void ShelfItemDialog::ContentsChanged(views::TextField* sender,
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

views::View* ShelfItemDialog::GetContentsView() {
  return this;
}

void ShelfItemDialog::PerformModelChange() {
  DCHECK(delegate_);
  GURL url(GetInputURL());
  const std::wstring title =
      title_field_ ? title_field_->GetText() : std::wstring();
  delegate_->AddBookmark(this, title, url);
}

gfx::Size ShelfItemDialog::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_SHELFITEM_DIALOG_WIDTH_CHARS,
      IDS_SHELFITEM_DIALOG_HEIGHT_LINES));
}

bool ShelfItemDialog::AcceleratorPressed(
    const views::Accelerator& accelerator) {
  if (accelerator.GetKeyCode() == VK_ESCAPE) {
    window()->Close();
  } else if (accelerator.GetKeyCode() == VK_RETURN) {
    views::FocusManager* fm = views::FocusManager::GetFocusManager(
        GetWidget()->GetNativeView());
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
