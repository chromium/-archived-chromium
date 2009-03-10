// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/html_dialog_view.h"

#include "chrome/browser/browser.h"
#include "chrome/views/root_view.h"
#include "chrome/views/window.h"

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView, public:

HtmlDialogView::HtmlDialogView(Browser* parent_browser,
                               Profile* profile,
                               HtmlDialogContentsDelegate* delegate)
    : DOMView(delegate->GetDialogContentURL()),
      parent_browser_(parent_browser),
      profile_(profile),
      delegate_(delegate) {
  DCHECK(parent_browser);
  DCHECK(profile);
}

HtmlDialogView::~HtmlDialogView() {
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView, views::View implementation:

gfx::Size HtmlDialogView::GetPreferredSize() {
  gfx::Size out;
  delegate_->GetDialogSize(&out);
  return out;
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView, views::WindowDelegate implementation:

bool HtmlDialogView::CanResize() const {
  return true;
}

bool HtmlDialogView::IsModal() const {
  return delegate_->IsDialogModal();
}

std::wstring HtmlDialogView::GetWindowTitle() const {
  return delegate_->GetDialogTitle();
}

void HtmlDialogView::WindowClosing() {
  // If we still have a delegate that means we haven't notified it of the
  // dialog closing. This happens if the user clicks the Close button on the
  // dialog.
  if (delegate_)
    OnDialogClosed("");
}

views::View* HtmlDialogView::GetContentsView() {
  return this;
}

views::View* HtmlDialogView::GetInitiallyFocusedView() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogContentsDelegate implementation:

bool HtmlDialogView::IsDialogModal() const {
  return IsModal();
}

std::wstring HtmlDialogView::GetDialogTitle() const {
  return GetWindowTitle();
}

GURL HtmlDialogView::GetDialogContentURL() const {
  return delegate_->GetDialogContentURL();
}

void HtmlDialogView::GetDialogSize(gfx::Size* size) const {
  delegate_->GetDialogSize(size);
}

std::string HtmlDialogView::GetDialogArgs() const {
  return delegate_->GetDialogArgs();
}

void HtmlDialogView::OnDialogClosed(const std::string& json_retval) {
  delegate_->OnDialogClosed(json_retval);
  delegate_ = NULL;  // We will not communicate further with the delegate.
  window()->Close();
}

////////////////////////////////////////////////////////////////////////////////
// PageNavigator implementation:
void HtmlDialogView::OpenURLFromTab(TabContents* source,
                                    const GURL& url,
                                    const GURL& referrer,
                                    WindowOpenDisposition disposition,
                                    PageTransition::Type transition) {
  // Force all links to open in a new window, ignoring the incoming
  // disposition. This is a tabless, modal dialog so we can't just
  // open it in the current frame.
  parent_browser_->OpenURLFromTab(source, url, referrer, NEW_WINDOW,
                                  transition);
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsDelegate implementation:

void HtmlDialogView::NavigationStateChanged(const TabContents* source,
                                            unsigned changed_flags) {
  // We shouldn't receive any NavigationStateChanged except the first
  // one, which we ignore because we're a dialog box.
}

void HtmlDialogView::ReplaceContents(TabContents* source,
                                     TabContents* new_contents) {
}

void HtmlDialogView::AddNewContents(TabContents* source,
                    TabContents* new_contents,
                    WindowOpenDisposition disposition,
                    const gfx::Rect& initial_pos,
                    bool user_gesture) {
  parent_browser_->AddNewContents(
      source, new_contents, NEW_WINDOW, initial_pos, user_gesture);
}

void HtmlDialogView::ActivateContents(TabContents* contents) {
  // We don't do anything here because there's only one TabContents in
  // this frame and we don't have a TabStripModel.
}

void HtmlDialogView::LoadingStateChanged(TabContents* source) {
  // We don't care about this notification
}

void HtmlDialogView::CloseContents(TabContents* source) {
  // We receive this message but don't handle it because we really do the
  // cleanup in OnDialogClosed().
}

void HtmlDialogView::MoveContents(TabContents* source, const gfx::Rect& pos) {
  // The contained web page wishes to resize itself. We let it do this because
  // if it's a dialog we know about, we trust it not to be mean to the user.
  window()->SetBounds(pos);
}

bool HtmlDialogView::IsPopup(TabContents* source) {
  // This needs to return true so that we are allowed to be resized by our
  // contents.
  return true;
}

void HtmlDialogView::ToolbarSizeChanged(TabContents* source,
                                        bool is_animating) {
  Layout();
}

void HtmlDialogView::URLStarredChanged(TabContents* source, bool starred) {
  // We don't have a visible star to click in the window.
  NOTREACHED();
}

void HtmlDialogView::UpdateTargetURL(TabContents* source, const GURL& url) {
  // Ignored.
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView:

void HtmlDialogView::InitDialog() {
  // Now Init the DOMView. This view runs in its own process to render the html.
  DOMView::Init(profile_, NULL);

  // Make sure this new TabContents we just created in Init() knows about us.
  DCHECK(host_->type() == TAB_CONTENTS_HTML_DIALOG);
  HtmlDialogContents* host = static_cast<HtmlDialogContents*>(host_);
  host->Init(this);
  host->set_delegate(this);
}
