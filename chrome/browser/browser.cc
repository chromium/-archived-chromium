// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"

#include <windows.h>
#include <shellapi.h>

#include "base/command_line.h"
#include "base/idle_timer.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/browser_url_handler.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/debugger/debugger_window.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/browser/frame_util.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/plugin_process_host.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/ssl_error_info.h"
#include "chrome/browser/site_instance.h"
#include "chrome/browser/url_fixer_upper.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/download_shelf_view.h"
#include "chrome/browser/views/go_button.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/browser/views/html_dialog_view.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/browser/views/status_bubble.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/browser/views/toolbar_star_toggle.h"
#include "chrome/browser/web_contents_view.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/win_util.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_policy.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domain.h"

#include "chromium_strings.h"
#include "generated_resources.h"

using base::TimeDelta;

static BrowserList g_browserlist;

// How long we wait before updating the browser chrome while loading a page.
static const int kUIUpdateCoalescingTimeMS = 200;

// Idle time before helping prune memory consumption.
static const int kBrowserReleaseMemoryInterval = 30;  // In seconds.

// How much horizontal and vertical offset there is between newly opened
// windows.
static const int kWindowTilePixels = 20;

// How frequently we check for hung plugin windows.
static const int kDefaultHungPluginDetectFrequency = 2000;

// How long do we wait before we consider a window hung (in ms).
static const int kDefaultPluginMessageResponseTimeout = 30000;

////////////////////////////////////////////////////////////////////////////////

// A task to reduce the working set of the plugins.
class ReducePluginsWorkingSetTask : public Task {
 public:
  virtual void Run() {
    for (PluginProcessHostIterator iter; !iter.Done(); ++iter) {
      PluginProcessHost* plugin = const_cast<PluginProcessHost*>(*iter);
      DCHECK(plugin->process());
      Process process(plugin->process());
      process.ReduceWorkingSet();
    }
  }
};

// A browser task to run when the user is not using the browser.
// In our case, we're trying to be nice to the operating system and release
// memory not in use.
class BrowserIdleTimer : public base::IdleTimer {
 public:
  BrowserIdleTimer()
      : base::IdleTimer(TimeDelta::FromSeconds(kBrowserReleaseMemoryInterval),
                        false) {
  }

  virtual void OnIdle() {
    // We're idle.  Release browser and renderer unused pages.

    // Handle the Browser.
    Process process(GetCurrentProcess());
    process.ReduceWorkingSet();

    // Handle the Renderer(s).
    RenderProcessHost::iterator renderer_iter;
    for (renderer_iter = RenderProcessHost::begin(); renderer_iter !=
       RenderProcessHost::end(); renderer_iter++) {
       Process process(renderer_iter->second->process());
       process.ReduceWorkingSet();
    }

    // Handle the Plugin(s).  We need to iterate through the plugin processes on
    // the IO thread because that thread manages the plugin process collection.
    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        new ReducePluginsWorkingSetTask());
  }
};

////////////////////////////////////////////////////////////////////////////////

struct Browser::UIUpdate {
  UIUpdate(const TabContents* src, unsigned flags)
      : source(src),
        changed_flags(flags) {
  }

  // The source of the update.
  const TabContents* source;

  // What changed in the UI.
  unsigned changed_flags;
};

////////////////////////////////////////////////////////////////////////////////

// static
Browser* Browser::GetBrowserForController(
    const NavigationController* controller, int* index_result) {
  BrowserList::const_iterator it;
  for (it = BrowserList::begin(); it != BrowserList::end(); ++it) {
    int index = (*it)->tabstrip_model_.GetIndexOfController(controller);
    if (index != TabStripModel::kNoTab) {
      if (index_result)
        *index_result = index;
      return *it;
    }
  }

  return NULL;
}

// static
void Browser::OpenNewBrowserWindow(Profile* profile, int show_command) {
  Browser* browser = new Browser(gfx::Rect(), show_command, profile,
                                 BrowserType::TABBED_BROWSER, L"");
  browser->AddBlankTab(true);
  browser->Show();
}

// static
void Browser::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kPluginMessageResponseTimeout,
      kDefaultPluginMessageResponseTimeout);
  prefs->RegisterIntegerPref(prefs::kHungPluginDetectFrequency,
      kDefaultHungPluginDetectFrequency);
  prefs->RegisterDictionaryPref(prefs::kBrowserWindowPlacement);
  prefs->RegisterIntegerPref(prefs::kOptionsWindowLastTabIndex, 0);
}

// static
void Browser::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kHomePage, L"chrome-internal:");
  prefs->RegisterBooleanPref(prefs::kHomePageIsNewTabPage, true);
  prefs->RegisterIntegerPref(prefs::kCookieBehavior,
      net::CookiePolicy::ALLOW_ALL_COOKIES);
  prefs->RegisterBooleanPref(prefs::kShowHomeButton, false);
  prefs->RegisterStringPref(prefs::kRecentlySelectedEncoding, L"");
  // TODO(peterson): bug #3870 move this to the AutofillManager once it is
  //                 checked-in.
  prefs->RegisterBooleanPref(prefs::kFormAutofillEnabled, true);
  prefs->RegisterBooleanPref(prefs::kDeleteBrowsingHistory, true);
  prefs->RegisterBooleanPref(prefs::kDeleteDownloadHistory, true);
  prefs->RegisterBooleanPref(prefs::kDeleteCache, true);
  prefs->RegisterBooleanPref(prefs::kDeleteCookies, true);
  prefs->RegisterBooleanPref(prefs::kDeletePasswords, false);
  prefs->RegisterBooleanPref(prefs::kDeleteFormData, true);
  prefs->RegisterIntegerPref(prefs::kDeleteTimePeriod, 0);
}

Browser::Browser(const gfx::Rect& initial_bounds,
                 int show_command,
                 Profile* profile,
                 BrowserType::Type type,
                 const std::wstring& app_name)
    : profile_(profile),
      window_(NULL),
      initial_show_command_(show_command),
      is_attempting_to_close_browser_(false),
      controller_(this),
      chrome_updater_factory_(this),
      method_factory_(this),
      hung_window_detector_(&hung_plugin_action_),
      ticker_(0),
      tabstrip_model_(this, profile),
      toolbar_model_(this),
      type_(type),
      app_name_(app_name),
      idle_task_(new BrowserIdleTimer()) {
  tabstrip_model_.AddObserver(this);

  CommandLine parsed_command_line;

  gfx::Rect create_bounds;
  bool maximized = false;
  WindowSizer::GetBrowserWindowBounds(app_name_, initial_bounds,
                                      &create_bounds, &maximized);
  if (parsed_command_line.HasSwitch(switches::kStartMaximized))
    maximized = true;
  if (maximized)
    initial_show_command_ = SW_SHOWMAXIMIZED;
  window_ = BrowserWindow::CreateBrowserWindow(this, create_bounds,
                                               show_command);

  // Start a hung plugin window detector for this browser object (as long as
  // hang detection is not disabled).
  if (!parsed_command_line.HasSwitch(switches::kDisableHangMonitor))
    InitHangMonitor();

  NotificationService::current()->AddObserver(
      this, NOTIFY_SSL_STATE_CHANGED, NotificationService::AllSources());

  InitCommandState();
  BrowserList::AddBrowser(this);

  encoding_auto_detect_.Init(prefs::kWebKitUsesUniversalDetector,
                             profile_->GetPrefs(), NULL);

  // Trim browser memory on idle for low & medium memory models.
  if (g_browser_process->memory_model() < BrowserProcess::HIGH_MEMORY_MODEL)
    idle_task_->Start();

  // Show the First Run information bubble if we've been told to.
  PrefService* local_state = g_browser_process->local_state();
  if (local_state->IsPrefRegistered(prefs::kShouldShowFirstRunBubble) &&
      local_state->GetBoolean(prefs::kShouldShowFirstRunBubble)) {
    // Reset the preference so we don't show the bubble for subsequent windows.
    local_state->ClearPref(prefs::kShouldShowFirstRunBubble);
    GetLocationBarView()->ShowFirstRunBubble();
  }
}

