// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/renderer_security_policy.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/common/url_constants.h"
#ifdef CHROME_PERSONALIZATION
#include "chrome/personalization/personalization.h"
#endif
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"

// The SecurityState class is used to maintain per-renderer security state
// information.
class RendererSecurityPolicy::SecurityState {
 public:
  SecurityState() : has_dom_ui_bindings_(false) { }

  // Grant permission to request URLs with the specified scheme.
  void GrantScheme(const std::string& scheme) {
    scheme_policy_[scheme] = true;
  }

  // Revoke permission to request URLs with the specified scheme.
  void RevokeScheme(const std::string& scheme) {
    scheme_policy_[scheme] = false;
  }

  // Grant permission to upload the specified file to the web.
  void GrantUploadFile(const std::wstring& file) {
    uploadable_files_.insert(file);
  }

  void GrantDOMUIBindings() {
    has_dom_ui_bindings_ = true;
  }

  // Determine whether permission has been granted to request url.
  // Schemes that have not been granted default to being denied.
  bool CanRequestURL(const GURL& url) {
    SchemeMap::const_iterator judgment(scheme_policy_.find(url.scheme()));

    if (judgment == scheme_policy_.end())
      return false;  // Unmentioned schemes are disallowed.

    return judgment->second;
  }

  // Determine whether permission has been granted to upload file.
  // Files that have not been granted default to being denied.
  bool CanUploadFile(const std::wstring& file) {
    return uploadable_files_.find(file) != uploadable_files_.end();
  }

  bool has_dom_ui_bindings() const { return has_dom_ui_bindings_; }

 private:
  typedef std::map<std::string, bool> SchemeMap;
  typedef std::set<std::wstring> FileSet;

  // Maps URL schemes to whether permission has been granted or revoked:
  //   |true| means the scheme has been granted.
  //   |false| means the scheme has been revoked.
  // If a scheme is not present in the map, then it has never been granted
  // or revoked.
  SchemeMap scheme_policy_;

  // The set of files the renderer is permited to upload to the web.
  FileSet uploadable_files_;

  bool has_dom_ui_bindings_;

  DISALLOW_COPY_AND_ASSIGN(SecurityState);
};

RendererSecurityPolicy::RendererSecurityPolicy() {
  // We know about these schemes and believe them to be safe.
  RegisterWebSafeScheme("http");
  RegisterWebSafeScheme("https");
  RegisterWebSafeScheme("ftp");
  RegisterWebSafeScheme("data");
  RegisterWebSafeScheme("feed");
  RegisterWebSafeScheme("chrome-extension");

  // We know about the following psuedo schemes and treat them specially.
  RegisterPseudoScheme(chrome::kAboutScheme);
  RegisterPseudoScheme(chrome::kJavaScriptScheme);
  RegisterPseudoScheme(chrome::kViewSourceScheme);
}

// static
RendererSecurityPolicy* RendererSecurityPolicy::GetInstance() {
  return Singleton<RendererSecurityPolicy>::get();
}

void RendererSecurityPolicy::Add(int renderer_id) {
  AutoLock lock(lock_);
  if (security_state_.count(renderer_id) != 0) {
    NOTREACHED() << "Add renderers at most once.";
    return;
  }

  security_state_[renderer_id] = new SecurityState();
}

void RendererSecurityPolicy::Remove(int renderer_id) {
  AutoLock lock(lock_);
  if (security_state_.count(renderer_id) != 1) {
    NOTREACHED() << "Remove renderers at most once.";
    return;
  }

  delete security_state_[renderer_id];
  security_state_.erase(renderer_id);
}

void RendererSecurityPolicy::RegisterWebSafeScheme(const std::string& scheme) {
  AutoLock lock(lock_);
  DCHECK(web_safe_schemes_.count(scheme) == 0) << "Add schemes at most once.";
  DCHECK(pseudo_schemes_.count(scheme) == 0) << "Web-safe implies not psuedo.";

  web_safe_schemes_.insert(scheme);
}

bool RendererSecurityPolicy::IsWebSafeScheme(const std::string& scheme) {
  AutoLock lock(lock_);

  return (web_safe_schemes_.find(scheme) != web_safe_schemes_.end());
}

void RendererSecurityPolicy::RegisterPseudoScheme(const std::string& scheme) {
  AutoLock lock(lock_);
  DCHECK(pseudo_schemes_.count(scheme) == 0) << "Add schemes at most once.";
  DCHECK(web_safe_schemes_.count(scheme) == 0) << "Psuedo implies not web-safe.";

  pseudo_schemes_.insert(scheme);
}

