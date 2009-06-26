// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/html_dialog_view.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

namespace browser {

// Declared in browser_dialogs.h so that others don't need to depend on our .h. 
void ShowHtmlDialogView(gfx::NativeWindow parent, Browser* browser,
                        HtmlDialogUIDelegate* delegate) {
  HtmlDialogView* html_view = new HtmlDialogView(browser, delegate);
  views::Window::CreateChromeWindow(parent, gfx::Rect(), html_view);
  html_view->InitDialog();
  html_view->window()->Show();
}

}  // namespace browser

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView, public:

HtmlDialogView::HtmlDialogView(Browser* parent_browser,
                               HtmlDialogUIDelegate* delegate)
    : DOMView(),
      parent_browser_(parent_browser),
      profile_(parent_browser->profile()),
      delegate_(delegate) {
  DCHECK(profile_);
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
// HtmlDialogUIDelegate implementation:

bool HtmlDialogView::IsDialogModal() const {
  return IsModal();
}

std::wstring HtmlDialogView::GetDialogTitle() const {
  return GetWindowTitle();
}

GURL HtmlDialogView::GetDialogContentURL() const {
  return delegate_->GetDialogContentURL();
}

void HtmlDialogView::GetDOMMessageHandlers(
    std::vector<DOMMessageHandler*>* handlers) const {
  delegate_->GetDOMMessageHandlers(handlers);
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
// TabContentsDelegate implementation:

void HtmlDialogView::OpenURLFromTab(TabContents* source,
                                    const GURL& url,
                                    const GURL& referrer,
                                    WindowOpenDisposition disposition,
                                    PageTransition::Type transition) {
  // Force all links to open in a new window, ignoring the incoming
  // disposition. This is a tabless, modal dialog so we can't just
  // open it in the current frame.
  static_cast<TabContentsDelegate*>(parent_browser_)->OpenURLFromTab(
      source, url, referrer, NEW_WINDOW, transition);
}

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
  static_cast<TabContentsDelegate*>(parent_browser_)->AddNewContents(
      source, new_contents, NEW_WINDOW, initial_pos, user_gesture);
}

void HtmlDialogView::ActivateContents(TabContents* contents) {
  // We don't do anything here because there's only one TabContents in
  // this frame and we don't have a TabStripModel.
}

void HtmlDialogView::LoadingStateChanged(TabContents* source) {
  // We don't care about this notification.
}

void HtmlDialogView::CloseContents(TabContents* source) {
  // We receive this message but don't handle it because we really do the
  // cleanup in OnDialogClosed().
}

void HtmlDialogView::MoveContents(TabContents* source, const gfx::Rect& pos) {
  // The contained web page wishes to resize itself. We let it do this because
  // if it's a dialog we know about, we trust it not to be mean to the user.
  GetWidget()->SetBounds(pos);
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

  tab_contents_->set_delegate(this);

  // Set the delegate. This must be done before loading the page. See
  // the comment above HtmlDialogUI in its header file for why.
  HtmlDialogUI::GetPropertyAccessor().SetProperty(tab_contents_->property_bag(),
                                                  this);

  DOMView::LoadURL(delegate_->GetDialogContentURL());
}