Browser::~Browser() {
  // The tab strip should be empty at this point.
  DCHECK(tabstrip_model_.empty());
  tabstrip_model_.RemoveObserver(this);

  BrowserList::RemoveBrowser(this);

  if (!BrowserList::HasBrowserWithProfile(profile_)) {
    // We're the last browser window with this profile. We need to nuke the
    // TabRestoreService, which will start the shutdown of the
    // NavigationControllers and allow for proper shutdown. If we don't do this
    // chrome won't shutdown cleanly, and may end up crashing when some
    // thread tries to use the IO thread (or another thread) that is no longer
    // valid.
    profile_->ResetTabRestoreService();
  }

  SessionService* session_service = profile_->GetSessionService();
  if (session_service)
    session_service->WindowClosed(session_id_);

  NotificationService::current()->RemoveObserver(
      this, NOTIFY_SSL_STATE_CHANGED, NotificationService::AllSources());

  // Stop hung plugin monitoring.
  ticker_.Stop();
  ticker_.UnregisterTickHandler(&hung_window_detector_);

  if (profile_->IsOffTheRecord() &&
      !BrowserList::IsOffTheRecordSessionActive()) {
    // We reuse the OTR cookie store across OTR windows.  If the last OTR
    // window is closed, then we want to wipe the cookie store clean, so when
    // an OTR window is open again, it starts with an empty cookie store.  This
    // also frees up the memory that the OTR cookies were using.  OTR never
    // loads or writes persistent cookies (there is no backing store), so we
    // can just delete all of the cookies in the store.
    profile_->GetRequestContext()->cookie_store()->DeleteAll(false);
  }

  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void Browser::ShowAndFit(bool resize_to_fit) {
  // Only allow one call after the browser is created.
  if (initial_show_command_ < 0) {
    // The frame is already visible, we're being invoked again either by the
    // user clicking a link in another app or from a desktop shortcut.
    window_->Activate();
    return;
  }
  window_->Show(initial_show_command_, resize_to_fit);
  if ((initial_show_command_ == SW_SHOWNORMAL) ||
      (initial_show_command_ == SW_SHOWMAXIMIZED))
    window_->Activate();
  initial_show_command_ = -1;

  // Setting the focus doesn't work when the window is invisible, so any focus
  // initialization that happened before this will be lost.
  //
  // We really "should" restore the focus whenever the window becomes unhidden,
  // but I think initializing is the only time where this can happen where there
  // is some focus change we need to pick up, and this is easier than plumbing
  // through an unhide message all the way from the frame.
  //
  // If we do find there are cases where we need to restore the focus on show,
  // that should be added and this should be removed.
  TabContents* selected_tab_contents = GetSelectedTabContents();
  if (selected_tab_contents)
    selected_tab_contents->RestoreFocus();
}

void Browser::CloseFrame() {
  window_->Close();
}

GURL Browser::GetHomePage() {
  if (profile_->GetPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage)) {
    return NewTabUIURL();
  } else {
    GURL home_page = GURL(URLFixerUpper::FixupURL(
        profile_->GetPrefs()->GetString(prefs::kHomePage),
        std::wstring()));
    if (!home_page.is_valid())
      return NewTabUIURL();

    return home_page;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Event Handlers

void Browser::WindowActivationChanged(bool is_active) {
  if (is_active)
    BrowserList::SetLastActive(this);
}

////////////////////////////////////////////////////////////////////////////////
// Toolbar creation, management

LocationBarView* Browser::GetLocationBarView() const {
  return window_->GetLocationBarView();
}

////////////////////////////////////////////////////////////////////////////////
// Chrome update coalescing

void Browser::UpdateToolBar(bool should_restore_state) {
  window_->UpdateToolbar(GetSelectedTabContents(), should_restore_state);
}

void Browser::ScheduleUIUpdate(const TabContents* source,
                               unsigned changed_flags) {
  // Synchronously update the URL.
  if (changed_flags & TabContents::INVALIDATE_URL &&
      source == GetSelectedTabContents()) {
    // Only update the URL for the current tab. Note that we do not update
    // the navigation commands since those would have already been updated
    // synchronously by NavigationStateChanged.
    UpdateToolBar(false);

    if (changed_flags == TabContents::INVALIDATE_URL)
      return;  // Just had an update URL and nothing else.
  }

  // Save the dirty bits.
  scheduled_updates_.push_back(UIUpdate(source, changed_flags));

  if (chrome_updater_factory_.empty()) {
    // No task currently scheduled, start another.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        chrome_updater_factory_.NewRunnableMethod(
            &Browser::ProcessPendingUIUpdates),
            kUIUpdateCoalescingTimeMS);
  }
}

