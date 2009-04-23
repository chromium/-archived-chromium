// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents.h"

#include "chrome/browser/autofill_manager.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/plugin_installer.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/generated_resources.h"

#if defined(OS_WIN)
// TODO(port): some of these headers should be ported.
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/views/download_started_animation.h"
#include "chrome/browser/views/blocked_popup_container.h"
#include "chrome/views/controls/scrollbar/native_scroll_bar.h"
#endif

// static
int TabContents::find_request_id_counter_ = -1;

// TODO(brettw) many of the data members here have casts to WebContents.
// This object is the same as WebContents and is currently being merged.
// When this merge is done, the casts can be removed.
TabContents::TabContents(Profile* profile)
    : delegate_(NULL),
      controller_(this, profile),
      view_(TabContentsView::Create(static_cast<WebContents*>(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(render_manager_(
          static_cast<WebContents*>(this),
          static_cast<WebContents*>(this))),
      property_bag_(),
      registrar_(),
      printing_(*static_cast<WebContents*>(this)),
      save_package_(),
      cancelable_consumer_(),
      autofill_manager_(),
      password_manager_(),
      plugin_installer_(),
      ALLOW_THIS_IN_INITIALIZER_LIST(fav_icon_helper_(
          static_cast<WebContents*>(this))),
      select_file_dialog_(),
      pending_install_(),
      is_loading_(false),
      is_crashed_(false),
      waiting_for_response_(false),
      max_page_id_(-1),
      current_load_start_(),
      load_state_(net::LOAD_STATE_IDLE),
      load_state_host_(),
      received_page_title_(false),
      is_starred_(false),
      contents_mime_type_(),
      encoding_(),
      download_shelf_(),
      shelf_visible_(false),
      blocked_popups_(NULL),
      infobar_delegates_(),
      last_download_shelf_show_(),
      find_ui_active_(false),
      find_op_aborted_(false),
      current_find_request_id_(find_request_id_counter_++),
      find_text_(),
      find_prepopulate_text_(NULL),
      find_result_(),
      capturing_contents_(false),
      is_being_destroyed_(false),
      notify_disconnection_(false),
      history_requests_(),
#if defined(OS_WIN)
      message_box_active_(CreateEvent(NULL, TRUE, FALSE, NULL)),
#endif
      last_javascript_message_dismissal_(),
      suppress_javascript_messages_(false) {
}

TabContents::~TabContents() {
}

// static
void TabContents::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kBlockPopups, false);
}

bool TabContents::SupportsURL(GURL* url) {
  // TODO(brettw) remove this function.
  return true;
}

AutofillManager* TabContents::GetAutofillManager() {
  if (autofill_manager_.get() == NULL)
    autofill_manager_.reset(new AutofillManager(this));
  return autofill_manager_.get();
}

PasswordManager* TabContents::GetPasswordManager() {
  if (password_manager_.get() == NULL)
    password_manager_.reset(new PasswordManager(this));
  return password_manager_.get();
}

PluginInstaller* TabContents::GetPluginInstaller() {
  if (plugin_installer_.get() == NULL)
    plugin_installer_.reset(new PluginInstaller(this));
  return plugin_installer_.get();
}

const GURL& TabContents::GetURL() const {
  // We may not have a navigation entry yet
  NavigationEntry* entry = controller_.GetActiveEntry();
  return entry ? entry->display_url() : GURL::EmptyGURL();
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
  NavigationEntry* entry = controller_.GetTransientEntry();
  if (entry)
    return entry->favicon().bitmap();

  entry = controller_.GetLastCommittedEntry();
  if (entry)
    return entry->favicon().bitmap();
  else if (controller_.LoadingURLLazily())
    return controller_.GetLazyFavIcon();
  return SkBitmap();
}

#if defined(OS_WIN)
SecurityStyle TabContents::GetSecurityStyle() const {
  // We may not have a navigation entry yet.
  NavigationEntry* entry = controller_.GetActiveEntry();
  return entry ? entry->ssl().security_style() : SECURITY_STYLE_UNKNOWN;
}

bool TabContents::GetSSLEVText(std::wstring* ev_text,
                               std::wstring* ev_tooltip_text) const {
  DCHECK(ev_text && ev_tooltip_text);
  ev_text->clear();
  ev_tooltip_text->clear();

  NavigationEntry* entry = controller_.GetActiveEntry();
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
#endif

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
  string16 default_title = WideToUTF16Hack(GetDefaultTitle());
  controller_.pending_entry()->set_title(default_title);
  controller_.CommitPendingEntry();
  return true;
}

#if defined(OS_WIN)
ConstrainedWindow* TabContents::CreateConstrainedDialog(
    views::WindowDelegate* window_delegate,
    views::View* contents_view) {
  ConstrainedWindow* window =
      ConstrainedWindow::CreateConstrainedDialog(
          this, gfx::Rect(), contents_view, window_delegate);
  child_windows_.push_back(window);
  return window;
}
#endif

void TabContents::AddNewContents(TabContents* new_contents,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) {
  if (!delegate_)
    return;

#if defined(OS_WIN)
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
#else
  // TODO(port): implement the popup blocker stuff
  delegate_->AddNewContents(this, new_contents, disposition, initial_pos,
                            user_gesture);
#endif
}

#if defined(OS_WIN)
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
#endif

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
    registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                   Source<NavigationController>(&controller_));
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
                        Source<NavigationController>(&controller_));
    }
  }
}

