// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header is meant to be included in multiple passes, hence no traditional
// header guard.
// See ipc_message_macros.h for explanation of the macros and passes.

#include <string>
#include <vector>

#include "build/build_config.h"

#include "base/clipboard.h"
#include "base/file_path.h"
#include "base/gfx/rect.h"
#include "base/gfx/native_widget_types.h"
#include "base/shared_memory.h"
#include "base/values.h"
#include "chrome/common/ipc_channel_handle.h"
#include "chrome/common/ipc_message_macros.h"
#include "chrome/common/transport_dib.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/webappcachecontext.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webplugin.h"

// TODO(mpcomplete): rename ViewMsg and ViewHostMsg to something that makes
// more sense with our current design.

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the browser to the renderer process.

IPC_BEGIN_MESSAGES(View)
  // Used typically when recovering from a crash.  The new rendering process
  // sets its global "next page id" counter to the given value.
  IPC_MESSAGE_CONTROL1(ViewMsg_SetNextPageID,
                       int32 /* next_page_id */)

  // Tells the renderer to create a new view.
  // This message is slightly different, the view it takes is the view to
  // create, the message itself is sent as a non-view control message.
  IPC_MESSAGE_CONTROL5(ViewMsg_New,
                       gfx::NativeViewId, /* parent window */
                       ModalDialogEvent, /* model dialog box event */
                       RendererPreferences,
                       WebPreferences,
                       int32 /* view id */)

  // Tells the renderer to set its maximum cache size to the supplied value
  IPC_MESSAGE_CONTROL3(ViewMsg_SetCacheCapacities,
                       size_t /* min_dead_capacity */,
                       size_t /* max_dead_capacity */,
                       size_t /* capacity */)

  // Reply in response to ViewHostMsg_ShowView or ViewHostMsg_ShowWidget.
  // similar to the new command, but used when the renderer created a view
  // first, and we need to update it
  IPC_MESSAGE_ROUTED1(ViewMsg_CreatingNew_ACK,
                      gfx::NativeViewId /* parent_hwnd */)

  // Sends updated preferences to the renderer.
  IPC_MESSAGE_ROUTED1(ViewMsg_SetRendererPrefs,
                      RendererPreferences)

  // Tells the render view to close.
  IPC_MESSAGE_ROUTED0(ViewMsg_Close)

  // Tells the render view to change its size.  A ViewHostMsg_PaintRect message
  // is generated in response provided new_size is not empty and not equal to
  // the view's current size.  The generated ViewHostMsg_PaintRect message will
  // have the IS_RESIZE_ACK flag set. It also receives the resizer rect so that
  // we don't have to fetch it every time WebKit asks for it.
  IPC_MESSAGE_ROUTED2(ViewMsg_Resize,
                      gfx::Size /* new_size */,
                      gfx::Rect /* resizer_rect */)

  // Sent to inform the view that it was hidden.  This allows it to reduce its
  // resource utilization.
  IPC_MESSAGE_ROUTED0(ViewMsg_WasHidden)

  // Tells the render view that it is no longer hidden (see WasHidden), and the
  // render view is expected to respond with a full repaint if needs_repainting
  // is true.  In that case, the generated ViewHostMsg_PaintRect message will
  // have the IS_RESTORE_ACK flag set.  If needs_repainting is false, then this
  // message does not trigger a message in response.
  IPC_MESSAGE_ROUTED1(ViewMsg_WasRestored,
                      bool /* needs_repainting */)

  // Tells the render view to capture a thumbnail image of the page. The
  // render view responds with a ViewHostMsg_Thumbnail.
  IPC_MESSAGE_ROUTED0(ViewMsg_CaptureThumbnail)

  // Tells the render view that a ViewHostMsg_PaintRect message was processed.
  // This signals the render view that it can send another PaintRect message.
  IPC_MESSAGE_ROUTED0(ViewMsg_PaintRect_ACK)

  // Tells the render view to switch the CSS to print media type, renders every
  // requested pages and switch back the CSS to display media type.
  IPC_MESSAGE_ROUTED0(ViewMsg_PrintPages)

  // Tells the render view that printing is done so it can clean up.
  IPC_MESSAGE_ROUTED2(ViewMsg_PrintingDone,
                      int /* document_cookie */,
                      bool /* success */)

  // Tells the render view that a ViewHostMsg_ScrollRect message was processed.
  // This signals the render view that it can send another ScrollRect message.
  IPC_MESSAGE_ROUTED0(ViewMsg_ScrollRect_ACK)

  // Message payload is a blob that should be cast to WebInputEvent
  IPC_MESSAGE_ROUTED0(ViewMsg_HandleInputEvent)

  // Message payload is the name/value of a WebCore edit command to execute.
  IPC_MESSAGE_ROUTED2(ViewMsg_ExecuteEditCommand,
                      std::string, /* name */
                      std::string /* value */)

  IPC_MESSAGE_ROUTED0(ViewMsg_MouseCaptureLost)

  // TODO(darin): figure out how this meshes with RestoreFocus
  IPC_MESSAGE_ROUTED1(ViewMsg_SetFocus, bool /* enable */)

  // Tells the renderer to focus the first (last if reverse is true) focusable
  // node.
  IPC_MESSAGE_ROUTED1(ViewMsg_SetInitialFocus, bool /* reverse */)

  // Tells the renderer to perform the specified navigation, interrupting any
  // existing navigation.
  IPC_MESSAGE_ROUTED1(ViewMsg_Navigate, ViewMsg_Navigate_Params)

  IPC_MESSAGE_ROUTED0(ViewMsg_Stop)

  // Tells the renderer to load the specified html text and report a navigation
  // to display_url if passing true for new navigation.
  IPC_MESSAGE_ROUTED4(ViewMsg_LoadAlternateHTMLText,
                      std::string /* utf8 html text */,
                      bool, /* new navigation */
                      GURL /* display url */,
                      std::string /* security info */)

  // This message notifies the renderer that the user has closed the FindInPage
  // window (and that the selection should be cleared and the tick-marks
  // erased). If |clear_selection| is true, it will also clear the current
  // selection.
  IPC_MESSAGE_ROUTED1(ViewMsg_StopFinding, bool /* clear_selection */)

  // These messages are typically generated from context menus and request the
  // renderer to apply the specified operation to the current selection.
  IPC_MESSAGE_ROUTED0(ViewMsg_Undo)
  IPC_MESSAGE_ROUTED0(ViewMsg_Redo)
  IPC_MESSAGE_ROUTED0(ViewMsg_Cut)
  IPC_MESSAGE_ROUTED0(ViewMsg_Copy)
  IPC_MESSAGE_ROUTED0(ViewMsg_Paste)
  IPC_MESSAGE_ROUTED1(ViewMsg_Replace, std::wstring)
  IPC_MESSAGE_ROUTED0(ViewMsg_ToggleSpellCheck)
  IPC_MESSAGE_ROUTED0(ViewMsg_Delete)
  IPC_MESSAGE_ROUTED0(ViewMsg_SelectAll)

  // Copies the image at location x, y to the clipboard (if there indeed is an
  // image at that location).
  IPC_MESSAGE_ROUTED2(ViewMsg_CopyImageAt,
                      int /* x */,
                      int /* y */)

  // History system notification that the visited link database has been
  // replaced. It has one SharedMemoryHandle argument consisting of the table
  // handle. This handle is valid in the context of the renderer
  IPC_MESSAGE_CONTROL1(ViewMsg_VisitedLink_NewTable, base::SharedMemoryHandle)

  // History system notification that a link has been added and the link
  // coloring state for the given hash must be re-calculated.
  IPC_MESSAGE_CONTROL1(ViewMsg_VisitedLink_Add, std::vector<uint64>)

  // History system notification that one or more history items have been
  // deleted, which at this point means that all link coloring state must be
  // re-calculated.
  IPC_MESSAGE_CONTROL0(ViewMsg_VisitedLink_Reset)

  // Notification that the user scripts have been updated. It has one
  // SharedMemoryHandle argument consisting of the pickled script data. This
  // handle is valid in the context of the renderer.
  IPC_MESSAGE_CONTROL1(ViewMsg_UserScripts_UpdatedScripts,
                       base::SharedMemoryHandle)

  // Sent when the user wants to search for a word on the page (find in page).
  IPC_MESSAGE_ROUTED3(ViewMsg_Find,
                      int /* request_id */,
                      string16 /* search_text */,
                      WebKit::WebFindOptions)

  // Sent when the headers are available for a resource request.
  IPC_MESSAGE_ROUTED2(ViewMsg_Resource_ReceivedResponse,
                      int /* request_id */,
                      ResourceResponseHead)

  // Sent as download progress is being made, size of the resource may be
  // unknown, in that case |size| is -1.
  IPC_MESSAGE_ROUTED3(ViewMsg_Resource_DownloadProgress,
                      int /* request_id */,
                      int64 /* position */,
                      int64 /* size */)

  // Sent as upload progress is being made.
  IPC_MESSAGE_ROUTED3(ViewMsg_Resource_UploadProgress,
                      int /* request_id */,
                      int64 /* position */,
                      int64 /* size */)

  // Sent when the request has been redirected.
  IPC_MESSAGE_ROUTED2(ViewMsg_Resource_ReceivedRedirect,
                      int /* request_id */,
                      GURL /* new_url */)

  // Sent when some data from a resource request is ready. The handle should
  // already be mapped into the process that receives this message.
  IPC_MESSAGE_ROUTED3(ViewMsg_Resource_DataReceived,
                      int /* request_id */,
                      base::SharedMemoryHandle /* data */,
                      int /* data_len */)

  // Sent when the request has been completed.
  IPC_MESSAGE_ROUTED3(ViewMsg_Resource_RequestComplete,
                      int /* request_id */,
                      URLRequestStatus /* status */,
                      std::string /* security info */)

  // Request for the renderer to evaluate an xpath to a frame and execute a
  // javascript: url in that frame's context. The message is completely
  // asynchronous and no corresponding response message is sent back.
  //
  // frame_xpath contains the modified xpath notation to identify an inner
  // subframe (starting from the root frame). It is a concatenation of
  // number of smaller xpaths delimited by '\n'. Each chunk in the string can
  // be evaluated to a frame in its parent-frame's context.
  //
  // Example: /html/body/iframe/\n/html/body/div/iframe/\n/frameset/frame[0]
  // can be broken into 3 xpaths
  // /html/body/iframe evaluates to an iframe within the root frame
  // /html/body/div/iframe evaluates to an iframe within the level-1 iframe
  // /frameset/frame[0] evaluates to first frame within the level-2 iframe
  //
  // jscript_url is the string containing the javascript: url to be executed
  // in the target frame's context. The string should start with "javascript:"
  // and continue with a valid JS text.
  IPC_MESSAGE_ROUTED2(ViewMsg_ScriptEvalRequest,
                      std::wstring,  /* frame_xpath */
                      std::wstring  /* jscript_url */)

  // Request for the renderer to evaluate an xpath to a frame and insert css
  // into that frame's document. See ViewMsg_ScriptEvalRequest for details on
  // allowed xpath expressions.
  IPC_MESSAGE_ROUTED2(ViewMsg_CSSInsertRequest,
                      std::wstring,  /* frame_xpath */
                      std::string  /* css string */)

  // Log a message to the console of the target frame
  IPC_MESSAGE_ROUTED3(ViewMsg_AddMessageToConsole,
                      string16 /* frame_xpath */,
                      string16 /* message */,
                      WebKit::WebConsoleMessage::Level /* message_level */)

  // RenderViewHostDelegate::RenderViewCreated method sends this message to a
  // new renderer to notify it that it will host developer tools UI and should
  // set up all neccessary bindings and create DevToolsClient instance that
  // will handle communication with inspected page DevToolsAgent.
  IPC_MESSAGE_ROUTED0(ViewMsg_SetupDevToolsClient)

  // Change the zoom level in the renderer.
  IPC_MESSAGE_ROUTED1(ViewMsg_Zoom,
                      int /* One of PageZoom::Function */)

  // Insert text in the currently focused input area.
  IPC_MESSAGE_ROUTED1(ViewMsg_InsertText,
                      string16  /* text */)

  // Change encoding of page in the renderer.
  IPC_MESSAGE_ROUTED1(ViewMsg_SetPageEncoding,
                      std::wstring /*new encoding name*/)

  // Requests the renderer to reserve a range of page ids.
  IPC_MESSAGE_ROUTED1(ViewMsg_ReservePageIDRange,
                      int  /* size_of_range */)

  // Fill a form with data and optionally submit it
  IPC_MESSAGE_ROUTED1(ViewMsg_FormFill,
                      FormData /* form */)

  // Fill a password form and prepare field autocomplete for multiple
  // matching logins.
  IPC_MESSAGE_ROUTED1(ViewMsg_FillPasswordForm,
                      webkit_glue::PasswordFormDomManager::FillData)

  // D&d drop target messages.
  IPC_MESSAGE_ROUTED3(ViewMsg_DragTargetDragEnter,
                      WebDropData /* drop_data */,
                      gfx::Point /* client_pt */,
                      gfx::Point /* screen_pt */)
  IPC_MESSAGE_ROUTED2(ViewMsg_DragTargetDragOver,
                      gfx::Point /* client_pt */,
                      gfx::Point /* screen_pt */)
  IPC_MESSAGE_ROUTED0(ViewMsg_DragTargetDragLeave)
  IPC_MESSAGE_ROUTED2(ViewMsg_DragTargetDrop,
                      gfx::Point /* client_pt */,
                      gfx::Point /* screen_pt */)

  IPC_MESSAGE_ROUTED1(ViewMsg_UploadFile, ViewMsg_UploadFile_Params)

  // Notifies the renderer of updates in mouse position of an in-progress
  // drag.  if |ended| is true, then the user has ended the drag operation.
  IPC_MESSAGE_ROUTED4(ViewMsg_DragSourceEndedOrMoved,
                      gfx::Point /* client_pt */,
                      gfx::Point /* screen_pt */,
                      bool /* ended */,
                      bool /* cancelled */)

  // Notifies the renderer that the system DoDragDrop call has ended.
  IPC_MESSAGE_ROUTED0(ViewMsg_DragSourceSystemDragEnded)

  // Used to tell a render view whether it should expose various bindings
  // that allow JS content extended privileges.  See BindingsPolicy for valid
  // flag values.
  IPC_MESSAGE_ROUTED1(ViewMsg_AllowBindings,
                      int /* enabled_bindings_flags */)

  // Tell the renderer to add a property to the DOMUI binding object.  This
  // only works if we allowed DOMUI bindings.
  IPC_MESSAGE_ROUTED2(ViewMsg_SetDOMUIProperty,
                      std::string /* property_name */,
                      std::string /* property_value_json */)

  // This message starts/stop monitoring the status of the focused edit
  // control of a renderer process.
  // Parameters
  // * is_active (bool)
  //   Represents whether or not the IME is active in a browser process.
  //   The possible actions when a renderer process receives this message are
  //   listed below:
  //     Value Action
  //     true  Start sending IPC messages, ViewHostMsg_ImeUpdateStatus
  //           to notify the status of the focused edit control.
  //     false Stop sending IPC messages, ViewHostMsg_ImeUpdateStatus.
  IPC_MESSAGE_ROUTED1(ViewMsg_ImeSetInputMode,
                      bool /* is_active */)

  // This message sends a string being composed with IME.
  // Parameters
  // * string_type (int)
  //   Represents the type of the 'ime_string' parameter.
  //   Its possible values and description are listed below:
  //     Value         Description
  //     -1            The parameter is not used.
  //     1             The parameter represents a result string.
  //     0             The parameter represents a composition string.
  // * cursor_position (int)
  //   Represents the position of the cursor
  // * target_start (int)
  //   Represents the position of the beginning of the selection
  // * target_end (int)
  //   Represents the position of the end of the selection
  // * ime_string (std::wstring)
  //   Represents the string retrieved from IME (Input Method Editor)
  IPC_MESSAGE_ROUTED5(ViewMsg_ImeSetComposition,
                      int, /* string_type */
                      int, /* cursor_position */
                      int, /* target_start */
                      int, /* target_end */
                      std::wstring /* ime_string */ )

  // This passes a set of webkit preferences down to the renderer.
  IPC_MESSAGE_ROUTED1(ViewMsg_UpdateWebPreferences, WebPreferences)

  // Used to notify the render-view that the browser has received a reply for
  // the Find operation and is interested in receiving the next one. This is
  // used to prevent the renderer from spamming the browser process with
  // results.
  IPC_MESSAGE_ROUTED0(ViewMsg_FindReplyACK)

  // Used to notify the render-view that we have received a target URL. Used
  // to prevent target URLs spamming the browser.
  IPC_MESSAGE_ROUTED0(ViewMsg_UpdateTargetURL_ACK)

  // Sets the alternate error page URL (link doctor) for the renderer process.
  IPC_MESSAGE_ROUTED1(ViewMsg_SetAltErrorPageURL, GURL)

  // Install the first missing pluign.
  IPC_MESSAGE_ROUTED0(ViewMsg_InstallMissingPlugin)

  // Tells the renderer to empty its plugin list cache.
  IPC_MESSAGE_CONTROL0(ViewMsg_PurgePluginListCache)

  IPC_MESSAGE_ROUTED1(ViewMsg_RunFileChooserResponse,
                      std::vector<FilePath> /* selected files */)

  // Used to instruct the RenderView to go into "view source" mode.
  IPC_MESSAGE_ROUTED0(ViewMsg_EnableViewSourceMode)

  IPC_MESSAGE_ROUTED2(ViewMsg_UpdateBackForwardListCount,
                      int /* back_list_count */,
                      int /* forward_list_count */)

  // Retreive information from the MSAA DOM subtree, for accessibility purposes.
  IPC_SYNC_MESSAGE_ROUTED1_1(ViewMsg_GetAccessibilityInfo,
                             webkit_glue::WebAccessibility::InParams
                             /* input parameters */,
                             webkit_glue::WebAccessibility::OutParams
                             /* output parameters */)

  // Requests the renderer to clear cashed accessibility information. Takes an
  // id to clear a specific hashmap entry, and a bool; true clears all, false
  // does not.
  IPC_MESSAGE_ROUTED2(ViewMsg_ClearAccessibilityInfo,
                      int  /* accessibility object id */,
                      bool /* clear_all */)

  // Get all savable resource links from current webpage, include main
  // frame and sub-frame.
  IPC_MESSAGE_ROUTED1(ViewMsg_GetAllSavableResourceLinksForCurrentPage,
                      GURL /* url of page which is needed to save */)

  // Get html data by serializing all frames of current page with lists
  // which contain all resource links that have local copy.
  IPC_MESSAGE_ROUTED3(ViewMsg_GetSerializedHtmlDataForCurrentPageWithLocalLinks,
                      std::vector<GURL> /* urls that have local copy */,
                      std::vector<FilePath> /* paths of local copy */,
                      FilePath /* local directory path */)

  // Requests application info for the page. The renderer responds back with
  // ViewHostMsg_DidGetApplicationInfo.
  IPC_MESSAGE_ROUTED1(ViewMsg_GetApplicationInfo, int32 /*page_id*/)

  // Requests the renderer to download the specified image encode it as PNG
  // and send the PNG data back ala ViewHostMsg_DidDownloadImage.
  IPC_MESSAGE_ROUTED3(ViewMsg_DownloadImage,
                      int /* identifier for the request */,
                      GURL /* URL of the image */,
                      int /* Size of the image. Normally 0, but set if you have
                             a preferred image size to request, such as when
                             downloading the favicon */)

  // When a renderer sends a ViewHostMsg_Focus to the browser process,
  // the browser has the option of sending a ViewMsg_CantFocus back to
  // the renderer.
  IPC_MESSAGE_ROUTED0(ViewMsg_CantFocus)

  // Instructs the renderer to invoke the frame's shouldClose method, which
  // runs the onbeforeunload event handler.  Expects the result to be returned
  // via ViewHostMsg_ShouldClose.
  IPC_MESSAGE_ROUTED0(ViewMsg_ShouldClose)

  // Instructs the renderer to close the current page, including running the
  // onunload event handler.  Expects a ClosePage_ACK message when finished.
  IPC_MESSAGE_ROUTED2(ViewMsg_ClosePage,
                      int /* new_render_process_host_id */,
                      int /* new_request_id */)

  // Asks the renderer to send back stats on the WebCore cache broken down by
  // resource types.
  IPC_MESSAGE_CONTROL0(ViewMsg_GetCacheResourceStats)

  // Asks the renderer to send back Histograms.
  IPC_MESSAGE_CONTROL1(ViewMsg_GetRendererHistograms,
                       int /* sequence number of Renderer Histograms. */)

  // Notifies the renderer about ui theme changes
  IPC_MESSAGE_ROUTED0(ViewMsg_ThemeChanged)

  // Notifies the renderer that a paint is to be generated for the rectangle
  // passed in.
  IPC_MESSAGE_ROUTED1(ViewMsg_Repaint,
                      gfx::Size /* The view size to be repainted */)

  // Posts a message to the renderer.
  IPC_MESSAGE_ROUTED3(ViewMsg_HandleMessageFromExternalHost,
                      std::string /* The message */,
                      std::string /* The origin */,
                      std::string /* The target*/)

  // Sent to the renderer when a popup window should no longer count against
  // the current popup count (either because it's not a popup or because it was
  // a generated by a user action or because a constrained popup got turned
  // into a full window).
  IPC_MESSAGE_ROUTED0(ViewMsg_DisassociateFromPopupCount)

  // Notifies the renderer of the AppCache that has been selected for a
  // a particular context (or frame). This is sent in reply to
  // one of the two AppCacheMsg_SelectAppCache messages.
  IPC_MESSAGE_CONTROL3(AppCacheMsg_AppCacheSelected,
                       int /* context_id */,
                       int /* select_request_id */,
                       int64 /* cache_id */)

  // Reply to the ViewHostMsg_QueryFormFieldAutofill message with the autofill
  // suggestions.
  IPC_MESSAGE_ROUTED4(ViewMsg_AutofillSuggestions,
                      int64 /* id of the text input field */,
                      int /* id of the request message */,
                      std::vector<std::wstring> /* suggestions */,
                      int /* index of default suggestion */)

  // Sent by the Browser process to alert a window about whether a blocked
  // popup notification is visible. The renderer assumes every new window is a
  // blocked popup until notified otherwise.
  IPC_MESSAGE_ROUTED1(ViewMsg_PopupNotificationVisibilityChanged,
                      bool /* Whether it is visible */)

  // Sent by AudioRendererHost to renderer to request an audio packet.
  IPC_MESSAGE_ROUTED3(ViewMsg_RequestAudioPacket,
                      int /* stream id */,
                      size_t /* bytes in buffer */,
                      int64 /* message timestamp */)

  // Tell the renderer process that the audio stream has been created, renderer
  // process would be given a ShareMemoryHandle that it should write to from
  // then on.
  IPC_MESSAGE_ROUTED3(ViewMsg_NotifyAudioStreamCreated,
                      int /* stream id */,
                      base::SharedMemoryHandle /* handle */,
                      int /* length */)

  // Notification message sent from AudioRendererHost to renderer for state
  // update after the renderer has requested a Create/Start/Close.
  IPC_MESSAGE_ROUTED3(ViewMsg_NotifyAudioStreamStateChanged,
                      int /* stream id */,
                      AudioOutputStream::State /* new state */,
                      int /* additional information (e.g. platform specific
                             error code*/)

  IPC_MESSAGE_ROUTED3(ViewMsg_NotifyAudioStreamVolume,
                      int /* stream id */,
                      double /* left channel */,
                      double /* right channel */)

  // Notification that a move or resize renderer's containing window has
  // started.
  IPC_MESSAGE_ROUTED0(ViewMsg_MoveOrResizeStarted)

  // The browser sends this message in response to all extension api calls.
  IPC_MESSAGE_ROUTED4(ViewMsg_ExtensionResponse,
                      int /* request_id */,
                      bool /* success */,
                      std::string /* response */,
                      std::string /* error */)

  // Call a javascript function in every registered context in this process.
  // |args| is a list of primitive Value types that are passed to the function.
  IPC_MESSAGE_CONTROL2(ViewMsg_ExtensionMessageInvoke,
                       std::string /* function_name */,
                       ListValue /* args */)

  // Tell the renderer process all known extension function names.
  IPC_MESSAGE_CONTROL1(ViewMsg_Extension_SetFunctionNames,
                       std::vector<std::string>)

  // Changes the text direction of a selected input field.
  // * direction (int)
  //   Represents the new text direction.
  //   Its possible values are listed below:
  //     Value                      New Text Direction
  //     WEB_TEXT_DIRECTION_DEFAULT NaturalWritingDirection ("inherit")
  //     WEB_TEXT_DIRECTION_LTR     LeftToRightWritingDirection ("rtl")
  //     WEB_TEXT_DIRECTION_RTL     RightToLeftWritingDirection ("ltr")
  IPC_MESSAGE_ROUTED1(ViewMsg_SetTextDirection,
                      int /* direction */)

  // Tells the renderer to clear the focused node (if any).
  IPC_MESSAGE_ROUTED0(ViewMsg_ClearFocusedNode)

  // Make the RenderView transparent and render it onto a custom background. The
  // background will be tiled in both directions if it is not large enough.
  IPC_MESSAGE_ROUTED1(ViewMsg_SetBackground,
                      SkBitmap /* background */)

  // Reply to ViewHostMsg_RequestMove, ViewHostMsg_ShowView, and
  // ViewHostMsg_ShowWidget to inform the renderer that the browser has
  // processed the move.  The browser may have ignored the move, but it finished
  // processing.  This is used because the renderer keeps a temporary cache of
  // the widget position while these asynchronous operations are in progress.
  IPC_MESSAGE_ROUTED0(ViewMsg_Move_ACK)

  // Used to instruct the RenderView to send back updates to the intrinsic
  // width.
  IPC_MESSAGE_ROUTED0(ViewMsg_EnableIntrinsicWidthChangedMode)

  //---------------------------------------------------------------------------
  // Utility process messages:
  // These are messages from the browser to the utility process.  They're here
  // because we ran out of spare message types.

  // Tell the utility process to unpack the given extension file in its
  // directory and verify that it is valid.
  IPC_MESSAGE_CONTROL1(UtilityMsg_UnpackExtension,
                       FilePath /* extension_filename */)

  // Response message to ViewHostMsg_CreateDedicatedWorker.  Sent when the
  // worker has started.
  IPC_MESSAGE_ROUTED0(ViewMsg_DedicatedWorkerCreated)

  // Tell the utility process to parse the given JSON data and verify its
  // validity.
  IPC_MESSAGE_CONTROL1(UtilityMsg_UnpackWebResource,
                       std::string /* JSON data */)

