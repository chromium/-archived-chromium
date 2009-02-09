// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents.h"

#include "chrome/browser/cert_store.h"
#include "chrome/browser/views/download_shelf_view.h"
#include "chrome/browser/views/download_started_animation.h"
#include "chrome/browser/views/blocked_popup_container.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/views/native_scroll_bar.h"
#include "chrome/views/root_view.h"
#include "chrome/views/view.h"
#include "chrome/views/view_storage.h"
#include "chrome/views/widget.h"

#include "generated_resources.h"

namespace {

BOOL CALLBACK InvalidateWindow(HWND hwnd, LPARAM lparam) {
  // Note: erase is required to properly paint some widgets borders. This can
  // be seen with textfields.
  InvalidateRect(hwnd, NULL, TRUE);
  return TRUE;
}

}  // namespace

TabContents::TabContents(TabContentsType type)
    : type_(type),
      delegate_(NULL),
      controller_(NULL),
      is_loading_(false),
      is_active_(true),
      is_crashed_(false),
      waiting_for_response_(false),
      shelf_visible_(false),
      max_page_id_(-1),
      blocked_popups_(NULL),
      capturing_contents_(false),
      is_being_destroyed_(false) {
  last_focused_view_storage_id_ =
      views::ViewStorage::GetSharedInstance()->CreateStorageID();
}

TabContents::~TabContents() {
  // Makes sure to remove any stored view we may still have in the ViewStorage.
  //
  // It is possible the view went away before us, so we only do this if the
  // view is registered.
  views::ViewStorage* view_storage = views::ViewStorage::GetSharedInstance();
  if (view_storage->RetrieveView(last_focused_view_storage_id_) != NULL)
    view_storage->RemoveView(last_focused_view_storage_id_);
}

// static
void TabContents::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kBlockPopups, false);
}

void TabContents::CloseContents() {
  // Destroy our NavigationController, which will Destroy all tabs it owns.
  controller_->Destroy();
  // Note that the controller may have deleted us at this point,
  // so don't touch any member variables here.
}

void TabContents::Destroy() {
  DCHECK(!is_being_destroyed_);
  is_being_destroyed_ = true;

  // First cleanly close all child windows.
  // TODO(mpcomplete): handle case if MaybeCloseChildWindows() already asked
  // some of these to close.  CloseWindows is async, so it might get called
  // twice before it runs.
  int size = static_cast<int>(child_windows_.size());
  for (int i = size - 1; i >= 0; --i) {
    ConstrainedWindow* window = child_windows_[i];
    if (window)
      window->CloseConstrainedWindow();
  }

  // Notify any lasting InfobarDelegates that have not yet been removed that
  // whatever infobar they were handling in this TabContents has closed,
  // because the TabContents is going away entirely.
  for (int i = 0; i < infobar_delegate_count(); ++i) {
    InfoBarDelegate* delegate = GetInfoBarDelegateAt(i);
    delegate->InfoBarClosed();
  }
  infobar_delegates_.clear();

  // Notify any observer that have a reference on this tab contents.
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_DESTROYED,
      Source<TabContents>(this),
      NotificationService::NoDetails());

  // If we still have a window handle, destroy it. GetContainerHWND can return
  // NULL if this contents was part of a window that closed.
  if (GetNativeView())
    ::DestroyWindow(GetNativeView());

  // Notify our NavigationController.  Make sure we are deleted first, so
  // that the controller is the last to die.
  NavigationController* controller = controller_;
  TabContentsType type = this->type();

  delete this;

  controller->TabContentsWasDestroyed(type);
}

void TabContents::SetupController(Profile* profile) {
  DCHECK(!controller_);
  controller_ = new NavigationController(this, profile);
}

bool TabContents::SupportsURL(GURL* url) {
  GURL u(*url);
  if (TabContents::TypeForURL(&u) == type()) {
    *url = u;
    return true;
  }
  return false;
}

const GURL& TabContents::GetURL() const {
  // We may not have a navigation entry yet
  NavigationEntry* entry = controller_->GetActiveEntry();
  return entry ? entry->display_url() : GURL::EmptyGURL();
}

