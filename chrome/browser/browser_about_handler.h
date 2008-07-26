// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Contains code for handling "about:" URLs in the browser process.

#ifndef CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H__
#define CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H__

#include <string>

#include "base/basictypes.h"
#include "base/image_util.h"
#include "chrome/browser/web_contents.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"

class GURL;
class Profile;
class ListValue;
class DictionaryValue;
class RenderProcessHost;
class AboutSource;
enum TabContentsType;
namespace process_util {
  struct CommittedKBytes;
  struct WorkingSetKBytes;
}

class BrowserAboutHandler : public WebContents {
 public:
  BrowserAboutHandler(Profile* profile,
                      SiteInstance* instance,
                      RenderViewHostFactory* render_view_factory);
  virtual ~BrowserAboutHandler() {}

  // We don't want a favicon on the about pages.
  virtual bool ShouldDisplayFavIcon() { return false; }
  // Enable javascript urls for the about pages.
  virtual bool BrowserAboutHandler::SupportsURL(GURL* url);

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
  static std::string AboutHistograms(const std::string query);

  // Renders a special page about:objects (about tracked objects such as Tasks).
  static std::string AboutObjects(const std::string& query);

  // Renders a special page for about:dns.
  static std::string AboutDns();

  // Renders a special page for about:stats.
  static std::string AboutStats();

  // Renders a special page for about:memory which displays
  // information about current state.
  static void AboutMemory(AboutSource*, int request_id);

 private:
  ChromeURLDataManager::DataSource* about_source_;
  DISALLOW_EVIL_CONSTRUCTORS(BrowserAboutHandler);
};

#endif  // CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H__
