// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/file_version_info.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/time.h"
#include "chrome/browser/autofill_manager.h"
#include "chrome/browser/blocked_popup_container.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/dom_ui/dom_ui_factory.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_request_manager.h"
#include "chrome/browser/gears_integration.h"
#include "chrome/browser/google_util.h"
#include "chrome/browser/hung_renderer_dialog.h"
#include "chrome/browser/jsmessage_box_handler.h"
#include "chrome/browser/load_from_memory_cache_details.h"
#include "chrome/browser/load_notification_details.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/plugin_installer.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/browser/thumbnail_store.h"
#include "chrome/browser/search_engines/template_url_fetcher.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/page_action.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domain.h"

#if defined(OS_WIN)
// For CRect
#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>
// TODO(port): some of these headers should be ported.
#include "chrome/browser/modal_html_dialog_delegate.h"
#include "views/controls/scrollbar/native_scroll_bar.h"
#endif

// Cross-Site Navigations
//
// If a TabContents is told to navigate to a different web site (as determined
// by SiteInstance), it will replace its current RenderViewHost with a new
// RenderViewHost dedicated to the new SiteInstance.  This works as follows:
//
// - Navigate determines whether the destination is cross-site, and if so,
//   it creates a pending_render_view_host_ and moves into the PENDING
//   RendererState.
// - The pending RVH is "suspended," so that no navigation messages are sent to
//   its renderer until the onbeforeunload JavaScript handler has a chance to
//   run in the current RVH.
// - The pending RVH tells CrossSiteRequestManager (a thread-safe singleton)
//   that it has a pending cross-site request.  ResourceDispatcherHost will
//   check for this when the response arrives.
// - The current RVH runs its onbeforeunload handler.  If it returns false, we
//   cancel all the pending logic and go back to NORMAL.  Otherwise we allow
//   the pending RVH to send the navigation request to its renderer.
// - ResourceDispatcherHost receives a ResourceRequest on the IO thread.  It
//   checks CrossSiteRequestManager to see that the RVH responsible has a
//   pending cross-site request, and then installs a CrossSiteEventHandler.
// - When RDH receives a response, the BufferedEventHandler determines whether
//   it is a download.  If so, it sends a message to the new renderer causing
//   it to cancel the request, and the download proceeds in the download
//   thread.  For now, we stay in a PENDING state (with a pending RVH) until
//   the next DidNavigate event for this TabContents.  This isn't ideal, but it
//   doesn't affect any functionality.
// - After RDH receives a response and determines that it is safe and not a
//   download, it pauses the response to first run the old page's onunload
//   handler.  It does this by asynchronously calling the OnCrossSiteResponse
//   method of TabContents on the UI thread, which sends a ClosePage message
//   to the current RVH.
// - Once the onunload handler is finished, a ClosePage_ACK message is sent to
//   the ResourceDispatcherHost, who unpauses the response.  Data is then sent
//   to the pending RVH.
// - The pending renderer sends a FrameNavigate message that invokes the
//   DidNavigate method.  This replaces the current RVH with the
//   pending RVH and goes back to the NORMAL RendererState.

namespace {

// Amount of time we wait between when a key event is received and the renderer
// is queried for its state and pushed to the NavigationEntry.
const int kQueryStateDelay = 5000;

const int kSyncWaitDelay = 40;

// If another javascript message box is displayed within
// kJavascriptMessageExpectedDelay of a previous javascript message box being
// dismissed, display an option to suppress future message boxes from this
// contents.
const int kJavascriptMessageExpectedDelay = 1000;

const char kLinkDoctorBaseURL[] =
    "http://linkhelp.clients.google.com/tbproxy/lh/fixurl";

// The list of prefs we want to observe.
const wchar_t* kPrefsToObserve[] = {
  prefs::kAlternateErrorPagesEnabled,
  prefs::kWebKitJavaEnabled,
  prefs::kWebKitJavascriptEnabled,
  prefs::kWebKitLoadsImagesAutomatically,
  prefs::kWebKitPluginsEnabled,
  prefs::kWebKitUsesUniversalDetector,
  prefs::kWebKitSerifFontFamily,
  prefs::kWebKitSansSerifFontFamily,
  prefs::kWebKitFixedFontFamily,
  prefs::kWebKitDefaultFontSize,
  prefs::kWebKitDefaultFixedFontSize,
  prefs::kDefaultCharset
  // kWebKitStandardFontIsSerif needs to be added
  // if we let users pick which font to use, serif or sans-serif when
  // no font is specified or a CSS generic family (serif or sans-serif)
  // is not specified.
};

const int kPrefsToObserveLength = arraysize(kPrefsToObserve);

// Limit on the number of suggestions to appear in the pop-up menu under an
// text input element in a form.
const int kMaxAutofillMenuItems = 6;

// Returns true if the entry's transition type is FORM_SUBMIT.
bool IsFormSubmit(const NavigationEntry* entry) {
  return (PageTransition::StripQualifier(entry->transition_type()) ==
          PageTransition::FORM_SUBMIT);
}

#if defined(OS_WIN)

BOOL CALLBACK InvalidateWindow(HWND hwnd, LPARAM lparam) {
  // Note: erase is required to properly paint some widgets borders. This can
  // be seen with textfields.
  InvalidateRect(hwnd, NULL, TRUE);
  return TRUE;
}
#endif

void MakeNavigateParams(const NavigationEntry& entry, bool reload,
                        ViewMsg_Navigate_Params* params) {
  params->page_id = entry.page_id();
  params->url = entry.url();
  params->referrer = entry.referrer();
  params->transition = entry.transition_type();
  params->state = entry.content_state();
  params->reload = reload;
  params->request_time = base::Time::Now();
}

}  // namespace

// -----------------------------------------------------------------------------

// static
int TabContents::find_request_id_counter_ = -1;

class TabContents::GearsCreateShortcutCallbackFunctor {
 public:
  explicit GearsCreateShortcutCallbackFunctor(TabContents* contents)
     : contents_(contents) {}

  void Run(const GearsShortcutData2& shortcut_data, bool success) {
    if (contents_)
      contents_->OnGearsCreateShortcutDone(shortcut_data, success);
    delete this;
  }
  void Cancel() {
    contents_ = NULL;
  }

 private:
  TabContents* contents_;
};