IPC_END_MESSAGES(View)


//-----------------------------------------------------------------------------
// TabContents messages
// These are messages sent from the renderer to the browser process.

IPC_BEGIN_MESSAGES(ViewHost)
  // Sent by the renderer when it is creating a new window.  The browser creates
  // a tab for it and responds with a ViewMsg_CreatingNew_ACK.  If route_id is
  // MSG_ROUTING_NONE, the view couldn't be created.  modal_dialog_event is set
  // by the browser when a modal dialog is shown.
  IPC_SYNC_MESSAGE_CONTROL2_2(ViewHostMsg_CreateWindow,
                              int /* opener_id */,
                              bool /* user_gesture */,
                              int /* route_id */,
                              ModalDialogEvent /* modal_dialog_event */)

  // Similar to ViewHostMsg_CreateWindow, except used for sub-widgets, like
  // <select> dropdowns.  This message is sent to the TabContents that
  // contains the widget being created.
  IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_CreateWidget,
                              int /* opener_id */,
                              bool /* focus on show */,
                              int /* route_id */)

  // These two messages are sent to the parent RenderViewHost to display the
  // page/widget that was created by CreateWindow/CreateWidget.  routing_id
  // refers to the id that was returned from the Create message above.
  // The initial_position parameter is a rectangle in screen coordinates.
  //
  // FUTURE: there will probably be flags here to control if the result is
  // in a new window.
  IPC_MESSAGE_ROUTED5(ViewHostMsg_ShowView,
                      int /* route_id */,
                      WindowOpenDisposition /* disposition */,
                      gfx::Rect /* initial_pos */,
                      bool /* opened_by_user_gesture */,
                      GURL /* creator_url */)

  IPC_MESSAGE_ROUTED2(ViewHostMsg_ShowWidget,
                      int /* route_id */,
                      gfx::Rect /* initial_pos */)

  // This message is sent after ViewHostMsg_ShowView to cause the RenderView
  // to run in a modal fashion until it is closed.
  IPC_SYNC_MESSAGE_ROUTED0_0(ViewHostMsg_RunModal)

  IPC_MESSAGE_CONTROL1(ViewHostMsg_UpdatedCacheStats,
                       WebKit::WebCache::UsageStats /* stats */)

  // Indicates the renderer is ready in response to a ViewMsg_New or
  // a ViewMsg_CreatingNew_ACK.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_RenderViewReady)

  // Indicates the renderer process is gone.  This actually is sent by the
  // browser process to itself, but keeps the interface cleaner.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_RenderViewGone)

  // Sent by the renderer process to request that the browser close the view.
  // This corresponds to the window.close() API, and the browser may ignore
  // this message.  Otherwise, the browser will generates a ViewMsg_Close
  // message to close the view.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_Close)

  // Sent by the renderer process to request that the browser move the view.
  // This corresponds to the window.resizeTo() and window.moveTo() APIs, and
  // the browser may ignore this message.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_RequestMove,
                      gfx::Rect /* position */)

  // Notifies the browser that a frame in the view has changed. This message
  // has a lot of parameters and is packed/unpacked by functions defined in
  // render_messages.h.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_FrameNavigate,
                      ViewHostMsg_FrameNavigate_Params)

  // Notifies the browser that we have session history information.
  // page_id: unique ID that allows us to distinguish between history entries.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateState,
                      int32 /* page_id */,
                      std::string /* state */)

  // Notifies the browser that a document has been loaded in a frame.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_DocumentLoadedInFrame)

  // Changes the title for the page in the UI when the page is navigated or the
  // title changes.
  // TODO(darin): use a UTF-8 string to reduce data size
  IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateTitle, int32, std::wstring)

  // Change the encoding name of the page in UI when the page has detected
  // proper encoding name.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateEncoding,
                      std::wstring /* new encoding name */)

  // Notifies the browser that we want to show a destination url for a potential
  // action (e.g. when the user is hovering over a link).
  IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateTargetURL, int32, GURL)

  // Sent when the renderer starts loading the page. This corresponds to
  // WebKit's notion of the throbber starting. Note that sometimes you may get
  // duplicates of these during a single load.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_DidStartLoading)

  // Sent when the renderer is done loading a page. This corresponds to WebKit's
  // noption of the throbber stopping.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_DidStopLoading)

  // Sent when the renderer loads a resource from its memory cache.
  // The security info is non empty if the resource was originally loaded over
  // a secure connection.
  // Note: May only be sent once per URL per frame per committed load.
  IPC_MESSAGE_ROUTED4(ViewHostMsg_DidLoadResourceFromMemoryCache,
                      GURL /* url */,
                      std::string  /* frame_origin */,
                      std::string  /* main_frame_origin */,
                      std::string  /* security info */)

  // Sent when the renderer starts a provisional load for a frame.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_DidStartProvisionalLoadForFrame,
                      bool /* true if it is the main frame */,
                      GURL /* url */)

  // Sent when the renderer fails a provisional load with an error.
  IPC_MESSAGE_ROUTED4(ViewHostMsg_DidFailProvisionalLoadWithError,
                      bool /* true if it is the main frame */,
                      int /* error_code */,
                      GURL /* url */,
                      bool /* true if the failure is the result of
                              navigating to a POST again and we're going to
                              show the POST interstitial */ )

  // Sent to paint part of the view.  In response to this message, the host
  // generates a ViewMsg_PaintRect_ACK message.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_PaintRect,
                      ViewHostMsg_PaintRect_Params)

  // Sent to scroll part of the view.  In response to this message, the host
  // generates a ViewMsg_ScrollRect_ACK message.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_ScrollRect,
                      ViewHostMsg_ScrollRect_Params)

  // Acknowledges receipt of a ViewMsg_HandleInputEvent message.
  // Payload is a WebInputEvent::Type which is the type of the event, followed
  // by an optional WebInputEvent which is provided only if the event was not
  // processed.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_HandleInputEvent_ACK)

  IPC_MESSAGE_ROUTED0(ViewHostMsg_Focus)
  IPC_MESSAGE_ROUTED0(ViewHostMsg_Blur)

  // Returns the window location of the given window.
  // TODO(shess): Provide a mapping from reply_msg->routing_id() to
  // HWND so that we can eliminate the NativeViewId parameter.
  IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_GetWindowRect,
                             gfx::NativeViewId /* window */,
                             gfx::Rect /* Out: Window location */)

  IPC_MESSAGE_ROUTED1(ViewHostMsg_SetCursor, WebCursor)
  // Result of string search in the page.
  // Response to ViewMsg_Find with the results of the requested find-in-page
  // search, the number of matches found and the selection rect (in screen
  // coordinates) for the string found. If |final_update| is false, it signals
  // that this is not the last Find_Reply message - more will be sent as the
  // scoping effort continues.
  IPC_MESSAGE_ROUTED5(ViewHostMsg_Find_Reply,
                      int /* request_id */,
                      int /* number of matches */,
                      gfx::Rect /* selection_rect */,
                      int /* active_match_ordinal */,
                      bool /* final_update */)

  // Makes a resource request via the browser.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_RequestResource,
                      int /* request_id */,
                      ViewHostMsg_Resource_Request)

  // Cancels a resource request with the ID given as the parameter.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_CancelRequest,
                      int /* request_id */)

  // Makes a synchronous resource request via the browser.
  IPC_SYNC_MESSAGE_ROUTED2_1(ViewHostMsg_SyncLoad,
                             int /* request_id */,
                             ViewHostMsg_Resource_Request,
                             SyncLoadResult)

  // Used to set a cookie.  The cookie is set asynchronously, but will be
  // available to a subsequent ViewHostMsg_GetCookies request.
  IPC_MESSAGE_CONTROL3(ViewHostMsg_SetCookie,
                       GURL /* url */,
                       GURL /* first_party_for_cookies */,
                       std::string /* cookie */)

  // Used to get cookies for the given URL
  IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_GetCookies,
                              GURL /* url */,
                              GURL /* first_party_for_cookies */,
                              std::string /* cookies */)

  // Used to get the list of plugins
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_GetPlugins,
                              bool /* refresh*/,
                              std::vector<WebPluginInfo> /* plugins */)

  // Returns a path to a plugin for the given url and mime type.  If there's
  // no plugin, an empty string is returned.
  IPC_SYNC_MESSAGE_CONTROL4_2(ViewHostMsg_GetPluginPath,
                              GURL /* url */,
                              GURL /* policy_url */,
                              std::string /* mime_type */,
                              std::string /* clsid */,
                              FilePath /* filename */,
                              std::string /* actual mime type for url */)

  // Retrieve the data directory associated with the renderer's profile.
  IPC_SYNC_MESSAGE_CONTROL0_1(ViewHostMsg_GetDataDir,
                              std::wstring /* data_dir_retval */)

  // Allows a chrome plugin loaded in a renderer process to send arbitrary
  // data to an instance of the same plugin loaded in the browser process.
  IPC_MESSAGE_CONTROL2(ViewHostMsg_PluginMessage,
                       FilePath /* plugin_path of plugin */,
                       std::vector<uint8> /* opaque data */)

  // Allows a chrome plugin loaded in a renderer process to send arbitrary
  // data to an instance of the same plugin loaded in the browser process.
  IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_PluginSyncMessage,
                              FilePath /* plugin_path of plugin */,
                              std::vector<uint8> /* opaque data */,
                              std::vector<uint8> /* opaque data */)

  // Requests spellcheck for a word.
  IPC_SYNC_MESSAGE_ROUTED1_2(ViewHostMsg_SpellCheck,
                             std::wstring /* word to check */,
                             int /* misspell location */,
                             int /* misspell length */)

  IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_GetAutoCorrectWord,
                             std::wstring /* word to check */,
                             std::wstring /* autocorrected word */)

  // Initiate a download based on user actions like 'ALT+click'.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_DownloadUrl,
                      GURL /* url */,
                      GURL /* referrer */)

  // Used to go to the session history entry at the given offset (ie, -1 will
  // return the "back" item).
  IPC_MESSAGE_ROUTED1(ViewHostMsg_GoToEntryAtOffset,
                      int /* offset (from current) of history item to get */)

  IPC_SYNC_MESSAGE_ROUTED4_2(ViewHostMsg_RunJavaScriptMessage,
                             std::wstring /* in - alert message */,
                             std::wstring /* in - default prompt */,
                             GURL         /* in - originating page URL */,
                             int          /* in - dialog flags */,
                             bool         /* out - success */,
                             std::wstring /* out - prompt field */)

  // Sets the contents for the given page (URL and page ID are the first two
  // arguments) given the contents that is the 3rd.
  IPC_MESSAGE_CONTROL3(ViewHostMsg_PageContents, GURL, int32, std::wstring)

  // Specifies the URL as the first parameter (a wstring) and thumbnail as
  // binary data as the second parameter.
  IPC_MESSAGE_ROUTED3(ViewHostMsg_Thumbnail,
                      GURL /* url */,
                      ThumbnailScore /* score */,
                      SkBitmap /* bitmap */)

  // Notification that the url for the favicon of a site has been determined.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateFavIconURL,
                      int32 /* page_id */,
                      GURL /* url of the favicon */)

  // Request that the browser get the text from the selection clipboard and send
  // it back to the renderer via ViewMsg_SelectionClipboardResponse.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_PasteFromSelectionClipboard)

  // Used to tell the parent that the user right clicked on an area of the
  // content area, and a context menu should be shown for it. The params
  // object contains information about the node(s) that were selected when the
  // user right clicked.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_ContextMenu, ContextMenuParams)

  // Request that the given URL be opened in the specified manner.
  IPC_MESSAGE_ROUTED3(ViewHostMsg_OpenURL,
                      GURL /* url */,
                      GURL /* referrer */,
                      WindowOpenDisposition /* disposition */)

  IPC_MESSAGE_ROUTED1(ViewHostMsg_DidContentsPreferredWidthChange,
                      int /* pref_width */)

  // Following message is used to communicate the values received by the
  // callback binding the JS to Cpp.
  // An instance of browser that has an automation host listening to it can
  // have a javascript send a native value (string, number, boolean) to the
  // listener in Cpp. (DomAutomationController)
  IPC_MESSAGE_ROUTED2(ViewHostMsg_DomOperationResponse,
                      std::string  /* json_string */,
                      int  /* automation_id */)

  // A message from HTML-based UI.  When (trusted) Javascript calls
  // send(message, args), this message is sent to the browser.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_DOMUISend,
                      std::string  /* message */,
                      std::string  /* args (as a JSON string) */)

  // A message for an external host.
  IPC_MESSAGE_ROUTED3(ViewHostMsg_ForwardMessageToExternalHost,
                      std::string  /* message */,
                      std::string  /* origin */,
                      std::string  /* target */)

  // A renderer sends this to the browser process when it wants to
  // create a plugin.  The browser will create the plugin process if
  // necessary, and will return a handle to the channel on success.
  // On error an empty string is returned.
  IPC_SYNC_MESSAGE_CONTROL4_2(ViewHostMsg_OpenChannelToPlugin,
                              GURL /* url */,
                              std::string /* mime_type */,
                              std::string /* clsid */,
                              std::wstring /* locale */,
                              IPC::ChannelHandle /* handle to channel */,
                              FilePath /* plugin_path */)

