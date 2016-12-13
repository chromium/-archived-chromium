// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"


// static
TabContents* DevToolsWindow::GetDevToolsContents(TabContents* inspected_tab) {
  if (!inspected_tab) {
    return NULL;
  }
  DevToolsClientHost* client_host = DevToolsManager::GetInstance()->
          GetDevToolsClientHostFor(inspected_tab->render_view_host());
  if (!client_host) {
    return NULL;
  }

  DevToolsWindow* window = client_host->AsDevToolsWindow();
  if (!window || !window->is_docked()) {
    return NULL;
  }
  return window->tab_contents();
}

DevToolsWindow::DevToolsWindow(Profile* profile,
                               RenderViewHost* inspected_rvh,
                               bool docked)
    : profile_(profile),
      browser_(NULL),
      inspected_window_(NULL),
      docked_(docked) {
  // Create TabContents with devtools.
  tab_contents_ = new TabContents(profile, NULL, MSG_ROUTING_NONE, NULL);
  GURL url(std::string(chrome::kChromeUIDevToolsURL) + "devtools.html");
  tab_contents_->render_view_host()->AllowBindings(BindingsPolicy::DOM_UI);
  tab_contents_->controller().LoadURL(url, GURL(), PageTransition::START_PAGE);

  // Wipe out page icon so that the default application icon is used.
  NavigationEntry* entry = tab_contents_->controller().GetActiveEntry();
  entry->favicon().set_bitmap(SkBitmap());
  entry->favicon().set_is_valid(true);

  // Register on-load actions.
  registrar_.Add(this,
                 NotificationType::LOAD_STOP,
                 Source<NavigationController>(&tab_contents_->controller()));
  registrar_.Add(this,
                 NotificationType::TAB_CLOSING,
                 Source<NavigationController>(&tab_contents_->controller()));

  inspected_tab_ = inspected_rvh->delegate()->GetAsTabContents();
}

DevToolsWindow::~DevToolsWindow() {
}

DevToolsWindow* DevToolsWindow::AsDevToolsWindow() {
  return this;
}

void DevToolsWindow::SendMessageToClient(const IPC::Message& message) {
  RenderViewHost* target_host = tab_contents_->render_view_host();
  IPC::Message* m =  new IPC::Message(message);
  m->set_routing_id(target_host->routing_id());
  target_host->Send(m);
}

void DevToolsWindow::InspectedTabClosing() {
  if (docked_) {
    // Update dev tools to reflect removed dev tools window.
    inspected_window_->UpdateDevTools();
    // In case of docked tab_contents we own it, so delete here.
    delete tab_contents_;

    delete this;
  } else {
    // First, initiate self-destruct to free all the registrars.
    // Then close all tabs. Browser will take care of deleting tab_contents
    // for us.
    Browser* browser = browser_;
    delete this;
    browser->CloseAllTabs();
  }
}

void DevToolsWindow::Show() {
  if (docked_) {
    // Just tell inspected browser to update splitter.
    inspected_window_ = GetInspectedBrowserWindow();
    if (inspected_window_) {
      tab_contents_->set_delegate(this);
      inspected_window_->UpdateDevTools();
      tab_contents_->view()->SetInitialFocus();
      return;
    } else {
      // Sometimes we don't know where to dock. Stay undocked.
      docked_ = false;
    }
  }

  if (!browser_) {
    CreateDevToolsBrowser();
  }
  browser_->window()->Show();
  tab_contents_->view()->SetInitialFocus();
}

void DevToolsWindow::Activate() {
  if (!docked_ && !browser_->window()->IsActive()) {
    browser_->window()->Activate();
  }
}

void DevToolsWindow::SetDocked(bool docked) {
  if (docked_ == docked) {
    return;
  }
  docked_ = docked;

  if (docked) {
    // Detach window from the external devtools browser. It will lead to
    // the browser object's close and delete. Remove observer first.
    TabStripModel* tabstrip_model = browser_->tabstrip_model();
    tabstrip_model->DetachTabContentsAt(
        tabstrip_model->GetIndexOfTabContents(tab_contents_));
    browser_ = NULL;
  } else {
    // Update inspected window to hide split and reset it.
    inspected_window_->UpdateDevTools();
    inspected_window_ = NULL;
  }
  Show();
}

RenderViewHost* DevToolsWindow::GetRenderViewHost() {
  return tab_contents_->render_view_host();
}

void DevToolsWindow::CreateDevToolsBrowser() {
  // TODO(pfeldman): Make browser's getter for this key static.
  std::wstring wp_key = L"";
  wp_key.append(prefs::kBrowserWindowPlacement);
  wp_key.append(L"_");
  wp_key.append(L"DevToolsApp");

  PrefService* prefs = g_browser_process->local_state();
  if (!prefs->FindPreference(wp_key.c_str())) {
    prefs->RegisterDictionaryPref(wp_key.c_str());
  }

  const DictionaryValue* wp_pref = prefs->GetDictionary(wp_key.c_str());
  if (!wp_pref) {
    DictionaryValue* defaults = prefs->GetMutableDictionary(wp_key.c_str());
    defaults->SetInteger(L"left", 100);
    defaults->SetInteger(L"top", 100);
    defaults->SetInteger(L"right", 740);
    defaults->SetInteger(L"bottom", 740);
    defaults->SetBoolean(L"maximized", false);
    defaults->SetBoolean(L"always_on_top", false);
  }

  browser_ = Browser::CreateForApp(L"DevToolsApp", profile_, false);
  browser_->tabstrip_model()->AddTabContents(
      tab_contents_, -1, false, PageTransition::START_PAGE, true);
}

BrowserWindow* DevToolsWindow::GetInspectedBrowserWindow() {
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end(); ++it) {
    Browser* browser = *it;
    for (int i = 0; i < browser->tab_count(); ++i) {
      TabContents* tab_contents = browser->GetTabContentsAt(i);
      if (tab_contents == inspected_tab_) {
        return browser->window();
      }
    }
  }
  return NULL;
}

void DevToolsWindow::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (type == NotificationType::LOAD_STOP) {
    tab_contents_->render_view_host()->
        ExecuteJavascriptInWebFrame(
            L"", docked_ ? L"WebInspector.setAttachedWindow(true);" :
                           L"WebInspector.setAttachedWindow(false);");
  } else if (type == NotificationType::TAB_CLOSING) {
    if (Source<NavigationController>(source).ptr() ==
            &tab_contents_->controller()) {
      // This happens when browser closes all of its tabs as a result
      // of window.Close event.
      // Notify manager that this DevToolsClientHost no longer exists and
      // initiate self-destuct here.
      NotifyCloseListener();
      delete this;
    }
  }
}
