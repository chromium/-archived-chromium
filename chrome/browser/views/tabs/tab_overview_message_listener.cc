// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/tab_overview_message_listener.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/views/new_browser_window_widget.h"
#include "chrome/browser/views/tabs/tab_overview_controller.h"
#include "chrome/common/x11_util.h"

// static
TabOverviewMessageListener* TabOverviewMessageListener::instance() {
  static TabOverviewMessageListener* instance = NULL;
  if (!instance) {
    instance = Singleton<TabOverviewMessageListener>::get();
    MessageLoopForUI::current()->AddObserver(instance);
  }
  return instance;
}

void TabOverviewMessageListener::WillProcessEvent(GdkEvent* event) {
}

void TabOverviewMessageListener::DidProcessEvent(GdkEvent* event) {
  if (event->type == GDK_CLIENT_EVENT) {
    TabOverviewTypes::Message message;
    GdkEventClient* client_event = reinterpret_cast<GdkEventClient*>(event);
    if (TabOverviewTypes::instance()->DecodeMessage(*client_event, &message))
      ProcessMessage(message, client_event->window);
  }
}

TabOverviewMessageListener::TabOverviewMessageListener() {
}

TabOverviewMessageListener::~TabOverviewMessageListener() {
}

void TabOverviewMessageListener::ProcessMessage(
    const TabOverviewTypes::Message& message,
    GdkWindow* window) {
  switch (message.type()) {
    case TabOverviewTypes::Message::CHROME_SET_TAB_SUMMARY_VISIBILITY: {
      if (message.param(0) == 0) {
        HideOverview();
      } else {
        BrowserWindowGtk* browser_window =
            BrowserWindowGtk::GetBrowserWindowForNativeWindow(
                BrowserWindowGtk::GetBrowserWindowForXID(
                    x11_util::GetX11WindowFromGdkWindow(window)));
        if (browser_window)
          ShowOverview(browser_window->browser(), message.param(1));
        else
          HideOverview();
      }
      break;
    }

    case TabOverviewTypes::Message::CHROME_NOTIFY_LAYOUT_MODE: {
      if (message.param(0) == 0) {
        new_browser_window_.reset(NULL);
        controller_.reset(NULL);
      } else if (BrowserList::size() > 0) {
        Browser* browser = *BrowserList::begin();
        controller_.reset(new TabOverviewController(
                              browser->window()->GetNormalBounds().origin()));
        new_browser_window_.reset(
            new NewBrowserWindowWidget(browser->profile()));
      }
      break;
    }

    case TabOverviewTypes::Message::CHROME_NOTIFY_FLOATING_TAB_OVER_TOPLEVEL: {
      if (!controller_.get())
        return;

      bool over_mini_window = message.param(1) == 1;
      controller_->SetMouseOverMiniWindow(over_mini_window);
      if (!over_mini_window)
        return;

      // Not over a mini-window, make sure the controller is showing the
      // contents of the browser the mouse is over.
      BrowserWindowGtk* browser_window =
          BrowserWindowGtk::GetBrowserWindowForNativeWindow(
              BrowserWindowGtk::GetBrowserWindowForXID(message.param(0)));
      if (!browser_window)
        return;

      if (controller_->browser()->window() == browser_window)
        return;

      TabOverviewTypes::Message select_message;
      select_message.set_type(TabOverviewTypes::Message::WM_MOVE_FLOATING_TAB);
      select_message.set_param(0, message.param(1));
      TabOverviewTypes::instance()->SendMessage(select_message);
    }

    default:
      break;
  }
}

void TabOverviewMessageListener::ShowOverview(Browser* browser,
                                              int horizontal_center) {
  if (!controller_.get()) {
    controller_.reset(new TabOverviewController(
                          browser->window()->GetNormalBounds().origin()));
  }
  controller_->SetBrowser(browser, horizontal_center);
  controller_->Show();
}

void TabOverviewMessageListener::HideOverview() {
  controller_->SetBrowser(NULL, -1);
}
