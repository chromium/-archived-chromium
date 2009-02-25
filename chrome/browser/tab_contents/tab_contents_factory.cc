// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/browser_url_handler.h"
#include "chrome/browser/dom_ui/dom_ui_contents.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/debugger/debugger_contents.h"
#include "chrome/browser/tab_contents/tab_contents_factory.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "net/base/net_util.h"

#if defined(OS_WIN)
// TODO(port): port these headers to posix.
#include "chrome/browser/dom_ui/html_dialog_contents.h"
#include "chrome/browser/tab_contents/native_ui_contents.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#elif defined(OS_POSIX)
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

typedef std::map<TabContentsType, TabContentsFactory*> TabContentsFactoryMap;
static TabContentsFactoryMap* g_extra_types;  // Only allocated if needed.

// static
TabContentsType TabContentsFactory::NextUnusedType() {
  int type = static_cast<int>(TAB_CONTENTS_NUM_TYPES);
  if (g_extra_types) {
    for (TabContentsFactoryMap::iterator i = g_extra_types->begin();
         i != g_extra_types->end(); ++i) {
      type = std::max(type, static_cast<int>(i->first));
    }
  }
  return static_cast<TabContentsType>(type + 1);
}

// static
TabContents* TabContents::CreateWithType(TabContentsType type,
                                         Profile* profile,
                                         SiteInstance* instance) {
  TabContents* contents = NULL;

  switch (type) {
    case TAB_CONTENTS_WEB:
      contents = new WebContents(profile, instance, NULL, MSG_ROUTING_NONE, NULL);
      break;
// TODO(port): remove this platform define, either by porting the tab contents
// types or removing them completely.
#if defined(OS_WIN)
    case TAB_CONTENTS_HTML_DIALOG:
      contents = new HtmlDialogContents(profile, instance, NULL);
      break;
    case TAB_CONTENTS_NATIVE_UI:
      contents = new NativeUIContents(profile);
      break;
    case TAB_CONTENTS_ABOUT_UI:
      contents = new BrowserAboutHandler(profile, instance, NULL);
      break;
    case TAB_CONTENTS_DEBUGGER:
    case TAB_CONTENTS_NEW_TAB_UI:
    case TAB_CONTENTS_DOM_UI:
      contents = new DOMUIContents(profile, instance, NULL);
      break;
#endif  // defined(OS_WIN)
    default:
      if (g_extra_types) {
        TabContentsFactoryMap::const_iterator it = g_extra_types->find(type);
        if (it != g_extra_types->end()) {
          contents = it->second->CreateInstance();
          break;
        }
      }
      NOTREACHED() << "Don't know how to create tab contents of type " << type;
      contents = NULL;
  }

  if (contents)
    contents->CreateView();

  return contents;
}

// static
TabContentsType TabContents::TypeForURL(GURL* url) {
  DCHECK(url);
  if (g_extra_types) {
    TabContentsFactoryMap::const_iterator it = g_extra_types->begin();
    for (; it != g_extra_types->end(); ++it) {
      if (it->second->CanHandleURL(*url))
        return it->first;
    }
  }

// TODO(port): port the rest of this function.
#if defined(OS_WIN)
  // Try to handle as a browser URL. If successful, |url| will end up
  // containing the real url being loaded (browser url's are just an alias).
  TabContentsType type(TAB_CONTENTS_UNKNOWN_TYPE);
  if (BrowserURLHandler::HandleBrowserURL(url, &type))
    return type;

  if (url->SchemeIs(NativeUIContents::GetScheme().c_str()))
    return TAB_CONTENTS_NATIVE_UI;

  if (HtmlDialogContents::IsHtmlDialogUrl(*url))
    return TAB_CONTENTS_HTML_DIALOG;

  if (DebuggerContents::IsDebuggerUrl(*url))
    return TAB_CONTENTS_DEBUGGER;

  if (url->SchemeIs(DOMUIContents::GetScheme().c_str()))
    return TAB_CONTENTS_DOM_UI;

#elif defined(OS_POSIX)
  NOTIMPLEMENTED();
#endif

  // NOTE: Even the empty string can be loaded by a WebContents.
  return TAB_CONTENTS_WEB;
}

// static
TabContentsFactory* TabContents::RegisterFactory(TabContentsType type,
                                                 TabContentsFactory* factory) {
  if (!g_extra_types)
    g_extra_types = new TabContentsFactoryMap;

  TabContentsFactory* prev_factory = NULL;
  TabContentsFactoryMap::const_iterator prev = g_extra_types->find(type);
  if (prev != g_extra_types->end())
    prev_factory = prev->second;

  if (factory) {
    g_extra_types->insert(TabContentsFactoryMap::value_type(type, factory));
  } else {
    g_extra_types->erase(type);
    if (g_extra_types->empty()) {
      delete g_extra_types;
      g_extra_types = NULL;
    }
  }

  return prev_factory;
}

