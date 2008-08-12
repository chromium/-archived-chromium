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

#include "chrome/browser/web_contents.h"

#include "base/command_line.h"
#include "base/file_version_info.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/bookmark_bar_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/cache_manager_host.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/download_manager.h"
#include "chrome/browser/find_in_page_controller.h"
#include "chrome/browser/find_notification_details.h"
#include "chrome/browser/google_util.h"
#include "chrome/browser/interstitial_page_delegate.h"
#include "chrome/browser/js_before_unload_handler.h"
#include "chrome/browser/jsmessage_box_handler.h"
#include "chrome/browser/load_from_memory_cache_details.h"
#include "chrome/browser/load_notification_details.h"
#include "chrome/browser/modal_html_dialog_delegate.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/navigation_profiler.h"
#include "chrome/browser/page_load_tracker.h"
#include "chrome/browser/password_manager.h"
#include "chrome/browser/plugin_installer.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/render_view_context_menu.h"
#include "chrome/browser/render_view_context_menu_controller.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/render_widget_host_hwnd.h"
#include "chrome/browser/template_url_fetcher.h"
#include "chrome/browser/template_url_model.h"
#include "chrome/browser/views/hung_renderer_view.h"
#include "chrome/browser/views/sad_tab_view.h"
#include "chrome/browser/web_drag_source.h"
#include "chrome/browser/web_drop_target.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/resource_bundle.h"
#include "net/base/mime_util.h"
#include "net/base/registry_controlled_domain.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"

#include "generated_resources.h"

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

// Minimum amount of time in ms that has to elapse since the download shelf was
// shown for us to hide it when navigating away from the current page.
const int kDownloadShelfHideDelay = 5000;

const wchar_t kLinkDoctorBaseURL[] =
    L"http://linkhelp.clients.google.com/tbproxy/lh/fixurl";

// The printer icon in shell32.dll. That's a standard icon user will quickly
// recognize.
const int kShell32PrinterIcon = 17;

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

void InitWebContentsClass() {
  static bool web_contents_class_initialized = false;
  if (!web_contents_class_initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    web_contents_class_initialized = true;
  }
}

GURL GURLWithoutRef(const GURL& url) {
  url_canon::Replacements<char> replacements;
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// WebContents

class WebContents::GearsCreateShortcutCallbackFunctor {
 public:
  explicit GearsCreateShortcutCallbackFunctor(WebContents* contents)
     : contents_(contents) {}

  void Run(const GearsShortcutData& shortcut_data, bool success) {
    if (contents_)
      contents_->OnGearsCreateShortcutDone(shortcut_data, success);
    delete this;
  }
  void Cancel() {
    contents_ = NULL;
  }

 private:
  WebContents* contents_;
};

// static
void WebContents::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kAlternateErrorPagesEnabled, true);

  WebPreferences pref_defaults;
  prefs->RegisterBooleanPref(prefs::kWebKitJavascriptEnabled,
                             pref_defaults.javascript_enabled);
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
  prefs->RegisterBooleanPref(prefs::kWebKitDeveloperExtrasEnabled,
                             true);
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

WebContents::WebContents(Profile* profile,
                         SiteInstance* site_instance,
                         RenderViewHostFactory* render_view_factory,
                         int routing_id,
                         HANDLE modal_dialog_event)
    : TabContents(TAB_CONTENTS_WEB),
#pragma warning(suppress: 4355)  // Okay to pass "this" here.
      render_manager_(render_view_factory, this, this),
      render_view_factory_(render_view_factory),
      is_profiling_(false),
      has_page_title_(false),
      info_bar_visible_(false),
      is_starred_(false),
      printing_(*this),
      notify_disconnection_(false),
      message_box_active_(CreateEvent(NULL, TRUE, FALSE, NULL)),
      capturing_contents_(false),
#pragma warning(suppress: 4355)  // Okay to pass "this" here.
      fav_icon_helper_(this),
      crashed_plugin_info_bar_(NULL),
      suppress_javascript_messages_(false),
      load_state_(net::LOAD_STATE_IDLE) {
  InitWebContentsClass();

  pending_install_.page_id = 0;
  pending_install_.callback_functor = NULL;

  render_manager_.Init(profile, site_instance, routing_id, modal_dialog_event);

  // Register for notifications about all interested prefs change.
  PrefService* prefs = profile->GetPrefs();
  if (prefs)
    for (int i = 0; i < kPrefsToObserveLength; ++i)
      prefs->AddPrefObserver(kPrefsToObserve[i], this);

  // Register for notifications about URL starredness changing on any profile.
  NotificationService::current()->
      AddObserver(this, NOTIFY_URLS_STARRED, NotificationService::AllSources());
  NotificationService::current()->
      AddObserver(this, NOTIFY_BOOKMARK_MODEL_LOADED,
                  NotificationService::AllSources());
}

WebContents::~WebContents() {
  if (web_app_.get())
    web_app_->RemoveObserver(this);
  if (pending_install_.callback_functor)
    pending_install_.callback_functor->Cancel();
}

void WebContents::CreateView(HWND parent_hwnd,
                             const gfx::Rect& initial_bounds) {
  set_delete_on_destroy(false);
  HWNDViewContainer::Init(parent_hwnd, initial_bounds, false);

  // Remove the root view drop target so we can register our own.
  RevokeDragDrop(GetHWND());
  drop_target_ = new WebDropTarget(GetHWND(), this);
}

void WebContents::GetContainerBounds(gfx::Rect *out) const {
  CRect r;
  GetBounds(&r, false);
  *out = r;
}

void WebContents::ShowContents() {
  if (view())
    view()->DidBecomeSelected();

  // Loop through children and send DidBecomeSelected to them, too.
  int count = static_cast<int>(child_windows_.size());
  for (int i = count - 1; i >= 0; --i) {
    ConstrainedWindow* window = child_windows_.at(i);
    window->DidBecomeSelected();
  }

  // If we have a FindInPage dialog, notify it that its tab was selected.
  if (find_in_page_controller_.get())
    find_in_page_controller_->DidBecomeSelected();
}

void WebContents::HideContents() {
  // TODO(pkasting): http://b/1239839  Right now we purposefully don't call
  // our superclass HideContents(), because some callers want to be very picky
  // about the order in which these get called.  In addition to making the code
  // here practically impossible to understand, this also means we end up
  // calling TabContents::WasHidden() twice if callers call both versions of
  // HideContents() on a WebContents.

  WasHidden();
}

void WebContents::SizeContents(const gfx::Size& size) {
  if (view())
    view()->SetSize(size);
  if (find_in_page_controller_.get())
    find_in_page_controller_->RespondToResize(size);
  RepositionSupressedPopupsToFit(size);
}

void WebContents::FirePageBeforeUnload() {
  render_view_host()->FirePageBeforeUnload();
}

void WebContents::FirePageUnload() {
  render_view_host()->FirePageUnload();
}

void WebContents::Destroy() {
  // Tell the notification service we no longer want notifications.
  NotificationService::current()->
      RemoveObserver(this, NOTIFY_URLS_STARRED,
                     NotificationService::AllSources());
  NotificationService::current()->
      RemoveObserver(this, NOTIFY_BOOKMARK_MODEL_LOADED,
                     NotificationService::AllSources());

  // Destroy the print manager right now since a Print command may be pending.
  printing_.Destroy();

  // Unregister the notifications of all observed prefs change.
  PrefService* prefs = profile()->GetPrefs();
  if (prefs) {
    for (int i = 0; i < kPrefsToObserveLength; ++i)
      prefs->RemovePrefObserver(kPrefsToObserve[i], this);
  }

  cancelable_consumer_.CancelAllRequests();

  // Close the Find in page dialog.
  if (find_in_page_controller_.get())
    find_in_page_controller_->Close();

  // Detach plugin windows so that they are not destroyed automatically.
  // They will be cleaned up properly in plugin process.
  DetachPluginWindows();

  NotifyDisconnected();
  HungRendererWarning::HideForWebContents(this);
  render_manager_.Shutdown();
  TabContents::Destroy();
}

///////////////////////////////////////////////////////////////////////////////
// Event Handlers

void WebContents::OnDestroy() {
  if (drop_target_.get()) {
    RevokeDragDrop(GetHWND());
    drop_target_ = NULL;
  }
}

void WebContents::OnWindowPosChanged(WINDOWPOS* window_pos) {
  if (window_pos->flags & SWP_HIDEWINDOW) {
    HideContents();
  } else {
    // The WebContents was shown by a means other than the user selecting a
    // Tab, e.g. the window was minimized then restored.
    if (window_pos->flags & SWP_SHOWWINDOW)
      ShowContents();
    // Unless we were specifically told not to size, cause the renderer to be
    // sized to the new bounds, which forces a repaint. Not required for the
    // simple minimize-restore case described above, for example, since the
    // size hasn't changed.
    if (!(window_pos->flags & SWP_NOSIZE)) {
      gfx::Size size(window_pos->cx, window_pos->cy);
      SizeContents(size);
    }

    // If we have a FindInPage dialog, notify it that the window changed.
    if (find_in_page_controller_.get() && find_in_page_controller_->IsVisible())
      find_in_page_controller_->MoveWindowIfNecessary(gfx::Rect());
  }
}

void WebContents::OnPaint(HDC junk_dc) {
  if (render_view_host() && !render_view_host()->IsRenderViewLive()) {
    if (!sad_tab_.get())
      sad_tab_.reset(new SadTabView);
    CRect cr;
    GetClientRect(&cr);
    sad_tab_->SetBounds(cr);
    ChromeCanvasPaint canvas(GetHWND(), true);
    sad_tab_->ProcessPaint(&canvas);
    return;
  }

  // We need to do this to validate the dirty area so we don't end up in a
  // WM_PAINTstorm that causes other mysterious bugs (such as WM_TIMERs not
  // firing etc). It doesn't matter that we don't have any non-clipped area.
  CPaintDC dc(GetHWND());
  SetMsgHandled(FALSE);
}

void WebContents::OnHScroll(int scroll_type, short position, HWND scrollbar) {
  // This window can receive scroll events as a result of the ThinkPad's
  // trackpad scroll wheel emulation.
  if (!ScrollZoom(scroll_type))
    SetMsgHandled(FALSE);
}

LRESULT WebContents::OnMouseRange(UINT msg, WPARAM w_param, LPARAM l_param) {
  switch (msg) {
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
      // Make sure this TabContents is activated when it is clicked on.
      if (delegate())
        delegate()->ActivateContents(this);
      break;
    case WM_MOUSEMOVE:
      // Let our delegate know that the mouse moved (useful for resetting status
      // bubble state).
      if (delegate())
        delegate()->ContentsMouseEvent(this, WM_MOUSEMOVE);
      break;
    case WM_MOUSEWHEEL:
      // This message is reflected from the view() to this window.
      if (GET_KEYSTATE_WPARAM(w_param) & MK_CONTROL) {
        WheelZoom(GET_WHEEL_DELTA_WPARAM(w_param));
        return 1;
      }
      break;
  }

  return 0;
}