TabContents::TabContents(Profile* profile,
                         SiteInstance* site_instance,
                         int routing_id,
                         base::WaitableEvent* modal_dialog_event)
    : delegate_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(controller_(this, profile)),
      ALLOW_THIS_IN_INITIALIZER_LIST(view_(
          TabContentsView::Create(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(render_manager_(this, this)),
      property_bag_(),
      registrar_(),
      ALLOW_THIS_IN_INITIALIZER_LIST(printing_(*this)),
      save_package_(),
      cancelable_consumer_(),
      autofill_manager_(),
      password_manager_(),
      plugin_installer_(),
      ALLOW_THIS_IN_INITIALIZER_LIST(fav_icon_helper_(this)),
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
      blocked_popups_(NULL),
      infobar_delegates_(),
      find_ui_active_(false),
      find_op_aborted_(false),
      current_find_request_id_(find_request_id_counter_++),
      find_text_(),
      last_search_case_sensitive_(false),
      last_search_prepopulate_text_(NULL),
      last_search_result_(),
      capturing_contents_(false),
      is_being_destroyed_(false),
      notify_disconnection_(false),
      history_requests_(),
#if defined(OS_WIN)
      message_box_active_(CreateEvent(NULL, TRUE, FALSE, NULL)),
#endif
      last_javascript_message_dismissal_(),
      suppress_javascript_messages_(false) {
  pending_install_.page_id = 0;
  pending_install_.callback_functor = NULL;

#if defined(LINUX2)
  // Make sure the thumbnailer is started before starting the render manager.
  // The thumbnailer will want to listen for RVH creations, one of which will
  // happen in RVHManager::Init.
  ThumbnailGenerator* generator = g_browser_process->GetThumbnailGenerator();
  if (generator)
    generator->StartThumbnailing();
#endif

  render_manager_.Init(profile, site_instance, routing_id, modal_dialog_event);

  view_->CreateView();

  // Register for notifications about all interested prefs change.
  PrefService* prefs = profile->GetPrefs();
  if (prefs) {
    for (int i = 0; i < kPrefsToObserveLength; ++i)
      prefs->AddPrefObserver(kPrefsToObserve[i], this);
  }

  // Register for notifications about URL starredness changing on any profile.
  registrar_.Add(this, NotificationType::URLS_STARRED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::BOOKMARK_MODEL_LOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::RENDER_WIDGET_HOST_DESTROYED,
                 NotificationService::AllSources());

  // Keep a global copy of the previous search string (if any).
  static string16 global_last_search = string16();
  last_search_prepopulate_text_ = &global_last_search;
}

TabContents::~TabContents() {
  is_being_destroyed_ = true;

  // We don't want any notifications while we're runnign our destructor.
  registrar_.RemoveAll();

  // Unregister the notifications of all observed prefs change.
  PrefService* prefs = profile()->GetPrefs();
  if (prefs) {
    for (int i = 0; i < kPrefsToObserveLength; ++i)
      prefs->RemovePrefObserver(kPrefsToObserve[i], this);
  }

  // Clean up subwindows like plugins and the find in page bar.
  view_->OnContentsDestroy();

  NotifyDisconnected();
  HungRendererDialog::HideForTabContents(this);

  if (pending_install_.callback_functor)
    pending_install_.callback_functor->Cancel();

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

  if (blocked_popups_)
    blocked_popups_->Destroy();

  // Notify any observer that have a reference on this tab contents.
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_DESTROYED,
      Source<TabContents>(this),
      NotificationService::NoDetails());

  // Notify any lasting InfobarDelegates that have not yet been removed that
  // whatever infobar they were handling in this TabContents has closed,
  // because the TabContents is going away entirely.
  // This must happen after the TAB_CONTENTS_DESTROYED notification as the
  // notification may trigger infobars calls that access their delegate. (and
  // some implementations of InfoBarDelegate do delete themselves on
  // InfoBarClosed()).
  for (int i = 0; i < infobar_delegate_count(); ++i) {
    InfoBarDelegate* delegate = GetInfoBarDelegateAt(i);
    delegate->InfoBarClosed();
  }
  infobar_delegates_.clear();

  // TODO(brettw) this should be moved to the view.
#if defined(OS_WIN)
  // If we still have a window handle, destroy it. GetNativeView can return
  // NULL if this contents was part of a window that closed.
  if (GetNativeView())
    ::DestroyWindow(GetNativeView());
#endif
}

// static
void TabContents::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kAlternateErrorPagesEnabled, true);

  WebPreferences pref_defaults;
  prefs->RegisterBooleanPref(prefs::kWebKitJavascriptEnabled,
                             pref_defaults.javascript_enabled);
  prefs->RegisterBooleanPref(prefs::kWebKitWebSecurityEnabled,
                             pref_defaults.web_security_enabled);
  prefs->RegisterBooleanPref(
      prefs::kWebKitJavascriptCanOpenWindowsAutomatically, true);
  prefs->RegisterBooleanPref(prefs::kWebKitLoadsImagesAutomatically,
                             pref_defaults.loads_images_automatically);
  prefs->RegisterBooleanPref(prefs::kWebKitPluginsEnabled,
                             pref_defaults.plugins_enabled);
  prefs->RegisterBooleanPref(prefs::kWebKitDomPasteEnabled,
                             pref_defaults.dom_paste_enabled);
  prefs->RegisterBooleanPref(prefs::kWebKitShrinksStandaloneImagesToFit,
                             pref_defaults.shrinks_standalone_images_to_fit);
  prefs->RegisterStringPref(prefs::kWebKitInspectorSettings,
                            pref_defaults.inspector_settings);
  prefs->RegisterBooleanPref(prefs::kWebKitTextAreasAreResizable,
                             pref_defaults.text_areas_are_resizable);
  prefs->RegisterBooleanPref(prefs::kWebKitJavaEnabled,
                             pref_defaults.java_enabled);

  prefs->RegisterLocalizedStringPref(prefs::kAcceptLanguages,
                                     IDS_ACCEPT_LANGUAGES);
  prefs->RegisterLocalizedStringPref(prefs::kDefaultCharset,
                                     IDS_DEFAULT_ENCODING);
  prefs->RegisterLocalizedBooleanPref(prefs::kWebKitStandardFontIsSerif,
                                      IDS_STANDARD_FONT_IS_SERIF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitFixedFontFamily,
                                     IDS_FIXED_FONT_FAMILY);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitSerifFontFamily,
                                     IDS_SERIF_FONT_FAMILY);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitSansSerifFontFamily,
                                     IDS_SANS_SERIF_FONT_FAMILY);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitCursiveFontFamily,
                                     IDS_CURSIVE_FONT_FAMILY);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitFantasyFontFamily,
                                     IDS_FANTASY_FONT_FAMILY);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitDefaultFontSize,
                                      IDS_DEFAULT_FONT_SIZE);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitDefaultFixedFontSize,
                                      IDS_DEFAULT_FIXED_FONT_SIZE);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitMinimumFontSize,
                                      IDS_MINIMUM_FONT_SIZE);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitMinimumLogicalFontSize,
                                      IDS_MINIMUM_LOGICAL_FONT_SIZE);
  prefs->RegisterLocalizedBooleanPref(prefs::kWebKitUsesUniversalDetector,
                                      IDS_USES_UNIVERSAL_DETECTOR);
  prefs->RegisterLocalizedStringPref(prefs::kStaticEncodings,
                                     IDS_STATIC_ENCODING_LIST);
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
  // Transient entries take precedence. They are used for interstitial pages
  // that are shown on top of existing pages.
  NavigationEntry* entry = controller_.GetTransientEntry();
  if (entry)
    return entry->GetTitleForDisplay(&controller_);

  DOMUI* our_dom_ui = render_manager_.pending_dom_ui() ?
      render_manager_.pending_dom_ui() : render_manager_.dom_ui();
  if (our_dom_ui) {
    // Don't override the title in view source mode.
    entry = controller_.GetActiveEntry();
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
  // IDs in sync. Only TabContents will also have site instances, except during
  // testing.
  if (GetSiteInstance())
    GetSiteInstance()->UpdateMaxPageID(page_id);
  process()->UpdateMaxPageID(page_id);
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

  // Hide the URL in chrome-extension://.
  if (GetURL().SchemeIs(chrome::kExtensionScheme))
    return false;

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
  NotifyNavigationStateChanged(INVALIDATE_TAB);
}

void TabContents::SetPageActionEnabled(const PageAction* page_action,
                                       bool enable) {
  DCHECK(page_action);

  if (enable == IsPageActionEnabled(page_action))
    return;  // Don't need to do anything more.

  if (enable)
    enabled_page_actions_.insert(page_action);
  else
    enabled_page_actions_.erase(page_action);
}

bool TabContents::IsPageActionEnabled(const PageAction* page_action) {
  DCHECK(page_action);
  return enabled_page_actions_.end() != enabled_page_actions_.find(page_action);
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
}

void TabContents::HideContents() {
  // TODO(pkasting): http://b/1239839  Right now we purposefully don't call
  // our superclass HideContents(), because some callers want to be very picky
  // about the order in which these get called.  In addition to making the code
  // here practically impossible to understand, this also means we end up
  // calling TabContents::WasHidden() twice if callers call both versions of
  // HideContents() on a TabContents.
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
  DevToolsManager* devtools_manager = DevToolsManager::GetInstance();
  if (devtools_manager) {  // NULL in unit tests.
    devtools_manager->OnNavigatingToPendingEntry(
        render_view_host(),
        dest_render_view_host,
        controller_.pending_entry()->url());
  }

  // Used for page load time metrics.
  current_load_start_ = base::TimeTicks::Now();

  // Navigate in the desired RenderViewHost.
  ViewMsg_Navigate_Params navigate_params;
  MakeNavigateParams(entry, reload, &navigate_params);
  dest_render_view_host->Navigate(navigate_params);

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
  TabContents* tc = new TabContents(profile(),
                                    SiteInstance::CreateSiteInstance(profile()),
                                    MSG_ROUTING_NONE, NULL);
  tc->controller().CopyStateFrom(controller_);
  return tc;
}

void TabContents::CreateShortcut() {
  NavigationEntry* entry = controller_.GetLastCommittedEntry();
  if (!entry)
    return;

  // We only allow one pending install request. By resetting the page id we
  // effectively cancel the pending install request.
  pending_install_.page_id = entry->page_id();
  pending_install_.icon = GetFavIcon();
  pending_install_.title = UTF16ToWideHack(GetTitle());
  pending_install_.url = GetURL();
  if (pending_install_.callback_functor) {
    pending_install_.callback_functor->Cancel();
    pending_install_.callback_functor = NULL;
  }
  DCHECK(!pending_install_.icon.isNull()) << "Menu item should be disabled.";
  if (pending_install_.title.empty())
    pending_install_.title = UTF8ToWide(GetURL().spec());

  // Request the application info. When done OnDidGetApplicationInfo is invoked
  // and we'll create the shortcut.
  render_view_host()->GetApplicationInfo(pending_install_.page_id);
}

