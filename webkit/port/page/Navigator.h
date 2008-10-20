// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Navigator_h
#define Navigator_h

#include "Language.h"
#include "CookieJar.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Document.h"
#include "NetworkStateNotifier.h"
#include "Settings.h"
#include "PluginInfoStore.h"
#include <wtf/RefCounted.h>

// Navigator strings copied from KJS, must be updated whenever
// Apple updated theirs.
#ifndef WEBCORE_NAVIGATOR_PLATFORM
#if PLATFORM(MAC) && PLATFORM(PPC)
#define WEBCORE_NAVIGATOR_PLATFORM "MacPPC"
#elif PLATFORM(MAC) && PLATFORM(X86)
#define WEBCORE_NAVIGATOR_PLATFORM "MacIntel"
#elif PLATFORM(WIN_OS)
#define WEBCORE_NAVIGATOR_PLATFORM "Win32"
#else
#define WEBCORE_NAVIGATOR_PLATFORM ""
#endif
#endif  // ifndef WEBCORE_NAVIGATOR_PLATFORM

#ifndef WEBCORE_NAVIGATOR_PRODUCT
#define WEBCORE_NAVIGATOR_PRODUCT "Gecko"
#endif  // ifndef WEBCORE_NAVIGATOR_PRODUCT

#ifndef WEBCORE_NAVIGATOR_PRODUCT_SUB
#define WEBCORE_NAVIGATOR_PRODUCT_SUB "20030107"
#endif  // ifndef WEBCORE_NAVIGATOR_PRODUCT_SUB

#ifndef WEBCORE_NAVIGATOR_VENDOR
#define WEBCORE_NAVIGATOR_VENDOR "Apple Computer, Inc."
#endif  // ifndef WEBCORE_NAVIGATOR_VENDOR

#ifndef WEBCORE_NAVIGATOR_VENDOR_SUB
#define WEBCORE_NAVIGATOR_VENDOR_SUB ""
#endif  // ifndef WEBCORE_NAVIGATOR_VENDOR_SUB

namespace WebCore {

class Plugin;
class Navigator;

// The following two classes (MimeType and Plugin) are wrappers around
// MimeClassInfo and PluginInfo structs defined by WebKit. These are created
// by PluginInfoStore::createPluginInfoForPluginAtIndex and are responsible
// for cleaning up *info objects on deletion. The cleanup for MimeClassInfo
// as well as PluginInfo is done by Plugin wrapper because each Plugin has its
// own list of supported mime_types that should not be needed once the plugin
// is gone.
class MimeType : public RefCounted<MimeType> {
 public:
  explicit MimeType(MimeClassInfo* info, Plugin* plugin)
      : m_info(info), m_plugin(plugin) { }

  String description() const { return m_info->desc; }
  Plugin* enabledPlugin() const { return m_plugin; }
  String suffixes() const { return m_info->suffixes; }
  String type() const { return m_info->type; }
 private:
  MimeClassInfo* m_info;
  Plugin* m_plugin;
};

class Plugin : public RefCounted<Plugin> {
 public:
  explicit Plugin(PluginInfo* info) : m_info(info) {
  }
  ~Plugin() {
    if (m_info) {
      for (size_t i = 0; i < m_info->mimes.size(); i++) {
        if (m_info->mimes[i]) {
          delete m_info->mimes[i];
          m_info->mimes[i] = NULL;
        }
      }
      delete m_info;
      m_info = NULL;
    }
  }
  String description() const { return m_info->desc; }
  String filename() const { return m_info->file; }
  String name() const { return m_info->name; }
  int length() const { return m_info->mimes.size(); }

  MimeType* item(int index) {
    if (index >= 0 && index < length())
      return new MimeType(m_info->mimes[index], this);
    return NULL;
  }

