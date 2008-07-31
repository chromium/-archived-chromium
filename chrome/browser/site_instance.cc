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

#include "chrome/browser/site_instance.h"

#include "net/base/registry_controlled_domain.h"

SiteInstance::~SiteInstance() {
  // Now that no one is referencing us, we can safely remove ourselves from
  // the BrowsingInstance.  Any future visits to a page from this site
  // (within the same BrowsingInstance) can safely create a new SiteInstance.
  if (has_site_)
    browsing_instance_->UnregisterSiteInstance(this);
}

RenderProcessHost* SiteInstance::GetProcess() {
  RenderProcessHost* process = NULL;
  if (process_host_id_ != -1)
    process = RenderProcessHost::FromID(process_host_id_);

  // Create a new process if ours went away or was reused.
  if (!process) {
    // See if we should reuse an old process
    if (RenderProcessHost::ShouldTryToUseExistingProcessHost())
      process = RenderProcessHost::GetExistingProcessHost(
          browsing_instance_->profile());

    // Otherwise (or if that fails), create a new one.
    if (!process)
      process = new RenderProcessHost(browsing_instance_->profile());

    // Update our host ID, so all pages in this SiteInstance will use
    // the correct process.
    process_host_id_ = process->host_id();

    // Make sure the process starts at the right max_page_id
    process->UpdateMaxPageID(max_page_id_);
  }
  DCHECK(process);

  return process;
}

void SiteInstance::SetSite(const GURL& url) {
  // A SiteInstance's site should not change.
  // TODO(creis): When following links or script navigations, we can currently
  // render pages from other sites in this SiteInstance.  This will eventually
  // be fixed, but until then, we should still not set the site of a
  // SiteInstance more than once.
  DCHECK(!has_site_);

  // Remember that this SiteInstance has been used to load a URL, even if the
  // URL is invalid.
  has_site_ = true;
  site_ = GetSiteForURL(url);

  // Now that we have a site, register it with the BrowsingInstance.  This
  // ensures that we won't create another SiteInstance for this site within
  // the same BrowsingInstance, because all same-site pages within a
  // BrowsingInstance can script each other.
  browsing_instance_->RegisterSiteInstance(this);
}

bool SiteInstance::HasRelatedSiteInstance(const GURL& url) {
  return browsing_instance_->HasSiteInstance(url);
}

SiteInstance* SiteInstance::GetRelatedSiteInstance(const GURL& url) {
  return browsing_instance_->GetSiteInstanceForURL(url);
}

/*static*/
SiteInstance* SiteInstance::CreateSiteInstance(Profile* profile) {
  return new SiteInstance(new BrowsingInstance(profile));
}

/*static*/
GURL SiteInstance::GetSiteForURL(const GURL& url) {
  // URLs with no host should have an empty site.
  GURL site;

  // TODO(creis): For many protocols, we should just treat the scheme as the
  // site, since there is no host.  e.g., file:, about:, chrome-resource:

  // If the url has a host, then determine the site.
  if (url.has_host()) {
    // Only keep the scheme, registered domain, and port as given by GetOrigin.
    site = url.GetOrigin();

    // If this URL has a registered domain, we only want to remember that part.
    std::string domain =
        net::RegistryControlledDomainService::GetDomainAndRegistry(url);
    if (!domain.empty()) {
      GURL::Replacements rep;
      rep.SetHostStr(domain);
      site = site.ReplaceComponents(rep);
    }
  }
  return site;
}

/*static*/
bool SiteInstance::IsSameWebSite(const GURL& url1, const GURL& url2) {
  // We infer web site boundaries based on the registered domain name of the
  // top-level page, as well as the scheme and the port.

  // We must treat javascript: URLs as part of the same site, regardless of
  // the site.
  if (url1.SchemeIs("javascript") || url2.SchemeIs("javascript"))
    return true;

  // We treat about:crash, about:hang, and about:shorthang as the same site as
  // any URL, since they are used as demos for crashing/hanging a process.
  GURL about_crash = GURL("about:crash");
  GURL about_hang = GURL("about:hang");
  GURL about_shorthang = GURL("about:shorthang");
  if (url1 == about_crash || url2 == about_crash ||
	  url1 == about_hang || url2 == about_hang ||
	  url1 == about_shorthang || url2 == about_shorthang)
    return true;

  // If either URL is invalid, they aren't part of the same site.
  if (!url1.is_valid() || !url2.is_valid()) {
    return false;
  }

  // If the scheme or port differ, they aren't part of the same site.
  if (url1.scheme() != url2.scheme() || url1.port() != url2.port()) {
    return false;
  }

  return net::RegistryControlledDomainService::SameDomainOrHost(url1, url2);
}
