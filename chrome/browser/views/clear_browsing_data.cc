// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/clear_browsing_data.h"

#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/background.h"
#include "chrome/views/checkbox.h"
#include "chrome/views/label.h"
#include "chrome/views/native_button.h"
#include "chrome/views/throbber.h"
#include "chrome/views/window.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "net/url_request/url_request_context.h"

#include "generated_resources.h"

using base::Time;
using base::TimeDelta;

// The combo box is vertically aligned to the 'time-period' label, which makes
// the combo box look a little too close to the check box above it when we use
// standard layout to separate them. We therefore add a little extra margin to
// the label, giving it a little breathing space.
static const int kExtraMarginForTimePeriodLabel = 3;

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView, public:

ClearBrowsingDataView::ClearBrowsingDataView(Profile* profile)
    : del_history_checkbox_(NULL),
      del_downloads_checkbox_(NULL),
      del_cache_checkbox_(NULL),
      del_cookies_checkbox_(NULL),
      del_passwords_checkbox_(NULL),
      del_form_data_checkbox_(NULL),
      time_period_label_(NULL),
      time_period_combobox_(NULL),
      delete_in_progress_(false),
      profile_(profile),
      remover_(NULL) {
  DCHECK(profile);
  Init();
}

ClearBrowsingDataView::~ClearBrowsingDataView(void) {
  if (remover_) {
    // We were destroyed while clearing history was in progress. This can only
    // occur during automated tests (normally the user can't close the dialog
    // while clearing is in progress as the dialog is modal and not closeable).
    remover_->RemoveObserver(this);
  }
}

void ClearBrowsingDataView::Init() {
  // Views we will add to the *parent* of this dialog, since it will display
  // next to the buttons which we don't draw ourselves.
  throbber_.reset(new views::Throbber(50, true));
  throbber_->SetParentOwned(false);
  throbber_->SetVisible(false);

  status_label_.SetText(l10n_util::GetString(IDS_CLEAR_DATA_DELETING));
  status_label_.SetVisible(false);
  status_label_.SetParentOwned(false);

  // Regular view controls we draw by ourself. First, we add the dialog label.
  delete_all_label_ = new views::Label(
      l10n_util::GetString(IDS_CLEAR_BROWSING_DATA_LABEL));
  AddChildView(delete_all_label_);

  // Add all the check-boxes.
  del_history_checkbox_ =
      AddCheckbox(l10n_util::GetString(IDS_DEL_BROWSING_HISTORY_CHKBOX),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteBrowsingHistory));

  del_downloads_checkbox_ =
      AddCheckbox(l10n_util::GetString(IDS_DEL_DOWNLOAD_HISTORY_CHKBOX),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteDownloadHistory));

  del_cache_checkbox_ =
      AddCheckbox(l10n_util::GetString(IDS_DEL_CACHE_CHKBOX),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteCache));

  del_cookies_checkbox_ =
      AddCheckbox(l10n_util::GetString(IDS_DEL_COOKIES_CHKBOX),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteCookies));

  del_passwords_checkbox_ =
      AddCheckbox(l10n_util::GetString(IDS_DEL_PASSWORDS_CHKBOX),
      profile_->GetPrefs()->GetBoolean(prefs::kDeletePasswords));

  del_form_data_checkbox_ =
      AddCheckbox(l10n_util::GetString(IDS_DEL_FORM_DATA_CHKBOX),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteFormData));

  // Add a label which appears before the combo box for the time period.
  time_period_label_ = new views::Label(
      l10n_util::GetString(IDS_CLEAR_BROWSING_DATA_TIME_LABEL));
  AddChildView(time_period_label_);

  // Add the combo box showing how far back in time we want to delete.
  time_period_combobox_ = new views::ComboBox(this);
  time_period_combobox_->SetSelectedItem(profile_->GetPrefs()->GetInteger(
                                         prefs::kDeleteTimePeriod));
  time_period_combobox_->SetListener(this);
  AddChildView(time_period_combobox_);
}

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView, views::View implementation:

gfx::Size ClearBrowsingDataView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_CLEARDATA_DIALOG_WIDTH_CHARS,
      IDS_CLEARDATA_DIALOG_HEIGHT_LINES));
}

