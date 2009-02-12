// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains code for handling "about:" URLs in the browser process.

#ifndef CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H_
#define CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/tab_contents/tab_contents_type.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"

class AboutSource;
class DictionaryValue;
class GURL;
class ListValue;
class Profile;
class RenderProcessHost;
class RenderViewHostFactory;
class SiteInstance;

class BrowserAboutHandler : public WebContents {
 public:
  BrowserAboutHandler(Profile* profile,
                      SiteInstance* instance,
                      RenderViewHostFactory* render_view_factory);
  virtual ~BrowserAboutHandler() {}

  // We don't want a favicon on the about pages.
  virtual bool ShouldDisplayFavIcon() { return false; }
  // Enable javascript urls for the about pages.
  virtual bool SupportsURL(GURL* url);

  // If |url| is a known "about:" URL, this method handles it
  // and sets |url| to an alternate URL indicating the real content to load.
  // (If it's not a URL that the function can handle, it's a no-op and returns
  // false.)
  static bool MaybeHandle(GURL* url, TabContentsType* type);

  // Renders a special page for "about:" which displays version information.
  static std::string AboutVersion();

  // Renders a special page for about:plugins.
  static std::string AboutPlugins();

  // Renders a special page for about:histograms.
  static std::string AboutHistograms(const std::string& query);

  // Renders a special page about:objects (about tracked objects such as Tasks).
  static std::string AboutObjects(const std::string& query);

  // Renders a special page for about:dns.
  static std::string AboutDns();

  // Renders a special page for about:stats.
  static std::string AboutStats();

  // Renders a special page for "about:credits" which displays our
  // acknowledgements and legal information for code we depend on.
  static std::string AboutCredits();

  // Renders a special page for "about:terms" which displays our
  // terms and conditions.
  static std::string AboutTerms();

  // Renders a special page for about:memory which displays
  // information about current state.
  static void AboutMemory(AboutSource*, int request_id);

 private:
  ChromeURLDataManager::DataSource* about_source_;
  DISALLOW_COPY_AND_ASSIGN(BrowserAboutHandler);
};

#endif  // CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H_
