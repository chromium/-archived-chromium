// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains stub implementations of the functions declared in
// browser_dialogs.h that are currently unimplemented in GTK-views.

#include "base/logging.h"
#include "chrome/browser/views/browser_dialogs.h"

namespace browser {

void ShowBugReportView(views::Widget* parent,
                       Profile* profile,
                       TabContents* tab) {
  NOTIMPLEMENTED();
}

void ShowClearBrowsingDataView(views::Widget* parent,
                               Profile* profile) {
  NOTIMPLEMENTED();
}

void ShowSelectProfileDialog() {
  NOTIMPLEMENTED();
}

void ShowImporterView(views::Widget* parent,
                      Profile* profile) {
  NOTIMPLEMENTED();
}

void ShowBookmarkBubbleView(views::Window* parent,
                            const gfx::Rect& bounds,
                            InfoBubbleDelegate* delegate,
                            Profile* profile,
                            const GURL& url,
                            bool newly_bookmarked) {
  NOTIMPLEMENTED();
}

void HideBookmarkBubbleView() {
  NOTIMPLEMENTED();
}

bool IsBookmarkBubbleViewShowing() {
  NOTIMPLEMENTED();
  return false;
}

void ShowBookmarkManagerView(Profile* profile) {
  NOTIMPLEMENTED();
}

void ShowAboutChromeView(views::Widget* parent,
                         Profile* profile) {
  NOTIMPLEMENTED();
}

void ShowHtmlDialogView(gfx::NativeWindow parent, Browser* browser,
                        HtmlDialogUIDelegate* delegate) {
  NOTIMPLEMENTED();
}

FindBar* CreateFindBar(BrowserView* browser_view) {
  NOTIMPLEMENTED();
  return NULL;
}

void ShowPasswordsExceptionsWindowView(Profile* profile) {
  NOTIMPLEMENTED();
}

void ShowKeywordEditorView(Profile* profile) {
  NOTIMPLEMENTED();
}

void ShowNewProfileDialog() {
  NOTIMPLEMENTED();
}

void ShowTaskManager() {
  NOTIMPLEMENTED();
}

void EditSearchEngine(gfx::NativeWindow parent,
                      const TemplateURL* template_url,
                      EditSearchEngineControllerDelegate* delegate,
                      Profile* profile) {
  NOTIMPLEMENTED();
}

}  // namespace browser