void TabContents::ToolbarSizeChanged(bool is_animating) {
  TabContentsDelegate* d = delegate();
  if (d)
    d->ToolbarSizeChanged(this, is_animating);
}

void TabContents::OnStartDownload(DownloadItem* download) {
  DCHECK(download);
  TabContents* tab_contents = this;

// TODO(port): port contraining contents.
#if defined(OS_WIN)
  // Download in a constrained popup is shown in the tab that opened it.
  TabContents* constraining_tab = delegate()->GetConstrainingContents(this);
  if (constraining_tab)
    tab_contents = constraining_tab;
#endif

  // GetDownloadShelf creates the download shelf if it was not yet created.
  tab_contents->GetDownloadShelf()->AddDownload(
      new DownloadItemModel(download));
  tab_contents->SetDownloadShelfVisible(true);

// TODO(port): port animatinos.
#if defined(OS_WIN)
  // This animation will delete itself when it finishes, or if we become hidden
  // or destroyed.
  if (IsWindowVisible(GetNativeView())) {  // For minimized windows, unit
                                           // tests, etc.
    new DownloadStartedAnimation(tab_contents);
  }
#endif
}

DownloadShelf* TabContents::GetDownloadShelf() {
  if (!download_shelf_.get())
    download_shelf_.reset(DownloadShelf::Create(this));
  return download_shelf_.get();
}

void TabContents::MigrateShelfFrom(TabContents* tab_contents) {
  download_shelf_.reset(tab_contents->GetDownloadShelf());
  download_shelf_->ChangeTabContents(tab_contents, this);
  tab_contents->ReleaseDownloadShelf();
}

void TabContents::ReleaseDownloadShelf() {
  download_shelf_.release();
}

// static
void TabContents::MigrateShelf(TabContents* from, TabContents* to) {
  bool was_shelf_visible = from->IsDownloadShelfVisible();
  if (was_shelf_visible)
    to->MigrateShelfFrom(from);
  to->SetDownloadShelfVisible(was_shelf_visible);
}

void TabContents::WillClose(ConstrainedWindow* window) {
  ConstrainedWindowList::iterator it =
      find(child_windows_.begin(), child_windows_.end(), window);
  if (it != child_windows_.end())
    child_windows_.erase(it);

#if defined(OS_WIN)
  if (window == blocked_popups_)
    blocked_popups_ = NULL;

  if (::IsWindow(GetNativeView())) {
    CRect client_rect;
    GetClientRect(GetNativeView(), &client_rect);
    RepositionSupressedPopupsToFit(
        gfx::Size(client_rect.Width(), client_rect.Height()));
  }
#endif
}

void TabContents::DidMoveOrResize(ConstrainedWindow* window) {
#if defined(OS_WIN)
  UpdateWindow(GetNativeView());
#endif
}

void TabContents::StartFinding(const string16& find_text,
                               bool forward_direction) {
  // If find_text is empty, it means FindNext was pressed with a keyboard
  // shortcut so unless we have something to search for we return early.
  if (find_text.empty() && find_text_.empty())
    return;

  // This is a FindNext operation if we are searching for the same text again,
  // or if the passed in search text is empty (FindNext keyboard shortcut). The
  // exception to this is if the Find was aborted (then we don't want FindNext
  // because the highlighting has been cleared and we need it to reappear). We
  // therefore treat FindNext after an aborted Find operation as a full fledged
  // Find.
  bool find_next = (find_text_ == find_text || find_text.empty()) &&
                   !find_op_aborted_;
  if (!find_next)
    current_find_request_id_ = find_request_id_counter_++;

  if (!find_text.empty())
    find_text_ = find_text;

  find_op_aborted_ = false;

  // Keep track of what the last search was across the tabs.
  *find_prepopulate_text_ = find_text;

  render_view_host()->StartFinding(current_find_request_id_,
                                   find_text_,
                                   forward_direction,
                                   false,  // case sensitive
                                   find_next);
}

void TabContents::StopFinding(bool clear_selection) {
  find_ui_active_ = false;
  find_op_aborted_ = true;
  find_result_ = FindNotificationDetails();
  render_view_host()->StopFinding(clear_selection);
}

#if defined(OS_WIN)
// TODO(brettw) This should be on the TabContentsView.
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

bool TabContents::ShowingBlockedPopupNotification() const {
  return blocked_popups_ != NULL &&
      blocked_popups_->GetTabContentsCount() != 0;
}
#endif  // defined(OS_WIN)

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

void TabContents::OnGearsCreateShortcutDone(
    const GearsShortcutData2& shortcut_data, bool success) {
  NavigationEntry* current_entry = controller_.GetLastCommittedEntry();
  bool same_page =
      current_entry && pending_install_.page_id == current_entry->page_id();

  if (success && same_page) {
    // Only switch to app mode if the user chose to create a shortcut and
    // we're still on the same page that it corresponded to.
    if (delegate())
      delegate()->ConvertContentsToApplication(this);
  }

  // Reset the page id to indicate no requests are pending.
  pending_install_.page_id = 0;
  pending_install_.callback_functor = NULL;
}
