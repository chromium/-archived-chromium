// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/child_process_security_policy.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"

// The SecurityState class is used to maintain per-renderer security state
// information.
class ChildProcessSecurityPolicy::SecurityState {
 public:
  SecurityState() : enabled_bindings_(0) { }
  ~SecurityState() {
    scheme_policy_.clear();
  }

  // Grant permission to request URLs with the specified scheme.
  void GrantScheme(const std::string& scheme) {
    scheme_policy_[scheme] = true;
  }

  // Revoke permission to request URLs with the specified scheme.
  void RevokeScheme(const std::string& scheme) {
    scheme_policy_[scheme] = false;
  }

  // Grant permission to upload the specified file to the web.
  void GrantUploadFile(const FilePath& file) {
    uploadable_files_.insert(file);
  }

  void GrantBindings(int bindings) {
    enabled_bindings_ |= bindings;
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
  bool CanUploadFile(const FilePath& file) {
    return uploadable_files_.find(file) != uploadable_files_.end();
  }

  bool has_dom_ui_bindings() const {
    return BindingsPolicy::is_dom_ui_enabled(enabled_bindings_);
  }

  bool has_extension_bindings() const {
    return BindingsPolicy::is_extension_enabled(enabled_bindings_);
  }

 private:
  typedef std::map<std::string, bool> SchemeMap;
  typedef std::set<FilePath> FileSet;

  // Maps URL schemes to whether permission has been granted or revoked:
  //   |true| means the scheme has been granted.
  //   |false| means the scheme has been revoked.
  // If a scheme is not present in the map, then it has never been granted
  // or revoked.
  SchemeMap scheme_policy_;

  // The set of files the renderer is permited to upload to the web.
  FileSet uploadable_files_;

  int enabled_bindings_;

  DISALLOW_COPY_AND_ASSIGN(SecurityState);
};

ChildProcessSecurityPolicy::ChildProcessSecurityPolicy() {
  // We know about these schemes and believe them to be safe.
  RegisterWebSafeScheme(chrome::kHttpScheme);
  RegisterWebSafeScheme(chrome::kHttpsScheme);
  RegisterWebSafeScheme(chrome::kFtpScheme);
  RegisterWebSafeScheme(chrome::kDataScheme);
  RegisterWebSafeScheme("feed");
  RegisterWebSafeScheme("chrome-extension");

  // We know about the following psuedo schemes and treat them specially.
  RegisterPseudoScheme(chrome::kAboutScheme);
  RegisterPseudoScheme(chrome::kJavaScriptScheme);
  RegisterPseudoScheme(chrome::kViewSourceScheme);
  RegisterPseudoScheme(chrome::kPrintScheme);
}

ChildProcessSecurityPolicy::~ChildProcessSecurityPolicy() {
  web_safe_schemes_.clear();
  pseudo_schemes_.clear();
  STLDeleteContainerPairSecondPointers(security_state_.begin(),
                                       security_state_.end());
  security_state_.clear();
}

// static
ChildProcessSecurityPolicy* ChildProcessSecurityPolicy::GetInstance() {
  return Singleton<ChildProcessSecurityPolicy>::get();
}

void ChildProcessSecurityPolicy::Add(int renderer_id) {
  AutoLock lock(lock_);
  if (security_state_.count(renderer_id) != 0) {
    NOTREACHED() << "Add renderers at most once.";
    return;
  }

  security_state_[renderer_id] = new SecurityState();
}

void ChildProcessSecurityPolicy::Remove(int renderer_id) {
  AutoLock lock(lock_);
  if (!security_state_.count(renderer_id))
    return;  // May be called multiple times.

  delete security_state_[renderer_id];
  security_state_.erase(renderer_id);
}

void ChildProcessSecurityPolicy::RegisterWebSafeScheme(const std::string& scheme) {
  AutoLock lock(lock_);
  DCHECK(web_safe_schemes_.count(scheme) == 0) << "Add schemes at most once.";
  DCHECK(pseudo_schemes_.count(scheme) == 0) << "Web-safe implies not psuedo.";

  web_safe_schemes_.insert(scheme);
}

bool ChildProcessSecurityPolicy::IsWebSafeScheme(const std::string& scheme) {
  AutoLock lock(lock_);

  return (web_safe_schemes_.find(scheme) != web_safe_schemes_.end());
}

void ChildProcessSecurityPolicy::RegisterPseudoScheme(const std::string& scheme) {
  AutoLock lock(lock_);
  DCHECK(pseudo_schemes_.count(scheme) == 0) << "Add schemes at most once.";
  DCHECK(web_safe_schemes_.count(scheme) == 0) <<
      "Psuedo implies not web-safe.";

  pseudo_schemes_.insert(scheme);
}

bool ChildProcessSecurityPolicy::IsPseudoScheme(const std::string& scheme) {
  AutoLock lock(lock_);

  return (pseudo_schemes_.find(scheme) != pseudo_schemes_.end());
}

void ChildProcessSecurityPolicy::GrantRequestURL(int renderer_id, const GURL& url) {

  if (!url.is_valid())
    return;  // Can't grant the capability to request invalid URLs.

  if (IsWebSafeScheme(url.scheme()))
    return;  // The scheme has already been white-listed for every renderer.

  if (IsPseudoScheme(url.scheme())) {
    // The view-source and print schemes are a special case of a pseudo URL that
    // eventually results in requesting its embedded URL.
    if (url.SchemeIs(chrome::kViewSourceScheme) ||
        url.SchemeIs(chrome::kPrintScheme)) {
      // URLs with the view-source and print schemes typically look like:
      //   view-source:http://www.google.com/a
      // In order to request these URLs, the renderer needs to be able to
      // request the embedded URL.
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

void ChildProcessSecurityPolicy::GrantUploadFile(int renderer_id,
                                             const FilePath& file) {
  AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(renderer_id);
  if (state == security_state_.end())
    return;

  state->second->GrantUploadFile(file);
}

void ChildProcessSecurityPolicy::GrantInspectElement(int renderer_id) {
  AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(renderer_id);
  if (state == security_state_.end())
    return;

  // The inspector is served from a chrome: URL.  In order to run the
  // inspector, the renderer needs to be able to load chrome: URLs.
  state->second->GrantScheme(chrome::kChromeUIScheme);
}

void ChildProcessSecurityPolicy::GrantDOMUIBindings(int renderer_id) {
  AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(renderer_id);
  if (state == security_state_.end())
    return;

  state->second->GrantBindings(BindingsPolicy::DOM_UI);

  // DOM UI bindings need the ability to request chrome: URLs.
  state->second->GrantScheme(chrome::kChromeUIScheme);

  // DOM UI pages can contain links to file:// URLs.
  state->second->GrantScheme(chrome::kFileScheme);
}

void ChildProcessSecurityPolicy::GrantExtensionBindings(int renderer_id) {
  AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(renderer_id);
  if (state == security_state_.end())
    return;

  state->second->GrantBindings(BindingsPolicy::EXTENSION);
}

bool ChildProcessSecurityPolicy::CanRequestURL(int renderer_id, const GURL& url) {
  if (!url.is_valid())
    return false;  // Can't request invalid URLs.

  if (IsWebSafeScheme(url.scheme()))
    return true;  // The scheme has been white-listed for every renderer.

  if (IsPseudoScheme(url.scheme())) {
    // There are a number of special cases for pseudo schemes.

    if (url.SchemeIs(chrome::kViewSourceScheme) ||
        url.SchemeIs(chrome::kPrintScheme)) {
      // View-source and print URL's are allowed if the renderer is permitted
      // to request the embedded URL.
      return CanRequestURL(renderer_id, GURL(url.path()));
    }

    if (LowerCaseEqualsASCII(url.spec(), chrome::kAboutBlankURL))
      return true;  // Every renderer can request <about:blank>.

    // URLs like <about:memory> and <about:crash> shouldn't be requestable by
    // any renderer.  Also, this case covers <javascript:...>, which should be
    // handled internally by the renderer and not kicked up to the browser.
    return false;
  }

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

bool ChildProcessSecurityPolicy::CanUploadFile(int renderer_id,
                                           const FilePath& file) {
  AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(renderer_id);
  if (state == security_state_.end())
    return false;

  return state->second->CanUploadFile(file);
}

bool ChildProcessSecurityPolicy::HasDOMUIBindings(int renderer_id) {
  AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(renderer_id);
  if (state == security_state_.end())
    return false;

  return state->second->has_dom_ui_bindings();
}

bool ChildProcessSecurityPolicy::HasExtensionBindings(int renderer_id) {
  AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(renderer_id);
  if (state == security_state_.end())
    return false;

  return state->second->has_extension_bindings();
}
