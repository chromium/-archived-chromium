// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Proxy objects for exposing Chrome internals to V8.  Adds some convenience
// methods to simplify properties, indexes and functions, as well as helping
// with object lifetime bi-directionally.

// TODO: this code is temporary and will be converted to use IDL
// Also note that it's missing a lot of functionality and isn't correct.
// For example, objects aren't being cached properly (browser.foo = 1 wouldn't
// be remembered), and setters aren't implemented to begin with.

#ifndef CHROME_BROWSER_DEBUGGER_DEBUGGER_NODE_H_
#define CHROME_BROWSER_DEBUGGER_DEBUGGER_NODE_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/common/notification_observer.h"
#include "v8/include/v8.h"

class Browser;
class TabContents;
class DebuggerShell;
class NotificationService;
class WebContents;

class DebuggerNode : public NotificationObserver {
 public:
  DebuggerNode();
  virtual ~DebuggerNode();

  // does your object handle array references? (e.g. myobj[0])
  virtual bool IsCollection() = 0;
  // does your object work as a function (e.g. myobj())
  virtual bool IsFunction() = 0;
  // does your object contain other named properties? (e.g. myobj.foo)
  virtual bool IsObject() = 0;

  // Is the underlying C++ object valid or not?  It's possible for the JS object
  // to be alive after the underlying C++ object has gone away.  In that case,
  // the DebuggerNode stays around but is marked as invalid.
  bool IsValid() { return valid_; }
  virtual void Invalidate() { valid_ = false; }

  // Callback for DebuggerNode subclasses which use the NotificationService to
  // track object validity.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
  void StopObserving();
  virtual void StopObserving(NotificationService *service);

  // Index getter callback from V8 for objects where IsCollection is true
  virtual v8::Handle<v8::Value> IndexGetter(uint32_t index,
                                            const v8::AccessorInfo& info);
  // Index getter callback from V8 for objects where IsObject is true
  virtual v8::Handle<v8::Value> PropGetter(v8::Handle<v8::String> prop,
                                           const v8::AccessorInfo& info);
  // Functor callback from V8 for objects where IsFunction is true
  virtual v8::Handle<v8::Value> Function(const v8::Arguments& args);

  // Create a new instance of this JS object.
  virtual v8::Handle<v8::Value> NewInstance();

  // Generic DebuggerNode named property getter
  static v8::Handle<v8::Value> NodeGetter(v8::Local<v8::String> prop,
                                          const v8::AccessorInfo& info);
  // Generic DebuggerNode index getter
  static v8::Handle<v8::Value> NodeIndex(uint32_t index,
                                         const v8::AccessorInfo& info);
  // Generic DebuggerNode functor
  static v8::Handle<v8::Value> NodeFunc(const v8::Arguments& args);

 protected:
  void *data_;
  bool valid_;
  bool observing_;

 private:
};

// A wrapper around the proxy to handle two issues:
// - call virtual methods to stop observing at destruction time
// - call virtual methods during callbacks from V8 after a static_cast
//   from void*
// The point here is that we'd like to be able to stick DebuggerNode* objects
// into V8.  To do that, we need to cast them to void*, which means we need
// this additional layer of wrapper to protect them from the harmful effects
// of static_cast.  Rather than passing in a DebuggerNode*, we instead pass in
// a DebuggerNodeWrapper*.  Since this is what's being referenced by V8, we
// also handle lifetime issues (RefCounted) in the wrapper.
class DebuggerNodeWrapper : public base::RefCounted<DebuggerNodeWrapper> {
 public:
  DebuggerNodeWrapper(DebuggerNode* node) : node_(node) {}
  virtual ~DebuggerNodeWrapper() {
    node_->StopObserving();
    delete node_;
  }
  DebuggerNode* node() { return node_; }
 private:
  DebuggerNode* node_;
};

// top level chrome object implements:
// * pid() - process id of chrome browser process
// * browser[] - returns collection of browser objects
class ChromeNode : public DebuggerNode {
 public:
  ChromeNode(DebuggerShell* debugger);
  virtual ~ChromeNode();

