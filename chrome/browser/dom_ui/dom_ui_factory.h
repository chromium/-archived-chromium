// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_DOM_UI_FACTORY_H_
#define CHROME_BROWSER_DOM_UI_DOM_UI_FACTORY_H_

class DOMUI;
class GURL;
class WebContents;

class DOMUIFactory {
 public:
  // Returns true if the given URL's scheme would trigger the DOM UI system.
  // This is a less precise test than UseDONUIForURL, which tells you whether
  // that specific URL matches a known one. This one is faster and can be used
  // to determine security policy.
  static bool HasDOMUIScheme(const GURL& url);

  // Returns true if the given URL will use the DOM UI system.
  static bool UseDOMUIForURL(const GURL& url);

  // Allocates a new DOMUI object for the given URL, and returns it. If the URL
  // is not a DOM UI URL, then it will return NULL. When non-NULL, ownership of
  // the returned pointer is passed to the caller.
  static DOMUI* CreateDOMUIForURL(WebContents* web_contents, const GURL& url);

 private:
  // Class is for scoping only.
  DOMUIFactory() {};
};

#endif  // CHROME_BROWSER_DOM_UI_DOM_UI_FACTORY_H_
