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

#include "chrome/browser/views/options/advanced_page_view.h"

#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/standard_layout.h"
#include "chrome/browser/views/options/advanced_contents_view.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/message_box_view.h"
#include "chrome/views/native_button.h"
#include "chrome/views/scroll_view.h"
#include "chrome/views/window.h"
#include "generated_resources.h"

namespace {

// A dialog box that asks the user to confirm resetting settings.
class ResetDefaultsConfirmBox : public ChromeViews::DialogDelegate {
 public:
  // This box is modal to |parent_hwnd|.
  static void ShowConfirmBox(HWND parent_hwnd, AdvancedPageView* page_view) {
    // When the window closes, it will delete itself.
    new ResetDefaultsConfirmBox(parent_hwnd, page_view);
  }

 protected:
  // ChromeViews::DialogDelegate
  virtual int GetDialogButtons() const {
    return DIALOGBUTTON_OK | DIALOGBUTTON_CANCEL;
  }
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const {
    switch (button) {
      case DIALOGBUTTON_OK:
        return l10n_util::GetString(IDS_OPTIONS_RESET_OKLABEL);
      case DIALOGBUTTON_CANCEL:
        return l10n_util::GetString(IDS_OPTIONS_RESET_CANCELLABEL);
      default:
        break;
    }
    NOTREACHED();
    return std::wstring();
  }
  virtual std::wstring GetWindowTitle() const {
    return l10n_util::GetString(IDS_PRODUCT_NAME);
  }
  virtual bool Accept() {
    advanced_page_view_->ResetToDefaults();
    return true;
  }
  // ChromeViews::WindowDelegate
  virtual void WindowClosing() { delete this; }
  virtual bool IsModal() const { return true; }
  virtual ChromeViews::View* GetContentsView() { return message_box_view_; }

 private:
  ResetDefaultsConfirmBox(HWND parent_hwnd, AdvancedPageView* page_view)
      : advanced_page_view_(page_view) {
    const int kDialogWidth = 400;
    // Also deleted when the window closes.
    message_box_view_ = new MessageBoxView(
        MessageBoxView::kFlagHasMessage | MessageBoxView::kFlagHasOKButton,
        l10n_util::GetString(IDS_OPTIONS_RESET_MESSAGE).c_str(),
        std::wstring(),
        kDialogWidth);
    ChromeViews::Window::CreateChromeWindow(parent_hwnd, gfx::Rect(),
                                            this)->Show();
  }
  virtual ~ResetDefaultsConfirmBox() { }

  MessageBoxView* message_box_view_;
  AdvancedPageView* advanced_page_view_;

  DISALLOW_EVIL_CONSTRUCTORS(ResetDefaultsConfirmBox);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// AdvancedPageView, public:

AdvancedPageView::AdvancedPageView(Profile* profile)
    : advanced_scroll_view_(NULL),
      reset_to_default_button_(NULL),
      OptionsPageView(profile) {
}

AdvancedPageView::~AdvancedPageView() {
}

void AdvancedPageView::ResetToDefaults() {
  // TODO(tc): It would be nice if we could generate this list automatically so
  // changes to any of the options pages doesn't require updating this list
  // manually.
  PrefService* prefs = profile()->GetPrefs();
  const wchar_t* kUserPrefs[] = {
    prefs::kAcceptLanguages,
    prefs::kAlternateErrorPagesEnabled,
    prefs::kBlockPopups,
    prefs::kCookieBehavior,
    prefs::kDefaultCharset,
    prefs::kDnsPrefetchingEnabled,
    prefs::kDownloadDefaultDirectory,
    prefs::kDownloadExtensionsToOpen,
    prefs::kHomePage,
    prefs::kMixedContentFiltering,
    prefs::kPromptForDownload,
    prefs::kPasswordManagerEnabled,
    prefs::kRestoreOnStartup,
    prefs::kSafeBrowsingEnabled,
    prefs::kShowHomeButton,
    prefs::kSpellCheckDictionary,
    prefs::kURLsToRestoreOnStartup,
    prefs::kWebKitDefaultFixedFontSize,
    prefs::kWebKitDefaultFontSize,
    prefs::kWebKitFixedFontFamily,
    prefs::kWebKitJavaEnabled,
    prefs::kWebKitJavascriptEnabled,
    prefs::kWebKitLoadsImagesAutomatically,
    prefs::kWebKitPluginsEnabled,
    prefs::kWebKitSansSerifFontFamily,
    prefs::kWebKitSerifFontFamily,
  };
  for (int i = 0; i < arraysize(kUserPrefs); ++i)
    prefs->ClearPref(kUserPrefs[i]);

  PrefService* local_state = g_browser_process->local_state();
  // Note that we don't reset the kMetricsReportingEnabled preference here
  // because the reset will reset it to the default setting specified in Chrome
  // source, not the default setting selected by the user on the web page where
  // they downloaded Chrome. This means that if the user ever resets their
  // settings they'll either inadvertedly enable this logging or disable it.
  // One is undesirable for them, one is undesirable for us. For now, we just
  // don't reset it.
  const wchar_t* kLocalStatePrefs[] = {
    prefs::kApplicationLocale,
    prefs::kOptionsWindowLastTabIndex,
  };
  for (int i = 0; i < arraysize(kLocalStatePrefs); ++i)
    local_state->ClearPref(kLocalStatePrefs[i]);
}

///////////////////////////////////////////////////////////////////////////////
// AdvancedPageView, ChromeViews::NativeButton::Listener implementation:

void AdvancedPageView::ButtonPressed(ChromeViews::NativeButton* sender) {
  if (sender == reset_to_default_button_) {
    UserMetricsRecordAction(L"Options_ResetToDefaults", NULL);
    ResetDefaultsConfirmBox::ShowConfirmBox(GetRootWindow(), this);
  }
}

///////////////////////////////////////////////////////////////////////////////
// AdvancedPageView, OptionsPageView implementation:

void AdvancedPageView::InitControlLayout() {
  reset_to_default_button_ = new ChromeViews::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_RESET));
  reset_to_default_button_->SetListener(this);
  advanced_scroll_view_ = new AdvancedScrollViewContainer(profile());

  using ChromeViews::GridLayout;
  using ChromeViews::ColumnSet;

  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(1, single_column_view_set_id);
  layout->AddView(advanced_scroll_view_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(reset_to_default_button_, 1, 1,
                  GridLayout::TRAILING, GridLayout::CENTER);
}
