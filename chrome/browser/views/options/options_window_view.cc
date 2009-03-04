// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/options_window.h"

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
#ifdef CHROME_PERSONALIZATION
#include "chrome/personalization/views/user_data_page_view.h"
#endif
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/tabbed_pane.h"
#include "chrome/views/root_view.h"
#include "chrome/views/window.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowView
//
//  The contents of the Options dialog window.
//
class OptionsWindowView : public views::View,
                          public views::DialogDelegate,
                          public views::TabbedPane::Listener {
 public:
  explicit OptionsWindowView(Profile* profile);
  virtual ~OptionsWindowView();

  // Shows the Tab corresponding to the specified OptionsPage.
  void ShowOptionsPage(OptionsPage page, OptionsGroup highlight_group);

  // views::DialogDelegate implementation:
  virtual int GetDialogButtons() const { return DIALOGBUTTON_CANCEL; }
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual bool Cancel();
  virtual views::View* GetContentsView();

  // views::TabbedPane::Listener implementation:
  virtual void TabSelectedAt(int index);

  // views::View overrides:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);
 private:
  // Init the assorted Tabbed pages
  void Init();

  // Returns the currently selected OptionsPageView.
  OptionsPageView* GetCurrentOptionsPageView() const;

  // The Tab view that contains all of the options pages.
  views::TabbedPane* tabs_;

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
  if (!window()->IsVisible()) {
    window()->Show();
  } else {
    window()->Activate();
  }

  if (page == OPTIONS_PAGE_DEFAULT) {
    // Remember the last visited page from local state.
    page = static_cast<OptionsPage>(last_selected_page_.GetValue());
    if (page == OPTIONS_PAGE_DEFAULT)
      page = OPTIONS_PAGE_GENERAL;
  }
  // If the page number is out of bounds, reset to the first tab.
  if (page < 0 || page >= tabs_->GetTabCount())
    page = OPTIONS_PAGE_GENERAL;

  tabs_->SelectTabAt(static_cast<int>(page));

  GetCurrentOptionsPageView()->HighlightGroup(highlight_group);
}

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowView, views::DialogDelegate implementation:

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

views::View* OptionsWindowView::GetContentsView() {
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowView, views::TabbedPane::Listener implementation:

void OptionsWindowView::TabSelectedAt(int index) {
  DCHECK(index > OPTIONS_PAGE_DEFAULT && index < OPTIONS_PAGE_COUNT);
  last_selected_page_.SetValue(index);
}

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowView, views::View overrides:

void OptionsWindowView::Layout() {
  tabs_->SetBounds(kDialogPadding, kDialogPadding,
                   width() - (2 * kDialogPadding),
                   height() - (2 * kDialogPadding));
}

gfx::Size OptionsWindowView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_OPTIONS_DIALOG_WIDTH_CHARS,
      IDS_OPTIONS_DIALOG_HEIGHT_LINES));
}

void OptionsWindowView::ViewHierarchyChanged(bool is_add,
                                             views::View* parent,
                                             views::View* child) {
  // Can't init before we're inserted into a Container, because we require a
  // HWND to parent native child controls to.
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowView, private:

void OptionsWindowView::Init() {
  tabs_ = new views::TabbedPane;
  tabs_->SetListener(this);
  AddChildView(tabs_);

  int tab_index = 0;
  GeneralPageView* general_page = new GeneralPageView(profile_);
  tabs_->AddTabAtIndex(tab_index++,
                       l10n_util::GetString(IDS_OPTIONS_GENERAL_TAB_LABEL),
                       general_page, false);

  ContentPageView* content_page = new ContentPageView(profile_);
  tabs_->AddTabAtIndex(tab_index++,
                       l10n_util::GetString(IDS_OPTIONS_CONTENT_TAB_LABEL),
                       content_page, false);

#ifdef CHROME_PERSONALIZATION
  UserDataPageView* user_data_page = new UserDataPageView(profile_);
  tabs_->AddTabAtIndex(tab_index++,
                       l10n_util::GetString(IDS_OPTIONS_USER_DATA_TAB_LABEL),
                       user_data_page, false);
#endif

  AdvancedPageView* advanced_page = new AdvancedPageView(profile_);
  tabs_->AddTabAtIndex(tab_index++,
                       l10n_util::GetString(IDS_OPTIONS_ADVANCED_TAB_LABEL),
                       advanced_page, false);

  DCHECK(tabs_->GetTabCount() == OPTIONS_PAGE_COUNT);
}

OptionsPageView* OptionsWindowView::GetCurrentOptionsPageView() const {
  views::RootView* contents_root_view = tabs_->GetContentsRootView();
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
    views::Window::CreateChromeWindow(NULL, gfx::Rect(), instance_);
    // The window is alive by itself now...
  }
  instance_->ShowOptionsPage(page, highlight_group);
}

