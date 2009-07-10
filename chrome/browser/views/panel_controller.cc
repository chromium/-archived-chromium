// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/panel_controller.h"

#include <gdk/gdkx.h>
extern "C" {
#include <X11/Xlib.h>
}

#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/views/tabs/tab_overview_types.h"
#include "chrome/common/x11_util.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/image_button.h"
#include "views/controls/label.h"
#include "views/event.h"
#include "views/view.h"
#include "views/widget/widget_gtk.h"

static int close_button_width;
static int close_button_height;
static SkBitmap* close_button_n;
static SkBitmap* close_button_h;
static SkBitmap* close_button_p;
static gfx::Font* title_font = NULL;

namespace {

const int kTitleWidth = 200;
const int kTitleHeight = 24;
const int kTitlePad = 8;
const int kButtonPad = 8;

static bool resources_initialized;
static void InitializeResources() {
  if (resources_initialized) {
    return;
  }

  resources_initialized = true;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  title_font = new gfx::Font(rb.GetFont(ResourceBundle::BaseFont));
  close_button_n = rb.GetBitmapNamed(IDR_TAB_CLOSE);
  close_button_h = rb.GetBitmapNamed(IDR_TAB_CLOSE_H);
  close_button_p = rb.GetBitmapNamed(IDR_TAB_CLOSE_P);
  close_button_width = close_button_n->width();
  close_button_height = close_button_n->height();
}

}  // namespace

PanelController::PanelController(BrowserWindowGtk* browser_window)
    :  browser_window_(browser_window),
       panel_(browser_window->window()),
       panel_xid_(x11_util::GetX11WindowFromGtkWidget(GTK_WIDGET(panel_))),
       expanded_(true),
       mouse_down_(false),
       dragging_(false) {
  title_window_ = new views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW);
  gfx::Rect title_bounds(
      0, 0, browser_window->GetNormalBounds().width(), kTitleHeight);
  title_window_->Init(NULL, title_bounds);
  title_ = title_window_->GetNativeView();
  title_xid_ = x11_util::GetX11WindowFromGtkWidget(title_);

  TabOverviewTypes* tab_overview = TabOverviewTypes::instance();
  tab_overview->SetWindowType(
      title_,
      TabOverviewTypes::WINDOW_TYPE_CHROME_PANEL_TITLEBAR,
      NULL);
  std::vector<int> type_params;
  type_params.push_back(title_xid_);
  type_params.push_back(expanded_ ? 1 : 0);
  tab_overview->SetWindowType(
      GTK_WIDGET(panel_),
      TabOverviewTypes::WINDOW_TYPE_CHROME_PANEL,
      &type_params);

  g_signal_connect(
      panel_, "client-event", G_CALLBACK(OnPanelClientEvent), this);

  title_content_ = new TitleContentView(this);
  title_window_->SetContentsView(title_content_);
  title_window_->Show();
}

void PanelController::UpdateTitleBar() {
  title_content_->title_label()->SetText(
      UTF16ToWideHack(browser_window_->browser()->GetCurrentPageTitle()));
}

bool PanelController::TitleMousePressed(const views::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton()) {
    return false;
  }
  gfx::Point abs_location = event.location();
  views::View::ConvertPointToScreen(title_content_, &abs_location);
  mouse_down_ = true;
  mouse_down_abs_x_ = abs_location.x();
  mouse_down_abs_y_ = abs_location.y();
  mouse_down_offset_x_ = event.x();
  mouse_down_offset_y_ = event.y();
  dragging_ = false;
  return true;
}

void PanelController::TitleMouseReleased(
    const views::MouseEvent& event, bool canceled) {
  if (!event.IsOnlyLeftMouseButton()) {
    return;
  }
  // Only handle clicks that started in our window.
  if (!mouse_down_) {
    return;
  }

  mouse_down_ = false;
  if (!dragging_) {
    TabOverviewTypes::Message msg(
        TabOverviewTypes::Message::WM_SET_PANEL_STATE);
    msg.set_param(0, panel_xid_);
    msg.set_param(1, expanded_ ? 0 : 1);
    TabOverviewTypes::instance()->SendMessage(msg);
  } else {
    TabOverviewTypes::Message msg(
        TabOverviewTypes::Message::WM_NOTIFY_PANEL_DRAG_COMPLETE);
    msg.set_param(0, panel_xid_);
    TabOverviewTypes::instance()->SendMessage(msg);
    dragging_ = false;
  }
}

bool PanelController::TitleMouseDragged(const views::MouseEvent& event) {
  if (!mouse_down_) {
    return false;
  }

  gfx::Point abs_location = event.location();
  views::View::ConvertPointToScreen(title_content_, &abs_location);
  if (!dragging_) {
    if (views::View::ExceededDragThreshold(
        abs_location.x() - mouse_down_abs_x_,
        abs_location.y() - mouse_down_abs_y_)) {
      dragging_ = true;
    }
  }
  if (dragging_) {
    TabOverviewTypes::Message msg(TabOverviewTypes::Message::WM_MOVE_PANEL);
    msg.set_param(0, panel_xid_);
    msg.set_param(1, abs_location.x() - mouse_down_offset_x_);
    msg.set_param(2, abs_location.y() - mouse_down_offset_y_);
    TabOverviewTypes::instance()->SendMessage(msg);
  }
  return true;
}

// static
bool PanelController::OnPanelClientEvent(
    GtkWidget* widget,
    GdkEventClient* event,
    PanelController* panel_controller) {
  return panel_controller->PanelClientEvent(event);
}

bool PanelController::PanelClientEvent(GdkEventClient* event) {
  TabOverviewTypes::Message msg;
  TabOverviewTypes::instance()->DecodeMessage(*event, &msg);
  if (msg.type() == TabOverviewTypes::Message::CHROME_NOTIFY_PANEL_STATE) {
    expanded_ = msg.param(0);
  }
  return true;
}

void PanelController::Close() {
  title_window_->Close();
}

void PanelController::ButtonPressed(views::Button* sender) {
  if (sender == title_content_->close_button()) {
    browser_window_->Close();
  }
}

PanelController::TitleContentView::TitleContentView(
    PanelController* panel_controller)
        : panel_controller_(panel_controller) {
  InitializeResources();
  close_button_ = new views::ImageButton(panel_controller_);
  close_button_->SetImage(views::CustomButton::BS_NORMAL, close_button_n);
  close_button_->SetImage(views::CustomButton::BS_HOT, close_button_h);
  close_button_->SetImage(views::CustomButton::BS_PUSHED, close_button_p);
  AddChildView(close_button_);

  title_label_ = new views::Label(std::wstring(), *title_font);
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(title_label_);

  set_background(views::Background::CreateSolidBackground(0xdd, 0xdd, 0xdd, 1));
}

void PanelController::TitleContentView::Layout() {
  close_button_->SetBounds(
      bounds().width() - (close_button_width + kButtonPad),
      (bounds().height() - close_button_height) / 2,
      close_button_width,
      close_button_height);
  title_label_->SetBounds(
      kTitlePad,
      0,
      bounds().width() - (kTitlePad + close_button_width + 2 * kButtonPad),
      bounds().height());
}

bool PanelController::TitleContentView::OnMousePressed(
    const views::MouseEvent& event) {
  return panel_controller_->TitleMousePressed(event);
}

void PanelController::TitleContentView::OnMouseReleased(
    const views::MouseEvent& event, bool canceled) {
  return panel_controller_->TitleMouseReleased(event, canceled);
}

bool PanelController::TitleContentView::OnMouseDragged(
    const views::MouseEvent& event) {
  return panel_controller_->TitleMouseDragged(event);
}