void ClearBrowsingDataView::Layout() {
  gfx::Size panel_size = GetPreferredSize();

  // Delete All label goes to the top left corner.
  gfx::Size sz = delete_all_label_->GetPreferredSize();
  delete_all_label_->SetBounds(kPanelHorizMargin, kPanelVertMargin,
                               sz.width(), sz.height());

  // Check-boxes go beneath it (with a little indentation).
  sz = del_history_checkbox_->GetPreferredSize();
  del_history_checkbox_->SetBounds(2 * kPanelHorizMargin,
                                   delete_all_label_->y() +
                                       delete_all_label_->height() +
                                       kRelatedControlVerticalSpacing,
                                   sz.width(), sz.height());

  sz = del_downloads_checkbox_->GetPreferredSize();
  del_downloads_checkbox_->SetBounds(2 * kPanelHorizMargin,
                                     del_history_checkbox_->y() +
                                         del_history_checkbox_->height() +
                                         kRelatedControlVerticalSpacing,
                                     sz.width(), sz.height());

  sz = del_cache_checkbox_->GetPreferredSize();
  del_cache_checkbox_->SetBounds(2 * kPanelHorizMargin,
                                 del_downloads_checkbox_->y() +
                                     del_downloads_checkbox_->height() +
                                     kRelatedControlVerticalSpacing,
                                 sz.width(), sz.height());

  sz = del_cookies_checkbox_->GetPreferredSize();
  del_cookies_checkbox_->SetBounds(2 * kPanelHorizMargin,
                                   del_cache_checkbox_->y() +
                                       del_cache_checkbox_->height() +
                                       kRelatedControlVerticalSpacing,
                                   sz.width(), sz.height());

  sz = del_passwords_checkbox_->GetPreferredSize();
  del_passwords_checkbox_->SetBounds(2 * kPanelHorizMargin,
                                     del_cookies_checkbox_->y() +
                                         del_cookies_checkbox_->height() +
                                         kRelatedControlVerticalSpacing,
                                     sz.width(), sz.height());

  sz = del_form_data_checkbox_->GetPreferredSize();
  del_form_data_checkbox_->SetBounds(2 * kPanelHorizMargin,
                                     del_passwords_checkbox_->y() +
                                         del_passwords_checkbox_->height() +
                                         kRelatedControlVerticalSpacing,
                                     sz.width(), sz.height());

  // Time period label is next below the combo boxes.
  sz = time_period_label_->GetPreferredSize();
  time_period_label_->SetBounds(kPanelHorizMargin,
                                del_form_data_checkbox_->y() +
                                    del_form_data_checkbox_->height() +
                                    kRelatedControlVerticalSpacing +
                                    kExtraMarginForTimePeriodLabel,
                                sz.width(), sz.height());

  // Time period combo box goes on the right of the label, and we align it
  // vertically to the label as well.
  int label_y_size = sz.height();
  sz = time_period_combobox_->GetPreferredSize();
  time_period_combobox_->SetBounds(time_period_label_->x() +
                                       time_period_label_->width() +
                                       kRelatedControlVerticalSpacing,
                                   time_period_label_->y() -
                                       ((sz.height() - label_y_size) / 2),
                                   sz.width(), sz.height());

  // Get the y-coordinate of our parent so we can position the throbber and
  // status message at the bottom of the panel.
  gfx::Rect parent_bounds = GetParent()->GetLocalBounds(false);

  sz = throbber_->GetPreferredSize();
  int throbber_topleft_x = kPanelHorizMargin;
  int throbber_topleft_y = parent_bounds.bottom() - sz.height() -
                           kButtonVEdgeMargin - 3;
  throbber_->SetBounds(throbber_topleft_x, throbber_topleft_y, sz.width(),
                       sz.height());

  // The status label should be at the bottom of the screen, to the right of
  // the throbber.
  sz = status_label_.GetPreferredSize();
  int status_label_x = throbber_->x() + throbber_->width() +
                       kRelatedControlHorizontalSpacing;
  status_label_.SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  status_label_.SetBounds(status_label_x,
                          throbber_topleft_y + 1,
                          sz.width(),
                          sz.height());
}

