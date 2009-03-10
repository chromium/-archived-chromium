// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/fonts_languages_window_view.h"

#include "chrome/browser/profile.h"
#include "chrome/browser/views/options/fonts_page_view.h"
#include "chrome/browser/views/options/languages_page_view.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/window.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

// static
static FontsLanguagesWindowView* instance_ = NULL;
static const int kDialogPadding = 7;

///////////////////////////////////////////////////////////////////////////////
// FontsLanguagesWindowView, public:

FontsLanguagesWindowView::FontsLanguagesWindowView(Profile* profile)
      // Always show preferences for the original profile. Most state when off
      // the record comes from the original profile, but we explicitly use
      // the original profile to avoid potential problems.
    : profile_(profile->GetOriginalProfile()),
      fonts_page_(NULL),
      languages_page_(NULL) {
}

FontsLanguagesWindowView::~FontsLanguagesWindowView() {
}

///////////////////////////////////////////////////////////////////////////////
// FontsLanguagesWindowView, views::DialogDelegate implementation:

bool FontsLanguagesWindowView::Accept() {
  fonts_page_->SaveChanges();
  languages_page_->SaveChanges();
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// FontsLanguagesWindowView, views::WindowDelegate implementation:

std::wstring FontsLanguagesWindowView::GetWindowTitle() const {
  return l10n_util::GetStringF(IDS_FONT_LANGUAGE_SETTING_WINDOWS_TITLE,
                               l10n_util::GetString(IDS_PRODUCT_NAME));
}

views::View* FontsLanguagesWindowView::GetContentsView() {
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// FontsLanguagesWindowView, views::View overrides:

void FontsLanguagesWindowView::Layout() {
  tabs_->SetBounds(kDialogPadding, kDialogPadding,
                   width() - (2 * kDialogPadding),
                   height() - (2 * kDialogPadding));
}

gfx::Size FontsLanguagesWindowView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_FONTSLANG_DIALOG_WIDTH_CHARS,
      IDS_FONTSLANG_DIALOG_HEIGHT_LINES));
}

void FontsLanguagesWindowView::SelectLanguagesTab() {
  tabs_->SelectTabForContents(languages_page_);
}

void FontsLanguagesWindowView::ViewHierarchyChanged(
    bool is_add, views::View* parent, views::View* child) {
  // Can't init before we're inserted into a Container, because we require
  // a HWND to parent native child controls to.
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// FontsLanguagesWindowView, private:

void FontsLanguagesWindowView::Init() {
  tabs_ = new views::TabbedPane;
  AddChildView(tabs_);

  fonts_page_ = new FontsPageView(profile_);
  tabs_->AddTab(l10n_util::GetString(
      IDS_FONT_LANGUAGE_SETTING_FONT_TAB_TITLE), fonts_page_);

  languages_page_ = new LanguagesPageView(profile_);
  tabs_->AddTab(l10n_util::GetString(
      IDS_FONT_LANGUAGE_SETTING_LANGUAGES_TAB_TITLE), languages_page_);
}
