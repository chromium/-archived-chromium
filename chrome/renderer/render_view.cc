// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/render_view.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/gfx/gdi_util.h"
#include "base/gfx/native_theme.h"
#include "base/gfx/png_encoder.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/gfx/emf.h"
#include "chrome/common/gfx/favicon_size.h"
#include "chrome/common/gfx/color_utils.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/page_zoom.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/renderer/about_handler.h"
#include "chrome/renderer/chrome_plugin_host.h"
#include "chrome/renderer/debug_message_handler.h"
#include "chrome/renderer/localized_error.h"
#include "chrome/renderer/renderer_resources.h"
#include "chrome/renderer/user_script_slave.h"
#include "chrome/renderer/visitedlink_slave.h"
#include "chrome/renderer/webmediaplayer_delegate_impl.h"
#include "chrome/renderer/webplugin_delegate_proxy.h"
#include "chrome/views/message_box_view.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "skia/ext/bitmap_platform_device.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/vector_canvas.h"
#include "webkit/default_plugin/default_plugin_shared.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/dom_serializer.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/searchable_form_data.h"
#include "webkit/glue/webdatasource.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/weberror.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webhistoryitem.h"
#include "webkit/glue/webinputevent.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webresponse.h"
#include "webkit/glue/weburlrequest.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"
//#include "webkit/port/platform/graphics/PlatformContextSkia.h"

#include "generated_resources.h"

using base::TimeDelta;

//-----------------------------------------------------------------------------

// define to write the time necessary for thumbnail/DOM text retrieval,
// respectively, into the system debug log
// #define TIME_BITMAP_RETRIEVAL
// #define TIME_TEXT_RETRIEVAL

// maximum number of characters in the document to index, any text beyond this
// point will be clipped
static const int kMaxIndexChars = 65535;

// Size of the thumbnails that we'll generate
static const int kThumbnailWidth = 196;
static const int kThumbnailHeight = 136;

// Delay in milliseconds that we'll wait before capturing the page contents
// and thumbnail.
static const int kDelayForCaptureMs = 500;

// Typically, we capture the page data once the page is loaded.
// Sometimes, the page never finishes to load, preventing the page capture
// To workaround this problem, we always perform a capture after the following
// delay.
static const int kDelayForForcedCaptureMs = 6000;

// The default value for RenderView.delay_seconds_for_form_state_sync_, see
// that variable for more.
const int kDefaultDelaySecondsForFormStateSync = 5;

// The next available page ID to use. This ensures that the page IDs are
// globally unique in the renderer.
static int32 next_page_id_ = 1;

// The maximum number of popups that can be spawned from one page.
static const int kMaximumNumberOfUnacknowledgedPopups = 25;

static const char* const kUnreachableWebDataURL =
    "chrome://chromewebdata/";

static const char* const kBackForwardNavigationScheme = "history";

namespace {

// Associated with browser-initiated navigations to hold tracking data.
class RenderViewExtraRequestData : public WebRequest::ExtraData {
 public:
  RenderViewExtraRequestData(int32 pending_page_id,
                             PageTransition::Type transition,
                             const GURL& url)
      : pending_page_id_(pending_page_id),
        transition_type(transition),
        request_committed(false) {
  }

  // Contains the page_id for this navigation or -1 if there is none yet.
  int32 pending_page_id() const { return pending_page_id_; }

  // Is this a new navigation?
  bool is_new_navigation() const { return pending_page_id_ == -1; }

  // Contains the transition type that the browser specified when it
  // initiated the load.
  PageTransition::Type transition_type;

  // True if we have already processed the "DidCommitLoad" event for this
  // request.  Used by session history.
  bool request_committed;

 private:
  int32 pending_page_id_;

  DISALLOW_EVIL_CONSTRUCTORS(RenderViewExtraRequestData);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////

RenderView::RenderView(RenderThreadBase* render_thread)
    : RenderWidget(render_thread, true),
      enable_dom_automation_(false),
      enable_dom_ui_bindings_(false),
      enable_external_host_bindings_(false),
      target_url_status_(TARGET_NONE),
      is_loading_(false),
      navigation_gesture_(NavigationGestureUnknown),
      page_id_(-1),
      last_page_id_sent_to_browser_(-1),
      last_indexed_page_id_(-1),
      opened_by_user_gesture_(true),
      method_factory_(this),
      first_default_plugin_(NULL),
      printed_document_width_(0),
      history_back_list_count_(0),
      history_forward_list_count_(0),
      disable_popup_blocking_(false),
      has_unload_listener_(false),
      decrement_shared_popup_at_destruction_(false),
      user_scripts_enabled_(false),
      waiting_for_create_window_ack_(false),
      form_field_autofill_request_id_(0),
      popup_notification_visible_(false),
      delay_seconds_for_form_state_sync_(kDefaultDelaySecondsForFormStateSync) {
  resource_dispatcher_ = new ResourceDispatcher(this);
#ifdef CHROME_PERSONALIZATION
  personalization_ = Personalization::CreateRendererPersonalization();
#endif
}

RenderView::~RenderView() {
  if (decrement_shared_popup_at_destruction_)
    shared_popup_counter_->data--;

  resource_dispatcher_->ClearMessageSender();
  // Clear any back-pointers that might still be held by plugins.
  PluginDelegateList::iterator it = plugin_delegates_.begin();
  while (it != plugin_delegates_.end()) {
    (*it)->DropRenderView();
    it = plugin_delegates_.erase(it);
  }

  render_thread_->RemoveFilter(debug_message_handler_);

#ifdef CHROME_PERSONALIZATION
  Personalization::CleanupRendererPersonalization(personalization_);
  personalization_ = NULL;
#endif
}

/*static*/
RenderView* RenderView::Create(
    RenderThreadBase* render_thread,
    HWND parent_hwnd,
    base::WaitableEvent* modal_dialog_event,
    int32 opener_id,
    const WebPreferences& webkit_prefs,
    SharedRenderViewCounter* counter,
    int32 routing_id) {
  DCHECK(routing_id != MSG_ROUTING_NONE);
  scoped_refptr<RenderView> view = new RenderView(render_thread);
  view->Init(parent_hwnd,
             modal_dialog_event,
             opener_id,
             webkit_prefs,
             counter,
             routing_id);  // adds reference
  return view;
}

/*static*/
void RenderView::SetNextPageID(int32 next_page_id) {
  // This method should only be called during process startup, and the given
  // page id had better not exceed our current next page id!
  DCHECK(next_page_id_ == 1);
  DCHECK(next_page_id >= next_page_id_);
  next_page_id_ = next_page_id;
}

void RenderView::PluginDestroyed(WebPluginDelegateProxy* proxy) {
  PluginDelegateList::iterator it =
      std::find(plugin_delegates_.begin(), plugin_delegates_.end(), proxy);
  DCHECK(it != plugin_delegates_.end());
  plugin_delegates_.erase(it);
  // If the plugin is deleted, we need to clear our reference in case user
  // clicks the info bar to install. Unfortunately we are getting
  // PluginDestroyed in single process mode. However, that is not a huge
  // concern.
  if (proxy == first_default_plugin_)
    first_default_plugin_ = NULL;
}

void RenderView::PluginCrashed(const FilePath& plugin_path) {
  Send(new ViewHostMsg_CrashedPlugin(routing_id_, plugin_path));
}


void RenderView::JSOutOfMemory() {
  Send(new ViewHostMsg_JSOutOfMemory(routing_id_));
}

void RenderView::Init(HWND parent_hwnd,
                      base::WaitableEvent* modal_dialog_event,
                      int32 opener_id,
                      const WebPreferences& webkit_prefs,
                      SharedRenderViewCounter* counter,
                      int32 routing_id) {
  DCHECK(!webview());

  if (opener_id != MSG_ROUTING_NONE)
    opener_id_ = opener_id;

  if (counter) {
    shared_popup_counter_ = counter;
    shared_popup_counter_->data++;
    decrement_shared_popup_at_destruction_ = true;
  } else {
    shared_popup_counter_ = new SharedRenderViewCounter(0);
    decrement_shared_popup_at_destruction_ = false;
  }

  // Avoid a leak here by not assigning, since WebView::Create addrefs for us.
  WebWidget* view = WebView::Create(this, webkit_prefs);
  webwidget_.swap(&view);

  // Don't let WebCore keep a B/F list - we have our own.
  // We let it keep 1 entry because FrameLoader::goToItem expects an item in the
  // backForwardList, which is used only in ASSERTs.
  webview()->SetBackForwardListSize(1);

  routing_id_ = routing_id;
  render_thread_->AddRoute(routing_id_, this);
  // Take a reference on behalf of the RenderThread.  This will be balanced
  // when we receive ViewMsg_Close.
  AddRef();

  // If this is a popup, we must wait for the CreatingNew_ACK message before
  // completing initialization.  Otherwise, we can finish it now.
  if (opener_id == MSG_ROUTING_NONE) {
    did_show_ = true;
    CompleteInit(parent_hwnd);
  }

  host_window_ = parent_hwnd;
  modal_dialog_event_.reset(modal_dialog_event);

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  enable_dom_automation_ =
      command_line.HasSwitch(switches::kDomAutomationController);
  disable_popup_blocking_ =
      command_line.HasSwitch(switches::kDisablePopupBlocking);
  user_scripts_enabled_ =
      command_line.HasSwitch(switches::kEnableUserScripts);

  debug_message_handler_ = new DebugMessageHandler(this);
  render_thread_->AddFilter(debug_message_handler_);
}

void RenderView::OnMessageReceived(const IPC::Message& message) {
  // If the current RenderView instance represents a popup, then we
  // need to wait for ViewMsg_CreatingNew_ACK to be sent by the browser.
  // As part of this ack we also receive the browser window handle, which
  // parents any plugins instantiated in this RenderView instance.
  // Plugins can be instantiated only when we receive the parent window
  // handle as they are child windows.
  if (waiting_for_create_window_ack_ && 
      resource_dispatcher_->IsResourceMessage(message)) {
    queued_resource_messages_.push(new IPC::Message(message));
    return;
  }

  // Let the resource dispatcher intercept resource messages first.
  if (resource_dispatcher_->OnMessageReceived(message))
    return;

  IPC_BEGIN_MESSAGE_MAP(RenderView, message)
    IPC_MESSAGE_HANDLER(ViewMsg_CreatingNew_ACK, OnCreatingNewAck)
    IPC_MESSAGE_HANDLER(ViewMsg_CaptureThumbnail, SendThumbnail)
    IPC_MESSAGE_HANDLER(ViewMsg_GetPrintedPagesCount, OnGetPrintedPagesCount)
    IPC_MESSAGE_HANDLER(ViewMsg_PrintPages, OnPrintPages)
    IPC_MESSAGE_HANDLER(ViewMsg_Navigate, OnNavigate)
    IPC_MESSAGE_HANDLER(ViewMsg_Stop, OnStop)
    IPC_MESSAGE_HANDLER(ViewMsg_LoadAlternateHTMLText, OnLoadAlternateHTMLText)
    IPC_MESSAGE_HANDLER(ViewMsg_StopFinding, OnStopFinding)
    IPC_MESSAGE_HANDLER(ViewMsg_Undo, OnUndo)
    IPC_MESSAGE_HANDLER(ViewMsg_Redo, OnRedo)
    IPC_MESSAGE_HANDLER(ViewMsg_Cut, OnCut)
    IPC_MESSAGE_HANDLER(ViewMsg_Copy, OnCopy)
    IPC_MESSAGE_HANDLER(ViewMsg_Paste, OnPaste)
    IPC_MESSAGE_HANDLER(ViewMsg_Replace, OnReplace)
    IPC_MESSAGE_HANDLER(ViewMsg_ToggleSpellCheck, OnToggleSpellCheck)
    IPC_MESSAGE_HANDLER(ViewMsg_Delete, OnDelete)
    IPC_MESSAGE_HANDLER(ViewMsg_SelectAll, OnSelectAll)
    IPC_MESSAGE_HANDLER(ViewMsg_CopyImageAt, OnCopyImageAt)
    IPC_MESSAGE_HANDLER(ViewMsg_Find, OnFind)
    IPC_MESSAGE_HANDLER(ViewMsg_Zoom, OnZoom)
    IPC_MESSAGE_HANDLER(ViewMsg_SetPageEncoding, OnSetPageEncoding)
    IPC_MESSAGE_HANDLER(ViewMsg_InspectElement, OnInspectElement)
    IPC_MESSAGE_HANDLER(ViewMsg_ShowJavaScriptConsole, OnShowJavaScriptConsole)
    IPC_MESSAGE_HANDLER(ViewMsg_DownloadImage, OnDownloadImage)
    IPC_MESSAGE_HANDLER(ViewMsg_ScriptEvalRequest, OnScriptEvalRequest)
    IPC_MESSAGE_HANDLER(ViewMsg_AddMessageToConsole, OnAddMessageToConsole)
    IPC_MESSAGE_HANDLER(ViewMsg_DebugAttach, OnDebugAttach)
    IPC_MESSAGE_HANDLER(ViewMsg_DebugDetach, OnDebugDetach)
    IPC_MESSAGE_HANDLER(ViewMsg_ReservePageIDRange, OnReservePageIDRange)
    IPC_MESSAGE_HANDLER(ViewMsg_UploadFile, OnUploadFileRequest)
    IPC_MESSAGE_HANDLER(ViewMsg_FormFill, OnFormFill)
    IPC_MESSAGE_HANDLER(ViewMsg_FillPasswordForm, OnFillPasswordForm)
    IPC_MESSAGE_HANDLER(ViewMsg_DragTargetDragEnter, OnDragTargetDragEnter)
    IPC_MESSAGE_HANDLER(ViewMsg_DragTargetDragOver, OnDragTargetDragOver)
    IPC_MESSAGE_HANDLER(ViewMsg_DragTargetDragLeave, OnDragTargetDragLeave)
    IPC_MESSAGE_HANDLER(ViewMsg_DragTargetDrop, OnDragTargetDrop)
    IPC_MESSAGE_HANDLER(ViewMsg_AllowDomAutomationBindings,
                        OnAllowDomAutomationBindings)
    IPC_MESSAGE_HANDLER(ViewMsg_AllowBindings, OnAllowBindings)
    IPC_MESSAGE_HANDLER(ViewMsg_SetDOMUIProperty, OnSetDOMUIProperty)
    IPC_MESSAGE_HANDLER(ViewMsg_DragSourceEndedOrMoved,
                        OnDragSourceEndedOrMoved)
    IPC_MESSAGE_HANDLER(ViewMsg_DragSourceSystemDragEnded,
                        OnDragSourceSystemDragEnded)
    IPC_MESSAGE_HANDLER(ViewMsg_SetInitialFocus, OnSetInitialFocus)
    IPC_MESSAGE_HANDLER(ViewMsg_FindReplyACK, OnFindReplyAck)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateTargetURL_ACK, OnUpdateTargetURLAck)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateWebPreferences, OnUpdateWebPreferences)
    IPC_MESSAGE_HANDLER(ViewMsg_SetAltErrorPageURL, OnSetAltErrorPageURL)
    IPC_MESSAGE_HANDLER(ViewMsg_InstallMissingPlugin, OnInstallMissingPlugin)
    IPC_MESSAGE_HANDLER(ViewMsg_RunFileChooserResponse, OnFileChooserResponse)
    IPC_MESSAGE_HANDLER(ViewMsg_EnableViewSourceMode, OnEnableViewSourceMode)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateBackForwardListCount,
                        OnUpdateBackForwardListCount)
    IPC_MESSAGE_HANDLER(ViewMsg_GetAllSavableResourceLinksForCurrentPage,
                        OnGetAllSavableResourceLinksForCurrentPage)
    IPC_MESSAGE_HANDLER(ViewMsg_GetSerializedHtmlDataForCurrentPageWithLocalLinks,
                        OnGetSerializedHtmlDataForCurrentPageWithLocalLinks)
    IPC_MESSAGE_HANDLER(ViewMsg_GetApplicationInfo, OnGetApplicationInfo)
    IPC_MESSAGE_HANDLER(ViewMsg_GetAccessibilityInfo, OnGetAccessibilityInfo)
    IPC_MESSAGE_HANDLER(ViewMsg_ClearAccessibilityInfo,
                        OnClearAccessibilityInfo)
    IPC_MESSAGE_HANDLER(ViewMsg_ShouldClose, OnMsgShouldClose)
    IPC_MESSAGE_HANDLER(ViewMsg_ClosePage, OnClosePage)
    IPC_MESSAGE_HANDLER(ViewMsg_ThemeChanged, OnThemeChanged)
