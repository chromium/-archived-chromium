// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
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

#include "config.h"
#include "Location.h"
#include "PlatformString.h"
#include "KURL.h"
#include "Document.h"
#include "FrameLoader.h"
#include "ScriptController.h"
#include "CSSHelper.h"
#include "Frame.h"

namespace {

// The value of the location bar while the page is still loading (before we
// have a frame).
const char* kUrlWhileLoading = "about:blank";

// Get the URL for a WebCore::Frame.  If the frame is null or the URL is not
// defined, return about:blank instead.  This mostly matches Firefox's
// behavior of returning about:blank while a URL is loading.
WebCore::KURL GetFrameUrl(WebCore::Frame* frame) {
  if (!frame)
    return WebCore::KURL(kUrlWhileLoading);

  const WebCore::KURL& url = frame->loader()->url();

  if (!url.isValid())
    return WebCore::KURL(kUrlWhileLoading);
  return url;
}

}

namespace WebCore {

#if USE(V8)
  // Notes about V8/JSC porting of this file.
  // The getter functions on this class are generic across V8/JSC.
  // The setter functions are basically custom.  In JSC, this gets separated
  // into Location.h and JSLocation.h, with the implementation in
  // JSLocationCustom.cpp.  For V8, we include the custom functions here.
  //
  // This class is not very JS-engine specific.  If we can move a couple of
  // methods to the scriptController, we should be able to unify the code
  // between JSC and V8:
  //    retrieveActiveFrame()   - in JSC, this needs an ExecState.  Is there
  //                              a static accessor?
  //    isSafeScript()
#endif

Location::Location(Frame* frame)
    : m_frame(frame)
{
}

void Location::disconnectFrame()
{
    m_frame = 0;
}

String Location::hash() const {
  if (!m_frame)
    return String();
  KURL url = GetFrameUrl(m_frame);
  return url.ref().isNull() ? "" : "#" + url.ref();
}

String Location::host() const {
  KURL url = GetFrameUrl(m_frame);

  String str = url.host();
  if (url.port())
    str += ":" + String::number((int)url.port());

  return str;
}

String Location::hostname() const {
  KURL url = GetFrameUrl(m_frame);
  return url.host();
}

String Location::href() const {
  KURL url = GetFrameUrl(m_frame);

  if (!url.hasPath())
    return url.prettyURL() + "/";
  return url.prettyURL();
}

String Location::pathname() const {
  KURL url = GetFrameUrl(m_frame);
  return url.path().isEmpty() ? "/" : url.path();
}

String Location::port() const {
  KURL url = GetFrameUrl(m_frame);
  return url.port() ? String::number((int)url.port()) : String();
}

String Location::protocol() const {
  KURL url = GetFrameUrl(m_frame);
  return url.protocol() + ":";
}

String Location::search() const {
  KURL url = GetFrameUrl(m_frame);
  return url.query();
}

String Location::toString() const {
  return href();
}

#if USE(V8)
static void navigateIfAllowed(Frame* frame, const KURL& url, bool lock_history)
{
  if (url.isEmpty())
    return;

  Frame* activeFrame = ScriptController::retrieveActiveFrame();
  if (activeFrame && !url.protocolIs("javascript")) {
    bool user_gesture = activeFrame->script()->processingUserGesture();
    frame->loader()->scheduleLocationChange(url.string(), 
      activeFrame->loader()->outgoingReferrer(), lock_history, user_gesture);
  }
}

void Location::setHash(const String& hash) {
  if (!m_frame)
    return;

  KURL url = m_frame->loader()->url();
  String old_ref = url.ref();
  String str = hash;

  if (str.startsWith("#"))
    str = str.substring(1);
  if (old_ref == str || (old_ref.isNull() && str.isEmpty()))
    return;
  url.setRef(str);

  navigateIfAllowed(m_frame, url, false);
}

void Location::setHost(const String& host) {
  if (!m_frame)
    return;

  KURL url = m_frame->loader()->url();  
  String str = host;
  String newhost = str.left(str.find(":"));
  String newport = str.substring(str.find(":") + 1);
  url.setHost(newhost);
  url.setPort(newport.toUInt());

  navigateIfAllowed(m_frame, url, false);
}

void Location::setHostname(const String& hostname) {
  if (!m_frame)
    return;

  KURL url = m_frame->loader()->url();
  url.setHost(hostname);

  navigateIfAllowed(m_frame, url, false);  
}

void Location::setHref(const String& value) {
  if (!m_frame)
    return;

  Frame* active_frame = ScriptController::retrieveActiveFrame();
  if (!active_frame)
    return;

  if (!active_frame->loader()->shouldAllowNavigation(m_frame))
    return;

  // Allows cross domain access except javascript url.
  if (!parseURL(value).startsWith("javascript:", false) ||
      ScriptController::isSafeScript(m_frame)) {
    navigateIfAllowed(m_frame, active_frame->loader()->completeURL(value), false);
  }
}

void Location::setPathname(const String& pathname) {
  if (!m_frame)
    return;

  KURL url = m_frame->loader()->url();  
  url.setPath(pathname);

  navigateIfAllowed(m_frame, url, false);    
}

void Location::setPort(const String& port) {
  if (!m_frame)
    return;

  KURL url = m_frame->loader()->url();  
  url.setPort(port.toUInt());

  navigateIfAllowed(m_frame, url, false);  
}

void Location::setProtocol(const String& protocol) {
  if (!m_frame)
    return;

  KURL url = m_frame->loader()->url();
  url.setProtocol(protocol);
  
  navigateIfAllowed(m_frame, url, false); 
}

void Location::setSearch(const String& query) {
  if (!m_frame)
    return;

  KURL url = m_frame->loader()->url();
  url.setQuery(query);
  
  navigateIfAllowed(m_frame, url, false); 
}

void Location::reload(bool forceget)
{
  if (!m_frame)
    return;

  Frame* active_frame = ScriptController::retrieveActiveFrame();
  if (!active_frame)
    return;

  if (!ScriptController::isSafeScript(m_frame))
    return;

  bool userGesture = active_frame->script()->processingUserGesture();
  m_frame->loader()->scheduleRefresh(userGesture);
}

void Location::replace(const String& url) {
  if (!m_frame)
    return;

  Frame* active_frame = ScriptController::retrieveActiveFrame();
  if (!active_frame)
    return;

  if (!active_frame->loader()->shouldAllowNavigation(m_frame))
    return;

  // Allows cross domain access except javascript url.
  if (!parseURL(url).startsWith("javascript:", false) ||
      ScriptController::isSafeScript(m_frame)) {
    navigateIfAllowed(m_frame, active_frame->loader()->completeURL(url), true);
  }
}

void Location::assign(const String& url) {
  if (!m_frame) 
    return;

  Frame* active_frame = ScriptController::retrieveActiveFrame();
  if (!active_frame)
    return;

  if (!active_frame->loader()->shouldAllowNavigation(m_frame))
    return;

  if (!parseURL(url).startsWith("javascript:", false) ||
      ScriptController::isSafeScript(m_frame)) {
    navigateIfAllowed(m_frame, active_frame->loader()->completeURL(url), false);
  }
}
#endif  // USE(V8)


}  // namespace WebCore