  MimeType* namedItem(const String& name) {
    for (int i = 0; i < length(); ++i) {
      MimeClassInfo* m = m_info->mimes[i];
      if (m->type == name)
        return new MimeType(m, this);
    }
    return NULL;
  }
 private:
  PluginInfo* m_info;
};


template<class T>
class ArrayOf {
 public:
  ~ArrayOf() {
    // release contents
    Clear();
  }

  int length() const {
    return m_contents.size();
  }

  T* item(int index) {
    if (index >= 0 && index < length())
      return m_contents[index];
    return NULL;
  }

 protected:
  Vector<T*> m_contents;

 private:
  void Add(T* el) {
    el->ref();
    m_contents.append(el);
  }

  void Clear() {
    while (m_contents.size() > 0) {
        m_contents[0]->deref();
        m_contents.remove(0);
    }
  }

  friend class Navigator;
};

class MimeTypeArray : public ArrayOf<MimeType>,
                      public RefCounted<MimeTypeArray> {
 public:
  MimeType* namedItem(const String& name) {
    for (int i = 0; i < length(); ++i) {
      if (m_contents[i]->type() == name)
        return m_contents[i];
    }
    return NULL;
  }
};

class PluginArray : public ArrayOf<Plugin>, public RefCounted<PluginArray> {
 public:
  explicit PluginArray(Navigator* nav) : m_navigator(nav) { }
  Plugin* namedItem(const String& name) {
    for (int i = 0; i < length(); ++i) {
      if (m_contents[i]->name() == name)
        return m_contents[i];
    }
    return NULL;
  }

  void refresh(bool reloadDocuments);
 private:
  Navigator* m_navigator;
};

class Navigator : public RefCounted<Navigator> {
 public:
   static PassRefPtr<Navigator> create(Frame* frame)
   {
        return adoptRef(new Navigator(frame));
   }
  ~Navigator();
  String appCodeName() const { return "Mozilla"; }
  String appName() const { return "Netscape"; }

  String appVersion() const {
    if (!m_frame)
      return String();

    KURL url;
    if (m_frame->document()) {
      url = m_frame->document()->url();
    }
    const String user_agent = m_frame->loader()->userAgent(url);
    return user_agent.substring(user_agent.find('/') + 1);
  }
  String language() { return defaultLanguage(); }

  MimeTypeArray* mimeTypes() {
    if (!m_frame)
      return NULL;

    if (!m_mimetypes)
      Initialize(false);
    return m_mimetypes.get();
  }
  String platform() const { return WEBCORE_NAVIGATOR_PLATFORM; }
  String vendor() const { return WEBCORE_NAVIGATOR_VENDOR; }
  String vendorSub() const { return WEBCORE_NAVIGATOR_VENDOR_SUB; }
  String product() const { return WEBCORE_NAVIGATOR_PRODUCT; }
  String productSub() const { return WEBCORE_NAVIGATOR_PRODUCT_SUB; }

  PluginArray* plugins() {
    if (!m_frame)
      return NULL;

    if (!m_plugins)
      Initialize(false);
    return m_plugins.get();
  }

  String userAgent() const {
    if (!m_frame)
      return String();

    KURL url;
    if (m_frame->document()) {
      url = m_frame->document()->url();
    }
    return m_frame->loader()->userAgent(url);
  }

  bool cookieEnabled() {
    if (!m_frame)
      return false;

    return cookiesEnabled(m_frame->document());
  }

  bool javaEnabled() {
    if (!m_frame)
      return false;

    return m_frame->settings()->isJavaEnabled();
  }

  bool onLine() const {
    return networkStateNotifier().onLine();
  }

  Frame* frame() { return m_frame; }

  void disconnectFrame() { m_frame = NULL; }

 private:
  Navigator(Frame* frame);

  Frame* m_frame;
  RefPtr<MimeTypeArray> m_mimetypes;
  RefPtr<PluginArray> m_plugins;

  void Initialize(bool refresh);

  friend class PluginArray;  // need to call Refresh
  void Refresh(bool reload);
};

}  // namespace WebCore

#endif