#if defined(OS_LINUX)
  IPC_SYNC_MESSAGE_ROUTED0_1(ViewHostMsg_CreatePluginContainer,
                             gfx::PluginWindowHandle /* container */)
  IPC_SYNC_MESSAGE_ROUTED1_0(ViewHostMsg_DestroyPluginContainer,
                             gfx::PluginWindowHandle /* container */)
#endif

  // Clipboard IPC messages

  // This message is used when the object list does not contain a bitmap.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_ClipboardWriteObjectsAsync,
      Clipboard::ObjectMap /* objects */)
  // This message is used when the object list contains a bitmap.
  // It is synchronized so that the renderer knows when it is safe to
  // free the shared memory used to transfer the bitmap.
  IPC_SYNC_MESSAGE_CONTROL1_0(ViewHostMsg_ClipboardWriteObjectsSync,
      Clipboard::ObjectMap /* objects */)
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_ClipboardIsFormatAvailable,
                              std::string /* format */,
                              bool /* result */)
  IPC_SYNC_MESSAGE_CONTROL0_1(ViewHostMsg_ClipboardReadText,
                              string16 /* result */)
  IPC_SYNC_MESSAGE_CONTROL0_1(ViewHostMsg_ClipboardReadAsciiText,
                              std::string /* result */)
  IPC_SYNC_MESSAGE_CONTROL0_2(ViewHostMsg_ClipboardReadHTML,
                              string16 /* markup */,
                              GURL /* url */)

