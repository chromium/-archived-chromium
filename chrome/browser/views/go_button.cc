// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/go_button.h"

#include "app/l10n_util.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/views/event_utils.h"
#include "chrome/browser/views/location_bar_view.h"
#include "grit/generated_resources.h"

////////////////////////////////////////////////////////////////////////////////
// GoButton, public:

GoButton::GoButton(LocationBarView* location_bar, Browser* browser)
    : ToggleImageButton(this),
      button_delay_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(stop_timer_(this)),
      location_bar_(location_bar),
      browser_(browser),
      intended_mode_(MODE_GO),
      visible_mode_(MODE_GO) {
  DCHECK(location_bar_);
  set_triggerable_event_flags(views::Event::EF_LEFT_BUTTON_DOWN |
                           views::Event::EF_MIDDLE_BUTTON_DOWN);
}

GoButton::~GoButton() {
  stop_timer_.RevokeAll();
}

void GoButton::ChangeMode(Mode mode, bool force) {
  intended_mode_ = mode;

  // If the change is forced, or the user isn't hovering the icon, or it's safe
  // to change it to the other image type, make the change immediately;
  // otherwise we'll let it happen later.
  if (force || (state() != BS_HOT) || ((mode == MODE_STOP) ?
      stop_timer_.empty() : (visible_mode_ != MODE_STOP))) {
    stop_timer_.RevokeAll();
    SetToggled(mode == MODE_STOP);
    visible_mode_ = mode;
  }
}

////////////////////////////////////////////////////////////////////////////////
// GoButton, views::ButtonListener implementation:

void GoButton::ButtonPressed(views::Button* button) {
  if (visible_mode_ == MODE_STOP) {
    browser_->Stop();

    // The user has clicked, so we can feel free to update the button,
    // even if the mouse is still hovering.
    ChangeMode(MODE_GO, true);
  } else if (visible_mode_ == MODE_GO && stop_timer_.empty()) {
    // If the go button is visible and not within the double click timer, go.
    browser_->Go(event_utils::DispositionFromEventFlags(mouse_event_flags()));

    // Figure out the system double-click time.
    if (button_delay_ == 0) {
#if defined(OS_WIN)
      button_delay_ = GetDoubleClickTime();
#else
      NOTIMPLEMENTED();
      button_delay_ = 500;
#endif
    }

    // Stop any existing timers.
    stop_timer_.RevokeAll();

    // Start a timer - while this timer is running, the go button
    // cannot be changed to a stop button. We do not set intended_mode_
    // to MODE_STOP here as we want to wait for the browser to tell
    // us that it has started loading (and this may occur only after
    // some delay).
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        stop_timer_.NewRunnableMethod(&GoButton::OnButtonTimer),
        button_delay_);
  }
}

////////////////////////////////////////////////////////////////////////////////
// GoButton, View overrides:

void GoButton::OnMouseExited(const views::MouseEvent& e) {
  ChangeMode(intended_mode_, true);
  if (state() != BS_DISABLED)
    SetState(BS_NORMAL);
}

bool GoButton::GetTooltipText(int x, int y, std::wstring* tooltip) {
  if (visible_mode_ == MODE_STOP) {
    tooltip->assign(l10n_util::GetString(IDS_TOOLTIP_STOP));
    return true;
  }

  std::wstring current_text(location_bar_->location_entry()->GetText());
  if (current_text.empty())
    return false;

  // Need to make sure the text direction is adjusted based on the locale so
  // that pure LTR strings are displayed appropriately on RTL locales. For
  // example, not adjusting the string will cause the URL
  // "http://www.google.com/" to be displayed in the tooltip as
  // "/http://www.google.com".
  //
  // Note that we mark the URL's text as LTR (instead of examining the
  // characters and guessing the text directionality) since URLs are always
  // treated as left-to-right text, even when they contain RTL characters.
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
    l10n_util::WrapStringWithLTRFormatting(&current_text);

  AutocompleteEditModel* edit_model = location_bar_->location_entry()->model();
  if (edit_model->CurrentTextIsURL()) {
    tooltip->assign(l10n_util::GetStringF(IDS_TOOLTIP_GO_SITE, current_text));
  } else {
    std::wstring keyword(edit_model->keyword());
    TemplateURLModel* template_url_model =
        location_bar_->profile()->GetTemplateURLModel();
    const TemplateURL* provider =
        (keyword.empty() || edit_model->is_keyword_hint()) ?
        template_url_model->GetDefaultSearchProvider() :
        template_url_model->GetTemplateURLForKeyword(keyword);
    if (!provider)
        return false;
    tooltip->assign(l10n_util::GetStringF(IDS_TOOLTIP_GO_SEARCH,
        provider->AdjustedShortNameForLocaleDirection(), current_text));
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// GoButton, private:

void GoButton::OnButtonTimer() {
  stop_timer_.RevokeAll();
  ChangeMode(intended_mode_, true);
}