#if defined(OS_WIN) || defined(OS_LINUX)
ConstrainedWindow* TabContents::CreateConstrainedDialog(
      ConstrainedWindowDelegate* delegate) {
  ConstrainedWindow* window =
      ConstrainedWindow::CreateConstrainedDialog(this, delegate);
  child_windows_.push_back(window);
  return window;
}
#endif

void TabContents::AddNewContents(TabContents* new_contents,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture,
                                 const GURL& creator_url) {
  if (!delegate_)
    return;

#if defined(OS_WIN) || defined(OS_LINUX)
  if ((disposition == NEW_POPUP) && !user_gesture &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisablePopupBlocking)) {
    // Unrequested popups from normal pages are constrained unless they're in
    // the whitelist.  The popup owner will handle checking this.
    delegate_->GetConstrainingContents(this)->AddPopup(new_contents,
        initial_pos,
        creator_url.is_valid() ? creator_url.host() : std::string());
  } else {
    new_contents->DisassociateFromPopupCount();
    delegate_->AddNewContents(this, new_contents, disposition, initial_pos,
                              user_gesture);
  }
  PopupNotificationVisibilityChanged(ShowingBlockedPopupNotification());
#else
  // TODO(port): implement the popup blocker stuff
  delegate_->AddNewContents(this, new_contents, disposition, initial_pos,
                            user_gesture);
#endif
}

void TabContents::CloseAllSuppressedPopups() {
  if (blocked_popups_)
    blocked_popups_->CloseAll();
}

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

void TabContents::FocusThroughTabTraversal(bool reverse) {
  if (showing_interstitial_page()) {
    render_manager_.interstitial_page()->FocusThroughTabTraversal(reverse);
    return;
  }
  render_view_host()->SetInitialFocus(reverse);
}

