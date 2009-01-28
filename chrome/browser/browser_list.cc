// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/browser_list.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/app/result_codes.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profile_manager.h"
#if defined(OS_WIN)
// TODO(port): these can probably all go away, even on win
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/notification_service.h"
#endif


BrowserList::list_type BrowserList::browsers_;
std::vector<BrowserList::Observer*> BrowserList::observers_;

// static
void BrowserList::AddBrowser(Browser* browser) {
  browsers_.push_back(browser);

  g_browser_process->AddRefModule();

  NotificationService::current()->Notify(
    NOTIFY_BROWSER_OPENED,
    Source<Browser>(browser), NotificationService::NoDetails());

  // Send out notifications after add has occurred. Do some basic checking to
  // try to catch evil observers that change the list from under us.
  std::vector<Observer*>::size_type original_count = observers_.size();
  for (int i = 0; i < static_cast<int>(observers_.size()); i++)
    observers_[i]->OnBrowserAdded(browser);
  DCHECK_EQ(original_count, observers_.size())
      << "observer list modified during notification";
}

// static
void BrowserList::RemoveBrowser(Browser* browser) {
  RemoveBrowserFrom(browser, &last_active_browsers_);

  bool close_app = (browsers_.size() == 1);
  NotificationService::current()->Notify(
      NOTIFY_BROWSER_CLOSED,
      Source<Browser>(browser), Details<bool>(&close_app));

  // Send out notifications before anything changes. Do some basic checking to
  // try to catch evil observers that change the list from under us.
  std::vector<Observer*>::size_type original_count = observers_.size();
  for (int i = 0; i < static_cast<int>(observers_.size()); i++)
    observers_[i]->OnBrowserRemoving(browser);
  DCHECK_EQ(original_count, observers_.size())
      << "observer list modified during notification";

  RemoveBrowserFrom(browser, &browsers_);

  // If the last Browser object was destroyed, make sure we try to close any
  // remaining dependent windows too.
  if (browsers_.empty()) {
    NotificationService::current()->Notify(NOTIFY_ALL_APPWINDOWS_CLOSED,
                                           NotificationService::AllSources(),
                                           NotificationService::NoDetails());
  }

  g_browser_process->ReleaseModule();
}

// static
void BrowserList::AddObserver(BrowserList::Observer* observer) {
  DCHECK(std::find(observers_.begin(), observers_.end(), observer)
         == observers_.end()) << "Adding an observer twice";
  observers_.push_back(observer);
}

// static
void BrowserList::RemoveObserver(BrowserList::Observer* observer) {
  std::vector<Observer*>::iterator place =
      std::find(observers_.begin(), observers_.end(), observer);
  if (place == observers_.end()) {
    NOTREACHED() << "Removing an observer that isn't registered.";
    return;
  }
  observers_.erase(place);
}

// static
void BrowserList::CloseAllBrowsers(bool use_post) {
  // Before we close the browsers shutdown all session services. That way an
  // exit can restore all browsers open before exiting.
  ProfileManager::ShutdownSessionServices();

  BrowserList::const_iterator iter;
  for (iter = BrowserList::begin(); iter != BrowserList::end();) {
    if (use_post) {
      (*iter)->window()->Close();
      ++iter;
    } else {
      // This path is hit during logoff/power-down. In this case we won't get
      // a final message and so we force the browser to be deleted.
      Browser* browser = *iter;
      browser->window()->Close();
      // Close doesn't immediately destroy the browser
      // (Browser::TabStripEmpty() uses invoke later) but when we're ending the
      // session we need to make sure the browser is destroyed now. So, invoke
      // DestroyBrowser to make sure the browser is deleted and cleanup can
      // happen.
      browser->window()->DestroyBrowser();
      iter = BrowserList::begin();
      if (iter != BrowserList::end() && browser == *iter) {
        // Destroying the browser should have removed it from the browser list.
        // We should never get here.
        NOTREACHED();
        return;
      }
    }
  }
}

