// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_DOM_UI_H_
#define CHROME_BROWSER_DOM_UI_DOM_UI_H_

#include <map>
#include <string>
#include <vector>

#include "base/string16.h"
#include "base/task.h"
#include "chrome/common/page_transition_types.h"

class DictionaryValue;
class DOMMessageHandler;
class GURL;
class Profile;
class RenderViewHost;
class Value;
class TabContents;
class ThemeProvider;

// A DOMUI sets up the datasources and message handlers for a given HTML-based
// UI. It is contained by a DOMUIManager.
class DOMUI {
 public:
  explicit DOMUI(TabContents* contents);
  virtual ~DOMUI();

  virtual void RenderViewCreated(RenderViewHost* render_view_host) {}

  // Called from DOMUIContents.
  void ProcessDOMUIMessage(const std::string& message,
                           const std::string& content);

  // Used by DOMMessageHandlers.
  typedef Callback1<const Value*>::Type MessageCallback;
  void RegisterMessageCallback(const std::string& message,
                               MessageCallback* callback);

  // Returns true if the favicon should be hidden for the current tab.
  bool hide_favicon() const {
    return hide_favicon_;
  }

  // Returns true if the bookmark bar should be forced to being visible,
  // overriding the user's preference.
  bool force_bookmark_bar_visible() const {
    return force_bookmark_bar_visible_;
  }

  // Returns true if the location bar should be focused by default rather than
  // the page contents. Some pages will want to use this to encourage the user
  // to type in the URL bar.
  bool focus_location_bar_by_default() const {
    return focus_location_bar_by_default_;
  }

  // Returns true if the page's URL should be hidden. Some DOM UI pages
  // like the new tab page will want to hide it.
  bool should_hide_url() const {
    return should_hide_url_;
  }

  // Gets a custom tab title provided by the DOM UI. If there is no title
  // override, the string will be empty which should trigger the default title
  // behavior for the tab.
  const string16& overridden_title() const {
    return overridden_title_;
  }

  // Returns the transition type that should be used for link clicks on this
  // DOM UI. This will default to LINK but may be overridden.
  const PageTransition::Type link_transition_type() const {
    return link_transition_type_;
  }

  // Call a Javascript function by sending its name and arguments down to
  // the renderer.  This is asynchronous; there's no way to get the result
  // of the call, and should be thought of more like sending a message to
  // the page.
  // There are two function variants for one-arg and two-arg calls.
  void CallJavascriptFunction(const std::wstring& function_name);
  void CallJavascriptFunction(const std::wstring& function_name,
                              const Value& arg);
  void CallJavascriptFunction(const std::wstring& function_name,
                              const Value& arg1,
                              const Value& arg2);

  ThemeProvider* GetThemeProvider() const;

  TabContents* tab_contents() { return tab_contents_; }

  Profile* GetProfile();

 protected:
  void AddMessageHandler(DOMMessageHandler* handler);

  // Options that may be overridden by individual DOM UI implementations. The
  // bool options default to false. See the public getters for more information.
  bool hide_favicon_;
  bool force_bookmark_bar_visible_;
  bool focus_location_bar_by_default_;
  bool should_hide_url_;
  string16 overridden_title_;  // Defaults to empty string.
  PageTransition::Type link_transition_type_;  // Defaults to LINK.

 private:
  // Execute a string of raw Javascript on the page.
  void ExecuteJavascript(const std::wstring& javascript);

  // Non-owning pointer to the TabContents this DOMUI is associated with.
  TabContents* tab_contents_;

  // The DOMMessageHandlers we own.
  std::vector<DOMMessageHandler*> handlers_;

  // A map of message name -> message handling callback.
  typedef std::map<std::string, MessageCallback*> MessageCallbackMap;
  MessageCallbackMap message_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(DOMUI);
};

// Messages sent from the DOM are forwarded via the DOMUI to handler
// classes. These objects are owned by DOMUI and destroyed when the
// host is destroyed.
class DOMMessageHandler {
 public:
  DOMMessageHandler() : dom_ui_(NULL) {}
  virtual ~DOMMessageHandler() {};

  // Attaches |this| to |dom_ui| in order to handle messages from it.  Declared
  // virtual so that subclasses can do special init work as soon as the dom_ui
  // is provided.  Returns |this| for convenience.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);
 protected:
  // Adds "url" and "title" keys on incoming dictionary, setting title
  // as the url as a fallback on empty title.
  static void SetURLAndTitle(DictionaryValue* dictionary,
                             std::wstring title,
                             const GURL& gurl);

  // This is where subclasses specify which messages they'd like to handle.
  virtual void RegisterMessages() = 0;

  // Extract an integer value from a Value.
  bool ExtractIntegerValue(const Value* value, int* out_int);

  // Extract a string value from a Value.
  std::wstring ExtractStringValue(const Value* value);

  DOMUI* dom_ui_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DOMMessageHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_DOM_UI_H_