void WebContents::OnMouseLeave() {
  // Let our delegate know that the mouse moved (useful for resetting status
  // bubble state).
  if (delegate())
    delegate()->ContentsMouseEvent(this, WM_MOUSELEAVE);
  SetMsgHandled(FALSE);
}

LRESULT WebContents::OnReflectedMessage(UINT msg, WPARAM w_param,
                                        LPARAM l_param) {
  MSG* message = reinterpret_cast<MSG*>(l_param);
  LRESULT ret = 0;
  if (message) {
    ProcessWindowMessage(message->hwnd, message->message, message->wParam,
                         message->lParam, ret);
  }

  return ret;
}

void WebContents::OnVScroll(int scroll_type, short position, HWND scrollbar) {
  // This window can receive scroll events as a result of the ThinkPad's
  // TrackPad scroll wheel emulation.
  if (!ScrollZoom(scroll_type))
    SetMsgHandled(FALSE);
}

bool WebContents::ScrollZoom(int scroll_type) {
  // If ctrl is held, zoom the UI.  There are three issues with this:
  // 1) Should the event be eaten or forwarded to content?  We eat the event,
  //    which is like Firefox and unlike IE.
  // 2) Should wheel up zoom in or out?  We zoom in (increase font size), which
  //    is like IE and Google maps, but unlike Firefox.
  // 3) Should the mouse have to be over the content area?  We zoom as long as
  //    content has focus, although FF and IE require that the mouse is over
  //    content.  This is because all events get forwarded when content has
  //    focus.
  if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
    int distance = 0;
    switch (scroll_type) {
      case SB_LINEUP:
        distance = WHEEL_DELTA;
        break;
      case SB_LINEDOWN:
        distance = -WHEEL_DELTA;
        break;
        // TODO(joshia): Handle SB_PAGEUP, SB_PAGEDOWN, SB_THUMBPOSITION,
        // and SB_THUMBTRACK for completeness
      default:
        break;
    }

    WheelZoom(distance);
    return true;
  }
  return false;
}

void WebContents::WheelZoom(int distance) {
  if (delegate()) {
    bool zoom_in = distance > 0;
    delegate()->ContentsZoomChange(zoom_in);
  }
}

void WebContents::OnSetFocus(HWND window) {
  // TODO(jcampan): figure out why removing this prevents tabs opened in the
  //                background from properly taking focus.
  // We NULL-check the render_view_host_ here because Windows can send us
  // messages during the destruction process after it has been destroyed.
  if (view()) {
    HWND inner_hwnd = view()->GetPluginHWND();
    if (::IsWindow(inner_hwnd))
      ::SetFocus(inner_hwnd);
  }
}

NavigationProfiler* WebContents::GetNavigationProfiler() {
  return &g_navigation_profiler;
}

bool WebContents::EnableProfiling() {
  NavigationProfiler* profiler = GetNavigationProfiler();
  is_profiling_ = profiler->is_profiling();

  return is_profiling();
}

void WebContents::SaveCurrentProfilingEntry() {
  if (is_profiling()) {
    NavigationProfiler* profiler = GetNavigationProfiler();
    profiler->MoveActivePageToVisited(process()->host_id(),
                                      render_view_host()->routing_id());
  }
  is_profiling_ = false;
}

void WebContents::CreateNewProfilingEntry(const GURL& url) {
  SaveCurrentProfilingEntry();

  // Check new profiling status.
  if (EnableProfiling()) {
    NavigationProfiler* profiler = GetNavigationProfiler();
    TimeTicks current_time = TimeTicks::Now();

    PageLoadTracker *page = new PageLoadTracker(
        url, process()->host_id(), render_view_host()->routing_id(),
        current_time);

    profiler->AddActivePage(page);
  }
}

void WebContents::OnSavePage() {
  // If we can not save the page, try to download it.
  if (!SavePackage::IsSavableContents(contents_mime_type())) {
    DownloadManager* dlm = profile()->GetDownloadManager();
    const GURL& current_page_url = GetURL();
    if (dlm && current_page_url.is_valid())
      dlm->DownloadUrl(current_page_url, GURL(), this);
    return;
  }

  // Get our user preference state.
  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);

  std::wstring suggest_name =
      SavePackage::GetSuggestNameForSaveAs(prefs, GetTitle());

  SavePackage::SavePackageParam param(contents_mime_type());
  param.prefs = prefs;

  // TODO(rocking): Use new asynchronous dialog boxes to prevent the SaveAs
  // dialog blocking the UI thread. See bug: http://b/issue?id=1129694.
  if (SavePackage::GetSaveInfo(suggest_name, GetContainerHWND(), &param))
    SavePage(param.saved_main_file_path, param.dir, param.save_type);
}

void WebContents::SavePage(const std::wstring& main_file,
                           const std::wstring& dir_path,
                           SavePackage::SavePackageType save_type) {
  // Stop the page from navigating.
  Stop();

  save_package_ = new SavePackage(this, save_type, main_file, dir_path);
  save_package_->Init();
}

///////////////////////////////////////////////////////////////////////////////

// Cross-Site Navigations
//
// If a WebContents is told to navigate to a different web site (as determined
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
//   the next DidNavigate event for this WebContents.  This isn't ideal, but it
//   doesn't affect any functionality.
// - After RDH receives a response and determines that it is safe and not a
//   download, it pauses the response to first run the old page's onunload
//   handler.  It does this by asynchronously calling the OnCrossSiteResponse
//   method of WebContents on the UI thread, which sends a ClosePage message
//   to the current RVH.
// - Once the onunload handler is finished, a ClosePage_ACK message is sent to
//   the ResourceDispatcherHost, who unpauses the response.  Data is then sent
//   to the pending RVH.
// - The pending renderer sends a FrameNavigate message that invokes the
//   WebContents::DidNavigate method.  This replaces the current RVH with the
//   pending RVH and goes back to the NORMAL RendererState.

bool WebContents::Navigate(const NavigationEntry& entry, bool reload) {
  RenderViewHost* dest_render_view_host = render_manager_.Navigate(entry);

  CreateNewProfilingEntry(entry.GetURL());

  // Used for page load time metrics.
  current_load_start_ = TimeTicks::Now();

  // Navigate in the desired RenderViewHost
  dest_render_view_host->NavigateToEntry(entry, reload);

  if (entry.GetPageID() == -1) {
    // HACK!!  This code suppresses javascript: URLs from being added to
    // session history, which is what we want to do for javascript: URLs that
    // do not generate content.  What we really need is a message from the
    // renderer telling us that a new page was not created.  The same message
    // could be used for mailto: URLs and the like.
    if (entry.GetURL().SchemeIs("javascript"))
      return false;
  }

  if (reload && !profile()->IsOffTheRecord()) {
    HistoryService* history =
        profile()->GetHistoryService(Profile::IMPLICIT_ACCESS);
    if (history)
      history->SetFavIconOutOfDateForPage(entry.GetURL());
  }

  return true;
}

void WebContents::Stop() {
  render_manager_.Stop();
  printing_.Stop();
}

void WebContents::DidBecomeSelected() {
  TabContents::DidBecomeSelected();

  if (render_view_host() && view())
    view()->DidBecomeSelected();

  CacheManagerHost::GetInstance()->ObserveActivity(process()->host_id());
}

void WebContents::WasHidden() {
  if (!capturing_contents_) {
    // |render_view_host()| can be NULL if the user middle clicks a link to open
    // a tab in then background, then closes the tab before selecting it.  This
    // is because closing the tab calls WebContents::Destroy(), which removes
    // the |render_view_host()|; then when we actually destroy the window,
    // OnWindowPosChanged() notices and calls HideContents() (which calls us).
    if (render_view_host() && view())
      view()->WasHidden();

    // Loop through children and send WasHidden to them, too.
    int count = static_cast<int>(child_windows_.size());
    for (int i = count - 1; i >= 0; --i) {
      ConstrainedWindow* window = child_windows_.at(i);
      window->WasHidden();
    }
  }

  // If we have a FindInPage dialog, notify it that its tab was hidden.
  if (find_in_page_controller_.get())
    find_in_page_controller_->DidBecomeUnselected();

  TabContents::WasHidden();
}

void WebContents::StartFinding(int request_id,
                               const std::wstring& search_string,
                               bool forward,
                               bool match_case,
                               bool find_next) {
  if (search_string.empty())
    return;
  render_view_host()->StartFinding(request_id, search_string, forward,
                                   match_case, find_next);
}

void WebContents::StopFinding(bool clear_selection) {
  render_view_host()->StopFinding(clear_selection);
}

void WebContents::OpenFindInPageWindow(const Browser& browser) {
  if (!CanFind())
    return;

  if (!find_in_page_controller_.get()) {
    // Get the Chrome top-level (Frame) window.
    HWND hwnd = browser.GetTopLevelHWND();
    find_in_page_controller_.reset(new FindInPageController(this, hwnd));
  } else {
    find_in_page_controller_->Show();
  }
}

void WebContents::ReparentFindWindow(HWND new_parent) {
  DCHECK(new_parent);
  if (find_in_page_controller_.get()) {
    find_in_page_controller_->SetParent(new_parent);
  }
}

bool WebContents::AdvanceFindSelection(bool forward_direction) {
  if (!CanFind())
    return false;

  // If no controller has been created or it doesn't know what to search for
  // then just return false so that caller knows that it should create and
  // show the window.
  if (!find_in_page_controller_.get() ||
      find_in_page_controller_->find_string().empty())
    return false;

  // The dialog already exists, so show if hidden.
  if (!find_in_page_controller_->IsVisible())
    find_in_page_controller_->Show();

  find_in_page_controller_->StartFinding(forward_direction);
  return true;
}

void WebContents::AlterTextSize(text_zoom::TextSize size) {
  render_view_host()->AlterTextSize(size);
  // TODO(creis): should this be propagated to other and future RVHs?
}

void WebContents::SetPageEncoding(const std::wstring& encoding_name) {
  render_view_host()->SetPageEncoding(encoding_name);
  // TODO(creis): should this be propagated to other and future RVHs?
}

void WebContents::CopyImageAt(int x, int y) {
  render_view_host()->CopyImageAt(x, y);
}

void WebContents::InspectElementAt(int x, int y) {
  render_view_host()->InspectElementAt(x, y);
}

void WebContents::ShowJavaScriptConsole() {
  render_view_host()->ShowJavaScriptConsole();
}