#ifdef CHROME_PERSONALIZATION
    IPC_MESSAGE_HANDLER(ViewMsg_PersonalizationEvent, OnPersonalizationEvent)
#endif
    IPC_MESSAGE_HANDLER(ViewMsg_HandleMessageFromExternalHost,
                        OnMessageFromExternalHost)
    IPC_MESSAGE_HANDLER(ViewMsg_DisassociateFromPopupCount,
                        OnDisassociateFromPopupCount)
    IPC_MESSAGE_HANDLER(ViewMsg_AutofillSuggestions,
                        OnReceivedAutofillSuggestions)
    IPC_MESSAGE_HANDLER(ViewMsg_PopupNotificationVisiblityChanged,
                        OnPopupNotificationVisiblityChanged)

    // Have the super handle all other messages.
    IPC_MESSAGE_UNHANDLED(RenderWidget::OnMessageReceived(message))
  IPC_END_MESSAGE_MAP()
}

// Got a response from the browser after the renderer decided to create a new
// view.
void RenderView::OnCreatingNewAck(HWND parent) {
  CompleteInit(parent);

  waiting_for_create_window_ack_ = false;

  while (!queued_resource_messages_.empty()) {
    IPC::Message* queued_msg = queued_resource_messages_.front();
    queued_resource_messages_.pop();
    resource_dispatcher_->OnMessageReceived(*queued_msg);
    delete queued_msg;
  }
}

void RenderView::SendThumbnail() {
  WebFrame* main_frame = webview()->GetMainFrame();
  if (!main_frame)
    return;

  // get the URL for this page
  GURL url(main_frame->GetURL());
  if (url.is_empty())
    return;

  if (size_.IsEmpty())
    return;  // Don't create an empty thumbnail!

  ThumbnailScore score;
  SkBitmap thumbnail;
  if (!CaptureThumbnail(main_frame, kThumbnailWidth, kThumbnailHeight,
                        &thumbnail, &score))
    return;

  // send the thumbnail message to the browser process
  IPC::Message* thumbnail_msg = new IPC::Message(routing_id_,
      ViewHostMsg_Thumbnail::ID, IPC::Message::PRIORITY_NORMAL);
  IPC::ParamTraits<GURL>::Write(thumbnail_msg, url);
  IPC::ParamTraits<ThumbnailScore>::Write(thumbnail_msg, score);
  IPC::ParamTraits<SkBitmap>::Write(thumbnail_msg, thumbnail);
  Send(thumbnail_msg);
}

int RenderView::SwitchFrameToPrintMediaType(const ViewMsg_Print_Params& params,
                                            WebFrame* frame) {
  float ratio = static_cast<float>(params.desired_dpi / params.dpi);
  float paper_width = params.printable_size.width() * ratio;
  float paper_height = params.printable_size.height() * ratio;
  float minLayoutWidth = static_cast<float>(paper_width * params.min_shrink);
  float maxLayoutWidth = static_cast<float>(paper_width * params.max_shrink);

  // Safari uses: 765 & 1224. Margins aren't exactly the same either.
  // Scale = 2.222 for MDI printer.
  int pages;
  if (!frame->SetPrintingMode(true,
                              minLayoutWidth,
                              maxLayoutWidth,
                              &printed_document_width_)) {
    NOTREACHED();
    pages = 0;
  } else {
    DCHECK_GT(printed_document_width_, 0);
    // Force to recalculate the height, otherwise it reuse the current window
    // height as the default.
    float effective_shrink = printed_document_width_ / paper_width;
    gfx::Size page_size(printed_document_width_,
                        static_cast<int>(paper_height * effective_shrink) - 1);
    WebView* view = frame->GetView();
    if (view) {
      // Hack around an issue where if the current view height is higher than
      // the page height, empty pages will be printed even if the bottom of the
      // web page is empty.
      printing_view_size_ = view->GetSize();
      view->Resize(page_size);
      view->Layout();
    }
    pages = frame->ComputePageRects(params.printable_size);
    DCHECK(pages);
  }
  return pages;
}

void RenderView::SwitchFrameToDisplayMediaType(WebFrame* frame) {
  // Set the layout back to "normal" document; i.e. CSS media type = "screen".
  frame->SetPrintingMode(false, 0, 0, NULL);
  WebView* view = frame->GetView();
  if (view) {
    // Restore from the hack described at SwitchFrameToPrintMediaType().
    view->Resize(printing_view_size_);
    view->Layout();
    printing_view_size_.SetSize(0, 0);
  }
  printed_document_width_ = 0;
}

void RenderView::OnPrintPage(const ViewMsg_PrintPage_Params& params) {
  DCHECK(webview());
  if (webview())
    PrintPage(params, webview()->GetMainFrame());
}

void RenderView::PrintPage(const ViewMsg_PrintPage_Params& params,
                           WebFrame* frame) {
  if (printed_document_width_ <= 0) {
    NOTREACHED();
    return;
  }

  // Generate a memory-based EMF file. The EMF will use the current screen's
  // DPI.
  gfx::Emf emf;

  emf.CreateDc(NULL, NULL);
  HDC hdc = emf.hdc();
  DCHECK(hdc);
  skia::PlatformDeviceWin::InitializeDC(hdc);

  gfx::Rect rect;
  frame->GetPageRect(params.page_number, &rect);
  DCHECK(rect.height());
  DCHECK(rect.width());
  double shrink = static_cast<double>(printed_document_width_) /
                  params.params.printable_size.width();
  // This check would fire each time the page would get truncated on the
  // right. This is not worth a DCHECK() but should be looked into, for
  // example, wouldn't be worth trying in landscape?
  // DCHECK_LE(rect.width(), printed_document_width_);

  // Buffer one page at a time.
  int src_size_x = printed_document_width_;
  int src_size_y =
      static_cast<int>(ceil(params.params.printable_size.height() *
                            shrink));
#if 0
  // TODO(maruel): This code is kept for testing until the 100% GDI drawing
  // code is stable. maruels use this code's output as a reference when the
  // GDI drawing code fails.

  // Mix of Skia and GDI based.
  skia::PlatformCanvasWin canvas(src_size_x, src_size_y, true);
  canvas.drawARGB(255, 255, 255, 255, SkPorterDuff::kSrc_Mode);
  PlatformContextSkia context(&canvas);
  if (!frame->SpoolPage(params.page_number, &context)) {
    NOTREACHED() << "Printing page " << params.page_number << " failed.";
    return;
  }

  // Create a BMP v4 header that we can serialize.
  BITMAPV4HEADER bitmap_header;
  gfx::CreateBitmapV4Header(src_size_x, src_size_y, &bitmap_header);
  const SkBitmap& src_bmp = canvas.getDevice()->accessBitmap(true);
  SkAutoLockPixels src_lock(src_bmp);
  int retval = StretchDIBits(hdc,
                             0,
                             0,
                             src_size_x, src_size_y,
                             0, 0,
                             src_size_x, src_size_y,
                             src_bmp.getPixels(),
                             reinterpret_cast<BITMAPINFO*>(&bitmap_header),
                             DIB_RGB_COLORS,
                             SRCCOPY);
  DCHECK(retval != GDI_ERROR);
#else
  // 100% GDI based.
  skia::VectorCanvas canvas(hdc, src_size_x, src_size_y);
  // Set the clipping region to be sure to not overflow.
  SkRect clip_rect;
  clip_rect.set(0, 0, SkIntToScalar(src_size_x), SkIntToScalar(src_size_y));
  canvas.clipRect(clip_rect);
  if (!frame->SpoolPage(params.page_number, &canvas)) {
    NOTREACHED() << "Printing page " << params.page_number << " failed.";
    return;
  }
#endif

  // Done printing. Close the device context to retrieve the compiled EMF.
  if (!emf.CloseDc()) {
    NOTREACHED() << "EMF failed";
  }

  // Get the size of the compiled EMF.
  unsigned buf_size = emf.GetDataSize();
  DCHECK(buf_size > 128);
  ViewHostMsg_DidPrintPage_Params page_params;
  page_params.data_size = 0;
  page_params.emf_data_handle = NULL;
  page_params.page_number = params.page_number;
  page_params.document_cookie = params.params.document_cookie;
  page_params.actual_shrink = shrink;
  base::SharedMemory shared_buf;

  // http://msdn2.microsoft.com/en-us/library/ms535522.aspx
  // Windows 2000/XP: When a page in a spooled file exceeds approximately 350
  // MB, it can fail to print and not send an error message.
  if (buf_size < 350*1024*1024) {
    // Allocate a shared memory buffer to hold the generated EMF data.
    if (shared_buf.Create(L"", false, false, buf_size) &&
        shared_buf.Map(buf_size)) {
      // Copy the bits into shared memory.
      if (emf.GetData(shared_buf.memory(), buf_size)) {
        page_params.emf_data_handle = shared_buf.handle();
        page_params.data_size = buf_size;
      } else {
        NOTREACHED() << "GetData() failed";
      }
      shared_buf.Unmap();
    } else {
      NOTREACHED() << "Buffer allocation failed";
    }
  } else {
    NOTREACHED() << "Buffer too large: " << buf_size;
  }
  emf.CloseEmf();
  if (Send(new ViewHostMsg_DuplicateSection(routing_id_,
                                            page_params.emf_data_handle,
                                            &page_params.emf_data_handle))) {
    Send(new ViewHostMsg_DidPrintPage(routing_id_, page_params));
  }
}