void Browser::ProcessPendingUIUpdates() {
#ifndef NDEBUG
  // Validate that all tabs we have pending updates for exist. This is scary
  // because the pending list must be kept in sync with any detached or
  // deleted tabs. This code does not dereference any TabContents pointers.
  for (size_t i = 0; i < scheduled_updates_.size(); i++) {
    bool found = false;
    for (int tab = 0; tab < tab_count(); tab++) {
      if (GetTabContentsAt(tab)->controller() ==
          scheduled_updates_[i].source->controller()) {
        found = true;
        break;
      }
    }
    DCHECK(found);
  }
#endif

  chrome_updater_factory_.RevokeAll();

  // We could have many updates for the same thing in the queue. This map tracks
  // the bits of the stuff we've already updated for each TabContents so we
  // don't update again.
  typedef std::map<const TabContents*, unsigned> UpdateTracker;
  UpdateTracker updated_stuff;

  for (size_t i = 0; i < scheduled_updates_.size(); i++) {
    // Do not dereference |contents|, it may be out-of-date!
    const TabContents* contents = scheduled_updates_[i].source;
    unsigned flags = scheduled_updates_[i].changed_flags;

    // Remove any bits we have already updated, and save the new bits.
    UpdateTracker::iterator updated = updated_stuff.find(contents);
    if (updated != updated_stuff.end()) {
      // Turn off bits already set.
      flags &= ~updated->second;
      if (!flags)
        continue;

      updated->second |= flags;
    } else {
      updated_stuff[contents] = flags;
    }

    // Updates to the title or favicon require a tab repaint. However, the
    // inverse is not true since updates to the title also update the window
    // title.
    bool invalidate_tab = false;
    if (flags & TabContents::INVALIDATE_TITLE ||
        flags & TabContents::INVALIDATE_FAVICON) {
      invalidate_tab = true;

      // Anything that repaints the tab means the favicon is updated.
      updated_stuff[contents] |= TabContents::INVALIDATE_FAVICON;
    }

    // Updating the URL happens synchronously in ScheduleUIUpdate.

    if (flags & TabContents::INVALIDATE_LOAD)
      GetStatusBubble()->SetStatus(GetSelectedTabContents()->GetStatusText());

    if (invalidate_tab) {  // INVALIDATE_TITLE or INVALIDATE_FAVICON.
      tabstrip_model_.UpdateTabContentsStateAt(
          tabstrip_model_.GetIndexOfController(contents->controller()));
      window_->UpdateTitleBar();

      if (contents == GetSelectedTabContents()) {
        TabContents* current_tab = GetSelectedTabContents();
        controller_.UpdateCommandEnabled(IDC_CREATE_SHORTCUT,
            current_tab->type() == TAB_CONTENTS_WEB &&
            !current_tab->GetFavIcon().isNull());
      }
    }

    // We don't need to process INVALIDATE_STATE, since that's not visible.
  }

  scheduled_updates_.clear();
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsDelegate

void Browser::OpenURLFromTab(TabContents* source,
                             const GURL& url, const GURL& referrer,
                             WindowOpenDisposition disposition,
                             PageTransition::Type transition) {
  // No code for these yet
  DCHECK((disposition != NEW_POPUP) && (disposition != SAVE_TO_DISK));

  TabContents* current_tab = source ? source : GetSelectedTabContents();
  bool source_tab_was_frontmost = (current_tab == GetSelectedTabContents());
  TabContents* new_contents = NULL;

  // If the URL is part of the same web site, then load it in the same
  // SiteInstance (and thus the same process).  This is an optimization to
  // reduce process overhead; it is not necessary for compatibility.  (That is,
  // the new tab will not have script connections to the previous tab, so it
  // does not need to be part of the same SiteInstance or BrowsingInstance.)
  // Default to loading in a new SiteInstance and BrowsingInstance.
  // TODO(creis): should this apply to applications?
  SiteInstance* instance = NULL;
  // Don't use this logic when "--process-per-tab" is specified.
  if (!CommandLine().HasSwitch(switches::kProcessPerTab)) {
    if (current_tab) {
      const WebContents* const web_contents = current_tab->AsWebContents();
      if (web_contents) {
        const GURL& current_url = web_contents->GetURL();
        if (SiteInstance::IsSameWebSite(current_url, url))
          instance = web_contents->GetSiteInstance();
      }
    }
  }

  // If this is an application we can only have one tab so a new tab always
  // goes into a tabbed browser window.
  if (disposition != NEW_WINDOW && type_ == BrowserType::APPLICATION) {
    // If the disposition is OFF_THE_RECORD we don't want to create a new
    // browser that will itself create another OTR browser. This will result in
    // a browser leak (and crash below because no tab is created or selected).
    if (disposition == OFF_THE_RECORD) {
      OpenURLOffTheRecord(profile_, url);
      return;
    }

    Browser* b = GetOrCreateTabbedBrowser();
    DCHECK(b);

    // If we have just created a new browser window, make sure we select the
    // tab.
    if (b->tab_count() == 0 && disposition == NEW_BACKGROUND_TAB)
      disposition = NEW_FOREGROUND_TAB;

    b->OpenURL(url, referrer, disposition, transition);
    b->Show();
    b->MoveToFront(true);
    return;
  }

  if (profile_->IsOffTheRecord() && disposition == OFF_THE_RECORD)
    disposition = NEW_FOREGROUND_TAB;

  if (disposition == NEW_WINDOW) {
    Browser* new_browser = new Browser(gfx::Rect(), SW_SHOWNORMAL, profile_,
                                       BrowserType::TABBED_BROWSER, L"");
    new_contents = new_browser->AddTabWithURL(url, referrer, transition, true,
                                              instance);
    new_browser->Show();
  } else if ((disposition == CURRENT_TAB) && current_tab) {
    if (transition == PageTransition::TYPED ||
        transition == PageTransition::AUTO_BOOKMARK ||
        transition == PageTransition::GENERATED ||
        transition == PageTransition::START_PAGE) {
      // Don't forget the openers if this tab is a New Tab page opened at the
      // end of the TabStrip (e.g. by pressing Ctrl+T). Give the user one
      // navigation of one of these transition types before resetting the
      // opener relationships (this allows for the use case of opening a new
      // tab to do a quick look-up of something while viewing a tab earlier in
      // the strip). We can make this heuristic more permissive if need be.
      // TODO(beng): (http://b/1306495) write unit tests for this once this
      //             object is unit-testable.
      int current_tab_index =
          tabstrip_model_.GetIndexOfTabContents(current_tab);
      bool forget_openers = 
          !(current_tab->type() == TAB_CONTENTS_NEW_TAB_UI &&
          current_tab_index == (tab_count() - 1) &&
          current_tab->controller()->GetEntryCount() == 1);
      if (forget_openers) {
        // If the user navigates the current tab to another page in any way
        // other than by clicking a link, we want to pro-actively forget all
        // TabStrip opener relationships since we assume they're beginning a
        // different task by reusing the current tab.
        tabstrip_model_.ForgetAllOpeners();
        // In this specific case we also want to reset the group relationship,
        // since it is now technically invalid.
        tabstrip_model_.ForgetGroup(current_tab);
      }
    }
    current_tab->controller()->LoadURL(url, referrer, transition);
    // The TabContents might have changed as part of the navigation (ex: new tab
    // page can become WebContents).
    new_contents = current_tab->controller()->active_contents();
    GetStatusBubble()->Hide();

    // Synchronously update the location bar. This allows us to immediately
    // have the URL bar update when the user types something, rather than
    // going through the normal system of ScheduleUIUpdate which has a delay.
    UpdateToolBar(false);
  } else if (disposition == OFF_THE_RECORD) {
    OpenURLOffTheRecord(profile_, url);
    return;
  } else if (disposition != SUPPRESS_OPEN) {
    new_contents =
        AddTabWithURL(url, referrer, transition,
                      disposition != NEW_BACKGROUND_TAB, instance);
  }

  if (disposition != NEW_BACKGROUND_TAB && source_tab_was_frontmost) {
    // Give the focus to the newly navigated tab, if the source tab was
    // front-most.
    new_contents->Focus();
  }
}

void Browser::NavigationStateChanged(const TabContents* source,
                                     unsigned changed_flags) {
  if (!GetSelectedTabContents()) {
    // Nothing is selected. This can happen when being restored from history,
    // bail.
    return;
  }

  // Only update the UI when something visible has changed.
  if (changed_flags)
    ScheduleUIUpdate(source, changed_flags);

  // We don't schedule updates to the navigation commands since they will only
  // change once per navigation, so we don't have to worry about flickering.
  if (changed_flags & TabContents::INVALIDATE_URL)
    UpdateNavigationCommands();
}

void Browser::ReplaceContents(TabContents* source, TabContents* new_contents) {
  source->set_delegate(NULL);
  new_contents->set_delegate(this);

  RemoveScheduledUpdatesFor(source);

  int index = tabstrip_model_.GetIndexOfTabContents(source);
  tabstrip_model_.ReplaceTabContentsAt(index, new_contents);

  if (is_attempting_to_close_browser_) {
    // Need to do this asynchronously as it will close the tab, which is
    // currently on the call stack above us.
    MessageLoop::current()->PostTask(FROM_HERE,
        method_factory_.NewRunnableMethod(&Browser::ClearUnloadState,
        Source<TabContents>(source).ptr()));
  }
  // Need to remove ourselves as an observer for disconnection on the replaced
  // TabContents, since we only care to fire onbeforeunload handlers on active
  // Tabs. Make sure an observer is added for the replacement TabContents.
  NotificationService::current()->
      RemoveObserver(this, NOTIFY_WEB_CONTENTS_DISCONNECTED, 
                     Source<TabContents>(source));
  NotificationService::current()->
      AddObserver(this, NOTIFY_WEB_CONTENTS_DISCONNECTED,
                  Source<TabContents>(new_contents));
  
}

void Browser::AddNewContents(TabContents* source,
                             TabContents* new_contents,
                             WindowOpenDisposition disposition,
                             const gfx::Rect& initial_pos,
                             bool user_gesture) {
  DCHECK(disposition != SAVE_TO_DISK);  // No code for this yet

  // If this is an application we can only have one tab so we need to process
  // this in tabbed browser window.
  if (tabstrip_model_.count() > 0 &&
      disposition != NEW_WINDOW && disposition != NEW_POPUP &&
      type_ != BrowserType::TABBED_BROWSER) {
    Browser* b = GetOrCreateTabbedBrowser();
    DCHECK(b);
    PageTransition::Type transition = PageTransition::LINK;
    // If we were called from an "installed webapp" we want to emulate the code
    // that is run from browser_init.cc for links from external applications.
    // This means we need to open the tab with the START PAGE transition.
    // AddNewContents doesn't support this but the TabStripModel's
    // AddTabContents method does.
    if (type_ == BrowserType::APPLICATION)
      transition = PageTransition::START_PAGE;
    b->tabstrip_model()->AddTabContents(new_contents, -1, transition, true);
    b->Show();
    b->MoveToFront(true);
    return;
  }

  if (disposition == NEW_POPUP) {
    BuildPopupWindow(source, new_contents, initial_pos);
  } else if (disposition == NEW_WINDOW) {
    Browser* new_browser = new Browser(gfx::Rect(), SW_SHOWNORMAL, profile_,
                                       BrowserType::TABBED_BROWSER, L"");
    new_browser->AddNewContents(source, new_contents, NEW_FOREGROUND_TAB,
                                initial_pos, user_gesture);
    new_browser->Show();
  } else if (disposition == CURRENT_TAB) {
    ReplaceContents(source, new_contents);
  } else if (disposition != SUPPRESS_OPEN) {
    tabstrip_model_.AddTabContents(new_contents, -1, PageTransition::LINK,
                                   disposition == NEW_FOREGROUND_TAB);
  }
}

void Browser::ActivateContents(TabContents* contents) {
  tabstrip_model_.SelectTabContentsAt(
      tabstrip_model_.GetIndexOfTabContents(contents), false);
  window_->Activate();
}

HWND Browser::GetTopLevelHWND() const {
  return window_ ? reinterpret_cast<HWND>(window_->GetNativeHandle()) : NULL;
}

void Browser::LoadingStateChanged(TabContents* source) {
  tabstrip_model_.UpdateTabContentsLoadingAnimations();

  window_->UpdateTitleBar();

  // Let the go button know that it should change appearance if possible.
  if (source == GetSelectedTabContents()) {
    GetGoButton()->ScheduleChangeMode(
      source->is_loading() ? GoButton::MODE_STOP : GoButton::MODE_GO);

    GetStatusBubble()->SetStatus(GetSelectedTabContents()->GetStatusText());
  }
}

void Browser::CloseContents(TabContents* source) {
  if (is_attempting_to_close_browser_) {
    // If we're trying to close the browser, just clear the state related to
    // waiting for unload to fire. Don't actually try to close the tab as it 
    // will go down the slow shutdown path instead of the fast path of killing
    // all the renderer processes.
    ClearUnloadState(source);
    return;
  }

  int index = tabstrip_model_.GetIndexOfTabContents(source);
  if (index == TabStripModel::kNoTab) {
    NOTREACHED() << "CloseContents called for tab not in our strip";
    return;
  }
  tabstrip_model_.CloseTabContentsAt(index);
}

void Browser::MoveContents(TabContents* source, const gfx::Rect& pos) {
  if (GetType() != BrowserType::BROWSER) {
    NOTREACHED() << "moving invalid browser type";
    return;
  }

  ::SetWindowPos(GetTopLevelHWND(), NULL, pos.x(), pos.y(), pos.width(),
                 pos.height(), 0);
  win_util::AdjustWindowToFit(GetTopLevelHWND());
}

bool Browser::IsPopup(TabContents* source) {
  // A non-tabbed BROWSER is an unconstrained popup.
  return (GetType() == BrowserType::BROWSER);
}

void Browser::ShowHtmlDialog(HtmlDialogContentsDelegate* delegate,
                             HWND parent_hwnd) {
  parent_hwnd = parent_hwnd ? parent_hwnd : GetTopLevelHWND();
  HtmlDialogView* html_view = new HtmlDialogView(this, profile_, delegate);
  views::Window::CreateChromeWindow(parent_hwnd, gfx::Rect(), html_view);
  html_view->InitDialog();
  html_view->window()->Show();
}

void Browser::Observe(NotificationType type,
                      const NotificationSource& source,
                      const NotificationDetails& details) {
  switch (type) {
    case NOTIFY_WEB_CONTENTS_DISCONNECTED:
      if (is_attempting_to_close_browser_) {
        // Need to do this asynchronously as it will close the tab, which is
        // currently on the call stack above us.
        MessageLoop::current()->PostTask(FROM_HERE,
            method_factory_.NewRunnableMethod(&Browser::ClearUnloadState,
                Source<TabContents>(source).ptr()));
      }
      break;

    case NOTIFY_SSL_STATE_CHANGED:
      // When the current tab's SSL state changes, we need to update the URL
      // bar to reflect the new state. Note that it's possible for the selected
      // tab contents to be NULL. This is because we listen for all sources
      // (NavigationControllers) for convenience, so the notification could
      // actually be for a different window while we're doing asynchronous
      // closing of this one.
      if (GetSelectedTabContents() &&
          GetSelectedTabContents()->controller() ==
          Source<NavigationController>(source).ptr())
        UpdateToolBar(false);
      break;

    default:
      NOTREACHED() << "Got a notification we didn't register for.";
  }
}

void Browser::UpdateNavigationCommands() {
  const TabContents* const current_tab = GetSelectedTabContents();
  NavigationController* nc = current_tab->controller();
  controller_.UpdateCommandEnabled(IDC_BACK, nc->CanGoBack());
  controller_.UpdateCommandEnabled(IDC_FORWARD, nc->CanGoForward());

  const WebContents* const web_contents = current_tab->AsWebContents();

  if (web_contents) {
    controller_.UpdateCommandEnabled(IDC_STAR, true);
    SetStarredButtonToggled(web_contents->is_starred());

    // View-source should not be enabled if already in view-source mode.
    controller_.UpdateCommandEnabled(IDC_VIEWSOURCE,
        current_tab->type() != TAB_CONTENTS_VIEW_SOURCE &&
        current_tab->controller()->GetActiveEntry());

    controller_.UpdateCommandEnabled(IDC_ZOOM, true);
    bool enable_encoding =
        SavePackage::IsSavableContents(web_contents->contents_mime_type()) &&
        SavePackage::IsSavableURL(current_tab->GetURL());
    controller_.UpdateCommandEnabled(IDC_ENCODING, enable_encoding);

    controller_.UpdateCommandEnabled(IDC_SAVEPAGE,
        SavePackage::IsSavableURL(current_tab->GetURL()));
    controller_.UpdateCommandEnabled(IDC_SHOW_JS_CONSOLE, true);
  } else {
    controller_.UpdateCommandEnabled(IDC_VIEWSOURCE, false);
    controller_.UpdateCommandEnabled(IDC_SHOW_JS_CONSOLE, false);

    // Both disable the starring button and ensure it doesn't show a star.
    controller_.UpdateCommandEnabled(IDC_STAR, false);
    SetStarredButtonToggled(false);
    controller_.UpdateCommandEnabled(IDC_ZOOM, false);
    controller_.UpdateCommandEnabled(IDC_ENCODING, false);

    controller_.UpdateCommandEnabled(IDC_SAVEPAGE, false);
  }

  controller_.UpdateCommandEnabled(IDC_CREATE_SHORTCUT,
                                   current_tab->type() == TAB_CONTENTS_WEB &&
                                       !current_tab->GetFavIcon().isNull());
  controller_.UpdateCommandEnabled(IDC_FIND, web_contents != NULL);
  controller_.UpdateCommandEnabled(IDC_PRINT, web_contents != NULL);
  controller_.UpdateCommandEnabled(IDC_DUPLICATE,
                                   CanDuplicateContentsAt(selected_index()));
}

// Notification that the starredness of a tab changed.
void Browser::URLStarredChanged(TabContents* source, bool starred) {
  if (source == GetSelectedTabContents())
    SetStarredButtonToggled(starred);
}

StatusBubble* Browser::GetStatusBubble() {
  return window_->GetStatusBubble();
}

void Browser::ContentsMouseEvent(TabContents* source, UINT message) {
  if (source == GetSelectedTabContents()) {
    if (message == WM_MOUSEMOVE) {
      GetStatusBubble()->MouseMoved();
    } else if (message == WM_MOUSELEAVE) {
      GetStatusBubble()->SetURL(GURL(), std::wstring());
    }
  }
}

void Browser::UpdateTargetURL(TabContents* source, const GURL& url) {
  if (source == GetSelectedTabContents()) {
    PrefService* prefs = profile_->GetPrefs();
    GetStatusBubble()->SetURL(url, prefs->GetString(prefs::kAcceptLanguages));
  }
}

void Browser::SetStarredButtonToggled(bool starred) {
  window_->GetStarButton()->SetToggled(starred);
}

GoButton* Browser::GetGoButton() {
  return window_->GetGoButton();
}

void Browser::ContentsZoomChange(bool zoom_in) {
  controller_.ExecuteCommand(zoom_in ? IDC_ZOOM_PLUS : IDC_ZOOM_MINUS);
}

bool Browser::IsApplication() const {
  return type_ == BrowserType::APPLICATION;
}

void Browser::ContentsStateChanged(TabContents* source) {
  int index = tabstrip_model_.GetIndexOfTabContents(source);
  if (index != TabStripModel::kNoTab)
    tabstrip_model_.UpdateTabContentsStateAt(index);
}

bool Browser::ShouldDisplayURLField() {
  return !IsApplication();
}

void Browser::FocusLocationBar() {
  LocationBarView* location_bar = GetLocationBarView();
  if (location_bar)
    location_bar->location_entry()->SetFocus();
}

void Browser::SyncHistoryWithTabs(int index) {
  if (!profile()->HasSessionService())
    return;
  SessionService* session_service = profile()->GetSessionService();
  if (session_service) {
    for (int i = index; i < tab_count(); ++i) {
      TabContents* contents = GetTabContentsAt(i);
      if (contents) {
        session_service->SetTabIndexInWindow(
            session_id(), contents->controller()->session_id(), i);
      }
    }
  }
}

void Browser::ToolbarSizeChanged(TabContents* source, bool is_animating) {
  if (source == GetSelectedTabContents() || source == NULL) {
    // This will refresh the shelf if needed.
    window_->SelectedTabToolbarSizeChanged(is_animating);
  }
}

void Browser::MoveToFront(bool should_activate) {
  window_->Activate();
}

bool Browser::ShouldCloseWindow() {
  if (HasCompletedUnloadProcessing()) {
    return true;
  }
  is_attempting_to_close_browser_ = true;

  for (int i = 0; i < tab_count(); ++i) {
    if (tabstrip_model_.TabHasUnloadListener(i)) {
      TabContents* tab = GetTabContentsAt(i);
      tabs_needing_before_unload_fired_.push_back(tab);
    }
  }

  if (tabs_needing_before_unload_fired_.empty())
    return true;

  ProcessPendingTabs();
  return false;
}

void Browser::ProcessPendingTabs() {
  DCHECK(is_attempting_to_close_browser_);

  if (HasCompletedUnloadProcessing()) {
    // We've finished all the unload events and can proceed to close the
    // browser.
    OnWindowClosing();
    return;
  }

  // Process beforeunload tabs first. When that queue is empty, process
  // unload tabs.
  if (!tabs_needing_before_unload_fired_.empty()) {
    TabContents* tab = tabs_needing_before_unload_fired_.back();
    tab->AsWebContents()->render_view_host()->FirePageBeforeUnload();
  } else if (!tabs_needing_unload_fired_.empty()) {
    // We've finished firing all beforeunload events and can proceed with unload
    // events.
    // TODO(ojan): We should add a call to browser_shutdown::OnShutdownStarting
    // somewhere around here so that we have accurate measurements of shutdown
    // time.
    // TODO(ojan): We can probably fire all the unload events in parallel and
    // get a perf benefit from that in the cases where the tab hangs in it's
    // unload handler or takes a long time to page in.
    TabContents* tab = tabs_needing_unload_fired_.back();
    tab->AsWebContents()->render_view_host()->FirePageUnload();
  } else {
    NOTREACHED();
  }
}

bool Browser::HasCompletedUnloadProcessing() {
  return is_attempting_to_close_browser_ &&
      tabs_needing_before_unload_fired_.empty() &&
      tabs_needing_unload_fired_.empty();
}

void Browser::CancelWindowClose() {
  DCHECK(is_attempting_to_close_browser_);
  // Only cancelling beforeunload should be able to cancel the window's close.
  // So there had better be a tab that we think needs beforeunload fired.
  DCHECK(!tabs_needing_before_unload_fired_.empty());

  tabs_needing_before_unload_fired_.clear();
  tabs_needing_unload_fired_.clear();

  is_attempting_to_close_browser_ = false;
}

void Browser::BeforeUnloadFired(TabContents* tab,
                                bool proceed,
                                bool* proceed_to_fire_unload) {
  if (!is_attempting_to_close_browser_) {
    *proceed_to_fire_unload = proceed;
    return;
  }

  if (!proceed) {
    CancelWindowClose();
    *proceed_to_fire_unload = false;
    return;
  }

  if (RemoveFromVector(&tabs_needing_before_unload_fired_, tab)) {
    // Now that beforeunload has fired, put the tab on the queue to fire unload.
    tabs_needing_unload_fired_.push_back(tab);
    ProcessPendingTabs();
    // We want to handle firing the unload event ourselves since we want to 
    // fire all the beforeunload events before attempting to fire the unload
    // events should the user cancel closing the browser.
    *proceed_to_fire_unload = false;
    return;
  }

  *proceed_to_fire_unload = true;
}

void Browser::ClearUnloadState(TabContents* tab) {
  DCHECK(is_attempting_to_close_browser_);
  RemoveFromVector(&tabs_needing_before_unload_fired_, tab);
  RemoveFromVector(&tabs_needing_unload_fired_, tab);
  ProcessPendingTabs();
}

bool Browser::RemoveFromVector(UnloadListenerVector* vector, TabContents* tab) {
  DCHECK(is_attempting_to_close_browser_);

  for (UnloadListenerVector::iterator it = vector->begin();
       it != vector->end();
       ++it) {
    if (*it == tab) {
      vector->erase(it);
      return true;
    }
  }
  return false;
}

void Browser::OnWindowClosing() {
  if (!ShouldCloseWindow())
    return;

  if (BrowserList::size() == 1)
    browser_shutdown::OnShutdownStarting(browser_shutdown::WINDOW_CLOSE);

  // Don't use HasSessionService here, we want to force creation of the
  // session service so that user can restore what was open.
  SessionService* session_service = profile()->GetSessionService();
  if (session_service)
    session_service->WindowClosing(session_id());

  CloseAllTabs();
}

// Tab Creation Functions

TabContents* Browser::AddTabWithURL(
    const GURL& url, const GURL& referrer, PageTransition::Type transition,
    bool foreground, SiteInstance* instance) {
  if (type_ == BrowserType::APPLICATION && tabstrip_model_.count() == 1) {
    NOTREACHED() << "Cannot add a tab in a mono tab application.";
    return NULL;
  }

  GURL url_to_load = url;
  if (url_to_load.is_empty())
    url_to_load = GetHomePage();
  TabContents* contents =
      CreateTabContentsForURL(url_to_load, referrer, profile_, transition,
                              false, instance);
  tabstrip_model_.AddTabContents(contents, -1, transition, foreground);
  // By default, content believes it is not hidden.  When adding contents
  // in the background, tell it that it's hidden.
  if (!foreground)
    contents->WasHidden();
  return contents;
}

TabContents* Browser::AddWebApplicationTab(Profile* profile,
                                           WebApp* web_app,
                                           bool lazy) {
  DCHECK(web_app);

  // TODO(acw): Do we need an "application launched" transition type?
  // TODO(creis): Should we reuse the current instance (ie. process) here?
  TabContents* contents =
      CreateTabContentsForURL(web_app->url(), GURL(), profile,
                              PageTransition::LINK, lazy, NULL);
  if (contents->AsWebContents())
    contents->AsWebContents()->SetWebApp(web_app);

  if (lazy) {
    contents->controller()->LoadURLLazily(
        web_app->url(), GURL(), PageTransition::LINK, web_app->name(), NULL);
  }
  tabstrip_model_.AddTabContents(contents, -1, PageTransition::LINK, !lazy);
  return contents;
}

TabContents* Browser::AddTabWithNavigationController(
    NavigationController* ctrl, PageTransition::Type type) {
  TabContents* tc = ctrl->active_contents();
  tabstrip_model_.AddTabContents(tc, -1, type, true);
  return tc;
}

NavigationController* Browser::AddRestoredTab(
    const std::vector<TabNavigation>& navigations,
    int tab_index,
    int selected_navigation,
    bool select) {
  NavigationController* restored_controller =
      BuildRestoredNavigationController(navigations, selected_navigation);

  tabstrip_model_.InsertTabContentsAt(
      tab_index,
      restored_controller->active_contents(),
      select, false);
  if (profile_->HasSessionService()) {
    SessionService* session_service = profile_->GetSessionService();
    if (session_service)
      session_service->TabRestored(restored_controller);
  }
  return restored_controller;
}

void Browser::ReplaceRestoredTab(
    const std::vector<TabNavigation>& navigations,
    int selected_navigation) {
  NavigationController* restored_controller =
      BuildRestoredNavigationController(navigations, selected_navigation);

  tabstrip_model_.ReplaceNavigationControllerAt(
      tabstrip_model_.selected_index(),
      restored_controller);
}

////////////////////////////////////////////////////////////////////////////////
// Browser, TabStripModelDelegate implementation:

void Browser::CreateNewStripWithContents(TabContents* detached_contents,
                                         const gfx::Point& drop_point) {
  DCHECK(type_ == BrowserType::TABBED_BROWSER);

  // Create an empty new browser window the same size as the old one.
  // TODO(beng): move elsewhere
  CRect browser_rect;
  GetWindowRect(reinterpret_cast<HWND>(window_->GetNativeHandle()),
                &browser_rect);
  gfx::Rect rect(0, 0);
  if (drop_point.x() != 0 || drop_point.y() != 0) {
    rect.SetRect(drop_point.x(), drop_point.y(), browser_rect.Width(),
                 browser_rect.Height());
  }
  Browser* new_window =
      new Browser(rect, SW_SHOWNORMAL, profile_, BrowserType::TABBED_BROWSER,
                  std::wstring());
  // Append the TabContents before showing it so the window doesn't flash
  // black.
  new_window->tabstrip_model()->AppendTabContents(detached_contents, true);
  new_window->Show();

  // When we detach a tab we need to make sure any associated Find window moves
  // along with it to its new home (basically we just make new_window the parent
  // of the Find window).
  // TODO(brettw) this could probably be improved, see
  // WebContentsView::ReparentFindWindow for more.
  if (detached_contents->AsWebContents())
    detached_contents->AsWebContents()->view()->ReparentFindWindow(new_window);
}

int Browser::GetDragActions() const {
  int result = 0;
  if (BrowserList::GetBrowserCountForType(profile_,
                                          BrowserType::TABBED_BROWSER) > 1 ||
      tab_count() > 1)
    result |= TAB_TEAROFF_ACTION;
  if (tab_count() > 1)
    result |= TAB_MOVE_ACTION;
  return result;
}

TabContents* Browser::CreateTabContentsForURL(
    const GURL& url, const GURL& referrer, Profile* profile,
    PageTransition::Type transition, bool defer_load,
    SiteInstance* instance) const {
  // Create an appropriate tab contents.
  GURL real_url = url;
  TabContentsType type = TabContents::TypeForURL(&real_url);
  DCHECK(type != TAB_CONTENTS_UNKNOWN_TYPE);

  TabContents* contents =
    TabContents::CreateWithType(type, GetTopLevelHWND(), profile, instance);
  contents->SetupController(profile);

  if (!defer_load) {
    // Load the initial URL before adding the new tab contents to the tab strip
    // so that the tab contents has navigation state.
    contents->controller()->LoadURL(url, referrer, transition);
  }

  return contents;
}

void Browser::ValidateLoadingAnimations() {
  if (window_)
    window_->ValidateThrobber();
}

void Browser::CloseFrameAfterDragSession() {
  // This is scheduled to run after we return to the message loop because
  // otherwise the frame will think the drag session is still active and ignore
  // the request.
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&Browser::CloseFrame));
}

