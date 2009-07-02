// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/infobar_gtk.h"

#include <gtk/gtk.h>

#include "base/gfx/gtk_util.h"
#include "base/string_util.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/infobar_container_gtk.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/common/gtk_util.h"

namespace {

const double kBackgroundColorTop[3] =
    {255.0 / 255.0, 242.0 / 255.0, 183.0 / 255.0};
const double kBackgroundColorBottom[3] =
    {250.0 / 255.0, 230.0 / 255.0, 145.0 / 255.0};

// Border color (the top pixel of the infobar).
const GdkColor kBorderColor = GDK_COLOR_RGB(0xbe, 0xc8, 0xd4);

// The total height of the info bar.
const int kInfoBarHeight = 37;

// Pixels between infobar elements.
const int kElementPadding = 5;

// Extra padding on either end of info bar.
const int kLeftPadding = 5;
const int kRightPadding = 5;

static gboolean OnBackgroundExpose(GtkWidget* widget, GdkEventExpose* event,
                                   gpointer unused) {
  const int height = widget->allocation.height;

  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  cairo_rectangle(cr, event->area.x, event->area.y,
                  event->area.width, event->area.height);
  cairo_clip(cr);

  cairo_pattern_t* pattern = cairo_pattern_create_linear(0, 0, 0, height);
  cairo_pattern_add_color_stop_rgb(
      pattern, 0.0,
      kBackgroundColorTop[0], kBackgroundColorTop[1], kBackgroundColorTop[2]);
  cairo_pattern_add_color_stop_rgb(
      pattern, 1.0,
      kBackgroundColorBottom[0], kBackgroundColorBottom[1],
      kBackgroundColorBottom[2]);
  cairo_set_source(cr, pattern);
  cairo_paint(cr);
  cairo_pattern_destroy(pattern);

  cairo_destroy(cr);

  return FALSE;
}

}  // namespace

InfoBar::InfoBar(InfoBarDelegate* delegate)
    : container_(NULL),
      delegate_(delegate) {
  // Create |hbox_| and pad the sides.
  hbox_ = gtk_hbox_new(FALSE, kElementPadding);
  GtkWidget* padding = gtk_alignment_new(0, 0, 1, 1);
  gtk_alignment_set_padding(GTK_ALIGNMENT(padding),
      0, 0, kLeftPadding, kRightPadding);

  GtkWidget* bg_box = gtk_event_box_new();
  gtk_widget_set_app_paintable(bg_box, TRUE);
  g_signal_connect(bg_box, "expose-event",
                   G_CALLBACK(OnBackgroundExpose), NULL);
  gtk_container_add(GTK_CONTAINER(padding), hbox_);
  gtk_container_add(GTK_CONTAINER(bg_box), padding);

  border_bin_.Own(gtk_util::CreateGtkBorderBin(bg_box, &kBorderColor,
                                               0, 1, 0, 0));
  gtk_widget_set_size_request(border_bin_.get(), -1, kInfoBarHeight);

  // Add the icon on the left, if any.
  SkBitmap* icon = delegate->GetIcon();
  if (icon) {
    GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(icon);
    GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);
    gtk_box_pack_start(GTK_BOX(hbox_), image, FALSE, FALSE, 0);
  }

  close_button_.reset(CustomDrawButton::CloseButton());
  gtk_util::CenterWidgetInHBox(hbox_, close_button_->widget(), true, 0);
  g_signal_connect(close_button_->widget(), "clicked",
                   G_CALLBACK(OnCloseButton), this);

  slide_widget_.reset(new SlideAnimatorGtk(border_bin_.get(),
                                           SlideAnimatorGtk::DOWN,
                                           0, true, this));
  // We store a pointer back to |this| so we can refer to it from the infobar
  // container.
  g_object_set_data(G_OBJECT(slide_widget_->widget()), "info-bar", this);
}

InfoBar::~InfoBar() {
  border_bin_.Destroy();
}

GtkWidget* InfoBar::widget() {
  return slide_widget_->widget();
}

void InfoBar::AnimateOpen() {
  slide_widget_->Open();
}

void InfoBar::Open() {
  slide_widget_->OpenWithoutAnimation();
  if (border_bin_->window)
    gdk_window_lower(border_bin_->window);
}

void InfoBar::AnimateClose() {
  slide_widget_->Close();
}

void InfoBar::Close() {
  if (delegate_) {
    delegate_->InfoBarClosed();
    delegate_ = NULL;
  }
  delete this;
}

bool InfoBar::IsClosing() {
  return slide_widget_->IsClosing();
}

void InfoBar::RemoveInfoBar() const {
  container_->RemoveDelegate(delegate_);
}

void InfoBar::Closed() {
  Close();
}

// static
void InfoBar::OnCloseButton(GtkWidget* button, InfoBar* info_bar) {
  info_bar->RemoveInfoBar();
}

// AlertInfoBar ----------------------------------------------------------------

class AlertInfoBar : public InfoBar {
 public:
  AlertInfoBar(AlertInfoBarDelegate* delegate)
      : InfoBar(delegate) {
    std::wstring text = delegate->GetMessageText();
    GtkWidget* label = gtk_label_new(WideToUTF8(text).c_str());
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &gfx::kGdkBlack);
    gtk_box_pack_start(GTK_BOX(hbox_), label, FALSE, FALSE, 0);
  }
};