void RenderView::OnGetPrintedPagesCount(const ViewMsg_Print_Params& params) {
  DCHECK(webview());
  if (!webview()) {
    Send(new ViewHostMsg_DidGetPrintedPagesCount(routing_id_,
                                                 params.document_cookie,
                                                 0));
    return;
  }
  WebFrame* frame = webview()->GetMainFrame();
  int expected_pages = SwitchFrameToPrintMediaType(params, frame);
  Send(new ViewHostMsg_DidGetPrintedPagesCount(routing_id_,
                                               params.document_cookie,
                                               expected_pages));
  SwitchFrameToDisplayMediaType(frame);
}

void RenderView::OnPrintPages(const ViewMsg_PrintPages_Params& params) {
  DCHECK(webview());
  if (webview())
    PrintPages(params, webview()->GetMainFrame());
}

void RenderView::PrintPages(const ViewMsg_PrintPages_Params& params,
                            WebFrame* frame) {
  int pages = SwitchFrameToPrintMediaType(params.params, frame);
  Send(new ViewHostMsg_DidGetPrintedPagesCount(routing_id_,
                                               params.params.document_cookie,
                                               pages));
  if (pages) {
    ViewMsg_PrintPage_Params page_params;
    page_params.params = params.params;
    if (params.pages.empty()) {
      for (int i = 0; i < pages; ++i) {
        page_params.page_number = i;
        PrintPage(page_params, frame);
      }
    } else {
      for (size_t i = 0; i < params.pages.size(); ++i) {
        page_params.page_number = params.pages[i];
        PrintPage(page_params, frame);
      }
    }
  }
  SwitchFrameToDisplayMediaType(frame);
}

void RenderView::CapturePageInfo(int load_id, bool preliminary_capture) {
  if (load_id != page_id_)
    return;  // this capture call is no longer relevant due to navigation
  if (load_id == last_indexed_page_id_)
    return;  // we already indexed this page

  if (!webview())
    return;

  WebFrame* main_frame = webview()->GetMainFrame();
  if (!main_frame)
    return;

  // Don't index/capture pages that are in view source mode.
  if (main_frame->GetInViewSourceMode())
    return;

  // Don't index/capture pages that failed to load.  This only checks the top
  // level frame so the thumbnail may contain a frame that failed to load.
  WebDataSource* ds = main_frame->GetDataSource();
  if (ds && ds->HasUnreachableURL())
    return;

  if (!preliminary_capture)
    last_indexed_page_id_ = load_id;

  // get the URL for this page
  GURL url(main_frame->GetURL());
  if (url.is_empty())
    return;

  // full text
  std::wstring contents;
  CaptureText(main_frame, &contents);
  if (contents.size()) {
    // Send the text to the browser for indexing.
    Send(new ViewHostMsg_PageContents(url, load_id, contents));
  }

  // thumbnail
  SendThumbnail();
}

void RenderView::CaptureText(WebFrame* frame, std::wstring* contents) {
  contents->clear();
  if (!frame)
    return;

  // Don't index any https pages. People generally don't want their bank
  // accounts, etc. indexed on their computer, especially since some of these
  // things are not marked cachable.
  // TODO(brettw) we may want to consider more elaborate heuristics such as
  // the cachability of the page. We may also want to consider subframes (this
  // test will still index subframes if the subframe is SSL).
  if (frame->GetURL().SchemeIsSecure())
    return;

#ifdef TIME_TEXT_RETRIEVAL
  double begin = time_util::GetHighResolutionTimeNow();
#endif

  // get the contents of the frame
  frame->GetContentAsPlainText(kMaxIndexChars, contents);

#ifdef TIME_TEXT_RETRIEVAL
  double end = time_util::GetHighResolutionTimeNow();
  char buf[128];
  sprintf_s(buf, "%d chars retrieved for indexing in %gms\n",
            contents.size(), (end - begin)*1000);
  OutputDebugStringA(buf);
#endif

  // When the contents are clipped to the maximum, we don't want to have a
  // partial word indexed at the end that might have been clipped. Therefore,
  // terminate the string at the last space to ensure no words are clipped.
  if (contents->size() == kMaxIndexChars) {
    size_t last_space_index = contents->find_last_of(kWhitespaceWide);
    if (last_space_index == std::wstring::npos)
      return;  // don't index if we got a huge block of text with no spaces
    contents->resize(last_space_index);
  }
}

bool RenderView::CaptureThumbnail(WebFrame* frame,
                                  int w,
                                  int h,
                                  SkBitmap* thumbnail,
                                  ThumbnailScore* score) {
#ifdef TIME_BITMAP_RETRIEVAL
  double begin = time_util::GetHighResolutionTimeNow();
#endif

  scoped_ptr<skia::BitmapPlatformDevice> device;
  if (!frame->CaptureImage(&device, true))
    return false;

  const SkBitmap& src_bmp = device->accessBitmap(false);

  SkRect dest_rect;
  dest_rect.set(0, 0, SkIntToScalar(w), SkIntToScalar(h));
  float dest_aspect = dest_rect.width() / dest_rect.height();

  // Get the src rect so that we can preserve the aspect ratio while filling
  // the destination.
  SkIRect src_rect;
  if (src_bmp.width() < dest_rect.width() ||
      src_bmp.height() < dest_rect.height()) {
    // Source image is smaller: we clip the part of source image within the
    // dest rect, and then stretch it to fill the dest rect. We don't respect
    // the aspect ratio in this case.
    src_rect.set(0, 0, static_cast<S16CPU>(dest_rect.width()),
                 static_cast<S16CPU>(dest_rect.height()));
    score->good_clipping = false;
  } else {
    float src_aspect = static_cast<float>(src_bmp.width()) / src_bmp.height();
    if (src_aspect > dest_aspect) {
      // Wider than tall, clip horizontally: we center the smaller thumbnail in
      // the wider screen.
      S16CPU new_width = static_cast<S16CPU>(src_bmp.height() * dest_aspect);
      S16CPU x_offset = (src_bmp.width() - new_width) / 2;
      src_rect.set(x_offset, 0, new_width + x_offset, src_bmp.height());
      score->good_clipping = false;
    } else {
      src_rect.set(0, 0, src_bmp.width(),
                   static_cast<S16CPU>(src_bmp.width() / dest_aspect));
      score->good_clipping = true;
    }
  }

  score->at_top = (frame->ScrollOffset().height() == 0);

  SkBitmap subset;
  device->accessBitmap(false).extractSubset(&subset, src_rect);

  // Resample the subset that we want to get it the right size.
  *thumbnail = skia::ImageOperations::Resize(
      subset, skia::ImageOperations::RESIZE_LANCZOS3, w, h);

  score->boring_score = CalculateBoringScore(thumbnail);

#ifdef TIME_BITMAP_RETRIEVAL
  double end = time_util::GetHighResolutionTimeNow();
  char buf[128];
  sprintf_s(buf, "thumbnail in %gms\n", (end - begin) * 1000);
  OutputDebugStringA(buf);
#endif
  return true;
}

double RenderView::CalculateBoringScore(SkBitmap* bitmap) {
  int histogram[256] = {0};
  color_utils::BuildLumaHistogram(bitmap, histogram);

  int color_count = *std::max_element(histogram, histogram + 256);
  int pixel_count = bitmap->width() * bitmap->height();
  return static_cast<double>(color_count) / pixel_count;
}

void RenderView::OnNavigate(const ViewMsg_Navigate_Params& params) {
  if (!webview())
    return;

  AboutHandler::MaybeHandle(params.url);

  bool is_reload = params.reload;

  WebFrame* main_frame = webview()->GetMainFrame();
  if (is_reload && !main_frame->HasCurrentHistoryState()) {
    // We cannot reload if we do not have any history state.  This happens, for
    // example, when recovering from a crash.  Our workaround here is a bit of
    // a hack since it means that reload after a crashed tab does not cause an
    // end-to-end cache validation.
    is_reload = false;
  }

  WebRequestCachePolicy cache_policy;
  if (is_reload) {
    cache_policy = WebRequestReloadIgnoringCacheData;
  } else if (params.page_id != -1 || main_frame->GetInViewSourceMode()) {
    cache_policy = WebRequestReturnCacheDataElseLoad;
  } else {
    cache_policy = WebRequestUseProtocolCachePolicy;
  }

  scoped_ptr<WebRequest> request(WebRequest::Create(params.url));
  request->SetCachePolicy(cache_policy);
  request->SetExtraData(new RenderViewExtraRequestData(
      params.page_id, params.transition, params.url));

  // If we are reloading, then WebKit will use the state of the current page.
  // Otherwise, we give it the state to navigate to.
  if (!is_reload)
    request->SetHistoryState(params.state);

  if (params.referrer.is_valid()) {
    request->SetHttpHeaderValue("Referer",
                                params.referrer.spec());
  }

  main_frame->LoadRequest(request.get());
}

// Stop loading the current page
void RenderView::OnStop() {
  if (webview())
    webview()->StopLoading();
}

void RenderView::OnLoadAlternateHTMLText(const std::string& html_contents,
                                         bool new_navigation,
                                         const GURL& display_url,
                                         const std::string& security_info) {
  if (!webview())
    return;

  scoped_ptr<WebRequest> request(WebRequest::Create(
      GURL(kUnreachableWebDataURL)));
  request->SetSecurityInfo(security_info);

  webview()->GetMainFrame()->LoadAlternateHTMLString(request.get(),
                                                     html_contents,
                                                     display_url,
                                                     !new_navigation);
}

void RenderView::OnCopyImageAt(int x, int y) {
  webview()->CopyImageAt(x, y);
}

void RenderView::OnInspectElement(int x, int y) {
  webview()->InspectElement(x, y);
}

void RenderView::OnShowJavaScriptConsole() {
  webview()->ShowJavaScriptConsole();
}

void RenderView::OnStopFinding(bool clear_selection) {
  WebView* view = webview();
  if (!view)
    return;

  if (clear_selection)
    view->GetFocusedFrame()->ClearSelection();

  WebFrame* frame = view->GetMainFrame();
  while (frame) {
    frame->StopFinding(clear_selection);
    frame = view->GetNextFrameAfter(frame, false);
  }
}

void RenderView::OnFindReplyAck() {
  // Check if there is any queued up request waiting to be sent.
  if (queued_find_reply_message_.get()) {
    // Send the search result over to the browser process.
    Send(queued_find_reply_message_.get());
    queued_find_reply_message_.release();
  }
}

void RenderView::OnUpdateTargetURLAck() {
  // Check if there is a targeturl waiting to be sent.
  if (target_url_status_ == TARGET_PENDING) {
    Send(new ViewHostMsg_UpdateTargetURL(routing_id_, page_id_,
                                         pending_target_url_));
  }

  target_url_status_ = TARGET_NONE;
}

void RenderView::OnUndo() {
  if (!webview())
    return;

  webview()->GetFocusedFrame()->Undo();
}

void RenderView::OnRedo() {
  if (!webview())
    return;

  webview()->GetFocusedFrame()->Redo();
}

void RenderView::OnCut() {
  if (!webview())
    return;

  webview()->GetFocusedFrame()->Cut();
}

void RenderView::OnCopy() {
  if (!webview())
    return;

  webview()->GetFocusedFrame()->Copy();
}

void RenderView::OnPaste() {
  if (!webview())
    return;

  webview()->GetFocusedFrame()->Paste();
}

void RenderView::OnReplace(const std::wstring& text) {
  if (!webview())
    return;

  webview()->GetFocusedFrame()->Replace(text);
}

void RenderView::OnToggleSpellCheck() {
  if (!webview())
    return;

  webview()->GetFocusedFrame()->ToggleSpellCheck();
}

void RenderView::OnDelete() {
  if (!webview())
    return;

  webview()->GetFocusedFrame()->Delete();
}

void RenderView::OnSelectAll() {
  if (!webview())
    return;

  webview()->GetFocusedFrame()->SelectAll();
}