bool TabContents::FocusLocationBarByDefault() {
  DOMUI* dom_ui = GetDOMUIForCurrentState();
  if (dom_ui)
    return dom_ui->focus_location_bar_by_default();
  NavigationEntry* entry = controller_.GetActiveEntry();
  if (entry && entry->url() == GURL("about:blank"))
    return true;
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

void TabContents::ToolbarSizeChanged(bool is_animating) {
  TabContentsDelegate* d = delegate();
  if (d)
    d->ToolbarSizeChanged(this, is_animating);
}

void TabContents::OnStartDownload(DownloadItem* download) {
  DCHECK(download);

  // Download in a constrained popup is shown in the tab that opened it.
  TabContents* tab_contents = delegate()->GetConstrainingContents(this);

  if (tab_contents && tab_contents->delegate())
    tab_contents->delegate()->OnStartDownload(download);
}

void TabContents::WillClose(ConstrainedWindow* window) {
  ConstrainedWindowList::iterator it =
      find(child_windows_.begin(), child_windows_.end(), window);
  if (it != child_windows_.end())
    child_windows_.erase(it);
}

void TabContents::WillCloseBlockedPopupContainer(
    BlockedPopupContainer* container) {
  DCHECK(blocked_popups_ == container);
  blocked_popups_ = NULL;
}

void TabContents::DidMoveOrResize(ConstrainedWindow* window) {
#if defined(OS_WIN)
  UpdateWindow(GetNativeView());
#endif
}

void TabContents::StartFinding(string16 find_text,
                               bool forward_direction,
                               bool case_sensitive) {
  // If find_text is empty, it means FindNext was pressed with a keyboard
  // shortcut so unless we have something to search for we return early.
  if (find_text.empty() && find_text_.empty()) {
    if (last_search_prepopulate_text_->empty())
      return;
    // Try whatever we searched for last in any tab.
    find_text = *last_search_prepopulate_text_;
  }

  // This is a FindNext operation if we are searching for the same text again,
  // or if the passed in search text is empty (FindNext keyboard shortcut). The
  // exception to this is if the Find was aborted (then we don't want FindNext
  // because the highlighting has been cleared and we need it to reappear). We
  // therefore treat FindNext after an aborted Find operation as a full fledged
  // Find.
  bool find_next = (find_text_ == find_text || find_text.empty()) &&
                   (last_search_case_sensitive_ == case_sensitive) &&
                   !find_op_aborted_;
  if (!find_next)
    current_find_request_id_ = find_request_id_counter_++;

  if (!find_text.empty())
    find_text_ = find_text;
  last_search_case_sensitive_ = case_sensitive;

  find_op_aborted_ = false;

  // Keep track of what the last search was across the tabs.
  *last_search_prepopulate_text_ = find_text;

  render_view_host()->StartFinding(current_find_request_id_,
                                   find_text_,
                                   forward_direction,
                                   case_sensitive,
                                   find_next);
}

void TabContents::StopFinding(bool clear_selection) {
  // When |clear_selection| is true, it means the find string has been cleared
  // by the user, but the UI has not been dismissed.
  if (!clear_selection)
    find_ui_active_ = false;
  find_op_aborted_ = true;
  last_search_result_ = FindNotificationDetails();
  render_view_host()->StopFinding(clear_selection);
}

void TabContents::OnJavaScriptMessageBoxClosed(IPC::Message* reply_msg,
                                               bool success,
                                               const std::wstring& prompt) {
  last_javascript_message_dismissal_ = base::TimeTicks::Now();
  render_manager_.OnJavaScriptMessageBoxClosed(reply_msg, success, prompt);
}

void TabContents::OnJavaScriptMessageBoxWindowDestroyed() {
  render_manager_.OnJavaScriptMessageBoxWindowDestroyed();
}

void TabContents::OnSavePage() {
  // If we can not save the page, try to download it.
  if (!SavePackage::IsSavableContents(contents_mime_type())) {
    DownloadManager* dlm = profile()->GetDownloadManager();
    const GURL& current_page_url = GetURL();
    if (dlm && current_page_url.is_valid())
      dlm->DownloadUrl(current_page_url, GURL(), "", this);
    return;
  }

  Stop();

  // Create the save package and possibly prompt the user for the name to save
  // the page as. The user prompt is an asynchronous operation that runs on
  // another thread.
  save_package_ = new SavePackage(this);
  save_package_->GetSaveInfo();
}

// Used in automated testing to bypass prompting the user for file names.
// Instead, the names and paths are hard coded rather than running them through
// file name sanitation and extension / mime checking.
void TabContents::SavePage(const std::wstring& main_file,
                           const std::wstring& dir_path,
                           SavePackage::SavePackageType save_type) {
  // Stop the page from navigating.
  Stop();

  save_package_ = new SavePackage(this, save_type,
                                  FilePath::FromWStringHack(main_file),
                                  FilePath::FromWStringHack(dir_path));
  save_package_->Init();
}

void TabContents::PrintPreview() {
  // We don't show the print preview yet, only the print dialog.
  PrintNow();
}

bool TabContents::PrintNow() {
  // We can't print interstitial page for now.
  if (showing_interstitial_page())
    return false;

  return render_view_host()->PrintPages();
}

void TabContents::PrintingDone(int document_cookie, bool success) {
  render_view_host()->PrintingDone(document_cookie, success);
}

bool TabContents::IsActiveEntry(int32 page_id) {
  NavigationEntry* active_entry = controller_.GetActiveEntry();
  return (active_entry != NULL &&
          active_entry->site_instance() == GetSiteInstance() &&
          active_entry->page_id() == page_id);
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
  NotifyNavigationStateChanged(INVALIDATE_LOAD);

  NotificationType type = is_loading ? NotificationType::LOAD_START :
      NotificationType::LOAD_STOP;
  NotificationDetails det = NotificationService::NoDetails();
  if (details)
      det = Details<LoadNotificationDetails>(details);
  NotificationService::current()->Notify(type,
      Source<NavigationController>(&controller_),
      det);
}

#if defined(OS_WIN) || defined(OS_LINUX)
void TabContents::CreateBlockedPopupContainerIfNecessary() {
  if (blocked_popups_)
    return;

  blocked_popups_ = BlockedPopupContainer::Create(this, profile());
}

void TabContents::AddPopup(TabContents* new_contents,
                           const gfx::Rect& initial_pos,
                           const std::string& host) {
  CreateBlockedPopupContainerIfNecessary();
  blocked_popups_->AddTabContents(new_contents, initial_pos, host);
}
#endif

// TODO(brettw) This should be on the TabContentsView.
void TabContents::RepositionSupressedPopupsToFit() {
  if (blocked_popups_)
    blocked_popups_->RepositionBlockedPopupContainer();
}

bool TabContents::ShowingBlockedPopupNotification() const {
  return blocked_popups_ != NULL &&
      blocked_popups_->GetBlockedPopupCount() != 0;
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

void TabContents::DidNavigateMainFramePostCommit(
    const NavigationController::LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
  if (details.is_user_initiated_main_frame_load()) {
    // Clear the status bubble. This is a workaround for a bug where WebKit
    // doesn't let us know that the cursor left an element during a
    // transition (this is also why the mouse cursor remains as a hand after
    // clicking on a link); see bugs 1184641 and 980803. We don't want to
    // clear the bubble when a user navigates to a named anchor in the same
    // page.
    UpdateTargetURL(details.entry->page_id(), GURL());

    // UpdateHelpersForDidNavigate will handle the case where the password_form
    // origin is valid.
    // TODO(brettw) bug 1343111: Password manager stuff in here needs to be
    // cleaned up and covered by tests.
    if (!params.password_form.origin.is_valid())
      GetPasswordManager()->DidNavigate();
  }

  // The keyword generator uses the navigation entries, so must be called after
  // the commit.
  GenerateKeywordIfNecessary(params);

  // Allow the new page to set the title again.
  received_page_title_ = false;

  // Get the favicon, either from history or request it from the net.
  fav_icon_helper_.FetchFavIcon(details.entry->url());

  // Disable all page actions.
  enabled_page_actions_.clear();

  // Close constrained popups if necessary.
  MaybeCloseChildWindows(details.previous_url, details.entry->url());

  // Update the starred state.
  UpdateStarredStateForCurrentURL();
}

void TabContents::DidNavigateAnyFramePostCommit(
    RenderViewHost* render_view_host,
    const NavigationController::LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
  // If we navigate, start showing messages again. This does nothing to prevent
  // a malicious script from spamming messages, since the script could just
  // reload the page to stop blocking.
  suppress_javascript_messages_ = false;

  // Update history. Note that this needs to happen after the entry is complete,
  // which WillNavigate[Main,Sub]Frame will do before this function is called.
  if (params.should_update_history) {
    // Most of the time, the displayURL matches the loaded URL, but for about:
    // URLs, we use a data: URL as the real value.  We actually want to save
    // the about: URL to the history db and keep the data: URL hidden. This is
    // what the TabContents' URL getter does.
    UpdateHistoryForNavigation(GetURL(), params);
  }

  // Notify the password manager of the navigation or form submit.
  // TODO(brettw) bug 1343111: Password manager stuff in here needs to be
  // cleaned up and covered by tests.
  if (params.password_form.origin.is_valid())
    GetPasswordManager()->ProvisionallySavePassword(params.password_form);
}

void TabContents::MaybeCloseChildWindows(const GURL& previous_url,
                                         const GURL& current_url) {
  if (net::RegistryControlledDomainService::SameDomainOrHost(
          previous_url, current_url))
    return;

  // Clear out any child windows since we are leaving this page entirely.
  // We use indices instead of iterators in case CloseWindow does something
  // that may invalidate an iterator.
  int size = static_cast<int>(child_windows_.size());
  for (int i = size - 1; i >= 0; --i) {
    ConstrainedWindow* window = child_windows_[i];
    if (window)
      window->CloseConstrainedWindow();
  }
}

void TabContents::UpdateStarredStateForCurrentURL() {
  BookmarkModel* model = profile()->GetBookmarkModel();
  const bool old_state = is_starred_;
  is_starred_ = (model && model->IsBookmarked(GetURL()));

  if (is_starred_ != old_state && delegate())
    delegate()->URLStarredChanged(this, is_starred_);
}

void TabContents::UpdateAlternateErrorPageURL() {
  GURL url = GetAlternateErrorPageURL();
  render_view_host()->SetAlternateErrorPageURL(url);
}

void TabContents::UpdateWebPreferences() {
  render_view_host()->UpdateWebPreferences(GetWebkitPrefs());
}

void TabContents::UpdateMaxPageIDIfNecessary(SiteInstance* site_instance,
                                             RenderViewHost* rvh) {
  // If we are creating a RVH for a restored controller, then we might
  // have more page IDs than the SiteInstance's current max page ID.  We must
  // make sure that the max page ID is larger than any restored page ID.
  // Note that it is ok for conflicting page IDs to exist in another tab
  // (i.e., NavigationController), but if any page ID is larger than the max,
  // the back/forward list will get confused.
  int max_restored_page_id = controller_.max_restored_page_id();
  if (max_restored_page_id > 0) {
    int curr_max_page_id = site_instance->max_page_id();
    if (max_restored_page_id > curr_max_page_id) {
      // Need to update the site instance immediately.
      site_instance->UpdateMaxPageID(max_restored_page_id);

      // Also tell the renderer to update its internal representation.  We
      // need to reserve enough IDs to make all restored page IDs less than
      // the max.
      if (curr_max_page_id < 0)
        curr_max_page_id = 0;
      rvh->ReservePageIDRange(max_restored_page_id - curr_max_page_id);
    }
  }
}

void TabContents::UpdateHistoryForNavigation(
    const GURL& display_url,
    const ViewHostMsg_FrameNavigate_Params& params) {
  if (profile()->IsOffTheRecord())
    return;

  // Add to history service.
  HistoryService* hs = profile()->GetHistoryService(Profile::IMPLICIT_ACCESS);
  if (hs) {
    if (PageTransition::IsMainFrame(params.transition) &&
        display_url != params.url) {
      // Hack on the "display" URL so that it will appear in history. For some
      // types of URLs, we will display a magic URL that is different from where
      // the page is actually navigated. We want the user to see in history
      // what they saw in the URL bar, so we add the display URL as a redirect.
      // This only applies to the main frame, as the display URL doesn't apply
      // to sub-frames.
      std::vector<GURL> redirects = params.redirects;
      if (!redirects.empty())
        redirects.back() = display_url;
      hs->AddPage(display_url, this, params.page_id, params.referrer,
                  params.transition, redirects);
    } else {
      hs->AddPage(params.url, this, params.page_id, params.referrer,
                  params.transition, params.redirects);
    }
  }
}

bool TabContents::UpdateTitleForEntry(NavigationEntry* entry,
                                      const std::wstring& title) {
  // For file URLs without a title, use the pathname instead. In the case of a
  // synthesized title, we don't want the update to count toward the "one set
  // per page of the title to history."
  std::wstring final_title;
  bool explicit_set;
  if (entry->url().SchemeIsFile() && title.empty()) {
    final_title = UTF8ToWide(entry->url().ExtractFileName());
    explicit_set = false;  // Don't count synthetic titles toward the set limit.
  } else {
    TrimWhitespace(title, TRIM_ALL, &final_title);
    explicit_set = true;
  }

  if (final_title == UTF16ToWideHack(entry->title()))
    return false;  // Nothing changed, don't bother.

  entry->set_title(WideToUTF16Hack(final_title));

  // Update the history system for this page.
  if (!profile()->IsOffTheRecord() && !received_page_title_) {
    HistoryService* hs =
        profile()->GetHistoryService(Profile::IMPLICIT_ACCESS);
    if (hs)
      hs->SetPageTitle(entry->display_url(), final_title);

    // Don't allow the title to be saved again for explicitly set ones.
    received_page_title_ = explicit_set;
  }

  // Lastly, set the title for the view.
  view_->SetPageTitle(final_title);

  return true;
}

void TabContents::NotifySwapped() {
  // After sending out a swap notification, we need to send a disconnect
  // notification so that clients that pick up a pointer to |this| can NULL the
  // pointer.  See Bug 1230284.
  notify_disconnection_ = true;
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_SWAPPED,
      Source<TabContents>(this),
      NotificationService::NoDetails());
}

void TabContents::NotifyConnected() {
  notify_disconnection_ = true;
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_CONNECTED,
      Source<TabContents>(this),
      NotificationService::NoDetails());
}

void TabContents::NotifyDisconnected() {
  if (!notify_disconnection_)
    return;

  notify_disconnection_ = false;
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_DISCONNECTED,
      Source<TabContents>(this),
      NotificationService::NoDetails());
}

