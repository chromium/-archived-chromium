// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_H__
#define CHROME_BROWSER_DOM_UI_H__

#include "base/task.h"
#include "chrome/browser/dom_ui/dom_ui_contents.h"

class DictionaryValue;
class DOMMessageHandler;
class RenderViewHost;
class Value;

// A DOMUI sets up the datasources and message handlers for a given HTML-based
// UI. It is contained by a DOMUIContents.
class DOMUI {
 public:
  DOMUI(DOMUIContents* contents);

  virtual ~DOMUI();
  virtual void Init() = 0;

  virtual void RenderViewCreated(RenderViewHost* render_view_host) {}

  // Called from DOMUIContents.
  void ProcessDOMUIMessage(const std::string& message,
                           const std::string& content);

  // Used by DOMMessageHandlers.
  typedef Callback1<const Value*>::Type MessageCallback;
  void RegisterMessageCallback (const std::string& message,
                                MessageCallback* callback);

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

  // Overriddable control over the contents.
  // Favicon should be displayed normally.
  virtual bool ShouldDisplayFavIcon() { return true; }
  // No special bookmark bar behavior
  virtual bool IsBookmarkBarAlwaysVisible() { return false; }
  // When NTP gets the initial focus, focus the URL bar.
  virtual void SetInitialFocus(bool reverse);
  // Whether we want to display the page's URL.
  virtual bool ShouldDisplayURL() { return true; }
  // Hide the referrer.
  virtual void RequestOpenURL(const GURL& url, const GURL&,
      WindowOpenDisposition disposition);

  DOMUIContents* get_contents() { return contents_; }
  Profile* get_profile() { return contents_->profile(); }

 protected:
  void AddMessageHandler(DOMMessageHandler* handler);

  DOMUIContents* contents_;

 private:
  // Execute a string of raw Javascript on the page.
  void ExecuteJavascript(const std::wstring& javascript);

  // The DOMMessageHandlers we own.
  std::vector<DOMMessageHandler*> handlers_;

  // A map of message name -> message handling callback.
  typedef std::map<std::string, MessageCallback*> MessageCallbackMap;
  MessageCallbackMap message_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(DOMUI);
};

// Messages sent from the DOM are forwarded via the DOMUIContents to handler
// classes. These objects are owned by DOMUIHost and destroyed when the
// host is destroyed.
class DOMMessageHandler {
 public:
  explicit DOMMessageHandler(DOMUI* dom_ui);
  virtual ~DOMMessageHandler() {};

 protected:
  // Adds "url" and "title" keys on incoming dictionary, setting title
  // as the url as a fallback on empty title.
  static void SetURLAndTitle(DictionaryValue* dictionary, 
                             std::wstring title,
                             const GURL& gurl);

  // Extract an integer value from a Value.
  bool ExtractIntegerValue(const Value* value, int* out_int);

  // Extract a string value from a Value.
  std::wstring ExtractStringValue(const Value* value);

  DOMUI* const dom_ui_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DOMMessageHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_H__