void RenderView::OnSetInitialFocus(bool reverse) {
  if (!webview())
    return;
  webview()->SetInitialFocus(reverse);
}

///////////////////////////////////////////////////////////////////////////////

// Tell the embedding application that the URL of the active page has changed
void RenderView::UpdateURL(WebFrame* frame) {
  WebDataSource* ds = frame->GetDataSource();
  DCHECK(ds);

  const WebRequest& request = ds->GetRequest();
  const WebRequest& initial_request = ds->GetInitialRequest();
  const WebResponse& response = ds->GetResponse();

  // We don't hold a reference to the extra data. The request's reference will
  // be sufficient because we won't modify it during our call. MAY BE NULL.
  RenderViewExtraRequestData* extra_data =
      static_cast<RenderViewExtraRequestData*>(request.GetExtraData());

  ViewHostMsg_FrameNavigate_Params params;
  params.is_post = false;
  params.page_id = page_id_;
  params.is_content_filtered = response.IsContentFiltered();
  if (!request.GetSecurityInfo().empty()) {
    // SSL state specified in the request takes precedence over the one in the
    // response.
    // So far this is only intended for error pages that are not expected to be
    // over ssl, so we should not get any clash.
    DCHECK(response.GetSecurityInfo().empty());
    params.security_info = request.GetSecurityInfo();
  } else {
    params.security_info = response.GetSecurityInfo();
  }

  // Set the URL to be displayed in the browser UI to the user.
  if (ds->HasUnreachableURL()) {
    params.url = ds->GetUnreachableURL();
  } else {
    params.url = request.GetURL();
  }

  params.redirects = ds->GetRedirectChain();
  params.should_update_history = !ds->HasUnreachableURL();

  const SearchableFormData* searchable_form_data =
      frame->GetDataSource()->GetSearchableFormData();
  if (searchable_form_data) {
    params.searchable_form_url = searchable_form_data->url();
    params.searchable_form_element_name = searchable_form_data->element_name();
    params.searchable_form_encoding = searchable_form_data->encoding();
  }

  const PasswordForm* password_form_data =
      frame->GetDataSource()->GetPasswordFormData();
  if (password_form_data)
    params.password_form = *password_form_data;

  params.gesture = navigation_gesture_;
  navigation_gesture_ = NavigationGestureUnknown;

  if (webview()->GetMainFrame() == frame) {
    // Top-level navigation.

    // Update contents MIME type for main frame.
    std::wstring mime_type = ds->GetResponseMimeType();
    params.contents_mime_type = WideToASCII(mime_type);

    // We assume top level navigations initiated by the renderer are link
    // clicks.
    params.transition = extra_data ?
        extra_data->transition_type : PageTransition::LINK;
    if (!PageTransition::IsMainFrame(params.transition)) {
      // If the main frame does a load, it should not be reported as a subframe
      // navigation.  This can occur in the following case:
      // 1. You're on a site with frames.
      // 2. You do a subframe navigation.  This is stored with transition type
      //    MANUAL_SUBFRAME.
      // 3. You navigate to some non-frame site, say, google.com.
      // 4. You navigate back to the page from step 2.  Since it was initially
      //    MANUAL_SUBFRAME, it will be that same transition type here.
      // We don't want that, because any navigation that changes the toplevel
      // frame should be tracked as a toplevel navigation (this allows us to
      // update the URL bar, etc).
      params.transition = PageTransition::LINK;
    }

    if (params.transition == PageTransition::LINK &&
        frame->GetDataSource()->IsFormSubmit()) {
      params.transition = PageTransition::FORM_SUBMIT;
    }

    // If we have a valid consumed client redirect source,
    // the page contained a client redirect (meta refresh, document.loc...),
    // so we set the referrer and transition to match.
    if (completed_client_redirect_src_.is_valid()) {
      DCHECK(completed_client_redirect_src_ == params.redirects[0]);
      params.referrer = completed_client_redirect_src_;
      params.transition = static_cast<PageTransition::Type>(
          params.transition | PageTransition::CLIENT_REDIRECT);
    } else {
      // Bug 654101: the referrer will be empty on https->http transitions. It
      // would be nice if we could get the real referrer from somewhere.
      params.referrer = GURL(initial_request.GetHttpReferrer());
    }

    std::string method = request.GetHttpMethod();
    if (method == "POST")
      params.is_post = true;

    Send(new ViewHostMsg_FrameNavigate(routing_id_, params));
  } else {
    // Subframe navigation: the type depends on whether this navigation
    // generated a new session history entry. When they do generate a session
    // history entry, it means the user initiated the navigation and we should
    // mark it as such. This test checks if this is the first time UpdateURL
    // has been called since WillNavigateToURL was called to initiate the load.
    if (page_id_ > last_page_id_sent_to_browser_)
      params.transition = PageTransition::MANUAL_SUBFRAME;
    else
      params.transition = PageTransition::AUTO_SUBFRAME;

    // The browser should never initiate a subframe navigation.
    DCHECK(!extra_data);
    Send(new ViewHostMsg_FrameNavigate(routing_id_, params));
  }

  last_page_id_sent_to_browser_ =
      std::max(last_page_id_sent_to_browser_, page_id_);

  // If we end up reusing this WebRequest (for example, due to a #ref click),
  // we don't want the transition type to persist.
  if (extra_data)
    extra_data->transition_type = PageTransition::LINK;  // Just clear it.

  if (glue_accessibility_.get()) {
    // Clear accessibility info cache. 
    glue_accessibility_->ClearIAccessibleMap(-1, true);
  }
}

// Tell the embedding application that the title of the active page has changed
void RenderView::UpdateTitle(WebFrame* frame, const std::wstring& title) {
  // Ignore all but top level navigations...
  if (webview()->GetMainFrame() == frame)
    Send(new ViewHostMsg_UpdateTitle(routing_id_, page_id_, title));
}

void RenderView::UpdateEncoding(WebFrame* frame,
                                const std::wstring& encoding_name) {
  // Only update main frame's encoding_name.
  if (webview()->GetMainFrame() == frame &&
      last_encoding_name_ != encoding_name) {
    // Save the encoding name for later comparing.
    last_encoding_name_ = encoding_name;

    Send(new ViewHostMsg_UpdateEncoding(routing_id_, last_encoding_name_));
  }
}

// Sends the previous session history state to the browser so it will be saved
// before we navigate to a new page. This must be called *before* the page ID
// has been updated so we know what it was.
void RenderView::UpdateSessionHistory(WebFrame* frame) {
  // If we have a valid page ID at this point, then it corresponds to the page
  // we are navigating away from.  Otherwise, this is the first navigation, so
  // there is no past session history to record.
  if (page_id_ == -1)
    return;

  std::string state;
  if (!webview()->GetMainFrame()->GetPreviousHistoryState(&state))
    return;
  Send(new ViewHostMsg_UpdateState(routing_id_, page_id_, state));
}

///////////////////////////////////////////////////////////////////////////////
// WebViewDelegate

void RenderView::DidStartLoading(WebView* webview) {
  if (is_loading_) {
    DLOG(WARNING) << "DidStartLoading called while loading";
    return;
  }

  is_loading_ = true;
  // Clear the pointer so that we can assign it only when there is an unknown
  // plugin on a page.
  first_default_plugin_ = NULL;

  Send(new ViewHostMsg_DidStartLoading(routing_id_, page_id_));
}

void RenderView::DidStopLoading(WebView* webview) {
  if (!is_loading_) {
    DLOG(WARNING) << "DidStopLoading called while not loading";
    return;
  }

  is_loading_ = false;

  // NOTE: For now we're doing the safest thing, and sending out notification
  // when done loading. This currently isn't an issue as the favicon is only
  // displayed when done loading. Ideally we would send notification when
  // finished parsing the head, but webkit doesn't support that yet.
  // The feed discovery code would also benefit from access to the head.
  GURL favicon_url(webview->GetMainFrame()->GetFavIconURL());
  if (!favicon_url.is_empty())
    Send(new ViewHostMsg_UpdateFavIconURL(routing_id_, page_id_, favicon_url));

  AddGURLSearchProvider(webview->GetMainFrame()->GetOSDDURL(),
                        true);  // autodetected

  Send(new ViewHostMsg_DidStopLoading(routing_id_, page_id_));

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&RenderView::CapturePageInfo, page_id_,
                                        false),
      kDelayForCaptureMs);

  // The page is loaded. Try to process the file we need to upload if any.
  ProcessPendingUpload();

  // Since the page is done loading, we are sure we don't need to try
  // again.
  ResetPendingUpload();
}

void RenderView::DidStartProvisionalLoadForFrame(
    WebView* webview,
    WebFrame* frame,
    NavigationGesture gesture) {
  if (webview->GetMainFrame() == frame) {
    navigation_gesture_ = gesture;

    // Make sure redirect tracking state is clear for the new load.
    completed_client_redirect_src_ = GURL();
  }

  Send(new ViewHostMsg_DidStartProvisionalLoadForFrame(
       routing_id_, webview->GetMainFrame() == frame,
       frame->GetProvisionalDataSource()->GetRequest().GetURL()));
}

bool RenderView::DidLoadResourceFromMemoryCache(WebView* webview,
                                                const WebRequest& request,
                                                const WebResponse& response,
                                                WebFrame* frame) {
  // Let the browser know we loaded a resource from the memory cache.  This
  // message is needed to display the correct SSL indicators.
  Send(new ViewHostMsg_DidLoadResourceFromMemoryCache(routing_id_,
      request.GetURL(), response.GetSecurityInfo()));

  return false;
}

void RenderView::DidReceiveProvisionalLoadServerRedirect(WebView* webview,
                                                         WebFrame* frame) {
  if (frame == webview->GetMainFrame()) {
    // Received a redirect on the main frame.
    WebDataSource* data_source =
        webview->GetMainFrame()->GetProvisionalDataSource();
    if (!data_source) {
      // Should only be invoked when we have a data source.
      NOTREACHED();
      return;
    }
    const std::vector<GURL>& redirects = data_source->GetRedirectChain();
    if (redirects.size() >= 2) {
      Send(new ViewHostMsg_DidRedirectProvisionalLoad(
           routing_id_, page_id_, redirects[redirects.size() - 2],
           redirects[redirects.size() - 1]));
    }
  }
}

void RenderView::DidFailProvisionalLoadWithError(WebView* webview,
                                                 const WebError& error,
                                                 WebFrame* frame) {
  // Notify the browser that we failed a provisional load with an error.
  //
  // Note: It is important this notification occur before DidStopLoading so the
  //       SSL manager can react to the provisional load failure before being
  //       notified the load stopped.
  //
  WebDataSource* ds = frame->GetProvisionalDataSource();
  DCHECK(ds);

  const WebRequest& failed_request = ds->GetRequest();

  bool show_repost_interstitial =
      (error.GetErrorCode() == net::ERR_CACHE_MISS &&
       LowerCaseEqualsASCII(failed_request.GetHttpMethod(), "post"));
  Send(new ViewHostMsg_DidFailProvisionalLoadWithError(
      routing_id_, frame == webview->GetMainFrame(),
      error.GetErrorCode(), error.GetFailedURL(),
      show_repost_interstitial));

  // Don't display an error page if this is simply a cancelled load.  Aside
  // from being dumb, WebCore doesn't expect it and it will cause a crash.
  if (error.GetErrorCode() == net::ERR_ABORTED)
    return;

  // If this is a failed back/forward/reload navigation, then we need to do a
  // 'replace' load.  This is necessary to avoid messing up session history.
  // Otherwise, we do a normal load, which simulates a 'go' navigation as far
  // as session history is concerned.
  RenderViewExtraRequestData* extra_data =
      static_cast<RenderViewExtraRequestData*>(failed_request.GetExtraData());
  bool replace = extra_data && !extra_data->is_new_navigation();

  // Use the alternate error page service if this is a DNS failure or
  // connection failure.  ERR_CONNECTION_FAILED can be dropped once we no longer
  // use winhttp.
  int ec = error.GetErrorCode();
  if (ec == net::ERR_NAME_NOT_RESOLVED ||
      ec == net::ERR_CONNECTION_FAILED ||
      ec == net::ERR_CONNECTION_REFUSED ||
      ec == net::ERR_ADDRESS_UNREACHABLE ||
      ec == net::ERR_TIMED_OUT) {
    const GURL& failed_url = error.GetFailedURL();
    const GURL& error_page_url = GetAlternateErrorPageURL(failed_url,
        ec == net::ERR_NAME_NOT_RESOLVED ? WebViewDelegate::DNS_ERROR
                                         : WebViewDelegate::CONNECTION_ERROR);
    if (error_page_url.is_valid()) {
      // Ask the WebFrame to fetch the alternate error page for us.
      frame->LoadAlternateHTMLErrorPage(&failed_request, error, error_page_url,
          replace, GURL(kUnreachableWebDataURL));
      return;
    }
  }

  // Fallback to a local error page.  
  LoadNavigationErrorPage(frame, &failed_request, error, std::string(),
                          replace);
}