bool RendererSecurityPolicy::IsPseudoScheme(const std::string& scheme) {
  AutoLock lock(lock_);

  return (pseudo_schemes_.find(scheme) != pseudo_schemes_.end());
}

void RendererSecurityPolicy::GrantRequestURL(int renderer_id, const GURL& url) {

  if (!url.is_valid())
    return;  // Can't grant the capability to request invalid URLs.

  if (IsWebSafeScheme(url.scheme()))
    return;  // The scheme has already been white-listed for every renderer.

  if (IsPseudoScheme(url.scheme())) {
    // The view-source scheme is a special case of a pseudo URL that eventually
    // results in requesting its embedded URL.
    if (url.SchemeIs("view-source")) {
      // URLs with the view-source scheme typically look like:
      //   view-source:http://www.google.com/a
      // In order to request these URLs, the renderer needs to be able to request
      // the embedded URL.
      GrantRequestURL(renderer_id, GURL(url.path()));
    }

    return;  // Can't grant the capability to request pseudo schemes.
  }

  {
    AutoLock lock(lock_);
    SecurityStateMap::iterator state = security_state_.find(renderer_id);
    if (state == security_state_.end())
      return;

    // If the renderer has been commanded to request a scheme, then we grant
    // it the capability to request URLs of that scheme.
    state->second->GrantScheme(url.scheme());
  }
}

void RendererSecurityPolicy::GrantUploadFile(int renderer_id,
                                             const std::wstring& file) {
  AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(renderer_id);
  if (state == security_state_.end())
    return;

  state->second->GrantUploadFile(file);
}

void RendererSecurityPolicy::GrantInspectElement(int renderer_id) {
  AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(renderer_id);
  if (state == security_state_.end())
    return;

  // The inspector is served from a chrome-ui: URL.  In order to run the
  // inspector, the renderer needs to be able to load chrome-ui URLs.
  state->second->GrantScheme("chrome-ui");
}

void RendererSecurityPolicy::GrantDOMUIBindings(int renderer_id) {
  AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(renderer_id);
  if (state == security_state_.end())
    return;

  state->second->GrantDOMUIBindings();

  // DOM UI bindings need the ability to request chrome-ui URLs.
  state->second->GrantScheme("chrome-ui");

  // DOM UI pages can contain links to file:// URLs.
  state->second->GrantScheme("file");
}

bool RendererSecurityPolicy::CanRequestURL(int renderer_id, const GURL& url) {
  if (!url.is_valid())
    return false;  // Can't request invalid URLs.

  if (IsWebSafeScheme(url.scheme()))
    return true;  // The scheme has been white-listed for every renderer.

  if (IsPseudoScheme(url.scheme())) {
    // There are a number of special cases for pseudo schemes.

    if (url.SchemeIs("view-source")) {
      // A view-source URL is allowed if the renderer is permitted to request
      // the embedded URL.
      return CanRequestURL(renderer_id, GURL(url.path()));
    }

    if (LowerCaseEqualsASCII(url.spec(), "about:blank"))
      return true;  // Every renderer can request <about:blank>.

    // URLs like <about:memory> and <about:crash> shouldn't be requestable by
    // any renderer.  Also, this case covers <javascript:...>, which should be
    // handled internally by the renderer and not kicked up to the browser.
    return false;
  }

#ifdef CHROME_PERSONALIZATION
  if (url.SchemeIs(kPersonalizationScheme))
    return true;
#endif

  if (!URLRequest::IsHandledURL(url))
    return true;  // This URL request is destined for ShellExecute.

  {
    AutoLock lock(lock_);

    SecurityStateMap::iterator state = security_state_.find(renderer_id);
    if (state == security_state_.end())
      return false;

    // Otherwise, we consult the renderer's security state to see if it is
    // allowed to request the URL.
    return state->second->CanRequestURL(url);
  }
}

bool RendererSecurityPolicy::CanUploadFile(int renderer_id,
                                           const std::wstring& file) {
  AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(renderer_id);
  if (state == security_state_.end())
    return false;

  return state->second->CanUploadFile(file);
}

bool RendererSecurityPolicy::HasDOMUIBindings(int renderer_id) {
  AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(renderer_id);
  if (state == security_state_.end())
    return false;

  return state->second->has_dom_ui_bindings();
}

