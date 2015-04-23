// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/go_button_gtk.h"

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/location_bar_view_gtk.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

GoButtonGtk::GoButtonGtk(LocationBarViewGtk* location_bar, Browser* browser)
    : location_bar_(location_bar),
      browser_(browser),
      button_delay_(0),
      stop_timer_(this),
      intended_mode_(MODE_GO),
      visible_mode_(MODE_GO),
      state_(BS_NORMAL),
      go_(browser ? browser->profile()->GetThemeProvider() : NULL,
          IDR_GO, IDR_GO_P, IDR_GO_H, 0),
      stop_(browser ? browser->profile()->GetThemeProvider() : NULL,
            IDR_STOP, IDR_STOP_P, IDR_STOP_H, 0),
      widget_(gtk_button_new()) {
  gtk_widget_set_size_request(widget_.get(),
                              gdk_pixbuf_get_width(go_.pixbufs(0)),
                              gdk_pixbuf_get_height(go_.pixbufs(0)));

  gtk_widget_set_app_paintable(widget_.get(), TRUE);
  // We effectively double-buffer by virtue of having only one image...
  gtk_widget_set_double_buffered(widget_.get(), FALSE);

  g_signal_connect(G_OBJECT(widget_.get()), "expose-event",
                   G_CALLBACK(OnExpose), this);
  g_signal_connect(G_OBJECT(widget_.get()), "enter",
                   G_CALLBACK(OnEnter), this);
  g_signal_connect(G_OBJECT(widget_.get()), "leave",
                   G_CALLBACK(OnLeave), this);
  g_signal_connect(G_OBJECT(widget_.get()), "clicked",
                   G_CALLBACK(OnClicked), this);
  GTK_WIDGET_UNSET_FLAGS(widget_.get(), GTK_CAN_FOCUS);

  SetTooltip();
}

GoButtonGtk::~GoButtonGtk() {
  widget_.Destroy();
}

void GoButtonGtk::ChangeMode(Mode mode, bool force) {
  intended_mode_ = mode;

  // If the change is forced, or the user isn't hovering the icon, or it's safe
  // to change it to the other image type, make the change immediately;
  // otherwise we'll let it happen later.
  if (force || (state() != BS_HOT) || ((mode == MODE_STOP) ?
      stop_timer_.empty() : (visible_mode_ != MODE_STOP))) {
    stop_timer_.RevokeAll();
    gtk_widget_queue_draw(widget_.get());
    SetTooltip();
    visible_mode_ = mode;
  }
}

Task* GoButtonGtk::CreateButtonTimerTask() {
  return stop_timer_.NewRunnableMethod(&GoButtonGtk::OnButtonTimer);
}

void GoButtonGtk::OnButtonTimer() {
  stop_timer_.RevokeAll();
  ChangeMode(intended_mode_, true);
}

// static
gboolean GoButtonGtk::OnExpose(GtkWidget* widget,
                               GdkEventExpose* e,
                               GoButtonGtk* button) {
  if (button->visible_mode_ == MODE_GO) {
    return button->go_.OnExpose(widget, e);
  } else {
    return button->stop_.OnExpose(widget, e);
  }
}

// static
gboolean GoButtonGtk::OnEnter(GtkButton* widget, GoButtonGtk* button) {
  DCHECK_EQ(BS_NORMAL, button->state());
  button->state_ = BS_HOT;
  return TRUE;
}

// static
gboolean GoButtonGtk::OnLeave(GtkButton* widget, GoButtonGtk* button) {
  // It's possible on shutdown for a "leave" event to be emitted twice in a row
  // for this button.  I'm not sure if this is a gtk quirk or something wrong
  // with our usage, but it's harmless.  I'm commenting out this DCHECK for now.
  // and adding a LOG(WARNING) instead.
  // See http://www.crbug.com/10851 for details.
  // DCHECK_EQ(BS_HOT, button->state());
  if (button->state() != BS_HOT)
    LOG(WARNING) << "Button state should be BS_HOT when leaving.";
  button->state_ = BS_NORMAL;
  button->ChangeMode(button->intended_mode_, true);
  return TRUE;
}

// static
gboolean GoButtonGtk::OnClicked(GtkButton* widget, GoButtonGtk* button) {
  if (button->visible_mode_ == MODE_STOP) {
    if (button->browser_)
      button->browser_->Stop();

    // The user has clicked, so we can feel free to update the button,
    // even if the mouse is still hovering.
    button->ChangeMode(MODE_GO, true);
  } else if (button->visible_mode_ == MODE_GO && button->stop_timer_.empty()) {
    // If the go button is visible and not within the double click timer, go.
    if (button->browser_)
      button->browser_->ExecuteCommand(IDC_GO);

    // Figure out the system double-click time.
    if (button->button_delay_ == 0) {
      GtkSettings* settings = gtk_settings_get_default();
      g_object_get(G_OBJECT(settings),
                   "gtk-double-click-time",
                   &button->button_delay_,
                   NULL);
    }

    // Stop any existing timers.
    button->stop_timer_.RevokeAll();

    // Start a timer - while this timer is running, the go button
    // cannot be changed to a stop button. We do not set intended_mode_
    // to MODE_STOP here as we want to wait for the browser to tell
    // us that it has started loading (and this may occur only after
    // some delay).
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                            button->CreateButtonTimerTask(),
                                            button->button_delay_);
  }

  return TRUE;
}

void GoButtonGtk::SetTooltip() {
  if (visible_mode_ == MODE_GO) {
    // |location_bar_| can be NULL in tests.
    std::wstring current_text(
        location_bar_ ?  location_bar_->location_entry()->GetText() :
        L"");
    if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) {
      l10n_util::WrapStringWithLTRFormatting(&current_text);
    }

    // TODO(pkasting): http://b/868940 Use the right strings at the right
    // times by asking the autocomplete system what to do.  Don't hardcode
    // "Google" as the search provider name.
    gtk_widget_set_tooltip_text(
        widget_.get(),
        true ? l10n_util::GetStringFUTF8(
            IDS_TOOLTIP_GO_SITE, WideToUTF16(current_text)).c_str() :
        l10n_util::GetStringFUTF8(IDS_TOOLTIP_GO_SEARCH, UTF8ToUTF16("Google"),
                                  WideToUTF16(current_text)).c_str());
  } else {
    gtk_widget_set_tooltip_text(
        widget_.get(), l10n_util::GetStringUTF8(IDS_TOOLTIP_STOP).c_str());
  }
}