void RenderView::LoadNavigationErrorPage(WebFrame* frame,
                                         const WebRequest* failed_request,
                                         const WebError& error,
                                         const std::string& html,
                                         bool replace) {
  const GURL& failed_url = error.GetFailedURL();

  std::string alt_html;
  if (html.empty()) {
    // Use a local error page.
    int resource_id;
    DictionaryValue error_strings;
    if (error.GetErrorCode() == net::ERR_CACHE_MISS &&
        LowerCaseEqualsASCII(failed_request->GetHttpMethod(), "post")) {
      GetFormRepostErrorValues(failed_url, &error_strings);
      resource_id = IDR_ERROR_NO_DETAILS_HTML;
    } else {
      GetLocalizedErrorValues(error, &error_strings);
      resource_id = IDR_NET_ERROR_HTML;
    }
    error_strings.SetString(L"textdirection",
      (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) ?
       L"rtl" : L"ltr");

    alt_html = GetAltHTMLForTemplate(error_strings, resource_id);
  } else {
    alt_html = html;
  }

  // Use a data: URL as the site URL to prevent against XSS attacks.
  scoped_ptr<WebRequest> request(failed_request->Clone());
  request->SetURL(GURL(kUnreachableWebDataURL));

  frame->LoadAlternateHTMLString(request.get(), alt_html, failed_url,
      replace);
}

void RenderView::DidCommitLoadForFrame(WebView *webview, WebFrame* frame,
                                       bool is_new_navigation) {
  const WebRequest& request =
      webview->GetMainFrame()->GetDataSource()->GetRequest();
  RenderViewExtraRequestData* extra_data =
      static_cast<RenderViewExtraRequestData*>(request.GetExtraData());

  if (is_new_navigation) {
    // When we perform a new navigation, we need to update the previous session
    // history entry with state for the page we are leaving.
    UpdateSessionHistory(frame);

    // We bump our Page ID to correspond with the new session history entry.
    page_id_ = next_page_id_++;

    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        method_factory_.NewRunnableMethod(&RenderView::CapturePageInfo,
                                          page_id_, true),
        kDelayForForcedCaptureMs);
  } else {
    // Inspect the extra_data on the main frame (set in our Navigate method) to
    // see if the navigation corresponds to a session history navigation...
    // Note: |frame| may or may not be the toplevel frame, but for the case
    // of capturing session history, the first committed frame suffices.  We
    // keep track of whether we've seen this commit before so that only capture
    // session history once per navigation.
    //
    // Note that we need to check if the page ID changed. In the case of a
    // reload, the page ID doesn't change, and UpdateSessionHistory gets the
    // previous URL and the current page ID, which would be wrong.
    if (extra_data && !extra_data->is_new_navigation() &&
        !extra_data->request_committed &&
        page_id_ != extra_data->pending_page_id()) {
      // This is a successful session history navigation!
      UpdateSessionHistory(frame);
      page_id_ = extra_data->pending_page_id();
    }
  }

  // Remember that we've already processed this request, so we don't update
  // the session history again.  We do this regardless of whether this is
  // a session history navigation, because if we attempted a session history
  // navigation without valid HistoryItem state, WebCore will think it is a
  // new navigation.
  if (extra_data)
    extra_data->request_committed = true;

  UpdateURL(frame);

  // If this committed load was initiated by a client redirect, we're
  // at the last stop now, so clear it.
  completed_client_redirect_src_ = GURL();

  // Check whether we have new encoding name.
  UpdateEncoding(frame, webview->GetMainFrameEncodingName());
}

void RenderView::DidReceiveTitle(WebView* webview,
                                 const std::wstring& title,
                                 WebFrame* frame) {
  UpdateTitle(frame, title);

  // Also check whether we have new encoding name.
  UpdateEncoding(frame, webview->GetMainFrameEncodingName());
}

void RenderView::DidFinishLoadForFrame(WebView* webview, WebFrame* frame) {
}

void RenderView::DidFailLoadWithError(WebView* webview,
                                      const WebError& error,
                                      WebFrame* frame) {
}

void RenderView::DidFinishDocumentLoadForFrame(WebView* webview,
                                               WebFrame* frame) {
  // Check whether we have new encoding name.
  UpdateEncoding(frame, webview->GetMainFrameEncodingName());

  // Inject any user scripts. Do not inject into chrome UI pages, but do inject
  // into any other document.
  if (user_scripts_enabled_) {
    const GURL &gurl = frame->GetURL();
    if (g_render_thread &&  // Will be NULL when testing.
        (gurl.SchemeIs("file") ||
         gurl.SchemeIs("http") ||
         gurl.SchemeIs("https"))) {
      g_render_thread->user_script_slave()->InjectScripts(frame);
    }
  }
}

void RenderView::DidHandleOnloadEventsForFrame(WebView* webview,
                                               WebFrame* frame) {
}

void RenderView::DidChangeLocationWithinPageForFrame(WebView* webview,
                                                     WebFrame* frame,
                                                     bool is_new_navigation) {
  DidCommitLoadForFrame(webview, frame, is_new_navigation);
  const std::wstring& title =
      webview->GetMainFrame()->GetDataSource()->GetPageTitle();
  UpdateTitle(frame, title);
}

void RenderView::DidReceiveIconForFrame(WebView* webview,
                                        WebFrame* frame) {
}

void RenderView::WillPerformClientRedirect(WebView* webview,
                                           WebFrame* frame,
                                           const GURL& src_url,
                                           const GURL& dest_url,
                                           unsigned int delay_seconds,
                                           unsigned int fire_date) {
}

void RenderView::DidCancelClientRedirect(WebView* webview,
                                         WebFrame* frame) {
}

void RenderView::DidCompleteClientRedirect(WebView* webview,
                                           WebFrame* frame,
                                           const GURL& source) {
  if (webview->GetMainFrame() == frame)
    completed_client_redirect_src_ = source;
}

void RenderView::BindDOMAutomationController(WebFrame* webframe) {
  dom_automation_controller_.set_message_sender(this);
  dom_automation_controller_.set_routing_id(routing_id_);
  dom_automation_controller_.BindToJavascript(webframe,
                                              L"domAutomationController");
}

void RenderView::WindowObjectCleared(WebFrame* webframe) {
  external_js_object_.set_render_view(this);
  external_js_object_.BindToJavascript(webframe, L"external");
  if (enable_dom_automation_)
    BindDOMAutomationController(webframe);
  if (enable_dom_ui_bindings_) {
    dom_ui_bindings_.set_message_sender(this);
    dom_ui_bindings_.set_routing_id(routing_id_);
    dom_ui_bindings_.BindToJavascript(webframe, L"chrome");
  }
  if (enable_external_host_bindings_) {
    external_host_bindings_.set_message_sender(this);
    external_host_bindings_.set_routing_id(routing_id_);
    external_host_bindings_.BindToJavascript(webframe, L"externalHost");
  }

#ifdef CHROME_PERSONALIZATION
  Personalization::ConfigureRendererPersonalization(personalization_, this,
                                                    routing_id_, webframe);
#endif
}

WindowOpenDisposition RenderView::DispositionForNavigationAction(
    WebView* webview,
    WebFrame* frame,
    const WebRequest* request,
    WebNavigationType type,
    WindowOpenDisposition disposition,
    bool is_redirect) {
  // Webkit is asking whether to navigate to a new URL.
  // This is fine normally, except if we're showing UI from one security
  // context and they're trying to navigate to a different context.
  const GURL& url = request->GetURL();
  // We only care about navigations that are within the current tab (as opposed
  // to, for example, opening a new window).
  // But we sometimes navigate to about:blank to clear a tab, and we want to
  // still allow that.
  if (disposition == CURRENT_TAB && !(url.SchemeIs("about"))) {
    // GetExtraData is NULL when we did not issue the request ourselves (see
    // OnNavigate), and so such a request may correspond to a link-click,
    // script, or drag-n-drop initiated navigation.
    if (frame == webview->GetMainFrame() && !request->GetExtraData()) {
      // When we received such unsolicited navigations, we sometimes want to
      // punt them up to the browser to handle.
      if (enable_dom_ui_bindings_ ||
          frame->GetInViewSourceMode() ||
          url.SchemeIs("view-source")) {
        OpenURL(webview, url, GURL(), disposition);
        return IGNORE_ACTION;  // Suppress the load here.
      } else if (url.SchemeIs(kBackForwardNavigationScheme)) {
        std::string offset_str = url.ExtractFileName();
        int offset;
        if (StringToInt(offset_str, &offset)) {
          GoToEntryAtOffset(offset);
          return IGNORE_ACTION;  // The browser process handles this one.
        }
      }
    }
  }

  // Detect when a page is "forking" a new tab that can be safely rendered in
  // its own process.  This is done by sites like Gmail that try to open links
  // in new windows without script connections back to the original page.  We
  // treat such cases as browser navigations (in which we will create a new
  // renderer for a cross-site navigation), rather than WebKit navigations.
  //
  // We use the following heuristic to decide whether to fork a new page in its
  // own process:
  // The parent page must open a new tab to about:blank, set the new tab's
  // window.opener to null, and then redirect the tab to a cross-site URL using
  // JavaScript.
  bool is_fork =
      // Must start from a tab showing about:blank, which is later redirected.
      frame->GetURL() == GURL("about:blank") &&
      // Must be the first real navigation of the tab.
      GetHistoryBackListCount() < 1 &&
      GetHistoryForwardListCount() < 1 &&
      // The parent page must have set the child's window.opener to null before
      // redirecting to the desired URL.
      frame->GetOpener() == NULL &&
      // Must be a top-level frame.
      frame->GetParent() == NULL &&
      // Must not have issued the request from this page.  GetExtraData is NULL
      // when the navigation is being done by something outside the page.
      !request->GetExtraData() &&
      // Must be targeted at the current tab.
      disposition == CURRENT_TAB &&
      // Must be a JavaScript navigation, which appears as "other".
      type == WebNavigationTypeOther;
  if (is_fork) {
    // Open the URL via the browser, not via WebKit.
    OpenURL(webview, url, GURL(), disposition);
    return IGNORE_ACTION;
  }

  return disposition;
}

void RenderView::RunJavaScriptAlert(WebView* webview,
                                    const std::wstring& message) {
  RunJavaScriptMessage(MessageBoxView::kIsJavascriptAlert,
                       message,
                       std::wstring(),
                       NULL);
}

bool RenderView::RunJavaScriptConfirm(WebView* webview,
                                      const std::wstring& message) {
  return RunJavaScriptMessage(MessageBoxView::kIsJavascriptConfirm,
                              message,
                              std::wstring(),
                              NULL);
}

bool RenderView::RunJavaScriptPrompt(WebView* webview,
                                     const std::wstring& message,
                                     const std::wstring& default_value,
                                     std::wstring* result) {
  return RunJavaScriptMessage(MessageBoxView::kIsJavascriptPrompt,
                              message,
                              default_value,
                              result);
}

bool RenderView::RunJavaScriptMessage(int type,
                                      const std::wstring& message,
                                      const std::wstring& default_value,
                                      std::wstring* result) {
  bool success = false;
  std::wstring result_temp;
  if (!result)
    result = &result_temp;
  IPC::SyncMessage* msg = new ViewHostMsg_RunJavaScriptMessage(
      routing_id_, message, default_value, type, &success, result);

  msg->set_pump_messages_event(modal_dialog_event_.get());
  Send(msg);

  return success;
}

void RenderView::AddGURLSearchProvider(const GURL& osd_url, bool autodetected) {
  if (!osd_url.is_empty())
    Send(new ViewHostMsg_PageHasOSDD(routing_id_, page_id_, osd_url,
                                     autodetected));
}

bool RenderView::RunBeforeUnloadConfirm(WebView* webview,
                                        const std::wstring& message) {
  bool success = false;
  // This is an ignored return value, but is included so we can accept the same
  // response as RunJavaScriptMessage.
  std::wstring ignored_result;
  IPC::SyncMessage* msg = new ViewHostMsg_RunBeforeUnloadConfirm(
      routing_id_, message, &success,  &ignored_result);

  msg->set_pump_messages_event(modal_dialog_event_.get());
  Send(msg);

  return success;
}

void RenderView::EnableSuddenTermination() {
  Send(new ViewHostMsg_UnloadListenerChanged(routing_id_, false));
}

