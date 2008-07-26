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

#include "chrome/browser/views/options/options_page_view.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/common/pref_service.h"

///////////////////////////////////////////////////////////////////////////////
// OptionsPageView

OptionsPageView::OptionsPageView(Profile* profile)
    : profile_(profile),
      initialized_(false) {
}

OptionsPageView::~OptionsPageView() {
}

void OptionsPageView::UserMetricsRecordAction(const wchar_t* action,
                                              PrefService* prefs) {
  UserMetrics::RecordComputedAction(action, profile());
  if (prefs)
    prefs->ScheduleSavePersistentPrefs(g_browser_process->file_thread());
}

///////////////////////////////////////////////////////////////////////////////
// OptionsPageView, NotificationObserver implementation:

void OptionsPageView::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  if (type == NOTIFY_PREF_CHANGED)
    NotifyPrefChanged(Details<std::wstring>(details).ptr());
}

///////////////////////////////////////////////////////////////////////////////
// OptionsPageView, ChromeViews::View overrides:

void OptionsPageView::ViewHierarchyChanged(bool is_add,
                                           ChromeViews::View* parent,
                                           ChromeViews::View* child) {
  if (!initialized_ && is_add && GetViewContainer()) {
    // It is important that this only get done _once_ otherwise we end up
    // duplicating the view hierarchy when tabs are switched.
    initialized_ = true;
    InitControlLayout();
    NotifyPrefChanged(NULL);
  }
}

HWND OptionsPageView::GetRootWindow() const {
  // Our ViewContainer is the TabbedPane content HWND, which is a child HWND.
  // We need the root HWND for parenting.
  return GetAncestor(GetViewContainer()->GetHWND(), GA_ROOT);
}