////////////////////////////////////////////////////////////////////////////////
// Browser, TabStripModelObserver implementation:

void Browser::TabInsertedAt(TabContents* contents,
                            int index,
                            bool foreground) {
  contents->set_delegate(this);
  contents->controller()->SetWindowID(session_id());

  SyncHistoryWithTabs(tabstrip_model_.GetIndexOfTabContents(contents));

  // When a tab is dropped into a tab strip we need to make sure that the
  // associated Find window is moved along with it. We therefore change the
  // parent of the Find window (if the parent is already correctly set this
  // does nothing).
  // TODO(brettw) this could probably be improved, see
  // WebContentsView::ReparentFindWindow for more.
  if (contents->AsWebContents())
    contents->AsWebContents()->view()->ReparentFindWindow(this);

  // If the tab crashes in the beforeunload or unload handler, it won't be
  // able to ack. But we know we can close it.
  NotificationService::current()->
      AddObserver(this, NOTIFY_WEB_CONTENTS_DISCONNECTED,
                  Source<TabContents>(contents));
}

void Browser::TabClosingAt(TabContents* contents, int index) {
  NavigationController* controller = contents->controller();
  DCHECK(controller);
  NotificationService::current()->
      Notify(NOTIFY_TAB_CLOSING,
              Source<NavigationController>(controller),
              NotificationService::NoDetails());

  // Sever the TabContents' connection back to us.
  contents->set_delegate(NULL);
}