void RenderView::DisableSuddenTermination() {
  Send(new ViewHostMsg_UnloadListenerChanged(routing_id_, true));
}

void RenderView::QueryFormFieldAutofill(const std::wstring& field_name,
                                        const std::wstring& text,
                                        int64 node_id) {
  static int message_id_counter = 0;
  form_field_autofill_request_id_ = message_id_counter++;
  Send(new ViewHostMsg_QueryFormFieldAutofill(routing_id_,
                                              field_name, text,
                                              node_id,
                                              form_field_autofill_request_id_));
}

void RenderView::OnReceivedAutofillSuggestions(
    int64 node_id,
    int request_id,
    const std::vector<std::wstring>& suggestions,
    int default_suggestion_index) {
  if (!webview() || request_id != form_field_autofill_request_id_)
    return;

  webview()->AutofillSuggestionsForNode(node_id, suggestions,
                                        default_suggestion_index);
}

void RenderView::OnPopupNotificationVisiblityChanged(bool visible) {
  popup_notification_visible_ = visible;
}

void RenderView::ShowModalHTMLDialog(const GURL& url, int width, int height,
                                     const std::string& json_arguments,
                                     std::string* json_retval) {
  IPC::SyncMessage* msg = new ViewHostMsg_ShowModalHTMLDialog(
      routing_id_, url, width, height, json_arguments, json_retval);

  msg->set_pump_messages_event(modal_dialog_event_.get());
  Send(msg);
}

uint32 RenderView::GetCPBrowsingContext() {
  uint32 context = 0;
  Send(new ViewHostMsg_GetCPBrowsingContext(&context));
  return context;
}

// Tell the browser to display a destination link.
void RenderView::UpdateTargetURL(WebView* webview, const GURL& url) {
  if (url != target_url_) {
    if (target_url_status_ == TARGET_INFLIGHT ||
        target_url_status_ == TARGET_PENDING) {
      // If we have a request in-flight, save the URL to be sent when we
      // receive an ACK to the in-flight request. We can happily overwrite
      // any existing pending sends.
      pending_target_url_ = url;
      target_url_status_ = TARGET_PENDING;
    } else {
      Send(new ViewHostMsg_UpdateTargetURL(routing_id_, page_id_, url));
      target_url_ = url;
      target_url_status_ = TARGET_INFLIGHT;
    }
  }
}

void RenderView::RunFileChooser(bool multi_select,
                                const std::wstring& title,
                                const std::wstring& default_filename,
                                const std::wstring& filter,
                                WebFileChooserCallback* file_chooser) {
  if (file_chooser_.get()) {
    // TODO(brettw): bug 1235154: This should be a synchronous message to deal
    // with the fact that web pages can programatically trigger this. With the
    // asnychronous messages, we can get an additional call when one is pending,
    // which this test is for. For now, we just ignore the additional file
    // chooser request. WebKit doesn't do anything to expect the callback, so
    // we can just ignore calling it.
    delete file_chooser;
    return;
  }
  file_chooser_.reset(file_chooser);
  Send(new ViewHostMsg_RunFileChooser(routing_id_, multi_select, title,
                                      default_filename, filter));
}

void RenderView::AddMessageToConsole(WebView* webview,
                                     const std::wstring& message,
                                     unsigned int line_no,
                                     const std::wstring& source_id) {
  Send(new ViewHostMsg_AddMessageToConsole(routing_id_, message,
                                           static_cast<int32>(line_no),
                                           source_id));
}

void RenderView::AddSearchProvider(const std::string& url) {
  AddGURLSearchProvider(GURL(url),
                        false);  // not autodetected
}

void RenderView::DebuggerOutput(const std::wstring& out) {
  Send(new ViewHostMsg_DebuggerOutput(routing_id_, out));
}

WebView* RenderView::CreateWebView(WebView* webview, bool user_gesture) {
  // Check to make sure we aren't overloading on popups.
  if (shared_popup_counter_->data > kMaximumNumberOfUnacknowledgedPopups)
    return NULL;

  // This window can't be closed from a window.close() call until we receive a
  // message from the Browser process explicitly allowing it.
  popup_notification_visible_ = true;

  int32 routing_id = MSG_ROUTING_NONE;
  HANDLE modal_dialog_event = NULL;
  bool result = render_thread_->Send(
      new ViewHostMsg_CreateWindow(routing_id_, user_gesture, &routing_id,
                                   &modal_dialog_event));
  if (routing_id == MSG_ROUTING_NONE) {
    DCHECK(modal_dialog_event == NULL);
    return NULL;
  }

  // The WebView holds a reference to this new RenderView
  const WebPreferences& prefs = webview->GetPreferences();
  base::WaitableEvent* waitable_event =
      new base::WaitableEvent(modal_dialog_event);
  RenderView* view = RenderView::Create(render_thread_,
                                        NULL, waitable_event, routing_id_,
                                        prefs, shared_popup_counter_,
                                        routing_id);
  view->set_opened_by_user_gesture(user_gesture);
  view->set_waiting_for_create_window_ack(true);

  // Copy over the alternate error page URL so we can have alt error pages in
  // the new render view (we don't need the browser to send the URL back down).
  view->alternate_error_page_url_ = alternate_error_page_url_;

  return view->webview();
}

WebWidget* RenderView::CreatePopupWidget(WebView* webview,
                                         bool activatable) {
  RenderWidget* widget = RenderWidget::Create(routing_id_,
                                              render_thread_,
                                              activatable);
  return widget->webwidget();
}

static bool ShouldLoadPluginInProcess(const std::string& mime_type,
                                      bool* is_gears) {
  if (RenderProcess::ShouldLoadPluginsInProcess())
    return true;

  if (mime_type == "application/x-googlegears") {
    *is_gears = true;
    return CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kGearsInRenderer);
  }

  return false;
}

WebPluginDelegate* RenderView::CreatePluginDelegate(
    WebView* webview,
    const GURL& url,
    const std::string& mime_type,
    const std::string& clsid,
    std::string* actual_mime_type) {
  bool is_gears = false;
  if (ShouldLoadPluginInProcess(mime_type, &is_gears)) {
    FilePath path;
    render_thread_->Send(
        new ViewHostMsg_GetPluginPath(url, mime_type, clsid, &path,
                                      actual_mime_type));
    if (path.value().empty())
      return NULL;

    std::string mime_type_to_use;
    if (actual_mime_type && !actual_mime_type->empty())
      mime_type_to_use = *actual_mime_type;
    else
      mime_type_to_use = mime_type;

    if (is_gears)
      ChromePluginLib::Create(path, GetCPBrowserFuncsForRenderer());
    return WebPluginDelegateImpl::Create(path,
                                         mime_type_to_use, host_window_);
  }

  WebPluginDelegateProxy* proxy =
      WebPluginDelegateProxy::Create(url, mime_type, clsid, this);
  if (!proxy)
    return NULL;

  // We hold onto the proxy so we can poke it when we are painting.  See our
  // DidPaint implementation below.
  plugin_delegates_.push_back(proxy);

  return proxy;
}

webkit_glue::WebMediaPlayerDelegate* RenderView::CreateMediaPlayerDelegate() {
  return new WebMediaPlayerDelegateImpl();
}

void RenderView::OnMissingPluginStatus(WebPluginDelegate* delegate,
                                       int status) {
  if (first_default_plugin_ == NULL) {
    // Show the InfoBar for the first available plugin.
    if (status == default_plugin::MISSING_PLUGIN_AVAILABLE) {
      first_default_plugin_ = delegate;
      Send(new ViewHostMsg_MissingPluginStatus(routing_id_, status));
    }
  } else {
    // Closes the InfoBar if user clicks on the plugin (instead of the InfoBar)
    // to start the download/install.
    if (status == default_plugin::MISSING_PLUGIN_USER_STARTED_DOWNLOAD) {
      Send(new ViewHostMsg_MissingPluginStatus(routing_id_, status));
    }
  }
}

void RenderView::OpenURL(WebView* webview, const GURL& url,
                         const GURL& referrer,
                         WindowOpenDisposition disposition) {
  Send(new ViewHostMsg_OpenURL(routing_id_, url, referrer, disposition));
}

// We are supposed to get a single call to Show for a newly created RenderView
// that was created via RenderView::CreateWebView.  So, we wait until this
// point to dispatch the ShowView message.
//
// This method provides us with the information about how to display the newly
// created RenderView (i.e., as a constrained popup or as a new tab).
//
void RenderView::Show(WebWidget* webwidget, WindowOpenDisposition disposition) {
  DCHECK(!did_show_) << "received extraneous Show call";
  DCHECK(opener_id_ != MSG_ROUTING_NONE);

  if (did_show_)
    return;
  did_show_ = true;

  // NOTE: initial_pos_ may still have its default values at this point, but
  // that's okay.  It'll be ignored if disposition is not NEW_POPUP, or the
  // browser process will impose a default position otherwise.
  Send(new ViewHostMsg_ShowView(
      opener_id_, routing_id_, disposition, initial_pos_,
      WasOpenedByUserGestureHelper()));
}

void RenderView::CloseWidgetSoon(WebWidget* webwidget) {
  if (popup_notification_visible_ == false)
    RenderWidget::CloseWidgetSoon(webwidget);
}

void RenderView::RunModal(WebWidget* webwidget) {
  DCHECK(did_show_) << "should already have shown the view";

  IPC::SyncMessage* msg = new ViewHostMsg_RunModal(routing_id_);

  msg->set_pump_messages_event(modal_dialog_event_.get());
  Send(msg);
}

void RenderView::SyncNavigationState() {
  if (!webview())
    return;

  std::string state;
  if (!webview()->GetMainFrame()->GetCurrentHistoryState(&state))
    return;
  Send(new ViewHostMsg_UpdateState(routing_id_, page_id_, state));
}

void RenderView::ShowContextMenu(WebView* webview,
                                 ContextNode::Type type,
                                 int x,
                                 int y,
                                 const GURL& link_url,
                                 const GURL& image_url,
                                 const GURL& page_url,
                                 const GURL& frame_url,
                                 const std::wstring& selection_text,
                                 const std::wstring& misspelled_word,
                                 int edit_flags,
                                 const std::string& security_info) {
  ViewHostMsg_ContextMenu_Params params;
  params.type = type;
  params.x = x;
  params.y = y;
  params.image_url = image_url;
  params.link_url = link_url;
  params.page_url = page_url;
  params.frame_url = frame_url;
  params.selection_text = selection_text;
  params.misspelled_word = misspelled_word;
  params.spellcheck_enabled = 
      webview->GetFocusedFrame()->SpellCheckEnabled();
  params.edit_flags = edit_flags;
  params.security_info = security_info;
  Send(new ViewHostMsg_ContextMenu(routing_id_, params));
}

void RenderView::StartDragging(WebView* webview, const WebDropData& drop_data) {
  Send(new ViewHostMsg_StartDragging(routing_id_, drop_data));
}

void RenderView::TakeFocus(WebView* webview, bool reverse) {
  Send(new ViewHostMsg_TakeFocus(routing_id_, reverse));
}

void RenderView::DidDownloadImage(int id,
                                  const GURL& image_url,
                                  bool errored,
                                  const SkBitmap& image) {
  Send(new ViewHostMsg_DidDownloadImage(routing_id_, id, image_url, errored,
                                        image));
}


void RenderView::OnDownloadImage(int id,
                                 const GURL& image_url,
                                 int image_size) {
  if (!webview()->DownloadImage(id, image_url, image_size))
    Send(new ViewHostMsg_DidDownloadImage(routing_id_, id, image_url, true,
                                          SkBitmap()));
}

void RenderView::OnGetApplicationInfo(int page_id) {
  webkit_glue::WebApplicationInfo app_info;
  if (page_id == page_id_)
    webkit_glue::GetApplicationInfo(webview(), &app_info);

  // Prune out any data URLs in the set of icons.  The browser process expects
  // any icon with a data URL to have originated from a favicon.  We don't want
  // to decode arbitrary data URLs in the browser process.  See
  // http://b/issue?id=1162972
  for (size_t i = 0; i < app_info.icons.size(); ++i) {
    if (app_info.icons[i].url.SchemeIs("data")) {
      app_info.icons.erase(app_info.icons.begin() + i);
      --i;
    }
  }

  Send(new ViewHostMsg_DidGetApplicationInfo(routing_id_, page_id, app_info));
}

