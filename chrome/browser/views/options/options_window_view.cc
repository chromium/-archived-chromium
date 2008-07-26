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

#include "chrome/browser/options_window.h"

#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/options/advanced_page_view.h"
#include "chrome/browser/views/options/content_page_view.h"
#include "chrome/browser/views/options/general_page_view.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/tabbed_pane.h"
#include "chrome/views/window.h"
#include "generated_resources.h"

static const int kDefaultWindowWidthChars = 85;
static const int kDefaultWindowHeightLines = 29;

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowView
//
//  The contents of the Options dialog window.
//
class OptionsWindowView : public ChromeViews::View,
                          public ChromeViews::DialogDelegate,
                          public ChromeViews::TabbedPane::Listener {
 public:
  explicit OptionsWindowView(Profile* profile);
  virtual ~OptionsWindowView();

  ChromeViews::Window* container() const { return container_; }
  void set_container(ChromeViews::Window* container) {
    container_ = container;
  }

  // Shows the Tab corresponding to the specified OptionsPage.
  void ShowOptionsPage(OptionsPage page, OptionsGroup highlight_group);

  // ChromeViews::DialogDelegate implementation:
  virtual int GetDialogButtons() const { return DIALOGBUTTON_CANCEL; }
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual bool Cancel();

  // ChromeViews::TabbedPane::Listener implementation:
  virtual void TabSelectedAt(int index);

  // ChromeViews::View overrides:
  virtual void Layout();
  virtual void GetPreferredSize(CSize* out);

 protected:
  // ChromeViews::View overrides:
  virtual void ViewHierarchyChanged(bool is_add,
                                    ChromeViews::View* parent,
                                    ChromeViews::View* child);
 private:
  // Init the assorted Tabbed pages
  void Init();

  // Returns the currently selected OptionsPageView.
  OptionsPageView* GetCurrentOptionsPageView() const;

  // The Tab view that contains all of the options pages.
  ChromeViews::TabbedPane* tabs_;

  // The Options dialog window.
  ChromeViews::Window* container_;

  // The Profile associated with these options.
  Profile* profile_;

  // The last page the user was on when they opened the Options window.
  IntegerPrefMember last_selected_page_;

  DISALLOW_EVIL_CONSTRUCTORS(OptionsWindowView);
};

// static
static OptionsWindowView* instance_ = NULL;
static const int kDialogPadding = 7;

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowView, public:

OptionsWindowView::OptionsWindowView(Profile* profile)
      // Always show preferences for the original profile. Most state when off
      // the record comes from the original profile, but we explicitly use
      // the original profile to avoid potential problems.
    : profile_(profile->GetOriginalProfile()) {
  // The download manager needs to be initialized before the contents of the
  // Options Window are created.
  profile_->GetDownloadManager();
  // We don't need to observe changes in this value.
  last_selected_page_.Init(prefs::kOptionsWindowLastTabIndex,
                           g_browser_process->local_state(), NULL);
}

OptionsWindowView::~OptionsWindowView() {
}

void OptionsWindowView::ShowOptionsPage(OptionsPage page,
                                        OptionsGroup highlight_group) {
  // If the window is not yet visible, we need to show it (it will become
  // active), otherwise just bring it to the front.
  if (!container_->IsVisible()) {
    container_->Show();
  } else {
    container_->Activate();
  }

  if (page == OPTIONS_PAGE_DEFAULT) {
    // Remember the last visited page from local state.
    page = static_cast<OptionsPage>(last_selected_page_.GetValue());
    if (page == OPTIONS_PAGE_DEFAULT)
      page = OPTIONS_PAGE_GENERAL;
  }
  DCHECK(page > OPTIONS_PAGE_DEFAULT && page < OPTIONS_PAGE_COUNT);
  tabs_->SelectTabAt(static_cast<int>(page));

  GetCurrentOptionsPageView()->HighlightGroup(highlight_group);
}

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowView, ChromeViews::DialogDelegate implementation:

std::wstring OptionsWindowView::GetWindowTitle() const {
  return l10n_util::GetStringF(IDS_OPTIONS_DIALOG_TITLE,
                               l10n_util::GetString(IDS_PRODUCT_NAME));
}

void OptionsWindowView::WindowClosing() {
  // Clear the static instance so that the next time ShowOptionsWindow() is
  // called a new window is opened.
  instance_ = NULL;
}

bool OptionsWindowView::Cancel() {
  return GetCurrentOptionsPageView()->CanClose();
}

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowView, ChromeViews::TabbedPane::Listener implementation:

void OptionsWindowView::TabSelectedAt(int index) {
  DCHECK(index > OPTIONS_PAGE_DEFAULT && index < OPTIONS_PAGE_COUNT);
  last_selected_page_.SetValue(index);
}

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowView, ChromeViews::View overrides:

void OptionsWindowView::Layout() {
  tabs_->SetBounds(kDialogPadding, kDialogPadding,
                   GetWidth() - (2 * kDialogPadding),
                   GetHeight() - (2 * kDialogPadding));
}

void OptionsWindowView::GetPreferredSize(CSize* out) {
  DCHECK(out);
  *out = ChromeViews::Window::GetLocalizedContentsSize(
      IDS_OPTIONS_DIALOG_WIDTH_CHARS,
      IDS_OPTIONS_DIALOG_HEIGHT_LINES).ToSIZE();
}

void OptionsWindowView::ViewHierarchyChanged(bool is_add,
                                             ChromeViews::View* parent,
                                             ChromeViews::View* child) {
  // Can't init before we're inserted into a ViewContainer, because we require
  // a HWND to parent native child controls to.
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowView, private:

void OptionsWindowView::Init() {
  tabs_ = new ChromeViews::TabbedPane;
  tabs_->SetListener(this);
  AddChildView(tabs_);

  GeneralPageView* general_page = new GeneralPageView(profile_);
  tabs_->AddTabAtIndex(0, l10n_util::GetString(IDS_OPTIONS_GENERAL_TAB_LABEL),
                       general_page, false);

  ContentPageView* content_page = new ContentPageView(profile_);
  tabs_->AddTabAtIndex(1, l10n_util::GetString(IDS_OPTIONS_CONTENT_TAB_LABEL),
                       content_page, false);

  AdvancedPageView* advanced_page = new AdvancedPageView(profile_);
  tabs_->AddTabAtIndex(2, l10n_util::GetString(IDS_OPTIONS_ADVANCED_TAB_LABEL),
                       advanced_page, false);

  DCHECK(tabs_->GetTabCount() == OPTIONS_PAGE_COUNT);
}

OptionsPageView* OptionsWindowView::GetCurrentOptionsPageView() const {
  ChromeViews::RootView* contents_root_view = tabs_->GetContentsRootView();
  DCHECK(contents_root_view->GetChildViewCount() == 1);
  return static_cast<OptionsPageView*>(contents_root_view->GetChildViewAt(0));
}

///////////////////////////////////////////////////////////////////////////////
// Factory/finder method:

void ShowOptionsWindow(OptionsPage page,
                       OptionsGroup highlight_group,
                       Profile* profile) {
  DCHECK(profile);
  // If there's already an existing options window, activate it and switch to
  // the specified page.
  // TODO(beng): note this is not multi-simultaneous-profile-safe. When we care
  //             about this case this will have to be fixed.
  if (!instance_) {
    instance_ = new OptionsWindowView(profile);
    instance_->set_container(ChromeViews::Window::CreateChromeWindow(
        NULL, gfx::Rect(), instance_, instance_));
    // The window is alive by itself now...
  }
  instance_->ShowOptionsPage(page, highlight_group);
}