void WebContents::AllowDomAutomationBindings() {
  render_view_host()->AllowDomAutomationBindings();
  // TODO(creis): should this be propagated to other and future RVHs?
}

void WebContents::OnJavaScriptMessageBoxClosed(IPC::Message* reply_msg,
                                               bool success,
                                               const std::wstring& prompt) {
  last_javascript_message_dismissal_ = TimeTicks::Now();
  render_manager_.OnJavaScriptMessageBoxClosed(reply_msg, success, prompt);
}

// Generic NotificationObserver callback.
void WebContents::Observe(NotificationType type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  TabContents::Observe(type, source, details);
  switch (type) {
    case NOTIFY_BOOKMARK_MODEL_LOADED:  // BookmarkBarModel finished loading,
                                        // fall through to update starred state.
    case NOTIFY_URLS_STARRED: {  // Somewhere, a URL has been starred.
      // Ignore notifications for profiles other than our current one.
      Profile* source_profile = Source<Profile>(source).ptr();
      if (!source_profile->IsSameProfile(profile()))
        return;

      UpdateStarredStateForCurrentURL();
      break;
    }
    case NOTIFY_PREF_CHANGED: {
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
    default: {
      NOTREACHED();
      break;
    }
  }
}

void WebContents::NotifySwapped() {
  // After sending out a swap notification, we need to send a disconnect
  // notification so that clients that pick up a pointer to |this| can NULL the
  // pointer.  See Bug 1230284.
  notify_disconnection_ = true;
  NotificationService::current()->
      Notify(NOTIFY_WEB_CONTENTS_SWAPPED,
             Source<WebContents>(this),
             NotificationService::NoDetails());
}

void WebContents::NotifyConnected() {
  notify_disconnection_ = true;
  NotificationService::current()->
      Notify(NOTIFY_WEB_CONTENTS_CONNECTED,
             Source<WebContents>(this),
             NotificationService::NoDetails());
}

void WebContents::NotifyDisconnected() {
  if (!notify_disconnection_)
    return;

  notify_disconnection_ = false;
  NotificationService::current()->
      Notify(NOTIFY_WEB_CONTENTS_DISCONNECTED,
             Source<WebContents>(this),
             NotificationService::NoDetails());
}

void WebContents::SetSuppressJavascriptMessageBoxes(
    bool suppress_javascript_messages) {
  suppress_javascript_messages_ = suppress_javascript_messages;
}

void WebContents::UpdateHistoryForNavigation(const GURL& display_url,
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

void WebContents::MaybeCloseChildWindows(
    const ViewHostMsg_FrameNavigate_Params& params) {
  if (net::RegistryControlledDomainService::SameDomainOrHost(
          last_url_, params.url))
    return;
  last_url_ = params.url;

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

void WebContents::SetDownloadShelfVisible(bool visible) {
  TabContents::SetDownloadShelfVisible(visible);
  if (visible) {
    // Always set this value as it reflects the last time the download shelf
    // was made visible (even if it was already visible).
    last_download_shelf_show_ = TimeTicks::Now();
  }
}

void WebContents::SetInfoBarVisible(bool visible) {
  if (info_bar_visible_ != visible) {
    info_bar_visible_ = visible;
    if (info_bar_visible_) {
      // Invoke GetInfoBarView to force the info bar to be created.
      GetInfoBarView();
    }
    ToolbarSizeChanged(false);
  }
}

void WebContents::SetFindInPageVisible(bool visible) {
  if (find_in_page_controller_.get()) {
    if (visible)
      find_in_page_controller_->Show();
    else
      find_in_page_controller_->EndFindSession();
  }
}

InfoBarView* WebContents::GetInfoBarView() {
  if (info_bar_view_.get() == NULL) {
    info_bar_view_.reset(new InfoBarView(this));
    // The WebContents owns the info-bar.
    info_bar_view_->SetParentOwned(false);
  }
  return info_bar_view_.get();
}

void WebContents::ExecuteJavascriptInWebFrame(
    const std::wstring& frame_xpath, const std::wstring& jscript) {
  render_view_host()->ExecuteJavascriptInWebFrame(frame_xpath, jscript);
}

void WebContents::AddMessageToConsole(
    const std::wstring& frame_xpath, const std::wstring& msg,
    ConsoleMessageLevel level) {
  render_view_host()->AddMessageToConsole(frame_xpath, msg, level);
}

void WebContents::Undo() {
   render_view_host()->Undo();
}

void WebContents::Redo() {
   render_view_host()->Redo();
}

void WebContents::Replace(const std::wstring& text) {
   render_view_host()->Replace(text);
}

void WebContents::Delete() {
   render_view_host()->Delete();
}

void WebContents::SelectAll() {
   render_view_host()->SelectAll();
}

void WebContents::StartFileUpload(const std::wstring& file_path,
                                  const std::wstring& form,
                                  const std::wstring& file,
                                  const std::wstring& submit,
                                  const std::wstring& other_values) {
  render_view_host()->UploadFile(file_path, form, file, submit, other_values);
}

void WebContents::SetWebApp(WebApp* web_app) {
  if (web_app_.get()) {
    web_app_->RemoveObserver(this);
    web_app_->SetWebContents(NULL);
  }

  web_app_ = web_app;
  if (web_app) {
    web_app->AddObserver(this);
    web_app_->SetWebContents(this);
  }
}

bool WebContents::IsWebApplication() const {
  return (web_app_.get() != NULL);
}

void WebContents::CreateShortcut() {
  NavigationEntry* entry = controller()->GetLastCommittedEntry();
  if (!entry)
    return;

  // We only allow one pending install request. By resetting the page id we
  // effectively cancel the pending install request.
  pending_install_.page_id = entry->GetPageID();
  pending_install_.icon = GetFavIcon();
  pending_install_.title = GetTitle();
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

void WebContents::FillForm(const FormData& form) {
  render_view_host()->FillForm(form);
}

void WebContents::FillPasswordForm(
    const PasswordFormDomManager::FillData& form_data) {
  render_view_host()->FillPasswordForm(form_data);
}

void WebContents::DragTargetDragEnter(const WebDropData& drop_data,
    const gfx::Point& client_pt, const gfx::Point& screen_pt) {
  render_view_host()->DragTargetDragEnter(drop_data, client_pt, screen_pt);
}

void WebContents::DragTargetDragOver(
    const gfx::Point& client_pt, const gfx::Point& screen_pt) {
  render_view_host()->DragTargetDragOver(client_pt, screen_pt);
}

void WebContents::DragTargetDragLeave() {
  render_view_host()->DragTargetDragLeave();
}

void WebContents::DragTargetDrop(
    const gfx::Point& client_pt, const gfx::Point& screen_pt) {
  render_view_host()->DragTargetDrop(client_pt, screen_pt);
}

PasswordManager* WebContents::GetPasswordManager() {
  if (password_manager_.get() == NULL)
    password_manager_.reset(new PasswordManager(this));
  return password_manager_.get();
}

PluginInstaller* WebContents::GetPluginInstaller() {
  if (plugin_installer_.get() == NULL)
    plugin_installer_.reset(new PluginInstaller(this));
  return plugin_installer_.get();
}

bool WebContents::IsActiveEntry(int32 page_id) {
  NavigationEntry* active_entry = controller()->GetActiveEntry();
  return (active_entry != NULL &&
          active_entry->site_instance() == site_instance() &&
          active_entry->GetPageID() == page_id);
}

///////////////////////////////////////////////////////////////////////////////
// RenderViewHostDelegate implementation:

Profile* WebContents::GetProfile() const {
  return profile();
}

void WebContents::CreateView(int route_id, HANDLE modal_dialog_event) {
  WebContents* new_view = new WebContents(profile(),
                                          site_instance(),
                                          render_view_factory_,
                                          route_id,
                                          modal_dialog_event);
  new_view->SetupController(profile());
  // TODO(beng)
  // The intention here is to create background tabs, which should ideally
  // be parented to NULL. However doing that causes the corresponding view
  // container windows to show up as overlapped windows, which causes
  // other issues. We should fix this.
  HWND new_view_parent_window = ::GetAncestor(GetHWND(), GA_ROOT);
  new_view->CreateView(new_view_parent_window, gfx::Rect());
  // TODO(brettw) it seems bogus that we have to call this function on the
  // newly created object and give it one of its own member variables.
  new_view->CreatePageView(new_view->render_view_host());

  // Don't show the view until we get enough context in ShowView.
  pending_views_[route_id] = new_view;
}

void WebContents::CreateWidget(int route_id) {
  RenderWidgetHost* widget_host = new RenderWidgetHost(process(), route_id);
  RenderWidgetHostHWND* widget_view = new RenderWidgetHostHWND(widget_host);
  widget_host->set_view(widget_view);
  // We set the parent HWDN explicitly as pop-up HWNDs are parented and owned by
  // the first non-child HWND of the HWND that was specified to the CreateWindow
  // call.
  widget_view->set_parent_hwnd(view()->GetPluginHWND());
  widget_view->set_close_on_deactivate(true);

  // Don't show the widget until we get its position in ShowWidget.
  pending_widgets_[route_id] = widget_host;
}

void WebContents::ShowView(int route_id,
                           WindowOpenDisposition disposition,
                           const gfx::Rect& initial_pos,
                           bool user_gesture) {
  PendingViews::iterator iter = pending_views_.find(route_id);
  if (iter == pending_views_.end()) {
    DCHECK(false);
    return;
  }

  WebContents* new_view = iter->second;
  pending_views_.erase(route_id);

  if (!new_view->view() ||
      !new_view->process()->channel()) {
    // The view has gone away or the renderer crashed. Nothing to do.
    return;
  }

  // TODO(brettw) this seems bogus to reach into here and initialize the host.
  new_view->render_view_host()->Init();
  AddNewContents(new_view, disposition, initial_pos, user_gesture);
}

void WebContents::ShowWidget(int route_id, const gfx::Rect& initial_pos) {
  PendingWidgets::iterator iter = pending_widgets_.find(route_id);
  if (iter == pending_widgets_.end()) {
    DCHECK(false);
    return;
  }

  RenderWidgetHost* widget_host = iter->second;
  pending_widgets_.erase(route_id);

  // TODO(beng): (Cleanup) move all this windows-specific creation and showing
  //             code into RenderWidgetHostHWND behind some API that a
  //             ChromeView can also reasonably implement.
  RenderWidgetHostHWND* widget_view =
      static_cast<RenderWidgetHostHWND*>(widget_host->view());

  if (!widget_view || !widget_host->process()->channel()) {
    // The view has gone away or the renderer crashed. Nothing to do.
    return;
  }
  widget_view->Create(GetHWND(), NULL, NULL, WS_POPUP, WS_EX_TOOLWINDOW);
  widget_view->MoveWindow(initial_pos.x(), initial_pos.y(), initial_pos.width(),
                          initial_pos.height(), TRUE);
  widget_view->ShowWindow(SW_SHOW);
  widget_host->Init();
}

void WebContents::RendererReady(RenderViewHost* rvh) {
  if (render_manager_.showing_interstitial_page() &&
      rvh == render_view_host()) {
    // We are showing an interstitial page, don't notify the world.
    return;
  } else if (rvh != render_view_host()) {
    // Don't notify the world, since this came from a renderer in the
    // background.
    return;
  }

  NotifyConnected();
  SetIsCrashed(false);
}

void WebContents::RendererGone(RenderViewHost* rvh) {
  // Ask the print preview if this renderer was valuable.
  if (!printing_.OnRendererGone(rvh))
    return;
  if (rvh != render_view_host()) {
    // The pending or interstitial page's RenderViewHost is gone.  If we are
    // showing an interstitial, this may mean that the original RenderViewHost
    // is gone.  If so, we will call RendererGone again if we try to swap that
    // RenderViewHost back in, in SwapToRenderView.
    return;
  }

  // Force an invalidation here to render sad tab.  however, it is possible for
  // our window to have already gone away (since we may be in the process of
  // closing this render view).
  if (::IsWindow(GetHWND()))
    InvalidateRect(GetHWND(), NULL, FALSE);

  SetIsLoading(false, NULL);

  // Ensure that this browser window is enabled.  This deals with the case where
  // a renderer crashed while showing a modal dialog.  We're assuming that the
  // browser code will never show a modal dialog, so we could only be disabled
  // by something the renderer (or some plug-in) did.
  HWND root_window = ::GetAncestor(GetHWND(), GA_ROOT);
  if (!::IsWindowEnabled(root_window))
    ::EnableWindow(root_window, TRUE);

  NotifyDisconnected();
  SetIsCrashed(true);
  // Hide any visible hung renderer warning for this web contents' process.
  HungRendererWarning::HideForWebContents(this);
}

void WebContents::DidNavigate(RenderViewHost* rvh,
                              const ViewHostMsg_FrameNavigate_Params& params) {
  if (PageTransition::IsMainFrame(params.transition))
    render_manager_.DidNavigateMainFrame(rvh);

  // In the case of interstitial, we don't mess with the navigation entries.
  if (render_manager_.showing_interstitial_page())
    return;

  // Check for navigations we don't expect.
  if (!controller() || !is_active_ || params.page_id == -1) {
    if (params.page_id == -1) {
      DCHECK(controller()->GetActiveEntry() == NULL)
          << "The renderer is permitted to send a FrameNavigate event for an "
             "invalid |page_id| if, and only if, this is the initial blank "
             "page for a main frame.";
    }
    BroadcastProvisionalLoadCommit(rvh, params);
    return;
  }

  // DO NOT ADD MORE STUFF TO THIS FUNCTION!  Don't make me come over there!
  // =======================================================================
  // Add your code to DidNavigateAnyFramePreCommit if you have a helper object
  // that needs to know about all navigations. If it needs to do it only for
  // main frame or sub-frame navigations, add your code to
  // DidNavigateMainFrame or DidNavigateSubFrame. If you need to run it after
  // the navigation has been committed, put it in a *PostCommit version.

  // Create the new navigation entry for this navigation and do work specific to
  // this frame type. The main frame / sub frame functions will do additional
  // updates to the NavigationEntry appropriate for the navigation type (in
  // addition to a lot of other stuff).
  scoped_ptr<NavigationEntry> entry(CreateNavigationEntryForCommit(params));
  if (PageTransition::IsMainFrame(params.transition))
    DidNavigateMainFramePreCommit(params, entry.get());
  else
    DidNavigateSubFramePreCommit(params, entry.get());

  // Now we do non-frame-specific work in *AnyFramePreCommit (this depends on
  // the entry being completed appropriately in the frame-specific versions
  // above before the call).
  DidNavigateAnyFramePreCommit(params, entry.get());

  // Commit the entry to the navigation controller.
  DidNavigateToEntry(entry.release());
  // WARNING: NavigationController will have taken ownership of entry at this
  // point, and may have deleted it. As such, do NOT use entry after this.

  // Run post-commit tasks.
  if (PageTransition::IsMainFrame(params.transition))
    DidNavigateMainFramePostCommit(params);
  DidNavigateAnyFramePostCommit(rvh, params);
}

NavigationEntry* WebContents::CreateNavigationEntryForCommit(
    const ViewHostMsg_FrameNavigate_Params& params) {
  // This new navigation entry will represent the navigation. Note that we
  // don't set the URL. This will happen in the DidNavigateMainFrame/SubFrame
  // because the entry's URL should represent the toplevel frame only.
  NavigationEntry* entry = new NavigationEntry(type());
  entry->SetPageID(params.page_id);
  entry->SetTransitionType(params.transition);
  entry->SetSiteInstance(site_instance());

  // Now that we've assigned a SiteInstance to this entry, we need to
  // assign it to the NavigationController's pending entry as well.  This
  // allows us to find it via GetEntryWithPageID, etc.
  if (controller()->GetPendingEntry())
    controller()->GetPendingEntry()->SetSiteInstance(entry->site_instance());

  // Update the site of the SiteInstance if it doesn't have one yet, unless we
  // are showing an interstitial page.  If we are, we should wait until the
  // real page commits.
  //
  // TODO(brettw) the old code only checked for INTERSTIAL, this new code also
  // checks for LEAVING_INTERSTITIAL mode in the manager. Is this difference
  // important?
  if (!site_instance()->has_site() &&
      !render_manager_.showing_interstitial_page())
    site_instance()->SetSite(params.url);

  // When the navigation is just a change in ref or a sub-frame navigation, the
  // new page should inherit the existing entry's title and favicon, since it
  // will be the same.  A change in ref also inherits the security style and SSL
  // associated info.
  bool in_page_nav;
  if ((in_page_nav = IsInPageNavigation(params.url)) ||
      !PageTransition::IsMainFrame(params.transition)) {
    // In the case of a sub-frame navigation within a window that was created
    // without an URL (via window.open), we may not have a committed entry yet!
    NavigationEntry* old_entry = controller()->GetLastCommittedEntry();
    if (old_entry) {
      entry->SetTitle(old_entry->GetTitle());
      entry->SetFavIcon(old_entry->GetFavIcon());
      entry->SetFavIconURL(old_entry->GetFavIconURL());
      if (in_page_nav) {
        entry->SetValidFavIcon(old_entry->IsValidFavIcon());
        entry->CopySSLInfoFrom(*old_entry);
      }
    }
  }

  return entry;
}

void WebContents::DidNavigateMainFramePreCommit(
    const ViewHostMsg_FrameNavigate_Params& params,
    NavigationEntry* entry) {
  // Update contents MIME type of the main webframe.
  contents_mime_type_ = params.contents_mime_type;

  entry->SetURL(params.url);

  NavigationEntry* pending = controller()->GetPendingEntry();
  if (pending) {
    // Copy fields from the pending NavigationEntry into the actual
    // NavigationEntry that we're committing to.
    entry->SetUserTypedURL(pending->GetUserTypedURL());
    if (pending->HasDisplayURL())
      entry->SetDisplayURL(pending->GetDisplayURL());
    if (pending->GetURL().SchemeIsFile())
      entry->SetTitle(pending->GetTitle());
    entry->SetContentState(pending->GetContentState());
  }

  // We no longer know the title after this navigation.
  has_page_title_ = false;

  // Reset the starred button to false by default, but also request from
  // history whether it's actually starred.
  //
  // Only save the URL in the entry for top-level frames. This will appear in
  // the UI for the page, so we always want to use the toplevel URL.
  //
  // The |user_initiated_big_change| flag indicates whether we can tell the
  // infobar/password manager about this navigation.  True for non-redirect,
  // non-in-page user initiated navigations; assume this is true and set false
  // below if not.
  //
  // TODO(pkasting): http://b/1048012 We should notify based on whether the
  // navigation was triggered by a user action rather than most of our current
  // heuristics.  Be careful with SSL infobars, though.
  //
  // See bug 1051891 for reasons why we need both a redirect check and gesture
  // check; basically gesture checking is not always accurate.
  //
  // Note that the redirect check also checks for a pending entry to
  // differentiate real redirects from browser initiated navigations to a
  // redirected entry (like when you hit back to go to a page that was the
  // destination of a redirect, we don't want to treat it as a redirect
  // even though that's what its transition will be) http://b/1117048.
  bool user_initiated_big_change = true;
  if ((PageTransition::IsRedirect(entry->GetTransitionType()) &&
       !controller()->GetPendingEntry()) ||
       (params.gesture == NavigationGestureAuto) ||
       IsInPageNavigation(params.url)) {
    user_initiated_big_change = false;
  } else {
    // Clear the status bubble. This is a workaround for a bug where WebKit
    // doesn't let us know that the cursor left an element during a
    // transition (this is also why the mouse cursor remains as a hand after
    // clicking on a link); see bugs 1184641 and 980803. We don't want to
    // clear the bubble when a user navigates to a named anchor in the same
    // page.
    UpdateTargetURL(params.page_id, GURL());
  }

  // Let the infobar know about the navigation to give the infobar a chance
  // to remove any views on navigating. Only do so if this navigation was
  // initiated by the user, and we are not simply following a fragment to
  // relocate within the current page.
  //
  // We must do this after calling DidNavigateToEntry(), since the info bar
  // view checks the controller's active entry to determine whether to auto-
  // expire any children.
  if (user_initiated_big_change && IsInfoBarVisible()) {
    InfoBarView* info_bar = GetInfoBarView();
    DCHECK(info_bar);
    info_bar->DidNavigate(entry);
  }

  // UpdateHelpersForDidNavigate will handle the case where the password_form
  // origin is valid.
  if (user_initiated_big_change && !params.password_form.origin.is_valid())
    GetPasswordManager()->DidNavigate();

  GenerateKeywordIfNecessary(params);

  // Close constrained popups if necessary.
  MaybeCloseChildWindows(params);

  // Get the favicon, either from history or request it from the net.
  fav_icon_helper_.FetchFavIcon(entry->GetURL());

  // We hide the FindInPage window when the user navigates away, except on
  // reload.
  if (PageTransition::StripQualifier(params.transition) !=
      PageTransition::RELOAD)
    SetFindInPageVisible(false);

  entry->SetHasPostData(params.is_post);
}

void WebContents::DidNavigateSubFramePreCommit(
    const ViewHostMsg_FrameNavigate_Params& params,
    NavigationEntry* entry) {
  NavigationEntry* last_committed = controller()->GetLastCommittedEntry();
  if (!last_committed) {
    // In the case of a sub-frame navigation within a window that was created
    // without an URL (via window.open), we may not have a committed entry yet!
    return;
  }

  // Reset entry state to match that of the pending entry.
  entry->set_unique_id(last_committed->unique_id());
  entry->SetURL(last_committed->GetURL());
  entry->SetSecurityStyle(last_committed->GetSecurityStyle());
  entry->SetContentState(last_committed->GetContentState());
  entry->SetTransitionType(last_committed->GetTransitionType());
  entry->SetUserTypedURL(last_committed->GetUserTypedURL());

  // TODO(jcampan): when navigating to an insecure/unsafe inner frame, the
  // main entry is the one that gets notified of the mixed/unsafe contents
  // (see SSLPolicy::OnRequestStarted()).  Here we are just transferring that
  // state.  We should find a better way to do this.
  // Note that it is OK that the mixed/unsafe contents is set on the wrong
  // navigation entry, as that state is reset when navigating back to it.
  if (last_committed->HasMixedContent())
    entry->SetHasMixedContent();
  if (last_committed->HasUnsafeContent())
    entry->SetHasUnsafeContent();
}

void WebContents::DidNavigateAnyFramePreCommit(
    const ViewHostMsg_FrameNavigate_Params& params,
    NavigationEntry* entry) {

  // Hide the download shelf if all the following conditions are true:
  // - there are no active downloads.
  // - this is a navigation to a different TLD.
  // - at least 5 seconds have elapsed since the download shelf was shown.
  // TODO(jcampan): bug 1156075 when user gestures are reliable, they should
  //                 be used to ensure we are hiding only on user initiated
  //                 navigations.
  NavigationEntry* current_entry = controller()->GetLastCommittedEntry();
  DownloadManager* download_manager = profile()->GetDownloadManager();
  // download_manager can be NULL in unit test context.
  if (download_manager && download_manager ->in_progress_count() == 0 &&
      current_entry && !net::RegistryControlledDomainService::SameDomainOrHost(
          current_entry->GetURL(), entry->GetURL())) {
    TimeDelta time_delta(
        TimeTicks::Now() - last_download_shelf_show_);
    if (time_delta >
        TimeDelta::FromMilliseconds(kDownloadShelfHideDelay)) {
      SetDownloadShelfVisible(false);
    }
  }

  // Reset timing data and log.
  HandleProfilingForDidNavigate(params);

  // Notify the password manager of the navigation or form submit.
  if (params.password_form.origin.is_valid())
    GetPasswordManager()->ProvisionallySavePassword(params.password_form);

  // If we navigate, start showing messages again. This does nothing to prevent
  // a malicious script from spamming messages, since the script could just
  // reload the page to stop blocking.
  suppress_javascript_messages_ = false;

  // Update history. Note that this needs to happen after the entry is complete,
  // which WillNavigate[Main,Sub]Frame will do before this function is called.
  if (params.should_update_history) {
    // Most of the time, the displayURL matches the loaded URL, but for about:
    // URLs, we use a data: URL as the real value.  We actually want to save
    // the about: URL to the history db and keep the data: URL hidden.
    UpdateHistoryForNavigation(entry->GetDisplayURL(), params);
  }
}

void WebContents::DidNavigateMainFramePostCommit(
    const ViewHostMsg_FrameNavigate_Params& params) {
  // The keyword generator uses the navigation entries, so must be called after
  // the commit.
  GenerateKeywordIfNecessary(params);

  // Update the starred state.
  UpdateStarredStateForCurrentURL();
}

void WebContents::DidNavigateAnyFramePostCommit(
    RenderViewHost* render_view_host,
    const ViewHostMsg_FrameNavigate_Params& params) {
  // Have the controller save the current session.
  controller()->SyncSessionWithEntryByPageID(type(),
                                             site_instance(),
                                             params.page_id);

  BroadcastProvisionalLoadCommit(render_view_host, params);
}

bool WebContents::IsWebApplicationActive() const {
  if (!web_app_.get())
    return false;

  // If we are inside an application, the application is always active. For
  // example, this allows us to display the GMail icon even when we are bounced
  // the login page.
  if (delegate() && delegate()->IsApplication())
    return true;

  return (GetURL() == web_app_->url());
}

void WebContents::WebAppImagesChanged(WebApp* web_app) {
  DCHECK(web_app == web_app_.get());
  if (delegate() && IsWebApplicationActive())
    delegate()->NavigationStateChanged(this, TabContents::INVALIDATE_FAVICON);
}

void WebContents::HandleProfilingForDidNavigate(
    const ViewHostMsg_FrameNavigate_Params& params) {
  PageTransition::Type stripped_transition_type =
      PageTransition::StripQualifier(params.transition);
  if (stripped_transition_type == PageTransition::LINK ||
      stripped_transition_type == PageTransition::FORM_SUBMIT) {
    CreateNewProfilingEntry(params.url);
  }

  current_load_start_ = TimeTicks::Now();

  if (is_profiling()) {
    NavigationProfiler* profiler = GetNavigationProfiler();

    FrameNavigationMetrics* frame =
        new FrameNavigationMetrics(PageTransition::FromInt(params.transition),
                                   current_load_start_,
                                   params.url,
                                   params.page_id);

    profiler->AddFrameMetrics(process()->host_id(),
                              render_view_host()->routing_id(),
                              frame);
  }
}

void WebContents::BroadcastProvisionalLoadCommit(
    RenderViewHost* render_view_host,
    const ViewHostMsg_FrameNavigate_Params& params) {
  ProvisionalLoadDetails details(
      PageTransition::IsMainFrame(params.transition),
      render_manager_.IsRenderViewInterstitial(render_view_host),
      IsInPageNavigation(params.url),
      params.url, params.security_info);
  NotificationService::current()->
      Notify(NOTIFY_FRAME_PROVISIONAL_LOAD_COMMITTED,
             Source<NavigationController>(controller()),
             Details<ProvisionalLoadDetails>(&details));
}

void WebContents::UpdateStarredStateForCurrentURL() {
  BookmarkBarModel* model = profile()->GetBookmarkBarModel();
  const bool old_state = is_starred_;
  is_starred_ = (model && model->GetNodeByURL(GetURL()));

  if (is_starred_ != old_state && delegate())
    delegate()->URLStarredChanged(this, is_starred_);
}

void WebContents::UpdateAlternateErrorPageURL() {
  GURL url = GetAlternateErrorPageURL();
  render_view_host()->SetAlternateErrorPageURL(url);
}

void WebContents::UpdateWebPreferences() {
  render_view_host()->UpdateWebPreferences(GetWebkitPrefs());
}

void WebContents::UpdateRenderViewSize() {
  // Using same technique as OnPaint, which sets size of SadTab.
  CRect cr;
  GetClientRect(&cr);
  gfx::Size new_size(cr.Width(), cr.Height());
  SizeContents(new_size);
}

void WebContents::UpdateState(RenderViewHost* rvh,
                              int32 page_id,
                              const GURL& url,
                              const std::wstring& title,
                              const std::string& state) {
  if (rvh != render_view_host() ||
      render_manager_.showing_interstitial_page()) {
    // This UpdateState is either:
    // - targeted not at the current RenderViewHost.  This could be that we are
    // showing the interstitial page and getting an update for the regular page,
    // or that we are navigating from the interstitial and getting an update
    // for it.
    // - targeted at the interstitial page. Ignore it as we don't want to update
    // the fake navigation entry.
    return;
  }

  if (!controller())
    return;

  // We must be prepared to handle state updates for any page, these occur
  // when the user is scrolling and entering form data, as well as when we're
  // leaving a page, in which case our state may have already been moved to
  // the next page. The navigation controller will look up the appropriate
  // NavigationEntry and update it when it is notified via the delegate.

  NavigationEntry* entry = controller()->GetEntryWithPageID(
      type(), site_instance(), page_id);
  if (!entry)
    return;

  unsigned changed_flags = 0;

  // Update the URL.
  if (url != entry->GetURL()) {
    changed_flags |= INVALIDATE_URL;
    if (entry == controller()->GetActiveEntry())
      fav_icon_helper_.FetchFavIcon(url);
    entry->SetURL(url);
  }

  // For file URLs without a title, use the pathname instead.
  std::wstring final_title;
  if (url.SchemeIsFile() && title.empty()) {
    final_title = UTF8ToWide(url.ExtractFileName());
  } else {
    TrimWhitespace(title, TRIM_ALL, &final_title);
  }
  if (final_title != entry->GetTitle()) {
    changed_flags |= INVALIDATE_TITLE;
    entry->SetTitle(final_title);

    // Update the history system for this page.
    if (!profile()->IsOffTheRecord()) {
      HistoryService* hs =
          profile()->GetHistoryService(Profile::IMPLICIT_ACCESS);
      if (hs)
        hs->SetPageTitle(entry->GetDisplayURL(), final_title);
    }
  }
  if (GetHWND()) {
    // It's possible to get this after the hwnd has been destroyed.
    ::SetWindowText(GetHWND(), title.c_str());
    ::SetWindowText(view()->GetPluginHWND(), title.c_str());
  }

  // Update the state (forms, etc.).
  if (state != entry->GetContentState()) {
    changed_flags |= INVALIDATE_STATE;
    entry->SetContentState(state);
  }

  // Notify everybody of the changes (only when the current page changed).
  if (changed_flags && entry == controller()->GetActiveEntry())
    NotifyNavigationStateChanged(changed_flags);
  controller()->SyncSessionWithEntryByPageID(type(), site_instance(), page_id);
}

void WebContents::UpdateTitle(RenderViewHost* rvh,
                              int32 page_id, const std::wstring& title) {
  if (!controller())
    return;

  // If we have a title, that's a pretty good indication that we've started
  // getting useful data.
  response_started_ = false;

  NavigationEntry* entry;
  if (render_manager_.showing_interstitial_page() &&
      (rvh == render_view_host())) {
    // We are showing an interstitial page in a different RenderViewHost, so
    // the page_id is not sufficient to find the entry from the controller.
    // (both RenderViewHost page_ids overlap).  We know it is the last entry,
    // so just use that.
    entry = controller()->GetLastCommittedEntry();
  } else {
    entry = controller()->GetEntryWithPageID(type(), site_instance(), page_id);
  }

  if (!entry)
    return;

  std::wstring trimmed_title;
  TrimWhitespace(title, TRIM_ALL, &trimmed_title);
  if (title == entry->GetTitle())
    return;  // Title did not change, do nothing.

  entry->SetTitle(trimmed_title);

  // Broadcast notifications when the UI should be updated.
  if (entry == controller()->GetEntryAtOffset(0))
    NotifyNavigationStateChanged(INVALIDATE_TITLE);

  // Update the history system for this page.
  if (profile()->IsOffTheRecord())
    return;

  HistoryService* hs = profile()->GetHistoryService(Profile::IMPLICIT_ACCESS);
  if (hs && !has_page_title_ && !trimmed_title.empty()) {
    hs->SetPageTitle(entry->GetDisplayURL(), trimmed_title);
    has_page_title_ = true;
  }
}


void WebContents::UpdateEncoding(RenderViewHost* render_view_host,
                                 const std::wstring& encoding_name) {
  SetEncoding(encoding_name);
}

void WebContents::UpdateTargetURL(int32 page_id, const GURL& url) {
  if (delegate())
    delegate()->UpdateTargetURL(this, url);
}

void WebContents::UpdateThumbnail(const GURL& url,
                                  const SkBitmap& bitmap,
                                  const ThumbnailScore& score) {
  // Tell History about this thumbnail
  HistoryService* hs;
  if (!profile()->IsOffTheRecord() &&
      (hs = profile()->GetHistoryService(Profile::IMPLICIT_ACCESS))) {
    hs->SetPageThumbnail(url, bitmap, score);
  }
}

void WebContents::Close(RenderViewHost* rvh) {
  // Ignore this if it comes from a RenderViewHost that we aren't showing.
  if (delegate() && rvh == render_view_host())
    delegate()->CloseContents(this);
}

void WebContents::RequestMove(const gfx::Rect& new_bounds) {
  if (delegate() && delegate()->IsPopup(this))
    delegate()->MoveContents(this, new_bounds);
}

void WebContents::DidStartLoading(RenderViewHost* rvh, int32 page_id) {
  if (plugin_installer_ != NULL)
    plugin_installer_->OnStartLoading();
  SetIsLoading(true, NULL);
}

void WebContents::DidStopLoading(RenderViewHost* rvh, int32 page_id) {
  TimeTicks current_time = TimeTicks::Now();
  if (is_profiling()) {
    NavigationProfiler* profiler = GetNavigationProfiler();

    profiler->SetLoadingEndTime(process()->host_id(),
                                render_view_host()->routing_id(), page_id,
                                current_time);
    SaveCurrentProfilingEntry();
  }

  scoped_ptr<LoadNotificationDetails> details;

  if (controller()) {
    NavigationEntry* entry = controller()->GetActiveEntry();
    if (entry) {
      scoped_ptr<process_util::ProcessMetrics> metrics(
          process_util::ProcessMetrics::CreateProcessMetrics(
              process()->process()));

      TimeDelta elapsed = current_time - current_load_start_;

      details.reset(new LoadNotificationDetails(
          entry->GetDisplayURL(),
          entry->GetTransitionType(),
          elapsed,
          controller(),
          controller()->GetCurrentEntryIndex()));
    } else {
      DCHECK(page_id == -1) <<
          "When a controller exists a NavigationEntry should always be "
          "available in OnMsgDidStopLoading unless we are loading the "
          "initial blank page.";
    }
  }

  // Tell PasswordManager we've finished a page load, which serves as a
  // green light to save pending passwords and reset itself.
  GetPasswordManager()->DidStopLoading();

  SetIsLoading(false, details.get());
}

void WebContents::DidStartProvisionalLoadForFrame(
    RenderViewHost* render_view_host,
    bool is_main_frame,
    const GURL& url) {
  ProvisionalLoadDetails details(
      is_main_frame,
      render_manager_.IsRenderViewInterstitial(render_view_host),
      IsInPageNavigation(url),
      url, std::string());
  NotificationService::current()->
      Notify(NOTIFY_FRAME_PROVISIONAL_LOAD_START,
             Source<NavigationController>(controller()),
             Details<ProvisionalLoadDetails>(&details));
}

void WebContents::DidRedirectProvisionalLoad(int32 page_id,
                                             const GURL& source_url,
                                             const GURL& target_url) {
  NavigationEntry* entry;
  if (page_id == -1)
    entry = controller()->GetPendingEntry();
  else
    entry = controller()->GetEntryWithPageID(type(), site_instance(), page_id);
  if (!entry || entry->GetType() != type() || entry->GetURL() != source_url)
      return;
  entry->SetURL(target_url);
}

void WebContents::DidLoadResourceFromMemoryCache(
    const GURL& url,
    const std::string& security_info) {
  if (!controller())
    return;

  // Send out a notification that we loaded a resource from our memory cache.
  int cert_id, cert_status, security_bits;
  SSLManager::DeserializeSecurityInfo(security_info,
                                      &cert_id, &cert_status,
                                      &security_bits);
  LoadFromMemoryCacheDetails details(url, cert_id, cert_status);

  NotificationService::current()->
      Notify(NOTIFY_LOAD_FROM_MEMORY_CACHE,
             Source<NavigationController>(controller()),
             Details<LoadFromMemoryCacheDetails>(&details));
}

void WebContents::DidFailProvisionalLoadWithError(
    RenderViewHost* render_view_host,
    bool is_main_frame,
    int error_code,
    const GURL& url,
    bool showing_repost_interstitial) {
  if (!controller())
    return;

  if (net::ERR_ABORTED == error_code) {
    // This will discard our pending entry if we cancelled the load (e.g., if we
    // decided to download the file instead of load it). Only discard the
    // pending entry if the URLs match, otherwise the user initiated a navigate
    // before the page loaded so that the discard would discard the wrong entry.
    NavigationEntry* pending_entry = controller()->GetPendingEntry();
    if (pending_entry && pending_entry->GetURL() == url)
      controller()->DiscardPendingEntry();

    render_manager_.RendererAbortedProvisionalLoad(render_view_host);
  }

  // Send out a notification that we failed a provisional load with an error.
  ProvisionalLoadDetails details(
      is_main_frame,
      render_manager_.IsRenderViewInterstitial(render_view_host),
      IsInPageNavigation(url),
      url, std::string());
  details.set_error_code(error_code);

  render_manager_.set_showing_repost_interstitial(showing_repost_interstitial);

  NotificationService::current()->
      Notify(NOTIFY_FAIL_PROVISIONAL_LOAD_WITH_ERROR,
             Source<NavigationController>(controller()),
             Details<ProvisionalLoadDetails>(&details));
}

void WebContents::FindReply(int request_id,
                            int number_of_matches,
                            const gfx::Rect& selection_rect,
                            int active_match_ordinal,
                            bool final_update) {
  // ViewMsgHost_FindResult message received. The find-in-page result
  // is obtained. Fire the notification
  FindNotificationDetails detail(request_id,
                                 number_of_matches,
                                 selection_rect,
                                 active_match_ordinal,
                                 final_update);
  // Notify all observers of this notification.
  // The current find box owns one such observer.
  NotificationService::current()->
      Notify(NOTIFY_FIND_RESULT_AVAILABLE, Source<TabContents>(this),
             Details<FindNotificationDetails>(&detail));
}

void WebContents::UpdateFavIconURL(RenderViewHost* render_view_host,
                                   int32 page_id,
                                   const GURL& icon_url) {
  fav_icon_helper_.SetFavIconURL(icon_url);
}

void WebContents::DidDownloadImage(
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
  if (web_app_.get() && !errored)
    web_app_->SetImage(image_url, image);
}

void WebContents::ShowContextMenu(
    const ViewHostMsg_ContextMenu_Params& params) {
  RenderViewContextMenuController menu_controller(this, params);
  RenderViewContextMenu menu(&menu_controller, GetHWND(), params.type,
                             params.dictionary_suggestions, profile());

  POINT screen_pt = { params.x, params.y };
  MapWindowPoints(GetHWND(), HWND_DESKTOP, &screen_pt, 1);

  // Enable recursive tasks on the message loop so we can get updates while
  // the context menu is being displayed.
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  menu.RunMenuAt(screen_pt.x, screen_pt.y);
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

void WebContents::StartDragging(const WebDropData& drop_data) {
  scoped_refptr<OSExchangeData> data(new OSExchangeData);

  // TODO(tc): Generate an appropriate drag image.

  // We set the file contents before the URL because the URL also sets file
  // contents (to a .URL shortcut).  We want to prefer file content data over a
  // shortcut.
  if (!drop_data.file_contents.empty()) {
    data->SetFileContents(drop_data.file_description_filename,
                          drop_data.file_contents);
  }
  if (!drop_data.cf_html.empty())
    data->SetCFHtml(drop_data.cf_html);
  if (drop_data.url.is_valid())
    data->SetURL(drop_data.url, drop_data.url_title);
  if (!drop_data.plain_text.empty())
    data->SetString(drop_data.plain_text);

  scoped_refptr<WebDragSource> drag_source(
      new WebDragSource(GetHWND(), render_view_host()));

  DWORD effects;

  // We need to enable recursive tasks on the message loop so we can get
  // updates while in the system DoDragDrop loop.
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  DoDragDrop(data, drag_source, DROPEFFECT_COPY | DROPEFFECT_LINK, &effects);
  MessageLoop::current()->SetNestableTasksAllowed(old_state);

  if (render_view_host())
    render_view_host()->DragSourceSystemDragEnded();
}

void WebContents::UpdateDragCursor(bool is_drop_target) {
  drop_target_->set_is_drop_target(is_drop_target);
}

void WebContents::RequestOpenURL(const GURL& url,
                                 WindowOpenDisposition disposition) {
  OpenURL(url, disposition, PageTransition::LINK);
}

void WebContents::DomOperationResponse(const std::string& json_string,
                                       int automation_id) {
  DomOperationNotificationDetails details(json_string, automation_id);
  NotificationService::current()->Notify(
      NOTIFY_DOM_OPERATION_RESPONSE, Source<WebContents>(this),
      Details<DomOperationNotificationDetails>(&details));
}

void WebContents::GoToEntryAtOffset(int offset) {
  if (!controller())
    return;

  controller()->GoToOffset(offset);
}

void WebContents::GetHistoryListCount(int* back_list_count,
                                      int* forward_list_count) {
  *back_list_count = 0;
  *forward_list_count = 0;

  if (controller()) {
    int current_index = controller()->GetLastCommittedEntryIndex();
    *back_list_count = current_index;
    *forward_list_count = controller()->GetEntryCount() - current_index - 1;
  }
}

void WebContents::RunFileChooser(const std::wstring& default_file) {
  HWND toplevel_hwnd = GetAncestor(GetContainerHWND(), GA_ROOT);
  if (!select_file_dialog_.get())
    select_file_dialog_ = SelectFileDialog::Create(this);
  select_file_dialog_->SelectFile(SelectFileDialog::SELECT_OPEN_FILE, L"",
                                  default_file, toplevel_hwnd, NULL);
}

void WebContents::RunJavaScriptMessage(
    const std::wstring& message,
    const std::wstring& default_prompt,
    const int flags,
    IPC::Message* reply_msg) {
  if (!suppress_javascript_messages_) {
    TimeDelta time_since_last_message(
        TimeTicks::Now() - last_javascript_message_dismissal_);
    bool show_suppress_checkbox = false;
    // Show a checkbox offering to suppress further messages if this message is
    // being displayed within kJavascriptMessageExpectedDelay of the last one.
    if (time_since_last_message <
        TimeDelta::FromMilliseconds(kJavascriptMessageExpectedDelay))
      show_suppress_checkbox = true;

    JavascriptMessageBoxHandler::RunJavascriptMessageBox(this,
                                                         flags,
                                                         message,
                                                         default_prompt,
                                                         show_suppress_checkbox,
                                                         reply_msg);
  } else {
    // If we are suppressing messages, just reply as is if the user immediately
    // pressed "Cancel".
    OnJavaScriptMessageBoxClosed(reply_msg, false, L"");
  }
}

void WebContents::RunBeforeUnloadConfirm(const std::wstring& message,
                                            IPC::Message* reply_msg) {
  JavascriptBeforeUnloadHandler::RunBeforeUnloadDialog(this, message,
                                                       reply_msg);
}

void WebContents::ShowModalHTMLDialog(const GURL& url, int width, int height,
                                      const std::string& json_arguments,
                                      IPC::Message* reply_msg) {
  if (delegate()) {
    ModalHtmlDialogDelegate* dialog_delegate =
        new ModalHtmlDialogDelegate(url, width, height, json_arguments,
                                    reply_msg, this);
    delegate()->ShowHtmlDialog(dialog_delegate, NULL);
  }
}

void WebContents::PasswordFormsSeen(
    const std::vector<PasswordForm>& forms) {
  GetPasswordManager()->PasswordFormsSeen(forms);
}

void WebContents::TakeFocus(bool reverse) {
  ChromeViews::FocusManager* focus_manager =
      ChromeViews::FocusManager::GetFocusManager(GetHWND());

  // We may not have a focus manager if the tab has been switched before this
  // message arrived.
  if (focus_manager) {
    focus_manager->AdvanceFocus(reverse);
  }
}

GURL WebContents::GetAlternateErrorPageURL() const {
  GURL url;
  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  if (prefs->GetBoolean(prefs::kAlternateErrorPagesEnabled)) {
    url = google_util::AppendGoogleLocaleParam(GURL(kLinkDoctorBaseURL));
    url = google_util::AppendGoogleTLDParam(url);
  }
  return url;
}

WebPreferences WebContents::GetWebkitPrefs() {
  // Initialize web_preferences_ to chrome defaults.
  WebPreferences web_prefs;
  PrefService* prefs = profile()->GetPrefs();

  // TODO(darin): Support overriding this value from prefs, which also
  // involves modifying our URLRequestContext.
  web_prefs.user_agent = webkit_glue::GetDefaultUserAgent();

  web_prefs.fixed_font_family =
      prefs->GetString(prefs::kWebKitFixedFontFamily);
  web_prefs.serif_font_family =
      prefs->GetString(prefs::kWebKitSerifFontFamily);
  web_prefs.sans_serif_font_family =
      prefs->GetString(prefs::kWebKitSansSerifFontFamily);
  if (prefs->GetBoolean(prefs::kWebKitStandardFontIsSerif))
    web_prefs.standard_font_family = web_prefs.serif_font_family;
  else
    web_prefs.standard_font_family = web_prefs.sans_serif_font_family;
  web_prefs.cursive_font_family =
      prefs->GetString(prefs::kWebKitCursiveFontFamily);
  web_prefs.fantasy_font_family =
      prefs->GetString(prefs::kWebKitFantasyFontFamily);

  web_prefs.default_font_size =
      prefs->GetInteger(prefs::kWebKitDefaultFontSize);
  web_prefs.default_fixed_font_size =
      prefs->GetInteger(prefs::kWebKitDefaultFixedFontSize);
  web_prefs.minimum_font_size =
      prefs->GetInteger(prefs::kWebKitMinimumFontSize);
  web_prefs.minimum_logical_font_size =
      prefs->GetInteger(prefs::kWebKitMinimumLogicalFontSize);

  web_prefs.default_encoding = prefs->GetString(prefs::kDefaultCharset);

  web_prefs.javascript_can_open_windows_automatically =
      prefs->GetBoolean(prefs::kWebKitJavascriptCanOpenWindowsAutomatically);
  web_prefs.dom_paste_enabled =
      prefs->GetBoolean(prefs::kWebKitDomPasteEnabled);
  web_prefs.shrinks_standalone_images_to_fit =
      prefs->GetBoolean(prefs::kWebKitShrinksStandaloneImagesToFit);

  {  // Command line switches are used for preferences with no user interface.
    CommandLine command_line;
    web_prefs.developer_extras_enabled =
        !command_line.HasSwitch(switches::kDisableDevTools) &&
        prefs->GetBoolean(prefs::kWebKitDeveloperExtrasEnabled);
    web_prefs.javascript_enabled =
        !command_line.HasSwitch(switches::kDisableJavaScript) &&
        prefs->GetBoolean(prefs::kWebKitJavascriptEnabled);
    web_prefs.plugins_enabled =
        !command_line.HasSwitch(switches::kDisablePlugins) &&
        prefs->GetBoolean(prefs::kWebKitPluginsEnabled);
    web_prefs.java_enabled =
        !command_line.HasSwitch(switches::kDisableJava) &&
        prefs->GetBoolean(prefs::kWebKitJavaEnabled);
    web_prefs.loads_images_automatically =
        !command_line.HasSwitch(switches::kDisableImages) &&
        prefs->GetBoolean(prefs::kWebKitLoadsImagesAutomatically);
  }

  web_prefs.uses_universal_detector =
      prefs->GetBoolean(prefs::kWebKitUsesUniversalDetector);
  web_prefs.text_areas_are_resizable =
      prefs->GetBoolean(prefs::kWebKitTextAreasAreResizable);

  // User CSS is currently disabled because it crashes chrome.  See
  // webkit/glue/webpreferences.h for more details.

  // Make sure we will set the default_encoding with canonical encoding name.
  web_prefs.default_encoding =
      CharacterEncoding::GetCanonicalEncodingNameByAliasName(
          web_prefs.default_encoding);
  if (web_prefs.default_encoding.empty()) {
    prefs->ClearPref(prefs::kDefaultCharset);
    web_prefs.default_encoding = prefs->GetString(
        prefs::kDefaultCharset);
  }
  DCHECK(!web_prefs.default_encoding.empty());
  return web_prefs;
}

void WebContents::OnMissingPluginStatus(int status) {
  GetPluginInstaller()->OnMissingPluginStatus(status);
}

void WebContents::OnCrashedPlugin(const std::wstring& plugin_path) {
  DCHECK(!plugin_path.empty());

  std::wstring plugin_name = plugin_path;
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(plugin_path));
  if (version_info.get()) {
    const std::wstring& product_name = version_info->product_name();
    if (!product_name.empty())
      plugin_name = product_name;
  }

  std::wstring info_bar_message =
      l10n_util::GetStringF(IDS_PLUGIN_CRASHED_PROMPT, plugin_name);

  InfoBarView* view = GetInfoBarView();
  if (-1 == view->GetChildIndex(crashed_plugin_info_bar_)) {
    crashed_plugin_info_bar_ = new InfoBarMessageView(info_bar_message);
    view->AddChildView(crashed_plugin_info_bar_);
  } else {
    crashed_plugin_info_bar_->SetMessageText(info_bar_message);
  }
}

void WebContents::OnJSOutOfMemory() {
  std::wstring info_bar_message =
      l10n_util::GetString(IDS_JS_OUT_OF_MEMORY_PROMPT);

  InfoBarView* view = GetInfoBarView();
  if (-1 == view->GetChildIndex(crashed_plugin_info_bar_)) {
    crashed_plugin_info_bar_ = new InfoBarMessageView(info_bar_message);
    view->AddChildView(crashed_plugin_info_bar_);
  } else {
    crashed_plugin_info_bar_->SetMessageText(info_bar_message);
  }
}

// Returns true if the entry's transition type is FORM_SUBMIT.
static bool IsFormSubmit(const NavigationEntry* entry) {
  DCHECK(entry);
  return (PageTransition::StripQualifier(entry->GetTransitionType()) ==
          PageTransition::FORM_SUBMIT);
}

void WebContents::PageHasOSDD(RenderViewHost* render_view_host,
                              int32 page_id, const GURL& url,
                              bool autodetected) {
  // Make sure page_id is the current page, and the TemplateURLModel is loaded.
  DCHECK(url.is_valid());
  if (!controller() || !IsActiveEntry(page_id))
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

  const NavigationEntry* entry = controller()->GetLastCommittedEntry();
  DCHECK(entry);

  const NavigationEntry* base_entry = entry;
  if (IsFormSubmit(base_entry)) {
    // If the current page is a form submit, find the last page that was not
    // a form submit and use its url to generate the keyword from.
    int index = controller()->GetLastCommittedEntryIndex() - 1;
    while (index >= 0 && IsFormSubmit(controller()->GetEntryAtIndex(index)))
      index--;
    if (index >= 0)
      base_entry = controller()->GetEntryAtIndex(index);
    else
      base_entry = NULL;
  }
  if (!base_entry || !base_entry->GetUserTypedURLOrURL().is_valid())
    return;
  std::wstring keyword =
      TemplateURLModel::GenerateKeyword(base_entry->GetUserTypedURLOrURL(),
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
      base_entry->GetFavIconURL(),
      GetAncestor(GetHWND(), GA_ROOT),
      autodetected);
}

void WebContents::OnDidGetApplicationInfo(
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

void WebContents::OnGearsCreateShortcutDone(
    const GearsShortcutData& shortcut_data, bool success) {
  NavigationEntry* current_entry = controller()->GetLastCommittedEntry();
  bool same_page =
      current_entry && pending_install_.page_id == current_entry->GetPageID();

  if (success && same_page) {
    // Only switch to app mode if the user chose to create a shortcut and
    // we're still on the same page that it corresponded to.
    SetWebApp(new WebApp(profile(), shortcut_data));
    if (delegate())
      delegate()->ConvertContentsToApplication(this);
  }

  // Reset the page id to indicate no requests are pending.
  pending_install_.page_id = 0;
  pending_install_.callback_functor = NULL;
}

void WebContents::UpdateMaxPageIDIfNecessary(SiteInstance* site_instance,
                                             RenderViewHost* rvh) {
  // If we are creating a RVH for a restored controller, then we might
  // have more page IDs than the SiteInstance's current max page ID.  We must
  // make sure that the max page ID is larger than any restored page ID.
  // Note that it is ok for conflicting page IDs to exist in another tab
  // (i.e., NavigationController), but if any page ID is larger than the max,
  // the back/forward list will get confused.
  int max_restored_page_id = controller()->max_restored_page_id();
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

void WebContents::BeforeUnloadFiredFromRenderManager(
    bool proceed, 
    bool* proceed_to_fire_unload) {
  delegate()->BeforeUnloadFired(this, proceed, proceed_to_fire_unload);
}


HWND WebContents::GetContentHWND() {
  if (!view())
    return NULL;
  return view()->GetPluginHWND();
}

bool WebContents::CanDisplayFile(const std::wstring& full_path) {
  bool allow_wildcard = false;
  std::string mime_type;
  net::GetMimeTypeFromFile(full_path, &mime_type);
  if (net::IsSupportedMimeType(mime_type) ||
      (PluginService::GetInstance() &&
       PluginService::GetInstance()->HavePluginFor(mime_type, allow_wildcard)))
    return true;
  return false;
}

void WebContents::PrintPreview() {
  // We can't print interstitial page for now.
  if (render_manager_.showing_interstitial_page())
    return;

  // If we have a FindInPage dialog, notify it that its tab was hidden.
  if (find_in_page_controller_.get())
    find_in_page_controller_->DidBecomeUnselected();

  // We don't show the print preview for the beta, only the print dialog.
  printing_.ShowPrintDialog();
}

bool WebContents::PrintNow() {
  // We can't print interstitial page for now.
  if (render_manager_.showing_interstitial_page())
    return false;

  // If we have a FindInPage dialog, notify it that its tab was hidden.
  if (find_in_page_controller_.get())
    find_in_page_controller_->DidBecomeUnselected();

  return printing_.PrintNow();
}

void WebContents::WillCaptureContents() {
  capturing_contents_ = true;
}

void WebContents::DidCaptureContents() {
  capturing_contents_ = false;
}

void WebContents::Cut() {
  render_view_host()->Cut();
}

void WebContents::Copy() {
  render_view_host()->Copy();
}

void WebContents::Paste() {
   render_view_host()->Paste();
}

void WebContents::SetInitialFocus(bool reverse) {
   render_view_host()->SetInitialFocus(reverse);
}

void WebContents::GenerateKeywordIfNecessary(
    const ViewHostMsg_FrameNavigate_Params& params) {
  DCHECK(controller());
  if (!params.searchable_form_url.is_valid())
    return;

  if (profile()->IsOffTheRecord())
    return;

  const int last_index = controller()->GetLastCommittedEntryIndex();
  // When there was no previous page, the last index will be 0. This is
  // normally due to a form submit that opened in a new tab.
  // TODO(brettw) bug 916126: we should support keywords when form submits
  //              happen in new tabs.
  if (last_index <= 0)
    return;
  const NavigationEntry* previous_entry =
      controller()->GetEntryAtIndex(last_index - 1);
  if (IsFormSubmit(previous_entry)) {
    // Only generate a keyword if the previous page wasn't itself a form
    // submit.
    return;
  }

  std::wstring keyword =
      TemplateURLModel::GenerateKeyword(previous_entry->GetUserTypedURLOrURL(),
                                        true);  // autodetected
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
  DCHECK(controller()->GetLastCommittedEntry());
  const GURL& favicon_url =
      controller()->GetLastCommittedEntry()->GetFavIconURL();
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

void WebContents::InspectElementReply(int num_resources) {
  // We have received reply from inspect element request. Notify the
  // automation provider in case we need to notify automation client.
  NotificationService::current()->
      Notify(NOTIFY_DOM_INSPECT_ELEMENT_RESPONSE, Source<WebContents>(this),
             Details<int>(&num_resources));
}

// The renderer sends back to the browser the key events it did not process.
void WebContents::HandleKeyboardEvent(const WebKeyboardEvent& event) {
  // The renderer returned a keyboard event it did not process. This may be
  // a keyboard shortcut that we have to process.
  if (event.type == WebInputEvent::KEY_DOWN) {
    ChromeViews::FocusManager* focus_manager =
        ChromeViews::FocusManager::GetFocusManager(GetHWND());
    // We may not have a focus_manager at this point (if the tab has been
    // switched by the time this message returned).
    if (focus_manager) {
      ChromeViews::Accelerator accelerator(event.key_code,
          (event.modifiers & WebInputEvent::SHIFT_KEY) ==
              WebInputEvent::SHIFT_KEY,
          (event.modifiers & WebInputEvent::CTRL_KEY) ==
              WebInputEvent::CTRL_KEY,
          (event.modifiers & WebInputEvent::ALT_KEY) ==
              WebInputEvent::ALT_KEY);
      if (focus_manager->ProcessAccelerator(accelerator, false))
        return;
    }
  }

  // Any unhandled keyboard/character messages should be defproced.
  // This allows stuff like Alt+F4, etc to work correctly.
  DefWindowProc(event.actual_message.hwnd,
                event.actual_message.message,
                event.actual_message.wParam,
                event.actual_message.lParam);
}

bool WebContents::CreateRenderViewForRenderManager(
    RenderViewHost* render_view_host) {
  RenderWidgetHostHWND* view = CreatePageView(render_view_host);

  bool ok = render_view_host->CreateRenderView();
  if (ok) {
    CRect client_rect;
    ::GetClientRect(GetHWND(), &client_rect);
    view->SetSize(gfx::Size(client_rect.Width(), client_rect.Height()));
    UpdateMaxPageIDIfNecessary(render_view_host->site_instance(),
                               render_view_host);
  }
  return ok;
}

RenderWidgetHostHWND* WebContents::CreatePageView(
    RenderViewHost* render_view_host) {
  // Create the View as well. Its lifetime matches the child process'.
  DCHECK(!render_view_host->view());
  RenderWidgetHostHWND* view = new RenderWidgetHostHWND(render_view_host);
  render_view_host->set_view(view);
  view->Create(GetHWND());
  view->ShowWindow(SW_SHOW);
  return view;
}

void WebContents::DidGetPrintedPagesCount(int cookie, int number_pages) {
  printing_.DidGetPrintedPagesCount(cookie, number_pages);
}

void WebContents::DidPrintPage(const ViewHostMsg_DidPrintPage_Params& params) {
  printing_.DidPrintPage(params);
}

void WebContents::SetIsLoading(bool is_loading,
                               LoadNotificationDetails* details) {
  if (!is_loading) {
    load_state_ = net::LOAD_STATE_IDLE;
    load_state_host_.clear();
  }

  TabContents::SetIsLoading(is_loading, details);
  render_manager_.SetIsLoading(is_loading);
}

void WebContents::FileSelected(const std::wstring& path, void* params) {
  render_view_host()->FileSelected(path);
}

void WebContents::FileSelectionCanceled(void* params) {
  // If the user cancels choosing a file to upload we need to pass back the
  // empty string.
   render_view_host()->FileSelected(std::wstring());
}

///////////////////////////////////////////////////////////////////////////////

bool WebContents::IsInPageNavigation(const GURL& url) const {
  // We compare to the last committed entry and not the active entry as the
  // active entry is the current pending entry (if any).
  // When this method is called when a navigation initiated from the browser
  // (ex: when typing the URL in the location bar) is committed, the pending
  // entry URL is the same as |url|.
  NavigationEntry* entry = controller()->GetLastCommittedEntry();
  return (entry && url.has_ref() &&
         (url != entry->GetURL()) &&  // Test for reload of a URL with a ref.
          GURLWithoutRef(entry->GetURL()) == GURLWithoutRef(url));
}

SkBitmap WebContents::GetFavIcon() {
  if (web_app_.get() && IsWebApplicationActive()) {
    SkBitmap app_icon = web_app_->GetFavIcon();
    if (!app_icon.isNull())
      return app_icon;
  }
  return TabContents::GetFavIcon();
}

std::wstring WebContents::GetStatusText() const {
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
  }

  return std::wstring();
}

// Called by PluginInstaller to start installation of missing plugin.
void WebContents::InstallMissingPlugin() {
  render_view_host()->InstallMissingPlugin();
}

void WebContents::GetAllSavableResourceLinksForCurrentPage(
    const GURL& page_url) {
  render_view_host()->GetAllSavableResourceLinksForCurrentPage(
      page_url);
}

void WebContents::OnReceivedSavableResourceLinksForCurrentPage(
    const std::vector<GURL>& resources_list,
    const std::vector<GURL>& referrers_list,
    const std::vector<GURL>& frames_list) {
  SavePackage* save_package = get_save_package();
  if (save_package) {
    save_package->ProcessCurrentPageAllSavableResourceLinks(resources_list,
                                                            referrers_list,
                                                            frames_list);
  }
}

void WebContents::GetSerializedHtmlDataForCurrentPageWithLocalLinks(
    const std::vector<std::wstring>& links,
    const std::vector<std::wstring>& local_paths,
    const std::wstring& local_directory_name) {
  render_view_host()->GetSerializedHtmlDataForCurrentPageWithLocalLinks(
      links, local_paths, local_directory_name);
}


void WebContents::OnReceivedSerializedHtmlData(const GURL& frame_url,
                                               const std::string& data,
                                               int32 status) {
  SavePackage* save_package = get_save_package();
  if (save_package)
    save_package->ProcessSerializedHtmlData(frame_url,
                                            data,
                                            status);
}

bool WebContents::CanBlur() const {
  return delegate() ? delegate()->CanBlur() : true;
}

void WebContents::RendererUnresponsive(RenderViewHost* rvh) {
  if (render_view_host() && render_view_host()->IsRenderViewLive())
    HungRendererWarning::ShowForWebContents(this);
}

void WebContents::RendererResponsive(RenderViewHost* render_view_host) {
  HungRendererWarning::HideForWebContents(this);
}

void WebContents::LoadStateChanged(const GURL& url,
                                   net::LoadState load_state) {
  load_state_ = load_state;
  load_state_host_ = UTF8ToWide(url.host());
  if (load_state_ == net::LOAD_STATE_READING_RESPONSE)
    response_started_ = false;
  if (is_loading())
    NotifyNavigationStateChanged(INVALIDATE_LOAD);
}

void WebContents::DetachPluginWindows() {
  EnumChildWindows(GetHWND(), WebContents::EnumPluginWindowsCallback, NULL);
}

BOOL WebContents::EnumPluginWindowsCallback(HWND window, LPARAM) {
  if (WebPluginDelegateImpl::IsPluginDelegateWindow(window)) {
    ::ShowWindow(window, SW_HIDE);
    SetParent(window, NULL);
  }

  return TRUE;
}