void Browser::TabDetachedAt(TabContents* contents, int index) {
  contents->set_delegate(NULL);
  if (!tabstrip_model_.closing_all())
    SyncHistoryWithTabs(0);

  RemoveScheduledUpdatesFor(contents);

  NotificationService::current()->
      RemoveObserver(this, NOTIFY_WEB_CONTENTS_DISCONNECTED, 
                     Source<TabContents>(contents));
}

void Browser::TabSelectedAt(TabContents* old_contents,
                            TabContents* new_contents,
                            int index,
                            bool user_gesture) {
  DCHECK(old_contents != new_contents);

  // If we have any update pending, do it now.
  if (!chrome_updater_factory_.empty() && old_contents)
    ProcessPendingUIUpdates();

  LocationBarView* location_bar = GetLocationBarView();
  if (old_contents) {
    // Save what the user's currently typing, so it can be restored when we
    // switch back to this tab.
    if (location_bar)
      location_bar->location_entry()->SaveStateToTab(old_contents);
  }

  // Propagate the profile to the location bar.
  UpdateToolBar(true);

  // Force the go/stop button to change.
  if (new_contents->AsWebContents()) {
    GetGoButton()->ChangeMode(
        new_contents->is_loading() ? GoButton::MODE_STOP : GoButton::MODE_GO);
  } else {
    GetGoButton()->ChangeMode(GoButton::MODE_GO);
  }

  // Update other parts of the toolbar.
  UpdateNavigationCommands();

  // Reset the status bubble.
  GetStatusBubble()->Hide();

  // Show the loading state (if any).
  GetStatusBubble()->SetStatus(GetSelectedTabContents()->GetStatusText());

  // Update sessions. Don't force creation of sessions. If sessions doesn't
  // exist, the change will be picked up by sessions when created.
  if (profile_->HasSessionService()) {
    SessionService* session_service = profile_->GetSessionService();
    if (session_service && !tabstrip_model_.closing_all()) {
      session_service->SetSelectedTabInWindow(session_id(),
                                              tabstrip_model_.selected_index());
    }
  }
}