const std::wstring& TabContents::GetTitle() const {
  // We use the title for the last committed entry rather than a pending
  // navigation entry. For example, when the user types in a URL, we want to
  // keep the old page's title until the new load has committed and we get a new
  // title.
  // The exception is with transient pages, for which we really want to use
  // their title, as they are not committed.
  NavigationEntry* entry = controller_->GetTransientEntry();
  if (entry)
    return entry->GetTitleForDisplay();
  
  entry = controller_->GetLastCommittedEntry();
  if (entry)
    return entry->GetTitleForDisplay();
  else if (controller_->LoadingURLLazily())
    return controller_->GetLazyTitle();
  return EmptyWString();
}

int32 TabContents::GetMaxPageID() {
  if (GetSiteInstance())
    return GetSiteInstance()->max_page_id();
  else
    return max_page_id_;
}

void TabContents::UpdateMaxPageID(int32 page_id) {
  // Ensure both the SiteInstance and RenderProcessHost update their max page
  // IDs in sync. Only WebContents will also have site instances, except during
  // testing.
  if (GetSiteInstance())
    GetSiteInstance()->UpdateMaxPageID(page_id);

  if (AsWebContents())
    AsWebContents()->process()->UpdateMaxPageID(page_id);
  else
    max_page_id_ = std::max(max_page_id_, page_id);
}

const std::wstring TabContents::GetDefaultTitle() const {
  return l10n_util::GetString(IDS_DEFAULT_TAB_TITLE);
}

SkBitmap TabContents::GetFavIcon() const {
  // Like GetTitle(), we also want to use the favicon for the last committed
  // entry rather than a pending navigation entry.
  NavigationEntry* entry = controller_->GetTransientEntry();
  if (entry)
    return entry->favicon().bitmap();

  entry = controller_->GetLastCommittedEntry();
  if (entry)
    return entry->favicon().bitmap();
  else if (controller_->LoadingURLLazily())
    return controller_->GetLazyFavIcon();
  return SkBitmap();
}

SecurityStyle TabContents::GetSecurityStyle() const {
  // We may not have a navigation entry yet.
  NavigationEntry* entry = controller_->GetActiveEntry();
  return entry ? entry->ssl().security_style() : SECURITY_STYLE_UNKNOWN;
}

bool TabContents::GetSSLEVText(std::wstring* ev_text,
                               std::wstring* ev_tooltip_text) const {
  DCHECK(ev_text && ev_tooltip_text);
  ev_text->clear();
  ev_tooltip_text->clear();

  NavigationEntry* entry = controller_->GetActiveEntry();
  if (!entry ||
      net::IsCertStatusError(entry->ssl().cert_status()) ||
      ((entry->ssl().cert_status() & net::CERT_STATUS_IS_EV) == 0))
    return false;

  scoped_refptr<net::X509Certificate> cert;
  CertStore::GetSharedInstance()->RetrieveCert(entry->ssl().cert_id(), &cert);
  if (!cert.get()) {
    NOTREACHED();
    return false;
  }

  return SSLManager::GetEVCertNames(*cert, ev_text, ev_tooltip_text);
}

void TabContents::SetIsCrashed(bool state) {
  if (state == is_crashed_)
    return;

  is_crashed_ = state;
  if (delegate_)
    delegate_->ContentsStateChanged(this);
}

void TabContents::NotifyNavigationStateChanged(unsigned changed_flags) {
  if (delegate_)
    delegate_->NavigationStateChanged(this, changed_flags);
}

void TabContents::DidBecomeSelected() {
  if (controller_)
    controller_->SetActive(true);

  // Invalidate all descendants. (take care to exclude invalidating ourselves!)
  EnumChildWindows(GetNativeView(), InvalidateWindow, 0);
}

void TabContents::WasHidden() {
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_HIDDEN,
      Source<TabContents>(this),
      NotificationService::NoDetails());
}

void TabContents::Activate() {
  if (delegate_)
    delegate_->ActivateContents(this);
}

void TabContents::OpenURL(const GURL& url, const GURL& referrer,
                          WindowOpenDisposition disposition,
                          PageTransition::Type transition) {
  if (delegate_)
    delegate_->OpenURLFromTab(this, url, referrer, disposition, transition);
}