#if defined(OS_WIN)
  // Request that the given font be loaded by the browser.
  // Please see ResourceMessageFilter::OnLoadFont for details.
  IPC_SYNC_MESSAGE_CONTROL1_0(ViewHostMsg_LoadFont,
                              LOGFONT /* font data */)
#endif  // defined(OS_WIN)

  // Returns WebScreenInfo corresponding to the view.
  // TODO(shess): Provide a mapping from reply_msg->routing_id() to
  // HWND so that we can eliminate the NativeViewId parameter.
  IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_GetScreenInfo,
                             gfx::NativeViewId /* view */,
                             WebKit::WebScreenInfo /* results */)

  // Send the tooltip text for the current mouse position to the browser.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_SetTooltipText,
                      std::wstring /* tooltip text string */)

  // Notification that the text selection has changed.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_SelectionChanged,
                      std::string /* currently selected text */)

  // Asks the browser to display the file chooser.  The result is returned in a
  // ViewHost_RunFileChooserResponse message.
  IPC_MESSAGE_ROUTED3(ViewHostMsg_RunFileChooser,
                      bool /* multiple_files */,
                      string16 /* title */,
                      FilePath /* Default file name */)

  // Notification that password forms have been seen that are candidates for
  // filling/submitting by the password manager
  IPC_MESSAGE_ROUTED1(ViewHostMsg_PasswordFormsSeen,
                      std::vector<webkit_glue::PasswordForm> /* forms */)

  // Notification that a form has been submitted.  The user hit the button.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_AutofillFormSubmitted,
                      webkit_glue::AutofillForm /* form */)

  // Used to tell the parent the user started dragging in the content area. The
  // WebDropData struct contains contextual information about the pieces of the
  // page the user dragged. The parent uses this notification to initiate a
  // drag session at the OS level.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_StartDragging,
                      WebDropData /* drop_data */)

  // The page wants to update the mouse cursor during a drag & drop operation.
  // |is_drop_target| is true if the mouse is over a valid drop target.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateDragCursor,
                      bool /* is_drop_target */)

  // Tells the browser to move the focus to the next (previous if reverse is
  // true) focusable element.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_TakeFocus, bool /* reverse */)

  // Notification that the page has an OpenSearch description document
  // associated with it.
  IPC_MESSAGE_ROUTED3(ViewHostMsg_PageHasOSDD,
                      int32 /* page_id */,
                      GURL /* url of OS description document */,
                      bool /* autodetected */)

  // required for synchronizing IME windows.
  // Parameters
  // * control (ViewHostMsg_ImeControl)
  //   It specifies the code for controlling the IME attached to
  //   the browser process. This parameter should be one of the values
  //   listed below.
  //     + IME_DISABLE
  //       Deactivate the IME attached to a browser process.
  //       This code is typically used for notifying a renderer process
  //       moves its input focus to a password input. A browser process
  //       finishes the current composition and deactivate IME.
  //       If a renderer process sets its input focus to another edit
  //       control which is not a password input, it needs to re-activate
  //       IME, it has to send another message with this code IME_MOVE_WINDOWS
  //       and set the new caret position.
  //     + IME_MOVE_WINDOWS
  //       Activate the IME attached to a browser process and set the position
  //       of its IME windows.
  //       This code is typically used for the following cases:
  //         - Notifying a renderer process moves the caret position of the
  //           focused edit control, or;
  //         - Notifying a renderer process moves its input focus from a
  //           password input to an editable control which is NOT a password
  //           input.
  //           A renderer process also has to set caret_rect and
  //           specify the new caret rectangle.
  //     + IME_COMPLETE_COMPOSITION
  //       Finish the current composition.
  //       This code is used for notifying a renderer process moves its
  //       input focus from an editable control being composed to another one
  //       which is NOT a password input. A browser process closes its IME
  //       windows without changing the activation status of its IME, i.e. it
  //       keeps activating its IME.
  // * caret_rect (gfx::Rect)
  //   They specify the rectangle of the input caret.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_ImeUpdateStatus,
                      ViewHostMsg_ImeControl, /* control */
                      gfx::Rect /* caret_rect */)

  // Tells the browser that the renderer is done calculating the number of
  // rendered pages according to the specified settings.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_DidGetPrintedPagesCount,
                      int /* rendered document cookie */,
                      int /* number of rendered pages */)

  // Sends back to the browser the rendered "printed page" that was requested by
  // a ViewMsg_PrintPage message or from scripted printing. The memory handle in
  // this message is already valid in the browser process.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_DidPrintPage,
                      ViewHostMsg_DidPrintPage_Params /* page content */)

  // The renderer wants to know the default print settings.
  IPC_SYNC_MESSAGE_ROUTED0_1(ViewHostMsg_GetDefaultPrintSettings,
                             ViewMsg_Print_Params /* default_settings */)