void Browser::TabMoved(TabContents* contents,
                       int from_index,
                       int to_index) {
  DCHECK(from_index >= 0 && to_index >= 0);
  // Notify the history service.
  SyncHistoryWithTabs(std::min(from_index, to_index));
}

void Browser::TabStripEmpty() {
  // Close the frame after we return to the message loop (not immediately,
  // otherwise it will destroy this object before the stack has a chance to
  // cleanly unwind.)
  // Note: This will be called several times if TabStripEmpty is called several
  //       times. This is because it does not close the window if tabs are
  //       still present.
  // NOTE: If you change to be immediate (no invokeLater) then you'll need to
  //       update BrowserList::CloseAllBrowsers.
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&Browser::CloseFrame));
}

BrowserType::Type Browser::GetType() const {
  return type_;
}

void Browser::InitHangMonitor() {
  PrefService* pref_service = g_browser_process->local_state();
  DCHECK(pref_service != NULL);
  int plugin_message_response_timeout =
      pref_service->GetInteger(prefs::kPluginMessageResponseTimeout);
  int hung_plugin_detect_freq =
      pref_service->GetInteger(prefs::kHungPluginDetectFrequency);
  if ((hung_plugin_detect_freq > 0) &&
      hung_window_detector_.Initialize(GetTopLevelHWND(),
                                       plugin_message_response_timeout)) {
    ticker_.set_tick_interval(hung_plugin_detect_freq);
    ticker_.RegisterTickHandler(&hung_window_detector_);
    ticker_.Start();

    pref_service->SetInteger(prefs::kPluginMessageResponseTimeout,
                             plugin_message_response_timeout);
    pref_service->SetInteger(prefs::kHungPluginDetectFrequency,
                             hung_plugin_detect_freq);
  }
}


