// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_LIST_H__
#define CHROME_BROWSER_BROWSER_LIST_H__

#include <algorithm>
#include <vector>

#include "chrome/browser/browser.h"

namespace views {
class AppModalDialogDelegate;
class Window;
};
class WebContents;

// Stores a list of all Browser objects.
class BrowserList {
 public:
  typedef std::vector<Browser*> list_type;
  typedef list_type::iterator iterator;
  typedef list_type::const_iterator const_iterator;
  typedef list_type::const_reverse_iterator const_reverse_iterator;

  // It is not allowed to change the global window list (add or remove any
  // browser windows while handling observer callbacks.
  class Observer {
   public:
    // Called immediately after a browser is added to the list
    virtual void OnBrowserAdded(const Browser* browser) = 0;

    // Called immediately before a browser is removed from the list
    virtual void OnBrowserRemoving(const Browser* browser) = 0;
  };

  // Adds and removes browsers from the global list. The browser object should
  // be valid BEFORE these calls (for the benefit of observers), so notify and
  // THEN delete the object.
  static void AddBrowser(Browser* browser);
  static void RemoveBrowser(Browser* browser);

  // Adds and removes non-browser dependent windows. These are windows that are
  // top level, but whose lifetime is associated wtih the existence of at least
  // one active Browser. When the last Browser is destroyed, all open dependent
  // windows are closed.
  static void AddDependentWindow(views::Window* window);
  static void RemoveDependentWindow(views::Window* window);

  static void AddObserver(Observer* observer);
  static void RemoveObserver(Observer* observer);

  // Called by Browser objects when their window is activated (focused).  This
  // allows us to determine what the last active Browser was.
  static void SetLastActive(Browser* browser);

  // Returns the Browser object whose window was most recently active.  If the
  // most recently open Browser's window was closed, returns the first Browser
  // in the list.  If no Browsers exist, returns NULL.
  static Browser* GetLastActive();

  // Find an existing browser window with the provided type. If the last active
  // has the right type, it is returned. Otherwise, the next available browser
  // is returned. Returns NULL if no such browser currently exists.
  static Browser* FindBrowserWithType(Profile* p, Browser::Type t);

  // Closes all browsers. If use_post is true the windows are closed by way of
  // posting a WM_CLOSE message, otherwise the windows are closed directly. In
  // almost all cases you'll want to use true, the one exception is ending
  // the session. use_post should only be false when invoked from end session.
  static void CloseAllBrowsers(bool use_post);

  // Begins shutdown of the application when the Windows session is ending.
  static void WindowsSessionEnding();

  // Returns true if there is at least one Browser with the specified profile.
  static bool HasBrowserWithProfile(Profile* profile);

  static const_iterator begin() {
    return browsers_.begin();
  }

  static const_iterator end() {
    return browsers_.end();
  }

  static size_t size() {
    return browsers_.size();
  }

  // Returns iterated access to list of open browsers ordered by when
  // they were last active. The underlying data structure is a vector
  // and we push_back on recent access so a reverse iterator gives the
  // latest accessed browser first.
  static const_reverse_iterator begin_last_active() {
    return last_active_browsers_.rbegin();
  }

  static const_reverse_iterator end_last_active() {
    return last_active_browsers_.rend();
  }

  // Return the number of browsers with the following profile which are
  // currently open.
  static size_t GetBrowserCount(Profile* p);

  // Return the number of browsers with the following profile and type which are
  // currently open.
  static size_t GetBrowserCountForType(Profile* p, Browser::Type type);

  // Returns true if at least one off the record session is active.
  static bool IsOffTheRecordSessionActive();

 private:
  // Closes all registered dependent windows.
  static void CloseAllDependentWindows();

  // Helper method to remove a browser instance from a list of browsers
  static void RemoveBrowserFrom(Browser* browser, list_type* browser_list);

  static list_type browsers_;
  static std::vector<Observer*> observers_;
  static list_type last_active_browsers_;
  typedef std::vector<views::Window*> DependentWindowList;
  static DependentWindowList dependent_windows_;

  // Set to the currently showing modal dialog delegate if any, NULL otherwise.
  static views::AppModalDialogDelegate* app_modal_dialog_;
};

class WebContents;

// Iterates through all web view hosts in all browser windows. Because the
// renderers act asynchronously, getting a host through this interface does
// not guarantee that the renderer is ready to go. Doing anything to affect
// browser windows or tabs while iterating may cause incorrect behavior.
//
// Example:
//   for (WebContentsIterator iterator; !iterator.done(); iterator++) {
//     WebContents* cur = *iterator;
//     -or-
//     iterator->operationOnWebContents();
//     ...
//   }
class WebContentsIterator {
 public:
  WebContentsIterator();

  // Returns true if we are past the last Browser.
  bool done() const {
    return cur_ == NULL;
  }

  // Returns the current WebContents, valid as long as !Done()
  WebContents* operator->() const {
    return cur_;
  }
  WebContents* operator*() const {
    return cur_;
  }

  // Incrementing operators, valid as long as !Done()
  WebContents* operator++() { // ++preincrement
    Advance();
    return cur_;
  }
  WebContents* operator++(int) { // postincrement++
    WebContents* tmp = cur_;
    Advance();
    return tmp;
  }

 private:
  // Loads the next host into Cur. This is designed so that for the initial
  // call when browser_iterator_ points to the first browser and
  // web_view_index_ is -1, it will fill the first host.
  void Advance();

  // iterator over all the Browser objects
  BrowserList::const_iterator browser_iterator_;

  // tab index into the current Browser of the current web view
  int web_view_index_;

  // Current WebContents, or NULL if we're at the end of the list. This can
  // be extracted given the browser iterator and index, but it's nice to cache
  // this since the caller may access the current host many times.
  WebContents* cur_;
};

#endif  // CHROME_BROWSER_BROWSER_LIST_H__