  bool IsCollection() { return false; }
  bool IsFunction() { return false; }
  bool IsObject() { return true; }

  virtual v8::Handle<v8::Value> PropGetter(v8::Handle<v8::String> prop,
                                           const v8::AccessorInfo& info);

  virtual void StopObserving(NotificationService *service);

 private:

  DebuggerShell* debugger_;
};

// browser collection, simply returns the n'th browser from BrowserList
class BrowserListNode : public DebuggerNode {
 public:
  bool IsCollection() { return true; }
  bool IsFunction() { return false; }
  bool IsObject() { return false; }

  virtual v8::Handle<v8::Value> IndexGetter(uint32_t index,
                                            const v8::AccessorInfo& info);
  virtual void StopObserving(NotificationService *service);

  static BrowserListNode* BrowserList();

 private:
  BrowserListNode();
  virtual ~BrowserListNode();
};

// Wrapper around Browser object.  implements:
// * title - title of the current tab
// * tab[] - collection of tabs
class BrowserNode : public DebuggerNode {
 public:
  bool IsCollection() { return false; }
  bool IsFunction() { return false; }
  bool IsObject() { return true; }

  static BrowserNode* BrowserAtIndex(int index);

  virtual void StopObserving(NotificationService *service);

  virtual v8::Handle<v8::Value> PropGetter(v8::Handle<v8::String> prop,
                                           const v8::AccessorInfo& info);

 private:
  BrowserNode(Browser* b);
  Browser* GetBrowser();
  virtual ~BrowserNode();
};

// tab collection, simply returns the n'th TabContents from Browser
class TabListNode : public DebuggerNode {
 public:
  bool IsCollection() { return true; }
  bool IsFunction() { return false; }
  bool IsObject() { return false; }

  virtual void StopObserving(NotificationService *service);

  virtual v8::Handle<v8::Value> IndexGetter(uint32_t index,
                                            const v8::AccessorInfo& info);
  static TabListNode* TabList(Browser* b);

 private:
  TabListNode(Browser* b);
  virtual ~TabListNode();
  Browser* GetBrowser();
};

// Wrapper around TabContents.  Implements:
// * title - tab title
// * attach - starts debugging in this tab (currently this just means log msgs)
// * detach - stops debugging in this tab
// * eval(xpath, expr), eval(expr) - evaluates JS expr in xpath iframe context
class TabNode : public DebuggerNode {
 public:
  bool IsCollection() { return false; }
  bool IsFunction() { return false; }
  bool IsObject() { return true; }

  TabNode(TabContents* c);
  virtual void StopObserving(NotificationService* service);
  virtual v8::Handle<v8::Value> PropGetter(v8::Handle<v8::String> prop,
                                           const v8::AccessorInfo& info);
 private:

  static v8::Handle<v8::Value> SendToDebugger(const v8::Arguments& args,
                                              WebContents* data);
  static v8::Handle<v8::Value> Attach(const v8::Arguments& args,
                                      WebContents* data);
  static v8::Handle<v8::Value> Detach(const v8::Arguments& args,
                                      WebContents* data);
  static v8::Handle<v8::Value> Break(const v8::Arguments& args,
                                     WebContents* data);

  virtual ~TabNode();
  TabContents* GetTab();
};

template<class T>
class FunctionNode : public DebuggerNode {
 public:
  bool IsCollection() { return false; }
  bool IsFunction() { return true; }
  bool IsObject() { return false; }

  typedef v8::Handle<v8::Value> (*Callback)(const v8::Arguments& args, T* data);

  FunctionNode(Callback f, T* data) :
      function_(f), data_(data) {};

private:
  // Functor callback from V8 for objects where IsFunction is true
  virtual v8::Handle<v8::Value> Function(const v8::Arguments& args);

  Callback function_;
  T* data_;
};

#endif // CHROME_BROWSER_DEBUGGER_DEBUGGER_NODE_H_
