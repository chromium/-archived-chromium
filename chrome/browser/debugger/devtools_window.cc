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
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"

namespace {

class FloatingWindow : public DevToolsWindow,
                              TabStripModelObserver {
 public:
  FloatingWindow(Profile* profile);
  virtual ~FloatingWindow();
  virtual void Show();
  virtual void InspectedTabClosing();

  // TabStripModelObserver implementation
  virtual void TabClosingAt(TabContents* contents, int index);
  virtual void TabStripEmpty();

 private:
  bool inspected_tab_closing_;
  DISALLOW_COPY_AND_ASSIGN(FloatingWindow);
};

class DockedWindow : public DevToolsWindow,
                            TabContentsDelegate {
 public:
  DockedWindow(Profile* profile, BrowserWindow* window);
  virtual ~DockedWindow();
  virtual void Show();
  virtual void InspectedTabClosing();

 private:
  // Overridden from TabContentsDelegate:
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url,
                              const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition) {}
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) {}
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) {}
  virtual void ActivateContents(TabContents* contents) {}
  virtual void LoadingStateChanged(TabContents* source) {}
  virtual void CloseContents(TabContents* source) {}
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  virtual bool IsPopup(TabContents* source) { return false; }
  virtual void URLStarredChanged(TabContents* source, bool starred) {}
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) {}
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) {}

  BrowserWindow* window_;
  DISALLOW_COPY_AND_ASSIGN(DockedWindow);
};

} //  namespace

// static
DevToolsWindow* DevToolsWindow::CreateDevToolsWindow(
    Profile* profile,
    RenderViewHost* inspected_rvh,
    bool docked) {
  if (docked) {
    BrowserWindow* window = DevToolsWindow::GetBrowserWindow(inspected_rvh);
    if (window) {
      return new DockedWindow(profile, window);
    }
  }
  return new FloatingWindow(profile);
}

// static
TabContents* DevToolsWindow::GetDevToolsContents(TabContents* inspected_tab) {
  if (!inspected_tab) {
    return NULL;
  }
  DevToolsClientHost* client_host =
      DevToolsManager::GetInstance()->
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

DevToolsWindow::DevToolsWindow(bool docked)
    : docked_(docked) {
}

DevToolsWindow::~DevToolsWindow() {
}

DevToolsWindow* DevToolsWindow::AsDevToolsWindow() {
  return this;
}

RenderViewHost* DevToolsWindow::GetRenderViewHost() {
  return tab_contents_->render_view_host();
}

void DevToolsWindow::SendMessageToClient(const IPC::Message& message) {
  RenderViewHost* target_host = tab_contents_->render_view_host();
  IPC::Message* m =  new IPC::Message(message);
  m->set_routing_id(target_host->routing_id());
  target_host->Send(m);
}

void DevToolsWindow::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  tab_contents_->render_view_host()->
      ExecuteJavascriptInWebFrame(
          L"", docked_ ? L"WebInspector.setAttachedWindow(true);" :
                         L"WebInspector.setAttachedWindow(false);");
}

GURL DevToolsWindow::GetContentsUrl() {
  return GURL(std::string(chrome::kChromeUIDevToolsURL) + "devtools.html");
}

void DevToolsWindow::InitTabContents(TabContents* tab_contents) {
  tab_contents_ = tab_contents;
  registrar_.Add(this, NotificationType::LOAD_STOP,
                 Source<NavigationController>(&tab_contents_->controller()));
}

// static
BrowserWindow* DevToolsWindow::GetBrowserWindow(RenderViewHost* rvh) {
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end(); ++it) {
    Browser* browser = *it;
    for (int i = 0; i < browser->tab_count(); ++i) {
      TabContents* tab_contents = browser->GetTabContentsAt(i);
      if (tab_contents->render_view_host() == rvh) {
        return browser->window();
      }
    }
  }
  return NULL;
}

//
// Floating window implementation
//
FloatingWindow::FloatingWindow(Profile* profile)
    : DevToolsWindow(false),
      TabStripModelObserver(),
      inspected_tab_closing_(false) {
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

  browser_ = Browser::CreateForApp(L"DevToolsApp", profile, false);
  browser_->AddTabWithURL(GetContentsUrl(), GURL(), PageTransition::START_PAGE,
                          true, -1, false, NULL);
  TabContents* tab_contents = browser_->GetSelectedTabContents();
  browser_->tabstrip_model()->AddObserver(this);

  // Wipe out page icon so that the default application icon is used.
  NavigationEntry* entry = tab_contents->controller().GetActiveEntry();
  entry->favicon().set_bitmap(SkBitmap());
  entry->favicon().set_is_valid(true);

  InitTabContents(tab_contents);
}

FloatingWindow::~FloatingWindow() {
}

void FloatingWindow::Show() {
  browser_->window()->Show();
  tab_contents_->view()->SetInitialFocus();
}

void FloatingWindow::InspectedTabClosing() {
  inspected_tab_closing_ = true;
  browser_->CloseAllTabs();
}

void FloatingWindow::TabClosingAt(TabContents* contents, int index) {
  if (!inspected_tab_closing_ && contents == tab_contents_) {
    // Notify manager that this DevToolsClientHost no longer exists.
    NotifyCloseListener();
  }
}

void FloatingWindow::TabStripEmpty() {
  delete this;
}

//
// Docked window implementation
//
DockedWindow::DockedWindow(Profile* profile, BrowserWindow* window)
    : DevToolsWindow(true),
      window_(window) {
  TabContents* tab_contents = new TabContents(profile,
      NULL, MSG_ROUTING_NONE, NULL);
  tab_contents->render_view_host()->AllowBindings(BindingsPolicy::DOM_UI);
  tab_contents->controller().LoadURL(GetContentsUrl(), GURL(),
                                      PageTransition::START_PAGE);
  tab_contents->set_delegate(this);
  browser_ = NULL;
  InitTabContents(tab_contents);
}

DockedWindow::~DockedWindow() {
}

void DockedWindow::Show() {
  window_->UpdateDevTools();
  tab_contents_->view()->SetInitialFocus();
}

void DockedWindow::InspectedTabClosing() {
  window_->UpdateDevTools();
  delete this;
}