GURL RenderView::GetAlternateErrorPageURL(const GURL& failedURL,
                                          ErrorPageType error_type) {
  if (failedURL.SchemeIsSecure()) {
    // If the URL that failed was secure, then the embedding web page was not
    // expecting a network attacker to be able to manipulate its contents.  As
    // we fetch alternate error pages over HTTP, we would be allowing a network
    // attacker to manipulate the contents of the response if we tried to use
    // the link doctor here.
    return GURL::EmptyGURL();
  }

  // Grab the base URL from the browser process.
  if (!alternate_error_page_url_.is_valid())
    return GURL::EmptyGURL();

  // Strip query params from the failed URL.
  GURL::Replacements remove_params;
  remove_params.ClearUsername();
  remove_params.ClearPassword();
  remove_params.ClearQuery();
  remove_params.ClearRef();
  const GURL url_to_send = failedURL.ReplaceComponents(remove_params);

  // Construct the query params to send to link doctor.
  std::string params(alternate_error_page_url_.query());
  params.append("&url=");
  params.append(EscapeQueryParamValue(url_to_send.spec()));
  params.append("&sourceid=chrome");
  params.append("&error=");
  switch (error_type) {
    case DNS_ERROR:
      params.append("dnserror");
      break;

    case HTTP_404:
      params.append("http404");
      break;

    case CONNECTION_ERROR:
      params.append("connectionerror");
      break;

    default:
      NOTREACHED() << "unknown ErrorPageType";
  }

  // OK, build the final url to return.
  GURL::Replacements link_doctor_params;
  link_doctor_params.SetQueryStr(params);
  GURL url = alternate_error_page_url_.ReplaceComponents(link_doctor_params);
  return url;
}

void RenderView::OnFind(const FindInPageRequest& request) {
  WebFrame* main_frame = webview()->GetMainFrame();
  WebFrame* frame_after_main = webview()->GetNextFrameAfter(main_frame, true);
  WebFrame* focused_frame = webview()->GetFocusedFrame();
  WebFrame* search_frame = focused_frame;  // start searching focused frame.

  bool multi_frame = (frame_after_main != main_frame);

  // If we have multiple frames, we don't want to wrap the search within the
  // frame, so we check here if we only have main_frame in the chain.
  bool wrap_within_frame = !multi_frame;

  gfx::Rect selection_rect;
  bool result = false;

  do {
    result = search_frame->Find(request, wrap_within_frame, &selection_rect);

    if (!result) {
      // don't leave text selected as you move to the next frame.
      search_frame->ClearSelection();

      // Find the next frame, but skip the invisible ones.
      do {
        // What is the next frame to search? (we might be going backwards). Note
        // that we specify wrap=true so that search_frame never becomes NULL.
        search_frame = request.forward ?
            webview()->GetNextFrameAfter(search_frame, true) :
            webview()->GetPreviousFrameBefore(search_frame, true);
      } while (!search_frame->Visible() && search_frame != focused_frame);

      // Make sure selection doesn't affect the search operation in new frame.
      search_frame->ClearSelection();

      // If we have multiple frames and we have wrapped back around to the
      // focused frame, we need to search it once more allowing wrap within
      // the frame, otherwise it will report 'no match' if the focused frame has
      // reported matches, but no frames after the focused_frame contain a
      // match for the search word(s).
      if (multi_frame && search_frame == focused_frame) {
        result = search_frame->Find(request, true,  // Force wrapping.
                                    &selection_rect);
      }
    }

    // TODO(jcampan): http://b/issue?id=1157486 Remove StoreForFocus call once
    //                we have the fix for 792423.
    search_frame->GetView()->StoreFocusForFrame(search_frame);
    webview()->SetFocusedFrame(search_frame);
  } while (!result && search_frame != focused_frame);

  // Make sure we don't leave any frame focused or the focus won't be restored
  // properly in WebViewImpl::SetFocus().  Note that we are talking here about
  // focused on the SelectionController, not FocusController.
  // webview()->GetFocusedFrame() will still return the last focused frame (as
  // it queries the FocusController).
  // TODO(jcampan): http://b/issue?id=1157486 Remove next line once we have the
  //                fix for 792423.
  webview()->SetFocusedFrame(NULL);

  // We send back word that we found some matches, because we don't want to lag
  // when notifying the user that we found something. At this point we only know
  // that we found 1 match, but the scoping effort will tell us more. However,
  // if this is a FindNext request, the scoping effort is already under way, or
  // done already, so we have partial results. In that case we set it to -1 so
  // that it gets ignored by the FindInPageController.
  int match_count = result ? 1 : 0;  // 1 here means possibly more coming.
  if (request.find_next)
    match_count = -1;

  // If we find no matches (or if this is Find Next) then this will be our last
  // status update. Otherwise the scoping effort will send more results.
  bool final_status_update = !result || request.find_next;

  // Send the search result over to the browser process.
  Send(new ViewHostMsg_Find_Reply(routing_id_, request.request_id,
                                  match_count,
                                  selection_rect,
                                  -1,  // Don't update active match ordinal.
                                  final_status_update));

  if (!request.find_next) {
    // Scoping effort begins, starting with the mainframe.
    search_frame = main_frame;

    main_frame->ResetMatchCount();

    do {
      // Cancel all old scoping requests before starting a new one.
      search_frame->CancelPendingScopingEffort();

      // We don't start another scoping effort unless at least one match has
      // been found.
      if (result) {
        // Start new scoping request. If the scoping function determines that it
        // needs to scope, it will defer until later.
        search_frame->ScopeStringMatches(request,
                                         true);  // reset the tickmarks
      }

      // Iterate to the next frame. The frame will not necessarily scope, for
      // example if it is not visible.
      search_frame = webview()->GetNextFrameAfter(search_frame, true);
    } while (search_frame != main_frame);
  }
}

void RenderView::ReportFindInPageMatchCount(int count, int request_id,
                                            bool final_update) {
  // If we have a message that has been queued up, then we should just replace
  // it. The ACK from the browser will make sure it gets sent when the browser
  // wants it.
  if (queued_find_reply_message_.get()) {
    IPC::Message* msg = new ViewHostMsg_Find_Reply(
        routing_id_,
        request_id,
        count,
        gfx::Rect(0, 0, 0, 0),
        -1,  // Don't update active match ordinal.
        final_update);
    queued_find_reply_message_.reset(msg);
  } else {
    // Send the search result over to the browser process.
    Send(new ViewHostMsg_Find_Reply(
        routing_id_,
        request_id,
        count,
        gfx::Rect(0, 0, 0, 0),
        -1,  // // Don't update active match ordinal.
        final_update));
  }
}

void RenderView::ReportFindInPageSelection(int request_id,
                                           int active_match_ordinal,
                                           const gfx::Rect& selection_rect) {
  // Send the search result over to the browser process.
  Send(new ViewHostMsg_Find_Reply(routing_id_,
                                  request_id,
                                  -1,
                                  selection_rect,
                                  active_match_ordinal,
                                  false));
}

bool RenderView::WasOpenedByUserGesture(WebView* webview) const {
  return WasOpenedByUserGestureHelper();
}

bool RenderView::WasOpenedByUserGestureHelper() const {
  // If pop-up blocking has been disabled, then treat all new windows as if
  // they were opened by a user gesture.  This will prevent them from being
  // blocked.  This is a bit of a hack, there should be a more straightforward
  // way to disable pop-up blocking.
  if (disable_popup_blocking_)
    return true;

  return opened_by_user_gesture_;
}

void RenderView::SpellCheck(const std::wstring& word, int& misspell_location,
                            int& misspell_length) {
  Send(new ViewHostMsg_SpellCheck(routing_id_, word, &misspell_location,
                                  &misspell_length));
}

void RenderView::SetInputMethodState(bool enabled) {
  // Save the updated IME status and mark the input focus has been updated.
  // The IME status is to be sent to a browser process next time when
  // the input caret is rendered.
  if (!ime_control_busy_) {
    ime_control_updated_ = true;
    ime_control_new_state_ = enabled;
  }
}

void RenderView::ScriptedPrint(WebFrame* frame) {
  // Retrieve the default print settings to calculate the expected number of
  // pages.
  ViewMsg_Print_Params default_settings;
  IPC::SyncMessage* msg =
      new ViewHostMsg_GetDefaultPrintSettings(routing_id_, &default_settings);
  if (Send(msg)) {
    msg = NULL;
    // Continue only if the settings are valid.
    if (default_settings.dpi && default_settings.document_cookie) {
      int expected_pages_count = SwitchFrameToPrintMediaType(default_settings,
                                                             frame);
      DCHECK(expected_pages_count);
      SwitchFrameToDisplayMediaType(frame);

      // Ask the browser to show UI to retrieve the final print settings.
      ViewMsg_PrintPages_Params print_settings;
      // host_window_ may be NULL at this point if the current window is a popup
      // and the print() command has been issued from the parent. The receiver
      // of this message has to deal with this.
      msg = new ViewHostMsg_ScriptedPrint(routing_id_,
                                          host_window_,
                                          default_settings.document_cookie,
                                          expected_pages_count,
                                          &print_settings);
      if (Send(msg)) {
        msg = NULL;

        // If the settings are invalid, early quit.
        if (print_settings.params.dpi &&
            print_settings.params.document_cookie) {
          // Render the printed pages. It will implicitly revert the document to
          // display CSS media type.
          PrintPages(print_settings, frame);
          // All went well.
          return;
        } else {
          // The user cancelled.
        }
      } else {
        // Send() failed.
        NOTREACHED();
      }
    } else {
      // The user cancelled.
    }
  } else {
    // Send() failed.
    NOTREACHED();
  }
  // TODO(maruel):  bug 1123882 Alert the user that printing failed.
}

void RenderView::WebInspectorOpened(int num_resources) {
  Send(new ViewHostMsg_InspectElement_Reply(routing_id_, num_resources));
}

void RenderView::UserMetricsRecordAction(const std::wstring& action) {
  Send(new ViewHostMsg_UserMetricsRecordAction(routing_id_, action));
}

void RenderView::DnsPrefetch(const std::vector<std::string>& host_names) {
  Send(new ViewHostMsg_DnsPrefetch(host_names));
}

void RenderView::OnZoom(int function) {
  static const bool kZoomIsTextOnly = false;
  switch (function) {
    case PageZoom::SMALLER:
      webview()->ZoomOut(kZoomIsTextOnly);
      break;
    case PageZoom::STANDARD:
      webview()->ResetZoom();
      break;
    case PageZoom::LARGER:
      webview()->ZoomIn(kZoomIsTextOnly);
      break;
    default:
      NOTREACHED();
  }
}

void RenderView::OnSetPageEncoding(const std::wstring& encoding_name) {
  webview()->SetPageEncoding(encoding_name);
}

void RenderView::OnPasswordFormsSeen(WebView* webview,
                                     const std::vector<PasswordForm>& forms) {
  Send(new ViewHostMsg_PasswordFormsSeen(routing_id_, forms));
}

void RenderView::OnAutofillFormSubmitted(WebView* webview,
                                         const AutofillForm& form) {
  Send(new ViewHostMsg_AutofillFormSubmitted(routing_id_, form));
}

WebHistoryItem* RenderView::GetHistoryEntryAtOffset(int offset) {
  // Our history list is kept in the browser process on the UI thread.  Since
  // we can't make a sync IPC call to that thread without risking deadlock,
  // we use a trick: construct a fake history item of the form:
  //   history://go/OFFSET
  // When WebCore tells us to navigate to it, we tell the browser process to
  // do a back/forward navigation instead.

  GURL url(StringPrintf("%s://go/%d", kBackForwardNavigationScheme, offset));
  history_navigation_item_ = WebHistoryItem::Create(url, L"", "", NULL);
  return history_navigation_item_.get();
}

void RenderView::GoToEntryAtOffset(int offset) {
  history_back_list_count_ += offset;
  history_forward_list_count_ -= offset;

  Send(new ViewHostMsg_GoToEntryAtOffset(routing_id_, offset));
}

int RenderView::GetHistoryBackListCount() {
  return history_back_list_count_;
}

int RenderView::GetHistoryForwardListCount() {
  return history_forward_list_count_;
}

void RenderView::OnNavStateChanged(WebView* webview) {
  if (!nav_state_sync_timer_.IsRunning()) {
    nav_state_sync_timer_.Start(
        TimeDelta::FromSeconds(delay_seconds_for_form_state_sync_), this,
        &RenderView::SyncNavigationState);
  }
}

void RenderView::SetTooltipText(WebView* webview,
                                const std::wstring& tooltip_text) {
  Send(new ViewHostMsg_SetTooltipText(routing_id_, tooltip_text));
}

void RenderView::DownloadUrl(const GURL& url, const GURL& referrer) {
  Send(new ViewHostMsg_DownloadUrl(routing_id_, url, referrer));
}