void TabContents::GenerateKeywordIfNecessary(
    const ViewHostMsg_FrameNavigate_Params& params) {
  if (!params.searchable_form_url.is_valid())
    return;

  if (profile()->IsOffTheRecord())
    return;

  int last_index = controller_.last_committed_entry_index();
  // When there was no previous page, the last index will be 0. This is
  // normally due to a form submit that opened in a new tab.
  // TODO(brettw) bug 916126: we should support keywords when form submits
  //              happen in new tabs.
  if (last_index <= 0)
    return;
  const NavigationEntry* previous_entry =
      controller_.GetEntryAtIndex(last_index - 1);
  if (IsFormSubmit(previous_entry)) {
    // Only generate a keyword if the previous page wasn't itself a form
    // submit.
    return;
  }

  GURL keyword_url = previous_entry->user_typed_url().is_valid() ?
          previous_entry->user_typed_url() : previous_entry->url();
  std::wstring keyword =
      TemplateURLModel::GenerateKeyword(keyword_url, true);  // autodetected
  if (keyword.empty())
    return;

  TemplateURLModel* url_model = profile()->GetTemplateURLModel();
  if (!url_model)
    return;

  if (!url_model->loaded()) {
    url_model->Load();
    return;
  }

  const TemplateURL* current_url;
  std::wstring url = UTF8ToWide(params.searchable_form_url.spec());
  if (!url_model->CanReplaceKeyword(keyword, url, &current_url))
    return;

  if (current_url) {
    if (current_url->originating_url().is_valid()) {
      // The existing keyword was generated from an OpenSearch description
      // document, don't regenerate.
      return;
    }
    url_model->Remove(current_url);
  }
  TemplateURL* new_url = new TemplateURL();
  new_url->set_keyword(keyword);
  new_url->set_short_name(keyword);
  new_url->SetURL(url, 0, 0);
  new_url->add_input_encoding(params.searchable_form_encoding);
  DCHECK(controller_.GetLastCommittedEntry());
  const GURL& favicon_url =
      controller_.GetLastCommittedEntry()->favicon().url();
  if (favicon_url.is_valid()) {
    new_url->SetFavIconURL(favicon_url);
  } else {
    // The favicon url isn't valid. This means there really isn't a favicon,
    // or the favicon url wasn't obtained before the load started. This assumes
    // the later.
    // TODO(sky): Need a way to set the favicon that doesn't involve generating
    // its url.
    new_url->SetFavIconURL(TemplateURL::GenerateFaviconURL(params.referrer));
  }
  new_url->set_safe_for_autoreplace(true);
  url_model->Add(new_url);
}

RenderViewHostDelegate::View* TabContents::GetViewDelegate() const {
  return view_.get();
}

RenderViewHostDelegate::Save* TabContents::GetSaveDelegate() const {
  return save_package_.get();  // May be NULL, but we can return NULL.
}

RendererPreferences TabContents::GetRendererPrefs() const {
  if (delegate())
    return delegate()->GetRendererPrefs();
  else
    return RendererPreferences();
}

ExtensionFunctionDispatcher* TabContents::CreateExtensionFunctionDispatcher(
    RenderViewHost* render_view_host,
    const std::string& extension_id) {
  return delegate()->CreateExtensionFunctionDispatcher(render_view_host,
      extension_id);
}

TabContents* TabContents::GetAsTabContents() {
  return this;
}

void TabContents::RenderViewCreated(RenderViewHost* render_view_host) {
  NavigationEntry* entry = controller_.GetActiveEntry();
  if (!entry)
    return;

  // When we're creating views, we're still doing initial setup, so we always
  // use the pending DOM UI rather than any possibly existing committed one.
  if (render_manager_.pending_dom_ui()) {
    render_manager_.pending_dom_ui()->RenderViewCreated(render_view_host);
  }

  if (entry->IsViewSourceMode()) {
    // Put the renderer in view source mode.
    render_view_host->Send(
        new ViewMsg_EnableViewSourceMode(render_view_host->routing_id()));
  }

  view()->RenderViewCreated(render_view_host);
}

void TabContents::RenderViewReady(RenderViewHost* rvh) {
  if (rvh != render_view_host()) {
    // Don't notify the world, since this came from a renderer in the
    // background.
    return;
  }

  NotifyConnected();
  SetIsCrashed(false);
}

void TabContents::RenderViewGone(RenderViewHost* rvh) {
  // Ask the print preview if this renderer was valuable.
  if (!printing_.OnRenderViewGone(rvh))
    return;
  if (rvh != render_view_host()) {
    // The pending page's RenderViewHost is gone.
    return;
  }

  SetIsLoading(false, NULL);
  NotifyDisconnected();
  SetIsCrashed(true);

  // Tell the view that we've crashed so it can prepare the sad tab page.
  view_->OnTabCrashed();

  // Hide any visible hung renderer warning for this web contents' process.
  HungRendererDialog::HideForTabContents(this);
}

void TabContents::DidNavigate(RenderViewHost* rvh,
                              const ViewHostMsg_FrameNavigate_Params& params) {
  if (PageTransition::IsMainFrame(params.transition))
    render_manager_.DidNavigateMainFrame(rvh);

  // Update the site of the SiteInstance if it doesn't have one yet.
  if (!GetSiteInstance()->has_site())
    GetSiteInstance()->SetSite(params.url);

  // Need to update MIME type here because it's referred to in
  // UpdateNavigationCommands() called by RendererDidNavigate() to
  // determine whether or not to enable the encoding menu.
  // It's updated only for the main frame. For a subframe,
  // RenderView::UpdateURL does not set params.contents_mime_type.
  // (see http://code.google.com/p/chromium/issues/detail?id=2929 )
  // TODO(jungshik): Add a test for the encoding menu to avoid
  // regressing it again.
  if (PageTransition::IsMainFrame(params.transition))
    contents_mime_type_ = params.contents_mime_type;

  NavigationController::LoadCommittedDetails details;
  if (!controller_.RendererDidNavigate(params, &details))
    return;  // No navigation happened.

  // DO NOT ADD MORE STUFF TO THIS FUNCTION! Your component should either listen
  // for the appropriate notification (best) or you can add it to
  // DidNavigateMainFramePostCommit / DidNavigateAnyFramePostCommit (only if
  // necessary, please).

  // Run post-commit tasks.
  if (details.is_main_frame)
    DidNavigateMainFramePostCommit(details, params);
  DidNavigateAnyFramePostCommit(rvh, details, params);
}

void TabContents::UpdateState(RenderViewHost* rvh,
                              int32 page_id,
                              const std::string& state) {
  DCHECK(rvh == render_view_host());

  // We must be prepared to handle state updates for any page, these occur
  // when the user is scrolling and entering form data, as well as when we're
  // leaving a page, in which case our state may have already been moved to
  // the next page. The navigation controller will look up the appropriate
  // NavigationEntry and update it when it is notified via the delegate.

  int entry_index = controller_.GetEntryIndexWithPageID(
      GetSiteInstance(), page_id);
  if (entry_index < 0)
    return;
  NavigationEntry* entry = controller_.GetEntryAtIndex(entry_index);

  if (state == entry->content_state())
    return;  // Nothing to update.
  entry->set_content_state(state);
  controller_.NotifyEntryChanged(entry, entry_index);
}

void TabContents::UpdateTitle(RenderViewHost* rvh,
                              int32 page_id, const std::wstring& title) {
  // If we have a title, that's a pretty good indication that we've started
  // getting useful data.
  SetNotWaitingForResponse();

  DCHECK(rvh == render_view_host());
  NavigationEntry* entry = controller_.GetEntryWithPageID(GetSiteInstance(),
                                                            page_id);
  if (!entry || !UpdateTitleForEntry(entry, title))
    return;

  // Broadcast notifications when the UI should be updated.
  if (entry == controller_.GetEntryAtOffset(0))
    NotifyNavigationStateChanged(INVALIDATE_TAB);
}

void TabContents::UpdateEncoding(RenderViewHost* render_view_host,
                                 const std::wstring& encoding) {
  set_encoding(encoding);
}

void TabContents::UpdateTargetURL(int32 page_id, const GURL& url) {
  if (delegate())
    delegate()->UpdateTargetURL(this, url);
}

void TabContents::UpdateThumbnail(const GURL& url,
                                  const SkBitmap& bitmap,
                                  const ThumbnailScore& score) {
  // Tell History about this thumbnail
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kThumbnailStore)) {
    profile()->GetThumbnailStore()->
        SetPageThumbnail(url, bitmap, score, !profile()->IsOffTheRecord());
  } else {
    HistoryService* hs;
    if (!profile()->IsOffTheRecord() &&
        (hs = profile()->GetHistoryService(Profile::IMPLICIT_ACCESS))) {
      hs->SetPageThumbnail(url, bitmap, score);
    }
  }
}