#if defined(OS_WIN)
  // It's the renderer that controls the printing process when it is generated
  // by javascript. This step is about showing UI to the user to select the
  // final print settings. The output parameter is the same as
  // ViewMsg_PrintPages which is executed implicitly.
  IPC_SYNC_MESSAGE_ROUTED4_1(ViewHostMsg_ScriptedPrint,
                             gfx::NativeViewId /* host_window */,
                             int /* cookie */,
                             int /* expected_pages_count */,
                             bool /* has_selection */,
                             ViewMsg_PrintPages_Params /* settings choosen by
                                                          the user*/)
#endif  // defined(OS_WIN)

  // WebKit and JavaScript error messages to log to the console
  // or debugger UI.
  IPC_MESSAGE_ROUTED3(ViewHostMsg_AddMessageToConsole,
                      std::wstring, /* msg */
                      int32, /* line number */
                      std::wstring /* source id */)

  // Stores new inspector settings in the profile.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateInspectorSettings,
                      std::wstring  /* raw_settings */)

  // Wraps an IPC message that's destined to the DevToolsClient on
  // DevToolsAgent->browser hop.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_ForwardToDevToolsClient,
                      IPC::Message /* one of DevToolsClientMsg_XXX types */)

  // Wraps an IPC message that's destined to the DevToolsAgent on
  // DevToolsClient->browser hop.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_ForwardToDevToolsAgent,
                      IPC::Message /* one of DevToolsAgentMsg_XXX types */)

  // Activates (brings to the front) corresponding dev tools window.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_ActivateDevToolsWindow)

  // Closes dev tools window that is inspecting current render_view_host.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_CloseDevToolsWindow)

  // Attaches dev tools window that is inspecting current render_view_host.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_DockDevToolsWindow)

  // Detaches dev tools window that is inspecting current render_view_host.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_UndockDevToolsWindow)

  // Send back a string to be recorded by UserMetrics.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_UserMetricsRecordAction,
                      std::wstring /* action */)

  // Send back histograms as vector of pickled-histogram strings.
  IPC_MESSAGE_CONTROL2(ViewHostMsg_RendererHistograms,
                       int, /* sequence number of Renderer Histograms. */
                       std::vector<std::string>)

  // Request for a DNS prefetch of the names in the array.
  // NameList is typedef'ed std::vector<std::string>
  IPC_MESSAGE_CONTROL1(ViewHostMsg_DnsPrefetch,
                      std::vector<std::string> /* hostnames */)

  // Notifies when default plugin updates status of the missing plugin.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_MissingPluginStatus,
                      int /* status */)

  // Sent by the renderer process to indicate that a plugin instance has
  // crashed.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_CrashedPlugin,
                      FilePath /* plugin_path */)

  // Displays a JavaScript out-of-memory message in the infobar.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_JSOutOfMemory)

  // Displays a box to confirm that the user wants to navigate away from the
  // page. Replies true if yes, false otherwise, the reply string is ignored,
  // but is included so that we can use OnJavaScriptMessageBoxClosed.
  IPC_SYNC_MESSAGE_ROUTED2_2(ViewHostMsg_RunBeforeUnloadConfirm,
                             GURL,        /* in - originating frame URL */
                             std::wstring /* in - alert message */,
                             bool         /* out - success */,
                             std::wstring /* out - This is ignored.*/)

  IPC_MESSAGE_ROUTED3(ViewHostMsg_SendCurrentPageAllSavableResourceLinks,
                      std::vector<GURL> /* all savable resource links */,
                      std::vector<GURL> /* all referrers of resource links */,
                      std::vector<GURL> /* all frame links */)

  IPC_MESSAGE_ROUTED3(ViewHostMsg_SendSerializedHtmlData,
                      GURL /* frame's url */,
                      std::string /* data buffer */,
                      int32 /* complete status */)

  IPC_SYNC_MESSAGE_ROUTED4_1(ViewHostMsg_ShowModalHTMLDialog,
                             GURL /* url */,
                             int /* width */,
                             int /* height */,
                             std::string /* json_arguments */,
                             std::string /* json_retval */)

  IPC_MESSAGE_ROUTED2(ViewHostMsg_DidGetApplicationInfo,
                      int32 /* page_id */,
                      webkit_glue::WebApplicationInfo)

  // Provides the result from running OnMsgShouldClose.  |proceed| matches the
  // return value of the the frame's shouldClose method (which includes the
  // onbeforeunload handler): true if the user decided to proceed with leaving
  // the page.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_ShouldClose_ACK,
                      bool /* proceed */)

  // Indicates that the current page has been closed, after a ClosePage
  // message.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_ClosePage_ACK,
                      int /* new_render_process_host_id */,
                      int /* new_request_id */)

  IPC_MESSAGE_ROUTED4(ViewHostMsg_DidDownloadImage,
                      int /* Identifier of the request */,
                      GURL /* URL of the image */,
                      bool /* true if there was a network error */,
                      SkBitmap /* image_data */)

  // Sent to query MIME information.
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_GetMimeTypeFromExtension,
                              FilePath::StringType /* extension */,
                              std::string /* mime_type */)
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_GetMimeTypeFromFile,
                              FilePath /* file_path */,
                              std::string /* mime_type */)
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_GetPreferredExtensionForMimeType,
                              std::string /* mime_type */,
                              FilePath::StringType /* extension */)

  // Get the CPBrowsingContext associated with the renderer sending this
  // message.
  IPC_SYNC_MESSAGE_CONTROL0_1(ViewHostMsg_GetCPBrowsingContext,
                              uint32 /* context */)

  // Sent when the renderer process is done processing a DataReceived
  // message.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_DataReceived_ACK,
                      int /* request_id */)

  // Sent when a provisional load on the main frame redirects.
  IPC_MESSAGE_ROUTED3(ViewHostMsg_DidRedirectProvisionalLoad,
                      int /* page_id */,
                      GURL /* last url */,
                      GURL /* url redirected to */)

  // Sent by the renderer process to acknowledge receipt of a
  // DownloadProgress message.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_DownloadProgress_ACK,
                      int /* request_id */)

  // Sent by the renderer process to acknowledge receipt of a
  // UploadProgress message.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_UploadProgress_ACK,
                      int /* request_id */)

  // Duplicates a shared memory handle from the renderer to the browser. Then
  // the renderer can flush the handle.
  IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_DuplicateSection,
                             base::SharedMemoryHandle /* renderer handle */,
                             base::SharedMemoryHandle /* browser handle */)

  // Provide the browser process with information about the WebCore resource
  // cache.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_ResourceTypeStats,
                       WebKit::WebCache::ResourceTypeStats)

  // Notify the browser that this render process can or can't be suddenly
  // terminated.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_SuddenTerminationChanged,
                       bool /* enabled */)

  // Returns the window location of the window this widget is embeded.
  // TODO(shess): Provide a mapping from reply_msg->routing_id() to
  // HWND so that we can eliminate the NativeViewId parameter.
  IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_GetRootWindowRect,
                             gfx::NativeViewId /* window */,
                             gfx::Rect /* Out: Window location */)

  // Informs the browser of a new context.
  IPC_MESSAGE_CONTROL3(AppCacheMsg_ContextCreated,
                       WebAppCacheContext::ContextType,
                       int /* context_id */,
                       int /* opt_parent_context_id */)

  // Informs the browser of a context being destroyed.
  IPC_MESSAGE_CONTROL1(AppCacheMsg_ContextDestroyed,
                       int /* context_id */)

  // Initiates the cache selection algorithm for the given context.
  // This is sent after new content has been committed, but prior to
  // any subresource loads. An AppCacheMsg_AppCacheSelected message will
  // be sent in response.
  // 'context_id' indentifies a specific frame or worker
  // 'select_request_id' indentifies this particular invocation the algorithm
  //    and will be returned to the caller with the response
  // 'document_url' the url of the main resource commited to the frame
  // 'cache_document_was_loaded_frame' the id of the appcache the main resource
  //    was loaded from or kNoAppCacheId
  // 'opt_manifest_url' the manifest url specified in the <html> tag if any
  IPC_MESSAGE_CONTROL5(AppCacheMsg_SelectAppCache,
                       int /* context_id */,
                       int /* select_request_id */,
                       GURL  /* document_url */,
                       int64 /* cache_document_was_loaded_from */,
                       GURL  /* opt_manifest_url */)

  // Returns the resizer box location in the window this widget is embeded.
  // Important for Mac OS X, but not Win or Linux.
  IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_GetRootWindowResizerRect,
                             gfx::NativeViewId /* window */,
                             gfx::Rect /* Out: Window location */)

  // Queries the browser for suggestion for autofill in a form input field.
  IPC_MESSAGE_ROUTED4(ViewHostMsg_QueryFormFieldAutofill,
                      std::wstring /* field name */,
                      std::wstring /* user entered text */,
                      int64 /* id of the text input field */,
                      int /* id of this message */)

  // Instructs the browser to remove the specified autofill-entry from the
  // database.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_RemoveAutofillEntry,
                      std::wstring /* field name */,
                      std::wstring /* value */)

  // Get the list of proxies to use for |url|, as a semicolon delimited list
  // of "<TYPE> <HOST>:<PORT>" | "DIRECT". See also
  // PluginProcessHostMsg_ResolveProxy which does the same thing.
  IPC_SYNC_MESSAGE_CONTROL1_2(ViewHostMsg_ResolveProxy,
                              GURL /* url */,
                              int /* network error */,
                              std::string /* proxy list */)

  // Request that got sent to browser for creating an audio output stream
  IPC_MESSAGE_ROUTED2(ViewHostMsg_CreateAudioStream,
                      int /* stream_id */,
                      ViewHostMsg_Audio_CreateStream)

  // Tell the browser the audio buffer prepared for stream
  // (render_view_id, stream_id) is filled and is ready to be consumed.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_NotifyAudioPacketReady,
                      int /* stream_id */,
                      size_t /* packet size */)

  // Start buffering the audio stream specified by (render_view_id, stream_id).
  IPC_MESSAGE_ROUTED1(ViewHostMsg_StartAudioStream,
                      int /* stream_id */)

  // Pause the audio stream specified by (render_view_id, stream_id).
  IPC_MESSAGE_ROUTED1(ViewHostMsg_PauseAudioStream,
                      int /* stream_id */)

  // Close an audio stream specified by (render_view_id, stream_id).
  IPC_MESSAGE_ROUTED1(ViewHostMsg_CloseAudioStream,
                      int /* stream_id */)

  // Get audio volume of the stream specified by (render_view_id, stream_id).
  IPC_MESSAGE_ROUTED1(ViewHostMsg_GetAudioVolume,
                      int /* stream_id */)

  // Set audio volume of the stream specified by (render_view_id, stream_id).
  // TODO(hclam): change this to vector if we have channel numbers other than 2.
  IPC_MESSAGE_ROUTED3(ViewHostMsg_SetAudioVolume,
                      int /* stream_id */,
                      double /* left_channel */,
                      double /* right_channel */)

  // A renderer sends this message when an extension process starts an API
  // request. The browser will always respond with a ViewMsg_ExtensionResponse.
  IPC_MESSAGE_ROUTED4(ViewHostMsg_ExtensionRequest,
                      std::string /* name */,
                      std::string /* argument */,
                      int /* callback id */,
                      bool /* has_callback */)

  // Notify the browser that this renderer added a listener to an event.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_ExtensionAddListener,
                       std::string /* name */)

  // Notify the browser that this renderer removed a listener from an event.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_ExtensionRemoveListener,
                       std::string /* name */)