Browser* Browser::GetOrCreateTabbedBrowser() {
  Browser* browser = BrowserList::FindBrowserWithType(
      profile_, BrowserType::TABBED_BROWSER);
  if (!browser) {
    browser = new Browser(gfx::Rect(), SW_SHOWNORMAL, profile_,
                          BrowserType::TABBED_BROWSER, std::wstring());
  }
  return browser;
}

void Browser::RemoveScheduledUpdatesFor(TabContents* contents) {
  if (!contents)
    return;

  // Remove any pending UI updates for the detached tab.
  UpdateVector::iterator cur_update = scheduled_updates_.begin();
  while (cur_update != scheduled_updates_.end()) {
    if (cur_update->source == contents) {
      cur_update = scheduled_updates_.erase(cur_update);
    } else {
      ++cur_update;
    }
  }
}

void Browser::ShowNativeUI(const GURL& url) {
  int i, c;
  TabContents* tc;
  for (i = 0, c = tabstrip_model_.count(); i < c; ++i) {
    tc = tabstrip_model_.GetTabContentsAt(i);
    if (tc->type() == TAB_CONTENTS_NATIVE_UI &&
        tc->GetURL() == url) {
      tabstrip_model_.SelectTabContentsAt(i, false);
      return;
    }
  }

  TabContents* contents = CreateTabContentsForURL(url, GURL(), profile_,
                                                  PageTransition::LINK, false,
                                                  NULL);
  AddNewContents(NULL, contents, NEW_FOREGROUND_TAB, gfx::Rect(), true);
}

NavigationController* Browser::BuildRestoredNavigationController(
      const std::vector<TabNavigation>& navigations,
      int selected_navigation) {
  if (!navigations.empty()) {
    DCHECK(selected_navigation >= 0 &&
           selected_navigation < static_cast<int>(navigations.size()));
    // We should have a valid URL, if we don't fall back to the default.
    GURL url = navigations[selected_navigation].url;
    if (url.is_empty())
      url = GetHomePage();

    // Create a NavigationController. This constructor creates the appropriate
    // set of TabContents.
    return new NavigationController(
        profile_, navigations, selected_navigation, GetTopLevelHWND());
  } else {
    // No navigations. Create a tab with about:blank.
    TabContents* contents =
        CreateTabContentsForURL(GURL("about:blank"), GURL(), profile_,
                                PageTransition::START_PAGE, false, NULL);
    return new NavigationController(contents, profile_);
  }
}