void TabContents::UpdateInspectorSettings(const std::wstring& raw_settings) {
  profile()->GetPrefs()->SetString(prefs::kWebKitInspectorSettings,
                                   raw_settings);
}

void TabContents::Close(RenderViewHost* rvh) {
  // Ignore this if it comes from a RenderViewHost that we aren't showing.
  if (delegate() && rvh == render_view_host())
    delegate()->CloseContents(this);
}

void TabContents::RequestMove(const gfx::Rect& new_bounds) {
  if (delegate() && delegate()->IsPopup(this))
    delegate()->MoveContents(this, new_bounds);
}

void TabContents::DidStartLoading(RenderViewHost* rvh) {
  SetIsLoading(true, NULL);
}

void TabContents::DidStopLoading(RenderViewHost* rvh) {
  scoped_ptr<LoadNotificationDetails> details;

  NavigationEntry* entry = controller_.GetActiveEntry();
  // An entry may not exist for a stop when loading an initial blank page or
  // if an iframe injected by script into a blank page finishes loading.
  if (entry) {
    scoped_ptr<base::ProcessMetrics> metrics(
        base::ProcessMetrics::CreateProcessMetrics(
            process()->process().handle()));

    base::TimeDelta elapsed = base::TimeTicks::Now() - current_load_start_;

    details.reset(new LoadNotificationDetails(
        entry->display_url(),
        entry->transition_type(),
        elapsed,
        &controller_,
        controller_.GetCurrentEntryIndex()));
  }

  // Tell PasswordManager we've finished a page load, which serves as a
  // green light to save pending passwords and reset itself.
  GetPasswordManager()->DidStopLoading();

  SetIsLoading(false, details.get());
}

void TabContents::DidStartProvisionalLoadForFrame(
    RenderViewHost* render_view_host,
    bool is_main_frame,
    const GURL& url) {
  ProvisionalLoadDetails details(is_main_frame,
                                 controller_.IsURLInPageNavigation(url),
                                 url, std::string(), false);
  NotificationService::current()->Notify(
      NotificationType::FRAME_PROVISIONAL_LOAD_START,
      Source<NavigationController>(&controller_),
      Details<ProvisionalLoadDetails>(&details));
}

void TabContents::DidRedirectProvisionalLoad(int32 page_id,
                                             const GURL& source_url,
                                             const GURL& target_url) {
  NavigationEntry* entry;
  if (page_id == -1)
    entry = controller_.pending_entry();
  else
    entry = controller_.GetEntryWithPageID(GetSiteInstance(), page_id);
  if (!entry || entry->url() != source_url)
    return;
  entry->set_url(target_url);
}

void TabContents::DidLoadResourceFromMemoryCache(
    const GURL& url,
    const std::string& frame_origin,
    const std::string& main_frame_origin,
    const std::string& security_info) {
  // Send out a notification that we loaded a resource from our memory cache.
  int cert_id = 0, cert_status = 0, security_bits = 0;
  SSLManager::DeserializeSecurityInfo(security_info,
                                      &cert_id, &cert_status,
                                      &security_bits);
  LoadFromMemoryCacheDetails details(url, frame_origin, main_frame_origin,
                                     process()->pid(), cert_id, cert_status);

  NotificationService::current()->Notify(
      NotificationType::LOAD_FROM_MEMORY_CACHE,
      Source<NavigationController>(&controller_),
      Details<LoadFromMemoryCacheDetails>(&details));
}

void TabContents::DidFailProvisionalLoadWithError(
    RenderViewHost* render_view_host,
    bool is_main_frame,
    int error_code,
    const GURL& url,
    bool showing_repost_interstitial) {
  if (net::ERR_ABORTED == error_code) {
    // EVIL HACK ALERT! Ignore failed loads when we're showing interstitials.
    // This means that the interstitial won't be torn down properly, which is
    // bad. But if we have an interstitial, go back to another tab type, and
    // then load the same interstitial again, we could end up getting the first
    // interstitial's "failed" message (as a result of the cancel) when we're on
    // the second one.
    //
    // We can't tell this apart, so we think we're tearing down the current page
    // which will cause a crash later one. There is also some code in
    // RenderViewHostManager::RendererAbortedProvisionalLoad that is commented
    // out because of this problem.
    //
    // http://code.google.com/p/chromium/issues/detail?id=2855
    // Because this will not tear down the interstitial properly, if "back" is
    // back to another tab type, the interstitial will still be somewhat alive
    // in the previous tab type. If you navigate somewhere that activates the
    // tab with the interstitial again, you'll see a flash before the new load
    // commits of the interstitial page.
    if (showing_interstitial_page()) {
      LOG(WARNING) << "Discarding message during interstitial.";
      return;
    }

    // This will discard our pending entry if we cancelled the load (e.g., if we
    // decided to download the file instead of load it). Only discard the
    // pending entry if the URLs match, otherwise the user initiated a navigate
    // before the page loaded so that the discard would discard the wrong entry.
    NavigationEntry* pending_entry = controller_.pending_entry();
    if (pending_entry && pending_entry->url() == url) {
      controller_.DiscardNonCommittedEntries();
      // Update the URL display.
      NotifyNavigationStateChanged(TabContents::INVALIDATE_URL);
    }

    render_manager_.RendererAbortedProvisionalLoad(render_view_host);
  }

  // Send out a notification that we failed a provisional load with an error.
  ProvisionalLoadDetails details(is_main_frame,
                                 controller_.IsURLInPageNavigation(url),
                                 url, std::string(), false);
  details.set_error_code(error_code);

  NotificationService::current()->Notify(
      NotificationType::FAIL_PROVISIONAL_LOAD_WITH_ERROR,
      Source<NavigationController>(&controller_),
      Details<ProvisionalLoadDetails>(&details));
}

void TabContents::UpdateFavIconURL(RenderViewHost* render_view_host,
                                   int32 page_id,
                                   const GURL& icon_url) {
  fav_icon_helper_.SetFavIconURL(icon_url);
}

void TabContents::DidDownloadImage(
    RenderViewHost* render_view_host,
    int id,
    const GURL& image_url,
    bool errored,
    const SkBitmap& image) {
  // A notification for downloading would be more flexible, but for now I'm
  // forwarding to the two places that could possibly have initiated the
  // request. If we end up with another place invoking DownloadImage, probably
  // best to refactor out into notification service, or something similar.
  if (errored)
    fav_icon_helper_.FavIconDownloadFailed(id);
  else
    fav_icon_helper_.SetFavIcon(id, image_url, image);
}

void TabContents::RequestOpenURL(const GURL& url, const GURL& referrer,
                                 WindowOpenDisposition disposition) {
  if (render_manager_.dom_ui()) {
    // When we're a DOM UI, it will provide a page transition type for us (this
    // is so the new tab page can specify AUTO_BOOKMARK for automatically
    // generated suggestions).
    //
    // Note also that we hide the referrer for DOM UI pages. We don't really
    // want web sites to see a referrer of "chrome://blah" (and some
    // chrome: URLs might have search terms or other stuff we don't want to
    // send to the site), so we send no referrer.
    OpenURL(url, GURL(), disposition,
            render_manager_.dom_ui()->link_transition_type());
  } else {
    OpenURL(url, referrer, disposition, PageTransition::LINK);
  }
}

void TabContents::DomOperationResponse(const std::string& json_string,
                                       int automation_id) {
  DomOperationNotificationDetails details(json_string, automation_id);
  NotificationService::current()->Notify(
      NotificationType::DOM_OPERATION_RESPONSE, Source<TabContents>(this),
      Details<DomOperationNotificationDetails>(&details));
}

void TabContents::ProcessDOMUIMessage(const std::string& message,
                                      const std::string& content) {
  if (!render_manager_.dom_ui()) {
    // We shouldn't get a DOM UI message when we haven't enabled the DOM UI.
    // Because the renderer might be owned and sending random messages, we need
    // to ignore these inproper ones.
    NOTREACHED();
    return;
  }
  render_manager_.dom_ui()->ProcessDOMUIMessage(message, content);
}

void TabContents::DocumentLoadedInFrame() {
  controller_.DocumentLoadedInFrame();
}

void TabContents::ProcessExternalHostMessage(const std::string& message,
                                             const std::string& origin,
                                             const std::string& target) {
  if (delegate())
    delegate()->ForwardMessageToExternalHost(message, origin, target);
}

