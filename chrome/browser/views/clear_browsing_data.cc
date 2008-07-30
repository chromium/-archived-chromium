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

#include "chrome/browser/views/clear_browsing_data.h"

#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/standard_layout.h"
#include "chrome/browser/template_url_model.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/background.h"
#include "chrome/views/checkbox.h"
#include "chrome/views/label.h"
#include "chrome/views/native_button.h"
#include "chrome/views/throbber.h"
#include "chrome/views/window.h"
#include "net/url_request/url_request_context.h"

#include "generated_resources.h"

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
      time_period_label_(NULL),
      time_period_combobox_(NULL),
      delete_in_progress_(false),
      profile_(profile) {
  DCHECK(profile);
  Init();
}

ClearBrowsingDataView::~ClearBrowsingDataView(void) {
}

void ClearBrowsingDataView::Init() {
  // Views we will add to the *parent* of this dialog, since it will display
  // next to the buttons which we don't draw ourselves.
  throbber_.reset(new ChromeViews::Throbber(50, true));
  throbber_->SetParentOwned(false);
  throbber_->SetVisible(false);

  status_label_.SetText(l10n_util::GetString(IDS_CLEAR_DATA_DELETING));
  status_label_.SetVisible(false);
  status_label_.SetParentOwned(false);

  // Regular view controls we draw by ourself. First, we add the dialog label.
  delete_all_label_ = new ChromeViews::Label(
      l10n_util::GetString(IDS_CLEAR_BROWSING_DATA_LABEL));
  AddChildView(delete_all_label_);

  // Add all the check-boxes.
  del_history_checkbox_ =
      AddCheckbox(l10n_util::GetString(IDS_DEL_BROWSING_HISTORY_CHKBOX), true);

  del_downloads_checkbox_ =
      AddCheckbox(l10n_util::GetString(IDS_DEL_DOWNLOAD_HISTORY_CHKBOX), true);

  del_cache_checkbox_ =
      AddCheckbox(l10n_util::GetString(IDS_DEL_CACHE_CHKBOX), true);

  del_cookies_checkbox_ =
      AddCheckbox(l10n_util::GetString(IDS_DEL_COOKIES_CHKBOX), true);

  del_passwords_checkbox_ =
      AddCheckbox(l10n_util::GetString(IDS_DEL_PASSWORDS_CHKBOX), false);

  // Add a label which appears before the combo box for the time period.
  time_period_label_ = new ChromeViews::Label(
    l10n_util::GetString(IDS_CLEAR_BROWSING_DATA_TIME_LABEL));
  AddChildView(time_period_label_);

  // Add the combo box showing how far back in time we want to delete.
  time_period_combobox_ = new ChromeViews::ComboBox(this);
  AddChildView(time_period_combobox_);
}

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView, ChromeViews::View implementation:

void ClearBrowsingDataView::GetPreferredSize(CSize *out) {
  DCHECK(out);
  *out = ChromeViews::Window::GetLocalizedContentsSize(
      IDS_CLEARDATA_DIALOG_WIDTH_CHARS,
      IDS_CLEARDATA_DIALOG_HEIGHT_LINES).ToSIZE();
}