WebFrame* RenderView::GetChildFrame(const std::wstring& frame_xpath) const {
  WebFrame* web_frame;
  if (frame_xpath.empty()) {
    web_frame = webview()->GetMainFrame();
  } else {
    web_frame = webview()->GetMainFrame()->GetChildFrame(frame_xpath);
  }

  return web_frame;
}

void RenderView::EvaluateScript(const std::wstring& frame_xpath,
                                const std::wstring& script) {
  WebFrame* web_frame = GetChildFrame(frame_xpath);
  if (!web_frame)
    return;

  web_frame->ExecuteJavaScript(WideToUTF8(script),
                               GURL(), // script url
                               1); // base line number
}

void RenderView::OnScriptEvalRequest(const std::wstring& frame_xpath,
                                     const std::wstring& jscript) {
  EvaluateScript(frame_xpath, jscript);
}

void RenderView::OnAddMessageToConsole(const std::wstring& frame_xpath,
                                       const std::wstring& msg,
                                       ConsoleMessageLevel level) {
  WebFrame* web_frame = GetChildFrame(frame_xpath);
  if (!web_frame)
    return;

  web_frame->AddMessageToConsole(msg, level);
}

void RenderView::OnDebugAttach() {
  Send(new ViewHostMsg_DidDebugAttach(routing_id_));
  // Tell the plugin host to stop accepting messages in order to avoid
  // hangs while the renderer is paused.
  // TODO(1243929): It might be an improvement to add more plumbing to do this
  // when the renderer is actually paused vs. just the debugger being attached.
  PluginChannelHost::SetListening(false);
}

void RenderView::OnDebugDetach() {
  // Tell the plugin host to start accepting plugin messages again.
  PluginChannelHost::SetListening(true);
}

void RenderView::OnAllowDomAutomationBindings(bool allow_bindings) {
  enable_dom_automation_ = allow_bindings;
}

void RenderView::OnAllowBindings(bool enable_dom_ui_bindings,
                                 bool enable_external_host_bindings) {
  enable_dom_ui_bindings_ = enable_dom_ui_bindings;
  enable_external_host_bindings_ = enable_external_host_bindings;
}

void RenderView::OnSetDOMUIProperty(const std::string& name,
                                    const std::string& value) {
  DCHECK(enable_dom_ui_bindings_);
  dom_ui_bindings_.SetProperty(name, value);
}

void RenderView::OnReservePageIDRange(int size_of_range) {
  next_page_id_ += size_of_range + 1;
}

void RenderView::OnDragSourceEndedOrMoved(int client_x,
                                          int client_y,
                                          int screen_x,
                                          int screen_y,
                                          bool ended) {
  if (ended)
    webview()->DragSourceEndedAt(client_x, client_y, screen_x, screen_y);
  else
    webview()->DragSourceMovedTo(client_x, client_y, screen_x, screen_y);
}

void RenderView::OnDragSourceSystemDragEnded() {
  webview()->DragSourceSystemDragEnded();
}

void RenderView::OnUploadFileRequest(const ViewMsg_UploadFile_Params& p) {
  webkit_glue::FileUploadData* f = new webkit_glue::FileUploadData;
  f->file_path = p.file_path;
  f->form_name = p.form;
  f->file_name = p.file;
  f->submit_name = p.submit;

  // Build the other form values map.
  if (!p.other_values.empty()) {
    std::vector<std::wstring> e;
    std::vector<std::wstring> kvp;
    std::vector<std::wstring>::iterator i;

    SplitString(p.other_values, L'\n', &e);
    for (i = e.begin(); i != e.end(); ++i) {
      SplitString(*i, L'=', &kvp);
      if (kvp.size() == 2)
        f->other_form_values[kvp[0]] = kvp[1];
      kvp.clear();
    }
  }

  pending_upload_data_.reset(f);
  ProcessPendingUpload();
}

void RenderView::ProcessPendingUpload() {
  webkit_glue::FileUploadData* f = pending_upload_data_.get();
  if (f && webview() && webkit_glue::FillFormToUploadFile(webview(), *f))
    ResetPendingUpload();
}

void RenderView::ResetPendingUpload() {
  pending_upload_data_.reset();
}

void RenderView::OnFormFill(const FormData& form) {
  webkit_glue::FillForm(this->webview(), form);
}

void RenderView::OnFillPasswordForm(
    const PasswordFormDomManager::FillData& form_data) {
  webkit_glue::FillPasswordForm(this->webview(), form_data);
}

void RenderView::OnDragTargetDragEnter(const WebDropData& drop_data,
    const gfx::Point& client_pt, const gfx::Point& screen_pt) {
  bool is_drop_target = webview()->DragTargetDragEnter(drop_data,
      client_pt.x(), client_pt.y(), screen_pt.x(), screen_pt.y());

  Send(new ViewHostMsg_UpdateDragCursor(routing_id_, is_drop_target));
}

void RenderView::OnDragTargetDragOver(const gfx::Point& client_pt,
                                      const gfx::Point& screen_pt) {
  bool is_drop_target = webview()->DragTargetDragOver(client_pt.x(),
      client_pt.y(), screen_pt.x(), screen_pt.y());

  Send(new ViewHostMsg_UpdateDragCursor(routing_id_, is_drop_target));
}

void RenderView::OnDragTargetDragLeave() {
  webview()->DragTargetDragLeave();
}

void RenderView::OnDragTargetDrop(const gfx::Point& client_pt,
                                  const gfx::Point& screen_pt) {
  webview()->DragTargetDrop(client_pt.x(), client_pt.y(), screen_pt.x(),
                            screen_pt.y());
}

void RenderView::OnUpdateWebPreferences(const WebPreferences& prefs) {
  webview()->SetPreferences(prefs);
}

void RenderView::OnSetAltErrorPageURL(const GURL& url) {
  alternate_error_page_url_ = url;
}

void RenderView::DidPaint() {
  PluginDelegateList::iterator it = plugin_delegates_.begin();
  while (it != plugin_delegates_.end()) {
    (*it)->FlushGeometryUpdates();
    ++it;
  }
}

void RenderView::OnInstallMissingPlugin() {
  // This could happen when the first default plugin is deleted.
  if (first_default_plugin_ == NULL)
    return;
  first_default_plugin_->InstallMissingPlugin();
}

void RenderView::OnFileChooserResponse(
         const std::vector<std::wstring>& file_names) {
  file_chooser_->OnFileChoose(file_names);
  file_chooser_.reset();
}

void RenderView::OnEnableViewSourceMode() {
  if (!webview())
    return;
  WebFrame* main_frame = webview()->GetMainFrame();
  if (!main_frame)
    return;

  main_frame->SetInViewSourceMode(true);
}

void RenderView::OnUpdateBackForwardListCount(int back_list_count,
                                              int forward_list_count) {
  history_back_list_count_ = back_list_count;
  history_forward_list_count_ = forward_list_count;
}

void RenderView::OnGetAccessibilityInfo(
    const ViewMsg_Accessibility_In_Params& in_params,
    ViewHostMsg_Accessibility_Out_Params* out_params) {

  if (!glue_accessibility_.get())
    glue_accessibility_.reset(new GlueAccessibility());

  if (!glue_accessibility_->
       GetAccessibilityInfo(webview(), in_params, out_params)) {
    return;
  }
}

void RenderView::OnClearAccessibilityInfo(int iaccessible_id, bool clear_all) {
  if (!glue_accessibility_.get()) {
    // If accessibility is not activated, ignore clearing message.
    return;
  }

  if (!glue_accessibility_->ClearIAccessibleMap(iaccessible_id, clear_all))
    return;
}

void RenderView::OnGetAllSavableResourceLinksForCurrentPage(
    const GURL& page_url) {
  // Prepare list to storage all savable resource links.
  std::vector<GURL> resources_list;
  std::vector<GURL> referrers_list;
  std::vector<GURL> frames_list;
  webkit_glue::SavableResourcesResult result(&resources_list,
                                             &referrers_list,
                                             &frames_list);

  if (!webkit_glue::GetAllSavableResourceLinksForCurrentPage(webview(),
                                                             page_url,
                                                             &result)) {
    // If something is wrong when collecting all savable resource links,
    // send empty list to embedder(browser) to tell it failed.
    referrers_list.clear();
    resources_list.clear();
    frames_list.clear();
  }

  // Send result of all savable resource links to embedder.
  Send(new ViewHostMsg_SendCurrentPageAllSavableResourceLinks(routing_id_,
                                                              resources_list,
                                                              referrers_list,
                                                              frames_list));
}

void RenderView::OnGetSerializedHtmlDataForCurrentPageWithLocalLinks(
    const std::vector<std::wstring>& links,
    const std::vector<std::wstring>& local_paths,
    const std::wstring& local_directory_name) {
  webkit_glue::DomSerializer dom_serializer(webview()->GetMainFrame(),
                                            true,
                                            this,
                                            links,
                                            local_paths,
                                            local_directory_name);
  dom_serializer.SerializeDom();
}

void RenderView::DidSerializeDataForFrame(const GURL& frame_url,
    const std::string& data, PageSavingSerializationStatus status) {
  Send(new ViewHostMsg_SendSerializedHtmlData(routing_id_,
      frame_url, data, static_cast<int32>(status)));
}

void RenderView::OnMsgShouldClose() {
  bool should_close = webview()->ShouldClose();
  Send(new ViewHostMsg_ShouldClose_ACK(routing_id_, should_close));
}

void RenderView::OnClosePage(int new_render_process_host_id,
                             int new_request_id) {
  // TODO(creis): We'd rather use webview()->Close() here, but that currently
  // sets the WebView's delegate_ to NULL, preventing any JavaScript dialogs
  // in the onunload handler from appearing.  For now, we're bypassing that and
  // calling the FrameLoader's CloseURL method directly.  This should be
  // revisited to avoid having two ways to close a page.  Having a single way
  // to close that can run onunload is also useful for fixing
  // http://b/issue?id=753080.
  WebFrame* main_frame = webview()->GetMainFrame();
  if (main_frame)
    main_frame->ClosePage();

  Send(new ViewHostMsg_ClosePage_ACK(routing_id_,
                                     new_render_process_host_id,
                                     new_request_id));
}

void RenderView::OnThemeChanged() {
  gfx::NativeTheme::instance()->CloseHandles();
  gfx::Rect view_rect(0, 0, size_.width(), size_.height());
  DidInvalidateRect(webwidget_, view_rect);
}

#ifdef CHROME_PERSONALIZATION
void RenderView::OnPersonalizationEvent(std::string event_name,
                                        std::string event_args) {
  Personalization::HandleViewMsgPersonalizationEvent(personalization_,
                                                     webview(),
                                                     event_name,
                                                     event_args);
}
#endif

void RenderView::TransitionToCommittedForNewPage() {
#ifdef CHROME_PERSONALIZATION
  Personalization::HandleTransitionToCommittedForNewPage(personalization_);
#endif
}

void RenderView::DidAddHistoryItem() {
  // We don't want to update the history length for the start page
  // navigation.
  WebFrame* main_frame = webview()->GetMainFrame();
  DCHECK(main_frame != NULL);

  WebDataSource* ds = main_frame->GetDataSource();
  DCHECK(ds != NULL);

  const WebRequest& request = ds->GetRequest();
  RenderViewExtraRequestData* extra_data =
      static_cast<RenderViewExtraRequestData*>(request.GetExtraData());

  if (extra_data && extra_data->transition_type == PageTransition::START_PAGE)
    return;

  history_back_list_count_++;
  history_forward_list_count_ = 0;
}

void RenderView::OnMessageFromExternalHost(
    const std::string& target, const std::string& message) {
  if (message.empty())
    return;

  WebFrame* main_frame = webview()->GetMainFrame();
  if (!main_frame)
    return;

  std::string script = "javascript:";
  script += target;
  script += "(";
  script += "'";
  script += message;
  script += "'";
  script += ");void(0);";

  GURL script_url(script);
  scoped_ptr<WebRequest> request(WebRequest::Create(script_url));
  // TODO(iyengar)
  // Need a mechanism to send results back.
  main_frame->LoadRequest(request.get());
}

void RenderView::OnDisassociateFromPopupCount() {
  if (decrement_shared_popup_at_destruction_)
    shared_popup_counter_->data--;
  shared_popup_counter_ = new SharedRenderViewCounter(0);
  decrement_shared_popup_at_destruction_ = false;
}

std::string RenderView::GetAltHTMLForTemplate(
    const DictionaryValue& error_strings, int template_resource_id) const {
  const StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          template_resource_id));

  if (template_html.empty()) {
    NOTREACHED() << "unable to load template. ID: " << template_resource_id;
    return "";
  }
  // "t" is the id of the templates root node.
  return jstemplate_builder::GetTemplateHtml(
      template_html, &error_strings, "t");
}