// LinkInfoBar -----------------------------------------------------------------

class LinkInfoBar : public InfoBar {
 public:
  LinkInfoBar(LinkInfoBarDelegate* delegate)
      : InfoBar(delegate) {
    size_t link_offset;
    std::wstring display_text =
        delegate->GetMessageTextWithOffset(&link_offset);
    std::wstring link_text = delegate->GetLinkText();

    // Create the link button.
    GtkWidget* link_button =
        gtk_chrome_link_button_new(WideToUTF8(link_text).c_str());
    g_signal_connect(link_button, "clicked",
                     G_CALLBACK(OnLinkClick), this);

    // If link_offset is npos, we right-align the link instead of embedding it
    // in the text.
    if (link_offset == std::wstring::npos) {
      gtk_box_pack_end(GTK_BOX(hbox_), link_button, FALSE, FALSE, 0);
      GtkWidget* label = gtk_label_new(WideToUTF8(display_text).c_str());
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &gfx::kGdkBlack);
      gtk_box_pack_start(GTK_BOX(hbox_), label, FALSE, FALSE, 0);
    } else {
      GtkWidget* initial_label = gtk_label_new(
          WideToUTF8(display_text.substr(0, link_offset)).c_str());
      GtkWidget* trailing_label = gtk_label_new(
          WideToUTF8(display_text.substr(link_offset)).c_str());

      gtk_widget_modify_fg(initial_label, GTK_STATE_NORMAL, &gfx::kGdkBlack);
      gtk_widget_modify_fg(trailing_label, GTK_STATE_NORMAL, &gfx::kGdkBlack);

      // We don't want any spacing between the elements, so we pack them into
      // this hbox that doesn't use kElementPadding.
      GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
      gtk_box_pack_start(GTK_BOX(hbox), initial_label, FALSE, FALSE, 0);
      gtk_util::CenterWidgetInHBox(hbox, link_button, false, 0);
      gtk_box_pack_start(GTK_BOX(hbox), trailing_label, FALSE, FALSE, 0);
      gtk_box_pack_start(GTK_BOX(hbox_), hbox, FALSE, FALSE, 0);
    }

    gtk_widget_show_all(border_bin_.get());
  }

 private:
  static void OnLinkClick(GtkWidget* button, LinkInfoBar* link_info_bar) {
    const GdkEventButton* button_click_event =
        gtk_chrome_link_button_get_event_for_click(
            GTK_CHROME_LINK_BUTTON(button));
    WindowOpenDisposition disposition = CURRENT_TAB;
    if (button_click_event) {
      disposition = event_utils::DispositionFromEventFlags(
          button_click_event->state);
    }

    if (link_info_bar->delegate_->AsLinkInfoBarDelegate()->
        LinkClicked(disposition)) {
      link_info_bar->RemoveInfoBar();
    }
  }
};

// ConfirmInfoBar --------------------------------------------------------------

class ConfirmInfoBar : public AlertInfoBar {
 public:
  ConfirmInfoBar(ConfirmInfoBarDelegate* delegate)
      : AlertInfoBar(delegate) {
    AddConfirmButton(ConfirmInfoBarDelegate::BUTTON_CANCEL);
    AddConfirmButton(ConfirmInfoBarDelegate::BUTTON_OK);

    gtk_widget_show_all(border_bin_.get());
  }

 private:
  // Adds a button to the info bar by type. It will do nothing if the delegate
  // doesn't specify a button of the given type.
  void AddConfirmButton(ConfirmInfoBarDelegate::InfoBarButton type) {
    if (delegate_->AsConfirmInfoBarDelegate()->GetButtons() & type) {
      GtkWidget* button = gtk_button_new_with_label(WideToUTF8(
          delegate_->AsConfirmInfoBarDelegate()->GetButtonLabel(type)).c_str());
      gtk_util::CenterWidgetInHBox(hbox_, button, true, 0);
      g_signal_connect(button, "clicked",
                       G_CALLBACK(type == ConfirmInfoBarDelegate::BUTTON_OK ?
                                  OnOkButton : OnCancelButton),
                       this);
    }
  }

  static void OnCancelButton(GtkWidget* button, ConfirmInfoBar* info_bar) {
    if (info_bar->delegate_->AsConfirmInfoBarDelegate()->Cancel())
      info_bar->RemoveInfoBar();
  }

  static void OnOkButton(GtkWidget* button, ConfirmInfoBar* info_bar) {
    if (info_bar->delegate_->AsConfirmInfoBarDelegate()->Accept())
      info_bar->RemoveInfoBar();
  }
};

// AlertInfoBarDelegate, InfoBarDelegate overrides: ----------------------------

InfoBar* AlertInfoBarDelegate::CreateInfoBar() {
  return new AlertInfoBar(this);
}

// LinkInfoBarDelegate, InfoBarDelegate overrides: -----------------------------

InfoBar* LinkInfoBarDelegate::CreateInfoBar() {
  return new LinkInfoBar(this);
}

// ConfirmInfoBarDelegate, InfoBarDelegate overrides: --------------------------

InfoBar* ConfirmInfoBarDelegate::CreateInfoBar() {
  return new ConfirmInfoBar(this);
}