void TabContents::GoToEntryAtOffset(int offset) {
  controller_.GoToOffset(offset);
}

void TabContents::GetHistoryListCount(int* back_list_count,
                                      int* forward_list_count) {
  int current_index = controller_.last_committed_entry_index();
  *back_list_count = current_index;
  *forward_list_count = controller_.entry_count() - current_index - 1;
}

void TabContents::RunFileChooser(bool multiple_files,
                                 const string16& title,
                                 const FilePath& default_file) {
  if (!select_file_dialog_.get())
    select_file_dialog_ = SelectFileDialog::Create(this);
  SelectFileDialog::Type dialog_type =
    multiple_files ? SelectFileDialog::SELECT_OPEN_MULTI_FILE :
                     SelectFileDialog::SELECT_OPEN_FILE;
  select_file_dialog_->SelectFile(dialog_type, title, default_file,
                                  NULL, 0, FILE_PATH_LITERAL(""),
                                  view_->GetTopLevelNativeWindow(), NULL);
}

void TabContents::RunJavaScriptMessage(
    const std::wstring& message,
    const std::wstring& default_prompt,
    const GURL& frame_url,
    const int flags,
    IPC::Message* reply_msg,
    bool* did_suppress_message) {
  // Suppress javascript messages when requested and when inside a constrained
  // popup window (because that activates them and breaks them out of the
  // constrained window jail).
  bool suppress_this_message = suppress_javascript_messages_;
  if (delegate())
    suppress_this_message |=
        (delegate()->GetConstrainingContents(this) != this);

  *did_suppress_message = suppress_this_message;

  if (!suppress_this_message) {
    base::TimeDelta time_since_last_message(
        base::TimeTicks::Now() - last_javascript_message_dismissal_);
    bool show_suppress_checkbox = false;
    // Show a checkbox offering to suppress further messages if this message is
    // being displayed within kJavascriptMessageExpectedDelay of the last one.
    if (time_since_last_message <
        base::TimeDelta::FromMilliseconds(kJavascriptMessageExpectedDelay))
      show_suppress_checkbox = true;

    RunJavascriptMessageBox(this, frame_url, flags, message, default_prompt,
                            show_suppress_checkbox, reply_msg);
  } else {
    // If we are suppressing messages, just reply as is if the user immediately
    // pressed "Cancel".
    OnJavaScriptMessageBoxClosed(reply_msg, false, std::wstring());
  }
}

void TabContents::RunBeforeUnloadConfirm(const std::wstring& message,
                                         IPC::Message* reply_msg) {
  RunBeforeUnloadDialog(this, message, reply_msg);
}

void TabContents::ShowModalHTMLDialog(const GURL& url, int width, int height,
                                      const std::string& json_arguments,
                                      IPC::Message* reply_msg) {
  if (delegate()) {
    HtmlDialogUIDelegate* dialog_delegate =
        new ModalHtmlDialogDelegate(url, width, height, json_arguments,
                                    reply_msg, this);
    delegate()->ShowHtmlDialog(dialog_delegate, NULL);
  }
}

void TabContents::PasswordFormsSeen(
    const std::vector<webkit_glue::PasswordForm>& forms) {
  GetPasswordManager()->PasswordFormsSeen(forms);
}

void TabContents::AutofillFormSubmitted(
    const webkit_glue::AutofillForm& form) {
  GetAutofillManager()->AutofillFormSubmitted(form);
}

void TabContents::GetAutofillSuggestions(const std::wstring& field_name,
    const std::wstring& user_text, int64 node_id, int request_id) {
  GetAutofillManager()->FetchValuesForName(field_name, user_text,
      kMaxAutofillMenuItems, node_id, request_id);
}

void TabContents::RemoveAutofillEntry(const std::wstring& field_name,
                                      const std::wstring& value) {
  GetAutofillManager()->RemoveValueForName(field_name, value);
}

// Checks to see if we should generate a keyword based on the OSDD, and if
// necessary uses TemplateURLFetcher to download the OSDD and create a keyword.
void TabContents::PageHasOSDD(RenderViewHost* render_view_host,
                              int32 page_id, const GURL& url,
                              bool autodetected) {
  // Make sure page_id is the current page, and the TemplateURLModel is loaded.
  DCHECK(url.is_valid());
  if (!IsActiveEntry(page_id))
    return;
  TemplateURLModel* url_model = profile()->GetTemplateURLModel();
  if (!url_model)
    return;
  if (!url_model->loaded()) {
    url_model->Load();
    return;
  }
  if (!profile()->GetTemplateURLFetcher())
    return;

  if (profile()->IsOffTheRecord())
    return;

  const NavigationEntry* entry = controller_.GetLastCommittedEntry();
  DCHECK(entry);

  const NavigationEntry* base_entry = entry;
  if (IsFormSubmit(base_entry)) {
    // If the current page is a form submit, find the last page that was not
    // a form submit and use its url to generate the keyword from.
    int index = controller_.last_committed_entry_index() - 1;
    while (index >= 0 && IsFormSubmit(controller_.GetEntryAtIndex(index)))
      index--;
    if (index >= 0)
      base_entry = controller_.GetEntryAtIndex(index);
    else
      base_entry = NULL;
  }

  // We want to use the user typed URL if available since that represents what
  // the user typed to get here, and fall back on the regular URL if not.
  if (!base_entry)
    return;
  GURL keyword_url = base_entry->user_typed_url().is_valid() ?
          base_entry->user_typed_url() : base_entry->url();
  if (!keyword_url.is_valid())
    return;
  std::wstring keyword = TemplateURLModel::GenerateKeyword(keyword_url,
                                                           autodetected);
  if (keyword.empty())
    return;
  const TemplateURL* template_url =
      url_model->GetTemplateURLForKeyword(keyword);
  if (template_url && (!template_url->safe_for_autoreplace() ||
                       template_url->originating_url() == url)) {
    // Either there is a user created TemplateURL for this keyword, or the
    // keyword has the same OSDD url and we've parsed it.
    return;
  }

  // Download the OpenSearch description document. If this is successful a
  // new keyword will be created when done.
  profile()->GetTemplateURLFetcher()->ScheduleDownload(
      keyword,
      url,
      base_entry->favicon().url(),
      this,
      autodetected);
}

void TabContents::DidGetPrintedPagesCount(int cookie, int number_pages) {
  printing_.DidGetPrintedPagesCount(cookie, number_pages);
}

void TabContents::DidPrintPage(const ViewHostMsg_DidPrintPage_Params& params) {
  printing_.DidPrintPage(params);
}

GURL TabContents::GetAlternateErrorPageURL() const {
  GURL url;
  // Disable alternate error pages when in OffTheRecord/Incognito mode.
  if (profile()->IsOffTheRecord())
    return url;

  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  if (prefs->GetBoolean(prefs::kAlternateErrorPagesEnabled)) {
    url = google_util::AppendGoogleLocaleParam(GURL(kLinkDoctorBaseURL));
    url = google_util::AppendGoogleTLDParam(url);
  }
  return url;
}

WebPreferences TabContents::GetWebkitPrefs() {
  PrefService* prefs = render_view_host()->process()->profile()->GetPrefs();
  bool isDomUI = false;
  return RenderViewHostDelegateHelper::GetWebkitPrefs(prefs, isDomUI);
}

void TabContents::OnMissingPluginStatus(int status) {
#if defined(OS_WIN)
// TODO(PORT): pull in when plug-ins work
  GetPluginInstaller()->OnMissingPluginStatus(status);
#endif
}

void TabContents::OnCrashedPlugin(const FilePath& plugin_path) {
#if defined(OS_WIN)
// TODO(PORT): pull in when plug-ins work
  DCHECK(!plugin_path.value().empty());

  std::wstring plugin_name = plugin_path.ToWStringHack();
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(plugin_path));
  if (version_info.get()) {
    const std::wstring& product_name = version_info->product_name();
    if (!product_name.empty())
      plugin_name = product_name;
  }
  AddInfoBar(new SimpleAlertInfoBarDelegate(
      this, l10n_util::GetStringF(IDS_PLUGIN_CRASHED_PROMPT, plugin_name),
      NULL));
#endif
}

void TabContents::OnCrashedWorker() {
  AddInfoBar(new SimpleAlertInfoBarDelegate(
      this, l10n_util::GetString(IDS_WEBWORKER_CRASHED_PROMPT),
      NULL));
}