bool TabContents::NavigateToPendingEntry(bool reload) {
  // Our benavior is just to report that the entry was committed.
  controller()->GetPendingEntry()->set_title(GetDefaultTitle());
  controller()->CommitPendingEntry();
  return true;
}

ConstrainedWindow* TabContents::CreateConstrainedDialog(
    views::WindowDelegate* window_delegate,
    views::View* contents_view) {
  ConstrainedWindow* window =
      ConstrainedWindow::CreateConstrainedDialog(
          this, gfx::Rect(), contents_view, window_delegate);
  child_windows_.push_back(window);
  return window;
}

void TabContents::AddNewContents(TabContents* new_contents,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) {
  if (!delegate_)
    return;

  if ((disposition == NEW_POPUP) && !user_gesture) {
    // Unrequested popups from normal pages are constrained.
    TabContents* popup_owner = this;
    TabContents* our_owner = delegate_->GetConstrainingContents(this);
    if (our_owner)
      popup_owner = our_owner;
    popup_owner->AddConstrainedPopup(new_contents, initial_pos);
  } else {
    new_contents->DisassociateFromPopupCount();

    delegate_->AddNewContents(this, new_contents, disposition, initial_pos,
                              user_gesture);

    PopupNotificationVisibilityChanged(ShowingBlockedPopupNotification());
  }
}

void TabContents::AddConstrainedPopup(TabContents* new_contents,
                                      const gfx::Rect& initial_pos) {
  if (!blocked_popups_) {
    CRect client_rect;
    GetClientRect(GetNativeView(), &client_rect);
    gfx::Point anchor_position(
        client_rect.Width() -
          views::NativeScrollBar::GetVerticalScrollBarWidth(),
        client_rect.Height());

    blocked_popups_ = BlockedPopupContainer::Create(
        this, profile(), anchor_position);
    child_windows_.push_back(blocked_popups_);
  }

  blocked_popups_->AddTabContents(new_contents, initial_pos);
  PopupNotificationVisibilityChanged(ShowingBlockedPopupNotification());
}

void TabContents::CloseAllSuppressedPopups() {
  if (blocked_popups_)
    blocked_popups_->CloseAllPopups();
}

void TabContents::Focus() {
  HWND container_hwnd = GetNativeView();
  if (!container_hwnd)
    return;

  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManager(container_hwnd);
  DCHECK(focus_manager);
  views::View* v = focus_manager->GetViewForWindow(container_hwnd, true);
  DCHECK(v);
  if (v)
    v->RequestFocus();
}

void TabContents::StoreFocus() {
  views::ViewStorage* view_storage =
      views::ViewStorage::GetSharedInstance();

  if (view_storage->RetrieveView(last_focused_view_storage_id_) != NULL)
    view_storage->RemoveView(last_focused_view_storage_id_);

  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManager(GetNativeView());
  if (focus_manager) {
    // |focus_manager| can be NULL if the tab has been detached but still
    // exists.
    views::View* focused_view = focus_manager->GetFocusedView();
    if (focused_view)
      view_storage->StoreView(last_focused_view_storage_id_, focused_view);

    // If the focus was on the page, explicitly clear the focus so that we
    // don't end up with the focused HWND not part of the window hierarchy.
    // TODO(brettw) this should move to the view somehow.
    HWND container_hwnd = GetNativeView();
    if (container_hwnd) {
      views::View* focused_view = focus_manager->GetFocusedView();
      if (focused_view) {
        HWND hwnd = focused_view->GetRootView()->GetWidget()->GetHWND();
        if (container_hwnd == hwnd || ::IsChild(container_hwnd, hwnd))
          focus_manager->ClearFocus();
      }
    }
  }
}

