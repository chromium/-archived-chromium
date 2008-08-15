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
// HtmlDialogView, ChromeViews::View implementation:

void HtmlDialogView::GetPreferredSize(CSize *out) {
  delegate_->GetDialogSize(out);
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView, ChromeViews::WindowDelegate implementation:

bool HtmlDialogView::CanResize() const {
  return true;
}

bool HtmlDialogView::IsModal() const {
  return delegate_->IsModal();
}

std::wstring HtmlDialogView::GetWindowTitle() const {
  return L"Google Gears";
}

void HtmlDialogView::WindowClosing() {
  // If we still have a delegate that means we haven't notified it of the
  // dialog closing. This happens if the user clicks the Close button on the
  // dialog.
  if (delegate_)
    OnDialogClosed("");
}

ChromeViews::View* HtmlDialogView::GetContentsView() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogContentsDelegate implementation:

GURL HtmlDialogView::GetDialogContentURL() const {
  return delegate_->GetDialogContentURL();
}

void HtmlDialogView::GetDialogSize(CSize* size) const {
  return delegate_->GetDialogSize(size);
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
                                    WindowOpenDisposition disposition,
                                    PageTransition::Type transition) {
  // Force all links to open in a new window, ignoring the incoming
  // disposition. This is a tabless, modal dialog so we can't just
  // open it in the current frame.
  parent_browser_->OpenURLFromTab(source, url, NEW_WINDOW, transition);
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

  // Determine the size the window containing the dialog at its requested size.
  gfx::Size window_size =
      window()->CalculateWindowSizeForClientSize(pos.size());

  // Actually size the window.
  CRect vc_bounds;
  GetViewContainer()->GetBounds(&vc_bounds, true);
  gfx::Rect bounds(vc_bounds.left, vc_bounds.top, window_size.width(),
                   window_size.height());
  window()->SetBounds(bounds);
}

bool HtmlDialogView::IsPopup(TabContents* source) {
  // This needs to return true so that we are allowed to be resized by our
  // contents.
  return true;
}

void HtmlDialogView::ToolbarSizeChanged(TabContents* source, bool is_animating) {
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