// static
void Browser::OpenURLOffTheRecord(Profile* profile, const GURL& url) {
  Profile* off_the_record_profile = profile->GetOffTheRecordProfile();
  Browser* browser = BrowserList::FindBrowserWithType(
      off_the_record_profile, BrowserType::TABBED_BROWSER);
  if (browser == NULL) {
    browser = new Browser(gfx::Rect(), SW_SHOWNORMAL, off_the_record_profile,
                          BrowserType::TABBED_BROWSER, L"");
  }
  // TODO(eroman): should we have referrer here?
  browser->AddTabWithURL(url, GURL(), PageTransition::LINK, true, NULL);
  browser->Show();
  browser->MoveToFront(true);
}

void Browser::ConvertToTabbedBrowser() {
  if (GetType() != BrowserType::BROWSER) {
    NOTREACHED();
    return;
  }

  int tab_strip_index = tabstrip_model_.selected_index();
  TabContents* contents = tabstrip_model_.DetachTabContentsAt(tab_strip_index);
  Browser* browser = new Browser(gfx::Rect(), SW_SHOWNORMAL, profile_,
                                 BrowserType::TABBED_BROWSER, L"");
  browser->AddNewContents(
    NULL, contents, NEW_FOREGROUND_TAB, gfx::Rect(), true);
  browser->Show();
}

void Browser::BuildPopupWindow(TabContents* source,
                               TabContents* new_contents,
                               const gfx::Rect& initial_pos) {
  BrowserType::Type type =
      type_ == BrowserType::APPLICATION ? type_ : BrowserType::BROWSER;
  Browser* browser = new Browser(initial_pos, SW_SHOWNORMAL, profile_, type,
                                 std::wstring());
  browser->AddNewContents(source, new_contents,
                          NEW_FOREGROUND_TAB, gfx::Rect(), true);
  browser->Show();
}

void Browser::ConvertContentsToApplication(TabContents* contents) {
  if (!contents->AsWebContents() || !contents->AsWebContents()->web_app()) {
    NOTREACHED();
    return;
  }

  int index = tabstrip_model_.GetIndexOfTabContents(contents);
  if (index < 0)
    return;

  WebApp* app = contents->AsWebContents()->web_app();
  const std::wstring& app_name =
      app->name().empty() ? ComputeApplicationNameFromURL(app->url()) :
                            app->name();
  RegisterAppPrefs(app_name);

  tabstrip_model_.DetachTabContentsAt(index);
  Browser* browser = new Browser(gfx::Rect(), SW_SHOWNORMAL, profile_,
                                 BrowserType::APPLICATION, app_name);
  browser->AddNewContents(
    NULL, contents, NEW_FOREGROUND_TAB, gfx::Rect(), true);
  browser->Show();
}

// static
std::wstring Browser::ComputeApplicationNameFromURL(const GURL& url) {
  std::string t;
  t.append(url.host());
  t.append("_");
  t.append(url.path());
  return UTF8ToWide(t);
}

// static
void Browser::OpenWebApplication(Profile* profile,
                                 WebApp* app,
                                 int show_command) {
  const std::wstring& app_name =
      app->name().empty() ? ComputeApplicationNameFromURL(app->url()) :
                            app->name();

  RegisterAppPrefs(app_name);
  Browser* browser = new Browser(gfx::Rect(), show_command, profile,
                                 BrowserType::APPLICATION, app_name);
  browser->AddWebApplicationTab(profile, app, false);
  browser->Show();
}

// static
void Browser::RegisterAppPrefs(const std::wstring& app_name) {
  // A set of apps that we've already started.
  static std::set<std::wstring>* g_app_names = NULL;

  if (!g_app_names)
    g_app_names = new std::set<std::wstring>;

  // Only register once for each app name.
  if (g_app_names->find(app_name) != g_app_names->end())
    return;
  g_app_names->insert(app_name);

  // We need to register the window position pref.
  std::wstring window_pref(prefs::kBrowserWindowPlacement);
  window_pref.append(L"_");
  window_pref.append(app_name);
  PrefService* prefs = g_browser_process->local_state();
  DCHECK(prefs);

  prefs->RegisterDictionaryPref(window_pref.c_str());
}

NavigationController* Browser::GetSelectedNavigationController() const {
  TabContents* tc = GetSelectedTabContents();
  if (tc)
    return tc->controller();
  else
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// NEW FRAME TODO(beng): clean up this file
// DO NOT PLACE METHODS NOT RELATED TO NEW FRAMES BELOW THIS LINE.

void Browser::SaveWindowPosition(const gfx::Rect& bounds, bool maximized) {
  // We don't save window position for popups.
  if (GetType() == BrowserType::BROWSER)
    return;

  // First save to local state, this is for remembering on subsequent starts.
  PrefService* prefs = g_browser_process->local_state();
  DCHECK(prefs);
  std::wstring name(prefs::kBrowserWindowPlacement);
  if (!app_name_.empty()) {
    name.append(L"_");
    name.append(app_name_);
  }

  DictionaryValue* win_pref = prefs->GetMutableDictionary(name.c_str());
  DCHECK(win_pref);
  win_pref->SetInteger(L"top", bounds.y());
  win_pref->SetInteger(L"left", bounds.x());
  win_pref->SetInteger(L"bottom", bounds.bottom());
  win_pref->SetInteger(L"right", bounds.right());
  win_pref->SetBoolean(L"maximized", maximized);

  // Then save to the session storage service, used when reloading a past
  // session. Note that we don't want to be the ones who cause lazy
  // initialization of the session service. This function gets called during
  // initial window showing, and we don't want to bring in the session service
  // this early.
  if (profile()->HasSessionService()) {
    SessionService* session_service = profile()->GetSessionService();
    if (session_service)
      session_service->SetWindowBounds(session_id_, bounds, maximized);
  }
}

void Browser::RestoreWindowPosition(gfx::Rect* bounds, bool* maximized) {
  DCHECK(bounds && maximized);
  WindowSizer::GetBrowserWindowBounds(app_name_, *bounds, bounds, maximized);
}

SkBitmap Browser::GetCurrentPageIcon() const {
  TabContents* contents = tabstrip_model_.GetSelectedTabContents();
  return contents ? contents->GetFavIcon() : SkBitmap();
}

std::wstring Browser::GetCurrentPageTitle() const {
  TabContents* contents = tabstrip_model_.GetSelectedTabContents();
  std::wstring title;
  if (contents) {
    title = contents->GetTitle();
    FormatTitleForDisplay(&title);
  }
  if (title.empty())
    title = l10n_util::GetString(IDS_TAB_UNTITLED_TITLE);

  return l10n_util::GetStringF(IDS_BROWSER_WINDOW_TITLE_FORMAT, title);
}

// static
void Browser::FormatTitleForDisplay(std::wstring* title) {
  size_t current_index = 0;
  size_t match_index;
  while ((match_index = title->find(L'\n', current_index)) !=
         std::wstring::npos) {
    title->replace(match_index, 1, L"");
    current_index = match_index;
  }
}
