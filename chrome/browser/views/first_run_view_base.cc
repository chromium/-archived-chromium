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

#include "chrome/browser/views/first_run_view_base.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/ref_counted.h"
#include "base/thread.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/standard_layout.h"
#include "chrome/browser/first_run.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/background.h"
#include "chrome/views/client_view.h"
#include "chrome/views/image_view.h"
#include "chrome/views/label.h"
#include "chrome/views/throbber.h"
#include "chrome/views/separator.h"
#include "chrome/views/window.h"

#include "generated_resources.h"

FirstRunViewBase::FirstRunViewBase(Profile* profile)
    : dialog_(NULL),
      preferred_width_(0),
      background_image_(NULL),
      separator_1_(NULL),
      separator_2_(NULL),
      importer_host_(NULL),
      profile_(profile) {
  DCHECK(profile);
  SetupControls();
}

FirstRunViewBase::~FirstRunViewBase() {
  // Register and set the "show first run information bubble" state so that the
  // browser can read it later.
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state->IsPrefRegistered(prefs::kShouldShowFirstRunBubble)) {
    local_state->RegisterBooleanPref(prefs::kShouldShowFirstRunBubble, false);
    local_state->SetBoolean(prefs::kShouldShowFirstRunBubble, true);
  }

  if (!local_state->IsPrefRegistered(prefs::kShouldShowWelcomePage)) {
    local_state->RegisterBooleanPref(prefs::kShouldShowWelcomePage, false);
    local_state->SetBoolean(prefs::kShouldShowWelcomePage, true);
  }
}

void FirstRunViewBase::SetupControls() {
  using ChromeViews::Label;
  using ChromeViews::ImageView;
  using ChromeViews::Background;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  background_image_ = new ChromeViews::ImageView();
  background_image_->SetImage(rb.GetBitmapNamed(IDR_WIZARD_ICON));
  background_image_->SetHorizontalAlignment(ImageView::TRAILING);

  int color = 0;
  {
    SkAutoLockPixels pixel_loc(background_image_->GetImage());
    uint32_t* pixel = background_image_->GetImage().getAddr32(0, 0);
    color = (0xff & (*pixel));
  }
  Background* bkg = Background::CreateSolidBackground(color, color, color);

  // The bitmap we use as the background contains a clipped logo and therefore
  // we can not automatically mirror it for RTL UIs by simply flipping it. This
  // is why we load a different bitmap if the View is using a right-to-left UI
  // layout.
  //
  // Note that we first load the LTR image and then replace it with the RTL
  // image because the code above derives the background color from the LTR
  // image so we have to use the LTR logo initially and then replace it with
  // the RTL logo if we find out that we are running in a right-to-left locale.
  if (UILayoutIsRightToLeft())
    background_image_->SetImage(rb.GetBitmapNamed(IDR_WIZARD_ICON_RTL));

  background_image_->SetBackground(bkg);
  AddChildView(background_image_);

  // The first separator marks the end of the image.
  separator_1_ = new ChromeViews::Separator;
  AddChildView(separator_1_);

  // The second separator marks the start of buttons.
  separator_2_ = new ChromeViews::Separator;
  AddChildView(separator_2_);
}

void FirstRunViewBase::AdjustDialogWidth(const ChromeViews::View* sub_view) {
  CRect bounds;
  sub_view->GetBounds(&bounds);
  preferred_width_ =
      std::max(preferred_width_,
               static_cast<int>(bounds.right) + kPanelHorizMargin);
}

void FirstRunViewBase::SetMinimumDialogWidth(int width) {
  preferred_width_ = std::max(preferred_width_, width);
}

void FirstRunViewBase::Layout() {
  const int kVertSpacing = 8;

  CSize canvas;
  GetPreferredSize(&canvas);

  CSize pref_size;
  background_image_->GetPreferredSize(&pref_size);
  background_image_->SetBounds(0, 0, canvas.cx, pref_size.cy);

  int next_v_space = background_image_->GetY() +
                     background_image_->GetHeight() - 2;

  separator_1_->GetPreferredSize(&pref_size);
  separator_1_->SetBounds(0, next_v_space,
                          canvas.cx + 1,
                          pref_size.cy);

  next_v_space = canvas.cy - kPanelSubVerticalSpacing;
  separator_2_->GetPreferredSize(&pref_size);
  separator_2_->SetBounds(kPanelHorizMargin , next_v_space,
                          canvas.cx - 2*kPanelHorizMargin, pref_size.cy);
}

bool FirstRunViewBase::CanResize() const {
  return false;
}

bool FirstRunViewBase::CanMaximize() const {
  return false;
}

bool FirstRunViewBase::IsAlwaysOnTop() const {
  return false;
}

bool FirstRunViewBase::HasAlwaysOnTopMenu() const {
  return false;
}

int FirstRunViewBase::GetDefaultImportItems() const {
  // It is best to avoid importing cookies because there is a bug that make
  // the process take way too much time among other issues. So for the time
  // being we say: TODO(CPU): Bug 1196875
  return HISTORY | FAVORITES | PASSWORDS | SEARCH_ENGINES | HOME_PAGE;
};

void FirstRunViewBase::DisableButtons() {
  dialog_->EnableClose(false);
  ChromeViews::ClientView* cv =
      reinterpret_cast<ChromeViews::ClientView*>(GetParent());
  if (cv) {
    cv->ok_button()->SetEnabled(false);
    cv->cancel_button()->SetEnabled(false);
  }
}

bool FirstRunViewBase::CreateDesktopShortcut() {
  return FirstRun::CreateChromeDesktopShortcut();
}

bool FirstRunViewBase::CreateQuickLaunchShortcut() {
  return FirstRun::CreateChromeQuickLaunchShortcut();
}

bool FirstRunViewBase::FirstRunComplete() {
  return FirstRun::CreateSentinel();
}