void ClearBrowsingDataView::Layout() {
  CSize panel_size;
  GetPreferredSize(&panel_size);

  CSize sz;

  // Delete All label goes to the top left corner.
  delete_all_label_->GetPreferredSize(&sz);
  delete_all_label_->SetBounds(kPanelHorizMargin, kPanelVertMargin,
                               sz.cx, sz.cy);

  // Check-boxes go beneath it (with a little indentation).
  del_history_checkbox_->GetPreferredSize(&sz);
  del_history_checkbox_->SetBounds(2 * kPanelHorizMargin,
                                   delete_all_label_->GetY() +
                                       delete_all_label_->GetHeight() +
                                       kRelatedControlVerticalSpacing,
                                   sz.cx, sz.cy);

  del_downloads_checkbox_->GetPreferredSize(&sz);
  del_downloads_checkbox_->SetBounds(2 * kPanelHorizMargin,
                                     del_history_checkbox_->GetY() +
                                         del_history_checkbox_->GetHeight() +
                                         kRelatedControlVerticalSpacing,
                                     sz.cx, sz.cy);

  del_cache_checkbox_->GetPreferredSize(&sz);
  del_cache_checkbox_->SetBounds(2 * kPanelHorizMargin,
                                 del_downloads_checkbox_->GetY() +
                                     del_downloads_checkbox_->GetHeight() +
                                     kRelatedControlVerticalSpacing,
                                 sz.cx, sz.cy);

  del_cookies_checkbox_->GetPreferredSize(&sz);
  del_cookies_checkbox_->SetBounds(2 * kPanelHorizMargin,
                                   del_cache_checkbox_->GetY() +
                                       del_cache_checkbox_->GetHeight() +
                                       kRelatedControlVerticalSpacing,
                                   sz.cx, sz.cy);

  del_passwords_checkbox_->GetPreferredSize(&sz);
  del_passwords_checkbox_->SetBounds(2 * kPanelHorizMargin,
                                     del_cookies_checkbox_->GetY() +
                                         del_cookies_checkbox_->GetHeight() +
                                         kRelatedControlVerticalSpacing,
                                     sz.cx, sz.cy);

  // Time period label is next below the combo boxes.
  time_period_label_->GetPreferredSize(&sz);
  time_period_label_->SetBounds(kPanelHorizMargin,
                                del_passwords_checkbox_->GetY() +
                                    del_passwords_checkbox_->GetHeight() +
                                    kRelatedControlVerticalSpacing +
                                    kExtraMarginForTimePeriodLabel,
                                sz.cx, sz.cy);

  // Time period combo box goes on the right of the label, and we align it
  // vertically to the label as well.
  int label_y_size = sz.cy;
  time_period_combobox_->GetPreferredSize(&sz);
  time_period_combobox_->SetBounds(time_period_label_->GetX() +
                                       time_period_label_->GetWidth() +
                                       kRelatedControlVerticalSpacing,
                                   time_period_label_->GetY() -
                                       ((sz.cy - label_y_size) / 2),
                                   sz.cx, sz.cy);

  // Get the y-coordinate of our parent so we can position the throbber and
  // status message at the bottom of the panel.
  CRect parent_bounds;
  GetParent()->GetLocalBounds(&parent_bounds, false);

  throbber_->GetPreferredSize(&sz);
  int throbber_topleft_x = kPanelHorizMargin;
  int throbber_topleft_y = parent_bounds.bottom - sz.cy -
                           kButtonVEdgeMargin - 3;
  throbber_->SetBounds(throbber_topleft_x, throbber_topleft_y, sz.cx, sz.cy);

  // The status label should be at the bottom of the screen, to the right of
  // the throbber.
  status_label_.GetPreferredSize(&sz);
  int status_label_x = throbber_->GetX() + throbber_->GetWidth() +
                       kRelatedControlHorizontalSpacing;
  status_label_.SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
  status_label_.SetBounds(status_label_x,
                          throbber_topleft_y + 1,
                          sz.cx,
                          sz.cy);
}

void ClearBrowsingDataView::ViewHierarchyChanged(bool is_add,
                                                 ChromeViews::View* parent,
                                                 ChromeViews::View* child) {
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
// ClearBrowsingDataView, ChromeViews::DialogDelegate implementation:

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
           del_passwords_checkbox_->IsSelected();
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
  return false;  // We close the dialog in OnDeletionDone().
}

ChromeViews::View* ClearBrowsingDataView::GetContentsView() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView, ChromeViews::ComboBox::Model implementation:

int ClearBrowsingDataView::GetItemCount(ChromeViews::ComboBox* source) {
  DCHECK(source == time_period_combobox_);
  return 4;
}

std::wstring ClearBrowsingDataView::GetItemAt(ChromeViews::ComboBox* source,
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
// ClearBrowsingDataView, ChromeViews::ButtonListener implementation:

void ClearBrowsingDataView::ButtonPressed(ChromeViews::NativeButton* sender) {
  // When no checkbox is checked we should not have the action button enabled.
  // This forces the button to evaluate what state they should be in.
  window()->UpdateDialogButtons();
}

////////////////////////////////////////////////////////////////////////////////
// ClearBrowsingDataView, private:

ChromeViews::CheckBox* ClearBrowsingDataView::AddCheckbox(
    const std::wstring& text, bool checked) {
  ChromeViews::CheckBox* checkbox = new ChromeViews::CheckBox(text);
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
  time_period_combobox_->SetEnabled(!delete_in_progress_);

  status_label_.SetVisible(delete_in_progress_);
  throbber_->SetVisible(delete_in_progress_);
  if (delete_in_progress_)
    throbber_->Start();
  else
    throbber_->Stop();

  // Make sure to update the state for OK and Cancel buttons.
  window()->UpdateDialogButtons();
}

// Convenience method that returns true if the supplied checkbox is selected
// and enabled.
static bool IsCheckBoxEnabledAndSelected(ChromeViews::CheckBox* cb) {
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
  if (IsCheckBoxEnabledAndSelected(del_cache_checkbox_))
    remove_mask |= BrowsingDataRemover::REMOVE_CACHE;

  delete_in_progress_ = true;
  UpdateControlEnabledState();

  // BrowsingDataRemover deletes itself when done.
  BrowsingDataRemover* remover =
      new BrowsingDataRemover(profile_, delete_begin, Time());
  remover->AddObserver(this);
  remover->Remove(remove_mask);
}

void ClearBrowsingDataView::OnBrowsingDataRemoverDone() {
  window()->Close();
}