#if defined(OS_MACOSX)
  // On OSX, we cannot allocated shared memory from within the sandbox, so
  // this call exists for the renderer to ask the browser to allocate memory
  // on its behalf. We return a file descriptor to the POSIX shared memory.
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_AllocTransportDIB,
                              size_t, /* bytes requested */
                              TransportDIB::Handle /* DIB */)

  // Since the browser keeps handles to the allocated transport DIBs, this
  // message is sent to tell the browser that it may release them when the
  // renderer is finished with them.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_FreeTransportDIB,
                       TransportDIB::Id /* DIB id */)
#endif

  // A renderer sends this to the browser process when it wants to create a
  // worker.  The browser will create the worker process if necessary, and
  // will return the route id on success.  On error returns MSG_ROUTING_NONE.
  IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_CreateDedicatedWorker,
                              GURL /* url */,
                              int /* render_view_route_id */,
                              int /* route_id */)

  // Sent if the worker object has sent a ViewHostMsg_CreateDedicatedWorker
  // message and not received a ViewMsg_DedicatedWorkerCreated reply, but in the
  // mean time it's destroyed.  This tells the browser to not create the queued
  // worker.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_CancelCreateDedicatedWorker,
                       int /* route_id */)

  // Wraps an IPC message that's destined to the worker on the renderer->browser
  // hop.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_ForwardToWorker,
                       IPC::Message /* message */)

  // Get a port handle to a currently-running extension process for the
  // extension with the given ID.  If no such extension is found, -1 is
  // returned.  The handle can be used for sending messages to the extension.
  IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_OpenChannelToExtension,
                              int /* routing_id */,
                              std::string /* extension_id */,
                              int /* port_id */)

  // Send a message to an extension process.  The handle is the value returned
  // by ViewHostMsg_OpenChannelToExtension.
  IPC_MESSAGE_ROUTED2(ViewHostMsg_ExtensionPostMessage,
                      int /* port_id */,
                      std::string /* message */)

  // Send a message to an extension process.  The handle is the value returned
  // by ViewHostMsg_OpenChannelToExtension.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_ExtensionCloseChannel,
                       int /* port_id */)

  // Message to show a popup menu using native cocoa controls (Mac only).
  IPC_MESSAGE_ROUTED1(ViewHostMsg_ShowPopup,
                      ViewHostMsg_ShowPopup_Params)

  // Sent as a result of a focus change in the renderer (if accessibility is
  // enabled), to notify the browser side that its accessibility focus needs to
  // change as well. Takes the id of the accessibility object that now has
  // focus.
  IPC_MESSAGE_ROUTED1(ViewHostMsg_AccessibilityFocusChange,
                      int /* accessibility object id */)

  // Message sent from the renderer to the browser to request that the browser
  // close all idle sockets.  Used for debugging/testing.
  IPC_MESSAGE_CONTROL0(ViewHostMsg_CloseIdleConnections)

  // Message sent from the renderer to the browser to request that the browser
  // close all idle sockets.  Used for debugging/testing.
  IPC_MESSAGE_CONTROL1(ViewHostMsg_SetCacheMode,
                       bool /* enabled */)

  // Get file size in bytes. Set result to -1 if failed to get the file size.
  IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_GetFileSize,
                              FilePath /* path */,
                              int64 /* result */)

  //---------------------------------------------------------------------------
  // Utility process host messages:
  // These are messages from the utility process to the browser.  They're here
  // because we ran out of spare message types.

  // Reply when the utility process is done unpacking an extension.  |manifest|
  // is the parsed manifest.json file.  The unpacker should also have written
  // out a file containing decoded images from the extension.  See
  // ExtensionUnpacker for details.
  IPC_MESSAGE_CONTROL1(UtilityHostMsg_UnpackExtension_Succeeded,
                       DictionaryValue /* manifest */)

  // Reply when the utility process has failed while unpacking an extension.
  // |error_message| is a user-displayable explanation of what went wrong.
  IPC_MESSAGE_CONTROL1(UtilityHostMsg_UnpackExtension_Failed,
                       std::string /* error_message, if any */)

  // Reply when the utility process is done unpacking and parsing JSON data
  // from a web resource.
  IPC_MESSAGE_CONTROL1(UtilityHostMsg_UnpackWebResource_Succeeded,
                       ListValue /* json data */)

  // Reply when the utility process has failed while unpacking and parsing a
  // web resource.  |error_message| is a user-readable explanation of what
  // went wrong.
  IPC_MESSAGE_CONTROL1(UtilityHostMsg_UnpackWebResource_Failed,
                       std::string /* error_message, if any */)

  // Sent by the renderer process to acknowledge receipt of a
  // ViewMsg_CSSInsertRequest message and css has been inserted into the frame.
  IPC_MESSAGE_ROUTED0(ViewHostMsg_OnCSSInserted)

IPC_END_MESSAGES(ViewHost)
