// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/passwords_exceptions_window_view.h"

#include "app/l10n_util.h"
#include "chrome/browser/views/options/passwords_page_view.h"
#include "chrome/browser/views/options/exceptions_page_view.h"
#include "grit/generated_resources.h"

// static
PasswordsExceptionsWindowView* PasswordsExceptionsWindowView::instance_ = NULL;

static const int kDefaultWindowWidth = 530;
static const int kDefaultWindowHeight = 240;
static const int kDialogPadding = 7;

namespace browser {

// Declared in browser_dialogs.h so others don't have to depend on our header.
void ShowPasswordsExceptionsWindowView(Profile* profile) {
  PasswordsExceptionsWindowView::Show(profile);
}

}  // namespace browser

///////////////////////////////////////////////////////////////////////////////
// PasswordsExceptionsWindowView, public

PasswordsExceptionsWindowView::PasswordsExceptionsWindowView(Profile* profile)
    : tabs_(NULL),
      passwords_page_view_(NULL),
      exceptions_page_view_(NULL),
      profile_(profile) {
}

// static
void PasswordsExceptionsWindowView::Show(Profile* profile) {
  DCHECK(profile);
  if (!instance_) {
    instance_ = new PasswordsExceptionsWindowView(profile);

    // instances_ will get deleted once Close() is called.
    views::Window::CreateChromeWindow(NULL, gfx::Rect(), instance_);
  }
  if (!instance_->window()->IsVisible()) {
    instance_->window()->Show();
  } else {
    instance_->window()->Activate();
  }
}

/////////////////////////////////////////////////////////////////////////////
// PasswordsExceptionsWindowView, views::View implementations

void PasswordsExceptionsWindowView::Layout() {
  tabs_->SetBounds(kDialogPadding, kDialogPadding,
                   width() - (2 * kDialogPadding),
                   height() - (2 * kDialogPadding));
}

gfx::Size PasswordsExceptionsWindowView::GetPreferredSize() {
  return gfx::Size(kDefaultWindowWidth, kDefaultWindowHeight);
}

void PasswordsExceptionsWindowView::ViewHierarchyChanged(
    bool is_add, views::View* parent, views::View* child) {
  if (is_add && child == this)
    Init();
}

/////////////////////////////////////////////////////////////////////////////
// PasswordsExceptionsWindowView, views::DisloagDelegate implementations

int PasswordsExceptionsWindowView::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

std::wstring PasswordsExceptionsWindowView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_PASSWORDS_EXCEPTIONS_WINDOW_TITLE);
}

void PasswordsExceptionsWindowView::WindowClosing() {
  // |instnace_| is deleted once the window is closed, so we just have to set
  // it to NULL.
  instance_ = NULL;
}

views::View* PasswordsExceptionsWindowView::GetContentsView() {
  return this;
}

/////////////////////////////////////////////////////////////////////////////
// PasswordsExceptionsWindowView, private

void PasswordsExceptionsWindowView::Init() {
  tabs_ = new views::TabbedPane();
  AddChildView(tabs_);

  passwords_page_view_ = new PasswordsPageView(profile_);
  tabs_->AddTab(l10n_util::GetString(
      IDS_PASSWORDS_SHOW_PASSWORDS_TAB_TITLE), passwords_page_view_);

  exceptions_page_view_ = new ExceptionsPageView(profile_);
  tabs_->AddTab(l10n_util::GetString(
      IDS_PASSWORDS_EXCEPTIONS_TAB_TITLE), exceptions_page_view_);
}