// static
void BrowserList::WindowsSessionEnding() {
  // EndSession is invoked once per frame. Only do something the first time.
  static bool already_ended = false;
  if (already_ended)
    return;
  already_ended = true;

  browser_shutdown::OnShutdownStarting(browser_shutdown::END_SESSION);

  // Write important data first.
  g_browser_process->EndSession();

  // Close all the browsers.
  BrowserList::CloseAllBrowsers(false);

  // Send out notification. This is used during testing so that the test harness
  // can properly shutdown before we exit.
  NotificationService::current()->Notify(NOTIFY_SESSION_END,
    NotificationService::AllSources(),
    NotificationService::NoDetails());

  // And shutdown.
  browser_shutdown::Shutdown();

#if defined(OS_WIN)
  // At this point the message loop is still running yet we've shut everything
  // down. If any messages are processed we'll likely crash. Exit now.
  ExitProcess(ResultCodes::NORMAL_EXIT);
#else
  NOTIMPLEMENTED();
#endif
}

// static
bool BrowserList::HasBrowserWithProfile(Profile* profile) {
  BrowserList::const_iterator iter;
  for (size_t i = 0; i < browsers_.size(); ++i) {
    if (browsers_[i]->profile() == profile)
      return true;
  }
  return false;
}

// static
BrowserList::list_type BrowserList::last_active_browsers_;

// static
void BrowserList::SetLastActive(Browser* browser) {
  RemoveBrowserFrom(browser, &last_active_browsers_);
  last_active_browsers_.push_back(browser);
}

// static
Browser* BrowserList::GetLastActive() {
  if (!last_active_browsers_.empty())
    return *(last_active_browsers_.rbegin());

  return NULL;
}

// static
Browser* BrowserList::FindBrowserWithType(Profile* p, Browser::Type t) {
  Browser* last_active = GetLastActive();
  if (last_active && last_active->profile() == p && last_active->type() == t)
    return last_active;

  BrowserList::const_iterator i;
  for (i = BrowserList::begin(); i != BrowserList::end(); ++i) {
    if (*i == last_active)
      continue;

    if ((*i)->profile() == p && (*i)->type() == t)
      return *i;
  }
  return NULL;
}

// static
size_t BrowserList::GetBrowserCountForType(Profile* p, Browser::Type type) {
  BrowserList::const_iterator i;
  size_t result = 0;
  for (i = BrowserList::begin(); i != BrowserList::end(); ++i) {
    if ((*i)->profile() == p && (*i)->type() == type)
      result++;
  }
  return result;
}

// static
size_t BrowserList::GetBrowserCount(Profile* p) {
  BrowserList::const_iterator i;
  size_t result = 0;
  for (i = BrowserList::begin(); i != BrowserList::end(); ++i) {
    if ((*i)->profile() == p)
      result++;
  }
  return result;
}

// static
bool BrowserList::IsOffTheRecordSessionActive() {
  BrowserList::const_iterator i;
  for (i = BrowserList::begin(); i != BrowserList::end(); ++i) {
    if ((*i)->profile()->IsOffTheRecord())
      return true;
  }
  return false;
}

// static
void BrowserList::RemoveBrowserFrom(Browser* browser, list_type* browser_list) {
  const iterator remove_browser =
      find(browser_list->begin(), browser_list->end(), browser);
  if (remove_browser != browser_list->end())
    browser_list->erase(remove_browser);
}

WebContentsIterator::WebContentsIterator()
    : browser_iterator_(BrowserList::begin()),
      web_view_index_(-1),
      cur_(NULL) {
    Advance();
  }

void WebContentsIterator::Advance() {
  // Unless we're at the beginning (index = -1) or end (iterator = end()),
  // then the current WebContents should be valid.
  DCHECK(web_view_index_ || browser_iterator_ == BrowserList::end() ||
    cur_) << "Trying to advance past the end";

  // Update cur_ to the next WebContents in the list.
  for (;;) {
    web_view_index_++;

    while (web_view_index_ >= (*browser_iterator_)->tab_count()) {
      // advance browsers
      ++browser_iterator_;
      web_view_index_ = 0;
      if (browser_iterator_ == BrowserList::end()) {
        cur_ = NULL;
        return;
      }
    }

    WebContents* next_tab =
      (*browser_iterator_)->GetTabContentsAt(web_view_index_)->AsWebContents();
    if (next_tab) {
      cur_ = next_tab;
      return;
    }
  }
}