void TabContents::OnJSOutOfMemory() {
  AddInfoBar(new SimpleAlertInfoBarDelegate(
      this, l10n_util::GetString(IDS_JS_OUT_OF_MEMORY_PROMPT), NULL));
}

void TabContents::ShouldClosePage(bool proceed) {
  render_manager_.ShouldClosePage(proceed);
}

void TabContents::OnCrossSiteResponse(int new_render_process_host_id,
                                      int new_request_id) {
  // Allows the TabContents to react when a cross-site response is ready to be
  // delivered to a pending RenderViewHost.  We must first run the onunload
  // handler of the old RenderViewHost before we can allow it to proceed.
  render_manager_.OnCrossSiteResponse(new_render_process_host_id,
                                      new_request_id);
}

bool TabContents::CanBlur() const {
  return delegate() ? delegate()->CanBlur() : true;
}

gfx::Rect TabContents::GetRootWindowResizerRect() const {
  if (delegate())
    return delegate()->GetRootWindowResizerRect();
  return gfx::Rect();
}

void TabContents::RendererUnresponsive(RenderViewHost* rvh,
                                       bool is_during_unload) {
  if (is_during_unload) {
    // Hang occurred while firing the beforeunload/unload handler.
    // Pretend the handler fired so tab closing continues as if it had.
    rvh->set_sudden_termination_allowed(true);

    if (!render_manager_.ShouldCloseTabOnUnresponsiveRenderer())
      return;

    // If the tab hangs in the beforeunload/unload handler there's really
    // nothing we can do to recover. Pretend the unload listeners have
    // all fired and close the tab. If the hang is in the beforeunload handler
    // then the user will not have the option of cancelling the close.
    Close(rvh);
    return;
  }

  if (render_view_host() && render_view_host()->IsRenderViewLive())
    HungRendererDialog::ShowForTabContents(this);
}

void TabContents::RendererResponsive(RenderViewHost* render_view_host) {
  HungRendererDialog::HideForTabContents(this);
}

void TabContents::LoadStateChanged(const GURL& url,
                                   net::LoadState load_state) {
  load_state_ = load_state;
  std::wstring languages =
      profile()->GetPrefs()->GetString(prefs::kAcceptLanguages);
  load_state_host_.clear();
  std::string host = url.host();
  net::IDNToUnicode(host.c_str(), host.size(), languages, &load_state_host_);
  if (load_state_ == net::LOAD_STATE_READING_RESPONSE)
    SetNotWaitingForResponse();
  if (is_loading())
    NotifyNavigationStateChanged(INVALIDATE_LOAD | INVALIDATE_TAB);
}

void TabContents::OnDidGetApplicationInfo(
    int32 page_id,
    const webkit_glue::WebApplicationInfo& info) {
  if (pending_install_.page_id != page_id)
    return;  // The user clicked create on a separate page. Ignore this.

  pending_install_.callback_functor =
      new GearsCreateShortcutCallbackFunctor(this);
  GearsCreateShortcut(
      info, pending_install_.title, pending_install_.url, pending_install_.icon,
      NewCallback(pending_install_.callback_functor,
                  &GearsCreateShortcutCallbackFunctor::Run));
}

void TabContents::OnUserGesture() {
  // See comment in RenderViewHostDelegate::OnUserGesture as to why we do this.
  DownloadRequestManager* drm = g_browser_process->download_request_manager();
  if (drm)
    drm->OnUserGesture(this);
  controller_.OnUserGesture();
}

void TabContents::OnFindReply(int request_id,
                              int number_of_matches,
                              const gfx::Rect& selection_rect,
                              int active_match_ordinal,
                              bool final_update) {
  // Ignore responses for requests other than the one we have most recently
  // issued. That way we won't act on stale results when the user has
  // already typed in another query.
  if (request_id != current_find_request_id_)
    return;

  if (number_of_matches == -1)
    number_of_matches = last_search_result_.number_of_matches();
  if (active_match_ordinal == -1)
    active_match_ordinal = last_search_result_.active_match_ordinal();

  gfx::Rect selection = selection_rect;
  if (selection.IsEmpty())
    selection = last_search_result_.selection_rect();

  // Notify the UI, automation and any other observers that a find result was
  // found.
  last_search_result_ = FindNotificationDetails(request_id, number_of_matches,
                                                selection, active_match_ordinal,
                                                final_update);
  NotificationService::current()->Notify(
      NotificationType::FIND_RESULT_AVAILABLE,
      Source<TabContents>(this),
      Details<FindNotificationDetails>(&last_search_result_));
}

bool TabContents::IsExternalTabContainer() const {
  if (!delegate())
    return false;

  return delegate()->IsExternalTabContainer();
}

void TabContents::FileSelected(const FilePath& path,
                               int index, void* params) {
  render_view_host()->FileSelected(path);
}

void TabContents::MultiFilesSelected(const std::vector<FilePath>& files,
                                     void* params) {
  render_view_host()->MultiFilesSelected(files);
}

void TabContents::FileSelectionCanceled(void* params) {
  // If the user cancels choosing a file to upload we pass back an
  // empty vector.
  render_view_host()->MultiFilesSelected(std::vector<FilePath>());
}

void TabContents::BeforeUnloadFiredFromRenderManager(
    bool proceed,
    bool* proceed_to_fire_unload) {
  if (delegate())
    delegate()->BeforeUnloadFired(this, proceed, proceed_to_fire_unload);
}

void TabContents::UpdateRenderViewSizeForRenderManager() {
  // TODO(brettw) this is a hack. See TabContentsView::SizeContents.
  view_->SizeContents(view_->GetContainerSize());
}

DOMUI* TabContents::CreateDOMUIForRenderManager(const GURL& url) {
  return DOMUIFactory::CreateDOMUIForURL(this, url);
}

NavigationEntry*
TabContents::GetLastCommittedNavigationEntryForRenderManager() {
  return controller_.GetLastCommittedEntry();
}

bool TabContents::CreateRenderViewForRenderManager(
    RenderViewHost* render_view_host) {
  // When we're running a DOM UI, the RenderViewHost needs to be put in DOM UI
  // mode before CreateRenderView is called. When we're asked to create a
  // RenderView, that means it's for the pending entry, so we have to use the
  // pending DOM UI.
  if (render_manager_.pending_dom_ui())
    render_view_host->AllowDOMUIBindings();

  // Ditto for extension bindings.
  if (controller().pending_entry()->url().SchemeIs(chrome::kExtensionScheme))
    render_view_host->AllowExtensionBindings();

  RenderWidgetHostView* rwh_view = view_->CreateViewForWidget(render_view_host);
  if (!render_view_host->CreateRenderView())
    return false;

  // Now that the RenderView has been created, we need to tell it its size.
  rwh_view->SetSize(view_->GetContainerSize());

  UpdateMaxPageIDIfNecessary(render_view_host->site_instance(),
                             render_view_host);
  return true;
}

void TabContents::Observe(NotificationType type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::BOOKMARK_MODEL_LOADED:
      // BookmarkModel finished loading, fall through to update starred state.
    case NotificationType::URLS_STARRED: {
      // Somewhere, a URL has been starred.
      // Ignore notifications for profiles other than our current one.
      Profile* source_profile = Source<Profile>(source).ptr();
      if (!source_profile || !source_profile->IsSameProfile(profile()))
        return;

      UpdateStarredStateForCurrentURL();
      break;
    }
    case NotificationType::PREF_CHANGED: {
      std::wstring* pref_name_in = Details<std::wstring>(details).ptr();
      DCHECK(Source<PrefService>(source).ptr() == profile()->GetPrefs());
      if (*pref_name_in == prefs::kAlternateErrorPagesEnabled) {
        UpdateAlternateErrorPageURL();
      } else if (*pref_name_in == prefs::kDefaultCharset ||
          StartsWithASCII(WideToUTF8(*pref_name_in), "webkit.webprefs.", true)
          ) {
        UpdateWebPreferences();
      } else {
        NOTREACHED() << "unexpected pref change notification" << *pref_name_in;
      }
      break;
    }
    case NotificationType::RENDER_WIDGET_HOST_DESTROYED:
      view_->RenderWidgetHostDestroyed(Source<RenderWidgetHost>(source).ptr());
      break;

    case NotificationType::NAV_ENTRY_COMMITTED: {
      DCHECK(&controller_ == Source<NavigationController>(source).ptr());

      NavigationController::LoadCommittedDetails& committed_details =
          *(Details<NavigationController::LoadCommittedDetails>(details).ptr());
      ExpireInfoBars(committed_details);
      break;
    }

    default:
      NOTREACHED();
  }
}
