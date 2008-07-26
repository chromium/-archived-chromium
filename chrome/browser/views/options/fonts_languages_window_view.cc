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

#include "chrome/browser/views/options/fonts_languages_window_view.h"

#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/options/fonts_page_view.h"
#include "chrome/browser/views/options/languages_page_view.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/window.h"
#include "generated_resources.h"

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
// FontsLanguagesWindowView, ChromeViews::DialogDelegate implementation:

std::wstring FontsLanguagesWindowView::GetWindowTitle() const {
  return l10n_util::GetStringF(IDS_FONT_LANGUAGE_SETTING_WINDOWS_TITLE,
                               l10n_util::GetString(IDS_PRODUCT_NAME));
}

bool FontsLanguagesWindowView::Accept() {
  fonts_page_->SaveChanges();
  languages_page_->SaveChanges();
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// FontsLanguagesWindowView, ChromeViews::View overrides:

void FontsLanguagesWindowView::Layout() {
  tabs_->SetBounds(kDialogPadding, kDialogPadding,
                   GetWidth() - (2 * kDialogPadding),
                   GetHeight() - (2 * kDialogPadding));
}

void FontsLanguagesWindowView::GetPreferredSize(CSize* out) {
  DCHECK(out);
  *out = ChromeViews::Window::GetLocalizedContentsSize(
      IDS_FONTSLANG_DIALOG_WIDTH_CHARS,
      IDS_FONTSLANG_DIALOG_HEIGHT_LINES).ToSIZE();
}

void FontsLanguagesWindowView::ViewHierarchyChanged(
    bool is_add, ChromeViews::View* parent, ChromeViews::View* child) {
  // Can't init before we're inserted into a ViewContainer, because we require
  // a HWND to parent native child controls to.
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// FontsLanguagesWindowView, private:

void FontsLanguagesWindowView::Init() {
  tabs_ = new ChromeViews::TabbedPane;
  AddChildView(tabs_);

  fonts_page_ = new FontsPageView(profile_);
  tabs_->AddTabAtIndex(0, l10n_util::GetString(
      IDS_FONT_LANGUAGE_SETTING_FONT_TAB_TITLE), fonts_page_, true);

  languages_page_ = new LanguagesPageView(profile_);
  tabs_->AddTabAtIndex(1, l10n_util::GetString(
      IDS_FONT_LANGUAGE_SETTING_LANGUAGES_TAB_TITLE), languages_page_, true);
}