void ClearBrowsingDataView::ViewHierarchyChanged(bool is_add,
                                                 views::View* parent,
                                                 views::View* child) {
  // Since we want the some of the controls to show up in the same visual row
  // as the buttons, which are provided by the framework, we must add the
  // buttons to the non-client view, which is the parent of this view.
  // Similarly, when we're removed from the view hierarchy, we must take care
  // to remove these items as well.
  if (child == this) {
    if (is_add) {
      parent->AddChildView(&status_label_);
      parent->AddChildView(throbber_.get());
    } else {
      parent->RemoveChildView(&status_label_);
      parent->RemoveChildView(throbber_.get());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView, views::DialogDelegate implementation:

std::wstring ClearBrowsingDataView::GetDialogButtonLabel(
    DialogButton button) const {
  if (button == DIALOGBUTTON_OK) {
    return l10n_util::GetString(IDS_CLEAR_BROWSING_DATA_COMMIT);
  } else {
    return std::wstring();
  }
}

bool ClearBrowsingDataView::IsDialogButtonEnabled(DialogButton button) const {
  if (delete_in_progress_)
    return false;

  if (button == DIALOGBUTTON_OK) {
    return del_history_checkbox_->IsSelected() ||
           del_downloads_checkbox_->IsSelected() ||
           del_cache_checkbox_->IsSelected() ||
           del_cookies_checkbox_->IsSelected() ||
           del_passwords_checkbox_->IsSelected() ||
           del_form_data_checkbox_->IsSelected();
  }

  return true;
}

bool ClearBrowsingDataView::CanResize() const {
  return false;
}

bool ClearBrowsingDataView::CanMaximize() const {
  return false;
}

bool ClearBrowsingDataView::IsAlwaysOnTop() const {
  return false;
}

bool ClearBrowsingDataView::HasAlwaysOnTopMenu() const {
  return false;
}

bool ClearBrowsingDataView::IsModal() const {
  return true;
}

std::wstring ClearBrowsingDataView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_CLEAR_BROWSING_DATA_TITLE);
}

bool ClearBrowsingDataView::Accept() {
  if (!IsDialogButtonEnabled(DIALOGBUTTON_OK)) {
    return false;
  }

  OnDelete();
  return false;  // We close the dialog in OnBrowsingDataRemoverDone().
}

views::View* ClearBrowsingDataView::GetContentsView() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView, views::ComboBox::Model implementation:

int ClearBrowsingDataView::GetItemCount(views::ComboBox* source) {
  DCHECK(source == time_period_combobox_);
  return 4;
}

std::wstring ClearBrowsingDataView::GetItemAt(views::ComboBox* source,
                                              int index) {
  DCHECK(source == time_period_combobox_);
  switch (index) {
    case 0: return l10n_util::GetString(IDS_CLEAR_DATA_DAY);
    case 1: return l10n_util::GetString(IDS_CLEAR_DATA_WEEK);
    case 2: return l10n_util::GetString(IDS_CLEAR_DATA_4WEEKS);
    case 3: return l10n_util::GetString(IDS_CLEAR_DATA_EVERYTHING);
    default: NOTREACHED() << L"Missing item";
             return L"?";
  }
}

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView, views::ComboBoxListener implementation:

void ClearBrowsingDataView::ItemChanged(views::ComboBox* sender,
                                        int prev_index, int new_index) {
  if (sender == time_period_combobox_ && prev_index != new_index)
    profile_->GetPrefs()->SetInteger(prefs::kDeleteTimePeriod, new_index);
}

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView, views::ButtonListener implementation:

void ClearBrowsingDataView::ButtonPressed(views::NativeButton* sender) {
  if (sender == del_history_checkbox_)
    profile_->GetPrefs()->SetBoolean(prefs::kDeleteBrowsingHistory,
        del_history_checkbox_->IsSelected() ? true : false);
  else if (sender == del_downloads_checkbox_)
    profile_->GetPrefs()->SetBoolean(prefs::kDeleteDownloadHistory,
        del_downloads_checkbox_->IsSelected() ? true : false);
  else if (sender == del_cache_checkbox_)
    profile_->GetPrefs()->SetBoolean(prefs::kDeleteCache,
        del_cache_checkbox_->IsSelected() ? true : false);
  else if (sender == del_cookies_checkbox_)
    profile_->GetPrefs()->SetBoolean(prefs::kDeleteCookies,
        del_cookies_checkbox_->IsSelected() ? true : false);
  else if (sender == del_passwords_checkbox_)
    profile_->GetPrefs()->SetBoolean(prefs::kDeletePasswords,
        del_passwords_checkbox_->IsSelected() ? true : false);
  else if (sender == del_form_data_checkbox_)
    profile_->GetPrefs()->SetBoolean(prefs::kDeleteFormData,
        del_form_data_checkbox_->IsSelected() ? true : false);

  // When no checkbox is checked we should not have the action button enabled.
  // This forces the button to evaluate what state they should be in.
  GetDialogClientView()->UpdateDialogButtons();
}

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView, private:

views::CheckBox* ClearBrowsingDataView::AddCheckbox(const std::wstring& text,
                                                    bool checked) {
  views::CheckBox* checkbox = new views::CheckBox(text);
  checkbox->SetIsSelected(checked);
  checkbox->SetListener(this);
  AddChildView(checkbox);
  return checkbox;
}

void ClearBrowsingDataView::UpdateControlEnabledState() {
  window()->EnableClose(!delete_in_progress_);

  del_history_checkbox_->SetEnabled(!delete_in_progress_);
  del_downloads_checkbox_->SetEnabled(!delete_in_progress_);
  del_cache_checkbox_->SetEnabled(!delete_in_progress_);
  del_cookies_checkbox_->SetEnabled(!delete_in_progress_);
  del_passwords_checkbox_->SetEnabled(!delete_in_progress_);
  del_form_data_checkbox_->SetEnabled(!delete_in_progress_);
  time_period_combobox_->SetEnabled(!delete_in_progress_);

  status_label_.SetVisible(delete_in_progress_);
  throbber_->SetVisible(delete_in_progress_);
  if (delete_in_progress_)
    throbber_->Start();
  else
    throbber_->Stop();

  // Make sure to update the state for OK and Cancel buttons.
  GetDialogClientView()->UpdateDialogButtons();
}

// Convenience method that returns true if the supplied checkbox is selected
// and enabled.
static bool IsCheckBoxEnabledAndSelected(views::CheckBox* cb) {
  return (cb->IsEnabled() && cb->IsSelected());
}

void ClearBrowsingDataView::OnDelete() {
  TimeDelta diff;
  Time delete_begin = Time::Now();

  int period_selected = time_period_combobox_->GetSelectedItem();
  switch (period_selected) {
    case 0: diff = TimeDelta::FromHours(24); break;        // Last day.
    case 1: diff = TimeDelta::FromHours(7*24); break;      // Last week.
    case 2: diff = TimeDelta::FromHours(4*7*24); break;    // Four weeks.
    case 3: delete_begin = Time(); break;                  // Everything.
    default: NOTREACHED() << L"Missing item"; break;
  }

  delete_begin = delete_begin - diff;

  int remove_mask = 0;
  if (IsCheckBoxEnabledAndSelected(del_history_checkbox_))
    remove_mask |= BrowsingDataRemover::REMOVE_HISTORY;
  if (IsCheckBoxEnabledAndSelected(del_downloads_checkbox_))
    remove_mask |= BrowsingDataRemover::REMOVE_DOWNLOADS;
  if (IsCheckBoxEnabledAndSelected(del_cookies_checkbox_))
    remove_mask |= BrowsingDataRemover::REMOVE_COOKIES;
  if (IsCheckBoxEnabledAndSelected(del_passwords_checkbox_))
    remove_mask |= BrowsingDataRemover::REMOVE_PASSWORDS;
  if (IsCheckBoxEnabledAndSelected(del_form_data_checkbox_))
    remove_mask |= BrowsingDataRemover::REMOVE_FORM_DATA;
  if (IsCheckBoxEnabledAndSelected(del_cache_checkbox_))
    remove_mask |= BrowsingDataRemover::REMOVE_CACHE;

  delete_in_progress_ = true;
  UpdateControlEnabledState();

  // BrowsingDataRemover deletes itself when done.
  remover_ =
      new BrowsingDataRemover(profile_, delete_begin, Time());
  remover_->AddObserver(this);
  remover_->Remove(remove_mask);
}

void ClearBrowsingDataView::OnBrowsingDataRemoverDone() {
  // No need to remove ourselves as an observer as BrowsingDataRemover deletes
  // itself after we return.
  remover_ = NULL;
  window()->Close();
}