void TabContents::RestoreFocus() {
  views::ViewStorage* view_storage =
      views::ViewStorage::GetSharedInstance();
  views::View* last_focused_view =
      view_storage->RetrieveView(last_focused_view_storage_id_);

  if (!last_focused_view) {
    SetInitialFocus();
  } else {
    views::FocusManager* focus_manager =
        views::FocusManager::GetFocusManager(GetNativeView());

    // If you hit this DCHECK, please report it to Jay (jcampan).
    DCHECK(focus_manager != NULL) << "No focus manager when restoring focus.";

    if (focus_manager && focus_manager->ContainsView(last_focused_view)) {
      last_focused_view->RequestFocus();
    } else {
      // The focused view may not belong to the same window hierarchy (for
      // example if the location bar was focused and the tab is dragged out).
      // In that case we default to the default focus.
      SetInitialFocus();
    }
    view_storage->RemoveView(last_focused_view_storage_id_);
  }
}

void TabContents::SetInitialFocus() {
  ::SetFocus(GetNativeView());
}

void TabContents::AddInfoBar(InfoBarDelegate* delegate) {
  // Look through the existing InfoBarDelegates we have for a match. If we've
  // already got one that matches, then we don't add the new one.
  for (int i = 0; i < infobar_delegate_count(); ++i) {
    if (GetInfoBarDelegateAt(i)->EqualsDelegate(delegate))
      return;
  }

  infobar_delegates_.push_back(delegate);
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_INFOBAR_ADDED,
      Source<TabContents>(this),
      Details<InfoBarDelegate>(delegate));

  // Add ourselves as an observer for navigations the first time a delegate is
  // added. We use this notification to expire InfoBars that need to expire on
  // page transitions.
  if (infobar_delegates_.size() == 1) {
    DCHECK(controller());
    registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                   Source<NavigationController>(controller()));
  }
}

void TabContents::RemoveInfoBar(InfoBarDelegate* delegate) {
  std::vector<InfoBarDelegate*>::iterator it =
      find(infobar_delegates_.begin(), infobar_delegates_.end(), delegate);
  if (it != infobar_delegates_.end()) {
    InfoBarDelegate* delegate = *it;
    NotificationService::current()->Notify(
        NotificationType::TAB_CONTENTS_INFOBAR_REMOVED,
        Source<TabContents>(this),
        Details<InfoBarDelegate>(delegate));
    infobar_delegates_.erase(it);

    // Remove ourselves as an observer if we are tracking no more InfoBars.
    if (infobar_delegates_.empty()) {
      registrar_.Remove(this, NotificationType::NAV_ENTRY_COMMITTED,
                        Source<NavigationController>(controller()));
    }
  }
}

void TabContents::SetDownloadShelfVisible(bool visible) {
  if (shelf_visible_ != visible) {
    if (visible) {
      // Invoke GetDownloadShelfView to force the shelf to be created.
      GetDownloadShelfView();
    }
    shelf_visible_ = visible;

    if (delegate_)
      delegate_->ContentsStateChanged(this);
  }

  // SetShelfVisible can force-close the shelf, so make sure we lay out
  // everything correctly, as if the animation had finished. This doesn't
  // matter for showing the shelf, as the show animation will do it.
  ToolbarSizeChanged(false);
}

void TabContents::ToolbarSizeChanged(bool is_animating) {
  TabContentsDelegate* d = delegate();
  if (d)
    d->ToolbarSizeChanged(this, is_animating);
}

void TabContents::OnStartDownload(DownloadItem* download) {
  DCHECK(download);
  TabContents* tab_contents = this;

  // Download in a constrained popup is shown in the tab that opened it.
  TabContents* constraining_tab = delegate()->GetConstrainingContents(this);
  if (constraining_tab)
    tab_contents = constraining_tab;

  // GetDownloadShelfView creates the download shelf if it was not yet created.
  tab_contents->GetDownloadShelfView()->AddDownload(download);
  tab_contents->SetDownloadShelfVisible(true);

  // This animation will delete itself when it finishes, or if we become hidden
  // or destroyed.
  if (IsWindowVisible(GetNativeView())) {  // For minimized windows, unit
                                           // tests, etc.
    new DownloadStartedAnimation(tab_contents);
  }
}

DownloadShelfView* TabContents::GetDownloadShelfView() {
  if (!download_shelf_view_.get()) {
    download_shelf_view_.reset(new DownloadShelfView(this));
    // The TabContents owns the download-shelf.
    download_shelf_view_->SetParentOwned(false);
  }
  return download_shelf_view_.get();
}

