// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents.h"

#include "base/string16.h"
#include "base/time.h"
#include "chrome/browser/autofill_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/dom_ui/dom_ui_factory.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/plugin_installer.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
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

const string16& TabContents::GetTitle() const {
  DOMUI* our_dom_ui = render_manager_.pending_dom_ui() ?
      render_manager_.pending_dom_ui() : render_manager_.dom_ui();
  if (our_dom_ui) {
    // Don't override the title in view source mode.
    NavigationEntry* entry = controller_.GetActiveEntry();
    if (!(entry && entry->IsViewSourceMode())) {
      // Give the DOM UI the chance to override our title.
      const string16& title = our_dom_ui->overridden_title();
      if (!title.empty())
        return title;
    }
  }

  // We use the title for the last committed entry rather than a pending
  // navigation entry. For example, when the user types in a URL, we want to
  // keep the old page's title until the new load has committed and we get a new
  // title.
  // The exception is with transient pages, for which we really want to use
  // their title, as they are not committed.
  NavigationEntry* entry = controller_.GetTransientEntry();
  if (entry)
    return entry->GetTitleForDisplay(&controller_);

  entry = controller_.GetLastCommittedEntry();
  if (entry)
    return entry->GetTitleForDisplay(&controller_);
  else if (controller_.LoadingURLLazily())
    return controller_.GetLazyTitle();
  return EmptyString16();
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

SiteInstance* TabContents::GetSiteInstance() const {
  return render_manager_.current_host()->site_instance();
}

const std::wstring TabContents::GetDefaultTitle() const {
  return l10n_util::GetString(IDS_DEFAULT_TAB_TITLE);
}

bool TabContents::ShouldDisplayURL() {
  // Don't hide the url in view source mode.
  NavigationEntry* entry = controller_.GetActiveEntry();
  if (entry && entry->IsViewSourceMode())
    return true;
  DOMUI* dom_ui = GetDOMUIForCurrentState();
  if (dom_ui)
    return !dom_ui->should_hide_url();
  return true;
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

bool TabContents::ShouldDisplayFavIcon() {
  // Always display a throbber during pending loads.
  if (controller_.GetLastCommittedEntry() && controller_.pending_entry())
    return true;

  DOMUI* dom_ui = GetDOMUIForCurrentState();
  if (dom_ui)
    return !dom_ui->hide_favicon();
  return true;
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

std::wstring TabContents::GetStatusText() const {
  if (!is_loading() || load_state_ == net::LOAD_STATE_IDLE)
    return std::wstring();

  switch (load_state_) {
    case net::LOAD_STATE_WAITING_FOR_CACHE:
      return l10n_util::GetString(IDS_LOAD_STATE_WAITING_FOR_CACHE);
    case net::LOAD_STATE_RESOLVING_PROXY_FOR_URL:
      return l10n_util::GetString(IDS_LOAD_STATE_RESOLVING_PROXY_FOR_URL);
    case net::LOAD_STATE_RESOLVING_HOST:
      return l10n_util::GetString(IDS_LOAD_STATE_RESOLVING_HOST);
    case net::LOAD_STATE_CONNECTING:
      return l10n_util::GetString(IDS_LOAD_STATE_CONNECTING);
    case net::LOAD_STATE_SENDING_REQUEST:
      return l10n_util::GetString(IDS_LOAD_STATE_SENDING_REQUEST);
    case net::LOAD_STATE_WAITING_FOR_RESPONSE:
      return l10n_util::GetStringF(IDS_LOAD_STATE_WAITING_FOR_RESPONSE,
                                   load_state_host_);
    // Ignore net::LOAD_STATE_READING_RESPONSE and net::LOAD_STATE_IDLE
    case net::LOAD_STATE_IDLE:
    case net::LOAD_STATE_READING_RESPONSE:
      break;
  }

  return std::wstring();
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
  controller_.SetActive(true);

  if (render_widget_host_view())
    render_widget_host_view()->DidBecomeSelected();

  // If pid() is -1, that means the RenderProcessHost still hasn't been
  // initialized.  It'll register with CacheManagerHost when it is.
  if (process()->pid() != -1)
    WebCacheManager::GetInstance()->ObserveActivity(process()->pid());
}

void TabContents::WasHidden() {
  if (!capturing_contents()) {
    // |render_view_host()| can be NULL if the user middle clicks a link to open
    // a tab in then background, then closes the tab before selecting it.  This
    // is because closing the tab calls TabContents::Destroy(), which removes
    // the |render_view_host()|; then when we actually destroy the window,
    // OnWindowPosChanged() notices and calls HideContents() (which calls us).
    if (render_widget_host_view())
      render_widget_host_view()->WasHidden();

    // Loop through children and send WasHidden to them, too.
    int count = static_cast<int>(child_windows_.size());
    for (int i = count - 1; i >= 0; --i) {
      ConstrainedWindow* window = child_windows_.at(i);
      window->WasHidden();
    }
  }

  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_HIDDEN,
      Source<TabContents>(this),
      NotificationService::NoDetails());
}

void TabContents::Activate() {
  if (delegate_)
    delegate_->ActivateContents(this);
}

void TabContents::ShowContents() {
  if (render_widget_host_view())
    render_widget_host_view()->DidBecomeSelected();

  // Loop through children and send DidBecomeSelected to them, too.
  int count = static_cast<int>(child_windows_.size());
  for (int i = count - 1; i >= 0; --i) {
    ConstrainedWindow* window = child_windows_.at(i);
    window->DidBecomeSelected();
  }
}

void TabContents::HideContents() {
  // TODO(pkasting): http://b/1239839  Right now we purposefully don't call
  // our superclass HideContents(), because some callers want to be very picky
  // about the order in which these get called.  In addition to making the code
  // here practically impossible to understand, this also means we end up
  // calling TabContents::WasHidden() twice if callers call both versions of
  // HideContents() on a WebContents.
  WasHidden();
}

void TabContents::OpenURL(const GURL& url, const GURL& referrer,
                          WindowOpenDisposition disposition,
                          PageTransition::Type transition) {
  if (delegate_)
    delegate_->OpenURLFromTab(this, url, referrer, disposition, transition);
}

bool TabContents::NavigateToPendingEntry(bool reload) {
  const NavigationEntry& entry = *controller_.pending_entry();

  RenderViewHost* dest_render_view_host = render_manager_.Navigate(entry);
  if (!dest_render_view_host)
    return false;  // Unable to create the desired render view host.

  // Tell DevTools agent that it is attached prior to the navigation.
  DevToolsManager* dev_tools_manager = g_browser_process->devtools_manager();
  if (dev_tools_manager)  // NULL in unit tests.
    dev_tools_manager->SendAttachToAgent(*this, dest_render_view_host);

  // Used for page load time metrics.
  current_load_start_ = base::TimeTicks::Now();

  // Navigate in the desired RenderViewHost.
  dest_render_view_host->NavigateToEntry(entry, reload);

  if (entry.page_id() == -1) {
    // HACK!!  This code suppresses javascript: URLs from being added to
    // session history, which is what we want to do for javascript: URLs that
    // do not generate content.  What we really need is a message from the
    // renderer telling us that a new page was not created.  The same message
    // could be used for mailto: URLs and the like.
    if (entry.url().SchemeIs(chrome::kJavaScriptScheme))
      return false;
  }

  // Clear any provisional password saves - this stops password infobars
  // showing up on pages the user navigates to while the right page is
  // loading.
  GetPasswordManager()->ClearProvisionalSave();

  if (reload && !profile()->IsOffTheRecord()) {
    HistoryService* history =
        profile()->GetHistoryService(Profile::IMPLICIT_ACCESS);
    if (history)
      history->SetFavIconOutOfDateForPage(entry.url());
  }

  return true;
}

void TabContents::Stop() {
  render_manager_.Stop();
  printing_.Stop();
}

void TabContents::Cut() {
  render_view_host()->Cut();
}

void TabContents::Copy() {
  render_view_host()->Copy();
}

void TabContents::Paste() {
  render_view_host()->Paste();
}

void TabContents::DisassociateFromPopupCount() {
  render_view_host()->DisassociateFromPopupCount();
}

TabContents* TabContents::Clone() {
  // We create a new SiteInstance so that the new tab won't share processes
  // with the old one. This can be changed in the future if we need it to share
  // processes for some reason.
  TabContents* tc = new WebContents(profile(),
                                    SiteInstance::CreateSiteInstance(profile()),
                                    MSG_ROUTING_NONE, NULL);
  tc->controller().CopyStateFrom(controller_);
  return tc;
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

void TabContents::PopupNotificationVisibilityChanged(bool visible) {
  render_view_host()->PopupNotificationVisibilityChanged(visible);
}

gfx::NativeView TabContents::GetContentNativeView() {
  return view_->GetContentNativeView();
}

gfx::NativeView TabContents::GetNativeView() const {
  return view_->GetNativeView();
}

void TabContents::GetContainerBounds(gfx::Rect *out) const {
  view_->GetContainerBounds(out);
}

void TabContents::Focus() {
  view_->Focus();
}

void TabContents::SetInitialFocus(bool reverse) {
  render_view_host()->SetInitialFocus(reverse);
}

bool TabContents::FocusLocationBarByDefault() {
  DOMUI* dom_ui = GetDOMUIForCurrentState();
  if (dom_ui)
    return dom_ui->focus_location_bar_by_default();
  return false;
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

bool TabContents::IsBookmarkBarAlwaysVisible() {
  // See GetDOMUIForCurrentState() comment for more info. This case is very
  // similar, but for non-first loads, we want to use the committed entry. This
  // is so the bookmarks bar disappears at the same time the page does.
  if (controller_.GetLastCommittedEntry()) {
    // Not the first load, always use the committed DOM UI.
    if (render_manager_.dom_ui())
      return render_manager_.dom_ui()->force_bookmark_bar_visible();
    return false;  // Default.
  }

  // When it's the first load, we know either the pending one or the committed
  // one will have the DOM UI in it (see GetDOMUIForCurrentState), and only one
  // of them will be valid, so we can just check both.
  if (render_manager_.pending_dom_ui())
    return render_manager_.pending_dom_ui()->force_bookmark_bar_visible();
  if (render_manager_.dom_ui())
    return render_manager_.dom_ui()->force_bookmark_bar_visible();
  return false;  // Default.
}

void TabContents::SetDownloadShelfVisible(bool visible) {
  if (shelf_visible_ != visible) {
    if (visible) {
      // Invoke GetDownloadShelf to force the shelf to be created.
      GetDownloadShelf();
    }
    shelf_visible_ = visible;

    if (delegate_)
      delegate_->ContentsStateChanged(this);
  }

  // SetShelfVisible can force-close the shelf, so make sure we lay out
  // everything correctly, as if the animation had finished. This doesn't
  // matter for showing the shelf, as the show animation will do it.
  ToolbarSizeChanged(false);

  if (visible) {
    // Always set this value as it reflects the last time the download shelf
    // was made visible (even if it was already visible).
    last_download_shelf_show_ = base::TimeTicks::Now();
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

// Notifies the RenderWidgetHost instance about the fact that the page is
// loading, or done loading and calls the base implementation.
void TabContents::SetIsLoading(bool is_loading,
                               LoadNotificationDetails* details) {
  if (is_loading == is_loading_)
    return;

  if (!is_loading) {
    load_state_ = net::LOAD_STATE_IDLE;
    load_state_host_.clear();
  }

  render_manager_.SetIsLoading(is_loading);

  is_loading_ = is_loading;
  waiting_for_response_ = is_loading;

  if (delegate_)
    delegate_->LoadingStateChanged(this);

  NotificationType type = is_loading ? NotificationType::LOAD_START :
      NotificationType::LOAD_STOP;
  NotificationDetails det = NotificationService::NoDetails();;
  if (details)
      det = Details<LoadNotificationDetails>(details);
  NotificationService::current()->Notify(type,
      Source<NavigationController>(&controller_),
      det);
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

DOMUI* TabContents::GetDOMUIForCurrentState() {
  // When there is a pending navigation entry, we want to use the pending DOMUI
  // that goes along with it to control the basic flags. For example, we want to
  // show the pending URL in the URL bar, so we want the display_url flag to
  // be from the pending entry.
  //
  // The confusion comes because there are multiple possibilities for the
  // initial load in a tab as a side effect of the way the RenderViewHostManager
  // works.
  //
  //  - For the very first tab the load looks "normal". The new tab DOM UI is
  //    the pending one, and we want it to apply here.
  //
  //  - For subsequent new tabs, they'll get a new SiteInstance which will then
  //    get switched to the one previously associated with the new tab pages.
  //    This switching will cause the manager to commit the RVH/DOMUI. So we'll
  //    have a committed DOM UI in this case.
  //
  // This condition handles all of these cases:
  //
  //  - First load in first tab: no committed nav entry + pending nav entry +
  //    pending dom ui:
  //    -> Use pending DOM UI if any.
  //
  //  - First load in second tab: no committed nav entry + pending nav entry +
  //    no pending DOM UI:
  //    -> Use the committed DOM UI if any.
  //
  //  - Second navigation in any tab: committed nav entry + pending nav entry:
  //    -> Use pending DOM UI if any.
  //
  //  - Normal state with no load: committed nav entry + no pending nav entry:
  //    -> Use committed DOM UI.
  if (controller_.pending_entry() &&
      (controller_.GetLastCommittedEntry() ||
       render_manager_.pending_dom_ui()))
    return render_manager_.pending_dom_ui();
  return render_manager_.dom_ui();
}
