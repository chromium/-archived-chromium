// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDERER_SECURITY_POLICY_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDERER_SECURITY_POLICY_H_

#include <string>
#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/singleton.h"

class GURL;

// The RendererSecurityPolicy class is used to grant and revoke security
// capabilities for renderers.  For example, it restricts whether a renderer
// is permmitted to loaded file:// URLs based on whether the renderer has ever
// been commanded to load file:// URLs by the browser.
//
// RendererSecurityPolicy is a singleton that may be used on any thread.
//
class RendererSecurityPolicy {
 public:
  // There is one global RendererSecurityPolicy object for the entire browser
  // processes.  The object returned by this method may be accessed on any
  // thread.
  static RendererSecurityPolicy* GetInstance();

  // Web-safe schemes can be requested by any renderer.  Once a web-safe scheme
  // has been registered, any renderer processes can request URLs with that
  // scheme.  There is no mechanism for revoking web-safe schemes.
  void RegisterWebSafeScheme(const std::string& scheme);

  // Returns true iff |scheme| has been registered as a web-safe scheme.
  bool IsWebSafeScheme(const std::string& scheme);

  // Pseudo schemes are treated differently than other schemes because they
  // cannot be requested like normal URLs.  There is no mechanism for revoking
  // pseudo schemes.
  void RegisterPseudoScheme(const std::string& scheme);

  // Returns true iff |scheme| has been registered as pseudo scheme.
  bool IsPseudoScheme(const std::string& scheme);

  // Upon creation, render processes should register themselves by calling this
  // this method exactly once.
  void Add(int renderer_id);

  // Upon destruction, render processess should unregister themselves by caling
  // this method exactly once.
  void Remove(int renderer_id);

  // Whenever the browser processes commands the renderer to request a URL, it
  // should call this method to grant the renderer process the capability to
  // request the URL.
  void GrantRequestURL(int renderer_id, const GURL& url);

  // Whenever the user picks a file from a <input type="file"> element, the
  // browser should call this function to grant the renderer the capability to
  // upload the file to the web.
  void GrantUploadFile(int renderer_id, const std::wstring& file);

  // Whenever the browser processes commands the renderer to run web inspector,
  // it should call this method to grant the renderer process the capability to
  // run the inspector.
  void GrantInspectElement(int renderer_id);

  // Grant this renderer the ability to use DOM UI Bindings.
  void GrantDOMUIBindings(int renderer_id);

  // Before servicing a renderer's request for a URL, the browser should call
  // this method to determine whether the renderer has the capability to
  // request the URL.
  bool CanRequestURL(int renderer_id, const GURL& url);

  // Before servicing a renderer's request to upload a file to the web, the
  // browser should call this method to determine whether the renderer has the
  // capability to upload the requested file.
  bool CanUploadFile(int renderer_id, const std::wstring& file);

  // Returns true of the specified renderer_id has been granted DOMUIBindings.
  // The browser should check this property before assuming the renderer is
  // allowed to use DOMUIBindings.
  bool HasDOMUIBindings(int renderer_id);

 private:
  class SecurityState;

  typedef std::set<std::string> SchemeSet;
  typedef std::map<int, SecurityState*> SecurityStateMap;

  // Obtain an instance of RendererSecurityPolicy via GetInstance().
  RendererSecurityPolicy();
  friend struct DefaultSingletonTraits<RendererSecurityPolicy>;

  // You must acquire this lock before reading or writing any members of this
  // class.  You must not block while holding this lock.
  Lock lock_;

  // These schemes are white-listed for all renderers.  This set is protected
  // by |lock_|.
  SchemeSet web_safe_schemes_;

  // These schemes do not actually represent retrievable URLs.  For example,
  // the the URLs in the "about" scheme are aliases to other URLs.  This set is
  // protected by |lock_|.
  SchemeSet pseudo_schemes_;

  // This map holds a SecurityState for each renderer process.  The key for the
  // map is the ID of the RenderProcessHost.  The SecurityState objects are
  // owned by this object and are protected by |lock_|.  References to them must
  // not escape this class.
  SecurityStateMap security_state_;

  DISALLOW_COPY_AND_ASSIGN(RendererSecurityPolicy);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDERER_SECURITY_POLICY_H_