void TabContents::MigrateShelfViewFrom(TabContents* tab_contents) {
  download_shelf_view_.reset(tab_contents->GetDownloadShelfView());
  download_shelf_view_->ChangeTabContents(tab_contents, this);
  tab_contents->ReleaseDownloadShelfView();
}

void TabContents::WillClose(ConstrainedWindow* window) {
  ConstrainedWindowList::iterator it =
      find(child_windows_.begin(), child_windows_.end(), window);
  if (it != child_windows_.end())
    child_windows_.erase(it);

  if (window == blocked_popups_)
    blocked_popups_ = NULL;

  if (::IsWindow(GetNativeView())) {
    CRect client_rect;
    GetClientRect(GetNativeView(), &client_rect);
    RepositionSupressedPopupsToFit(
        gfx::Size(client_rect.Width(), client_rect.Height()));
  }
}

void TabContents::DidMoveOrResize(ConstrainedWindow* window) {
  UpdateWindow(GetNativeView());
}

void TabContents::Observe(NotificationType type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  DCHECK(type == NotificationType::NAV_ENTRY_COMMITTED);
  DCHECK(controller() == Source<NavigationController>(source).ptr());

  NavigationController::LoadCommittedDetails& committed_details =
      *(Details<NavigationController::LoadCommittedDetails>(details).ptr());
  ExpireInfoBars(committed_details);
}

// static
void TabContents::MigrateShelfView(TabContents* from, TabContents* to) {
  bool was_shelf_visible = from->IsDownloadShelfVisible();
  if (was_shelf_visible)
    to->MigrateShelfViewFrom(from);
  to->SetDownloadShelfVisible(was_shelf_visible);
}

void TabContents::SetIsLoading(bool is_loading,
                               LoadNotificationDetails* details) {
  if (is_loading == is_loading_)
    return;

  is_loading_ = is_loading;
  waiting_for_response_ = is_loading;

  // Suppress notifications for this TabContents if we are not active.
  if (!is_active_)
    return;

  if (delegate_)
    delegate_->LoadingStateChanged(this);

  NotificationType type = is_loading ? NotificationType::LOAD_START :
      NotificationType::LOAD_STOP;
  NotificationDetails det = details ?
      Details<LoadNotificationDetails>(details) :
      NotificationService::NoDetails();
  NotificationService::current()->Notify(type, 
      Source<NavigationController>(this->controller()),
      det);
}

// TODO(brettw) This should be on the WebContentsView.
void TabContents::RepositionSupressedPopupsToFit(const gfx::Size& new_size) {
  // TODO(erg): There's no way to detect whether scroll bars are
  // visible, so for beta, we're just going to assume that the
  // vertical scroll bar is visible, and not care about covering up
  // the horizontal scroll bar. Fixing this is half of
  // http://b/1118139.
  gfx::Point anchor_position(
      new_size.width() -
          views::NativeScrollBar::GetVerticalScrollBarWidth(),
      new_size.height());

  if (blocked_popups_)
    blocked_popups_->RepositionConstrainedWindowTo(anchor_position);
}

void TabContents::ReleaseDownloadShelfView() {
  download_shelf_view_.release();
}

bool TabContents::ShowingBlockedPopupNotification() const {
  return blocked_popups_ != NULL &&
      blocked_popups_->GetTabContentsCount() != 0;
}

namespace {
bool TransitionIsReload(PageTransition::Type transition) {
  return PageTransition::StripQualifier(transition) == PageTransition::RELOAD;
}
}

void TabContents::ExpireInfoBars(
    const NavigationController::LoadCommittedDetails& details) {
  // Only hide InfoBars when the user has done something that makes the main
  // frame load. We don't want various automatic or subframe navigations making
  // it disappear.
  if (!details.is_user_initiated_main_frame_load())
    return;

  for (int i = infobar_delegate_count() - 1; i >= 0; --i) {
    InfoBarDelegate* delegate = GetInfoBarDelegateAt(i);
    if (delegate->ShouldExpire(details))
      RemoveInfoBar(delegate);
  }
}
