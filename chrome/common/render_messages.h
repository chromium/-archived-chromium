// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_RENDER_MESSAGES_H_
#define CHROME_COMMON_RENDER_MESSAGES_H_

#include <string>
#include <vector>
#include <map>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "chrome/common/filter_policy.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_message_utils.h"
#include "chrome/common/modal_dialog_event.h"
#include "chrome/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "net/base/upload_data.h"
#include "net/url_request/url_request_status.h"
#include "webkit/glue/autofill_form.h"
#include "webkit/glue/cache_manager.h"
#include "webkit/glue/context_node_types.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/password_form_dom_manager.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/glue/screen_info.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webview_delegate.h"

// Parameters structure for ViewMsg_Navigate, which has too many data
// parameters to be reasonably put in a predefined IPC message.
struct ViewMsg_Navigate_Params {
  // The page_id for this navigation, or -1 if it is a new navigation.  Back,
  // Forward, and Reload navigations should have a valid page_id.  If the load
  // succeeds, then this page_id will be reflected in the resultant
  // ViewHostMsg_FrameNavigate message.
  int32 page_id;

  // The URL to load.
  GURL url;

  // The URL to send in the "Referer" header field. Can be empty if there is
  // no referrer.
  GURL referrer;

  // The type of transition.
  PageTransition::Type transition;

  // Opaque history state (received by ViewHostMsg_UpdateState).
  std::string state;

  // Specifies if the URL should be loaded using 'reload' semantics (i.e.,
  // bypassing any locally cached content).
  bool reload;
};

// Parameters structure for ViewHostMsg_FrameNavigate, which has too many data
// parameters to be reasonably put in a predefined IPC message.
struct ViewHostMsg_FrameNavigate_Params {
  // Page ID of this navigation. The renderer creates a new unique page ID
  // anytime a new session history entry is created. This means you'll get new
  // page IDs for user actions, and the old page IDs will be reloaded when
  // iframes are loaded automatically.
  int32 page_id;

  // URL of the page being loaded.
  GURL url;

  // URL of the referrer of this load. WebKit generates this based on the
  // source of the event that caused the load.
  GURL referrer;

  // The type of transition.
  PageTransition::Type transition;

  // Lists the redirects that occurred on the way to the current page. This
  // vector has the same format as reported by the WebDataSource in the glue,
  // with the current page being the last one in the list (so even when
  // there's no redirect, there will be one entry in the list.
  std::vector<GURL> redirects;

  // Set to false if we want to update the session history but not update
  // the browser history.  E.g., on unreachable urls.
  bool should_update_history;

  // See SearchableFormData for a description of these.
  GURL searchable_form_url;
  std::wstring searchable_form_element_name;
  std::string searchable_form_encoding;

  // See password_form.h.
  PasswordForm password_form;

  // Information regarding the security of the connection (empty if the
  // connection was not secure).
  std::string security_info;

  // The gesture that initiated this navigation.
  NavigationGesture gesture;

  // Contents MIME type of main frame.
  std::string contents_mime_type;

  // True if this was a post request.
  bool is_post;

  // Whether the content of the frame was replaced with some alternate content
  // (this can happen if the resource was insecure).
  bool is_content_filtered;
};

// Parameters structure for ViewHostMsg_ContextMenu, which has too many data
// parameters to be reasonably put in a predefined IPC message.
// FIXME(beng): This would be more useful in the future and more efficient
//              if the parameters here weren't so literally mapped to what
//              they contain for the ContextMenu task. It might be better
//              to make the string fields more generic so that this object
//              could be used for more contextual actions.
struct ViewHostMsg_ContextMenu_Params {
  // This is the type of Context Node that the context menu was invoked on.
  ContextNode::Type type;

  // These values represent the coordinates of the mouse when the context menu
  // was invoked.  Coords are relative to the associated RenderView's origin.
  int x;
  int y;

  // This is the URL of the link that encloses the node the context menu was
  // invoked on.
  GURL link_url;

  // This is the URL of the image the context menu was invoked on.
  GURL image_url;

  // This is the URL of the top level page that the context menu was invoked
  // on.
  GURL page_url;

  // This is the URL of the subframe that the context menu was invoked on.
  GURL frame_url;

  // This is the text of the selection that the context menu was invoked on.
  std::wstring selection_text;

  // The misspelled word under the cursor, if any. Used to generate the
  // |dictionary_suggestions| list.
  std::wstring misspelled_word;

  // Suggested replacements for a misspelled word under the cursor.
  // This vector gets populated in the render process host
  // by intercepting ViewHostMsg_ContextMenu in ResourceMessageFilter
  // and populating dictionary_suggestions if the type is EDITABLE
  // and the misspelled_word is not empty.
  std::vector<std::wstring> dictionary_suggestions;

  // If editable, flag for whether spell check is enabled or not.
  bool spellcheck_enabled;

  // These flags indicate to the browser whether the renderer believes it is
  // able to perform the corresponding action.
  int edit_flags;

  // The security info for the resource we are showing the menu on.
  std::string security_info;
};

// Values that may be OR'd together to form the 'flags' parameter of a
// ViewHostMsg_PaintRect message.
struct ViewHostMsg_PaintRect_Flags {
  enum {
    IS_RESIZE_ACK = 1 << 0,
    IS_RESTORE_ACK = 1 << 1,
    IS_REPAINT_ACK = 1 << 2,
  };
  static bool is_resize_ack(int flags) {
    return (flags & IS_RESIZE_ACK) != 0;
  }
  static bool is_restore_ack(int flags) {
    return (flags & IS_RESTORE_ACK) != 0;
  }

  static bool is_repaint_ack(int flags) {
    return (flags & IS_REPAINT_ACK) != 0;
  }
};

#if defined(OS_WIN)
// TODO(port): Make these structs portable.

struct ViewHostMsg_PaintRect_Params {
  // The bitmap to be painted into the rect given by bitmap_rect.  Valid only
  // in the context of the renderer process.
  base::SharedMemoryHandle bitmap;

  // The position and size of the bitmap.
  gfx::Rect bitmap_rect;

  // The size of the RenderView when this message was generated.  This is
  // included so the host knows how large the view is from the perspective of
  // the renderer process.  This is necessary in case a resize operation is in
  // progress.
  gfx::Size view_size;

  // New window locations for plugin child windows.
  std::vector<WebPluginGeometry> plugin_window_moves;

  // The following describes the various bits that may be set in flags:
  //
  //   ViewHostMsg_PaintRect_Flags::IS_RESIZE_ACK
  //     Indicates that this is a response to a ViewMsg_Resize message.
  //
  //   ViewHostMsg_PaintRect_Flags::IS_RESTORE_ACK
  //     Indicates that this is a response to a ViewMsg_WasRestored message.
  //
  // If flags is zero, then this message corresponds to an unsoliticed paint
  // request by the render view.  Both of the above bits may be set in flags,
  // which would indicate that this paint message is an ACK for multiple
  // request messages.
  int flags;
};

// Parameters structure for ViewHostMsg_ScrollRect, which has too many data
// parameters to be reasonably put in a predefined IPC message.
struct ViewHostMsg_ScrollRect_Params {
  // The bitmap to be painted into the rect exposed by scrolling.  This handle
  // is valid only in the context of the renderer process.
  base::SharedMemoryHandle bitmap;

  // The position and size of the bitmap.
  gfx::Rect bitmap_rect;

  // The scroll offset.  Only one of these can be non-zero.
  int dx;
  int dy;

  // The rectangular region to scroll.
  gfx::Rect clip_rect;

  // The size of the RenderView when this message was generated.
  gfx::Size view_size;

  // New window locations for plugin child windows.
  std::vector<WebPluginGeometry> plugin_window_moves;
};
#endif  // defined(OS_WIN)

// Parameters structure for ViewMsg_UploadFile.
struct ViewMsg_UploadFile_Params {
  // See WebContents::StartFileUpload for a description of these fields.
  std::wstring file_path;
  std::wstring form;
  std::wstring file;
  std::wstring submit;
  std::wstring other_values;
};

// Parameters for a resource request.
struct ViewHostMsg_Resource_Request {
  // The request method: GET, POST, etc.
  std::string method;

  // The requested URL.
  GURL url;

  // The URL of the document in the top-level window, which may be checked by
  // the third-party cookie blocking policy. Leaving it empty may lead to
  // undesired cookie blocking. Third-party cookie blocking can be bypassed by
  // setting policy_url = url, but this should ideally only be done if there
  // really is no way to determine the correct value.
  GURL policy_url;

  // The referrer to use (may be empty).
  GURL referrer;

  // Additional HTTP request headers.
  std::string headers;

  // URLRequest load flags (0 by default).
  int load_flags;

  // Process ID of process that originated this request.
  int origin_pid;

  // What this resource load is for (main frame, sub-frame, sub-resource,
  // object).
  ResourceType::Type resource_type;

  // True if this request is for a resource loaded over HTTP when the main page
  // was loaded over HTTPS.
  bool mixed_content;

  // Used by plugin->browser requests to get the correct URLRequestContext.
  uint32 request_context;

  // Optional upload data (may be empty).
  std::vector<net::UploadData::Element> upload_content;
};

// Parameters for a resource response header.
struct ViewMsg_Resource_ResponseHead
    : webkit_glue::ResourceLoaderBridge::ResponseInfo {
  // The response status.
  URLRequestStatus status;

  // Specifies if the resource should be filtered before being displayed
  // (insecure resources can be filtered to keep the page secure).
  FilterPolicy::Type filter_policy;
};

// Parameters for a synchronous resource response.
struct ViewHostMsg_SyncLoad_Result : ViewMsg_Resource_ResponseHead {
  // The final URL after any redirects.
  GURL final_url;

  // The response data.
  std::string data;
};

// Parameters for a render request.
struct ViewMsg_Print_Params {
  // In pixels according to dpi_x and dpi_y.
  gfx::Size printable_size;

  // Specifies dots per inch.
  double dpi;

  // Minimum shrink factor. See PrintSettings::min_shrink for more information.
  double min_shrink;

  // Maximum shrink factor. See PrintSettings::max_shrink for more information.
  double max_shrink;

  // Desired apparent dpi on paper.
  int desired_dpi;

  // Cookie for the document to ensure correctness.
  int document_cookie;

  // Warning: do not compare document_cookie.
  bool Equals(const ViewMsg_Print_Params& rhs) const {
    return printable_size == rhs.printable_size &&
           dpi == rhs.dpi &&
           min_shrink == rhs.min_shrink &&
           max_shrink == rhs.max_shrink &&
           desired_dpi == rhs.desired_dpi;
  }
};

struct ViewMsg_PrintPage_Params {
  // Parameters to render the page as a printed page. It must always be the same
  // value for all the document.
  ViewMsg_Print_Params params;

  // The page number is the indicator of the square that should be rendered
  // according to the layout specified in ViewMsg_Print_Params.
  int page_number;
};

struct ViewMsg_PrintPages_Params {
  // Parameters to render the page as a printed page. It must always be the same
  // value for all the document.
  ViewMsg_Print_Params params;

  // If empty, this means a request to render all the printed pages.
  std::vector<int> pages;
};

// Parameters to describe a rendered page.
struct ViewHostMsg_DidPrintPage_Params {
  // A shared memory handle to the EMF data. This data can be quite large so a
  // memory map needs to be used.
  base::SharedMemoryHandle emf_data_handle;

  // Size of the EMF data.
  unsigned data_size;

  // Cookie for the document to ensure correctness.
  int document_cookie;

  // Page number.
  int page_number;

  // Shrink factor used to render this page.
  double actual_shrink;
};

// Parameters structure to hold a union of the possible IAccessible function
// INPUT variables, with the unused fields always set to default value. Used in
// ViewMsg_GetAccessibilityInfo, as only parameter.
struct ViewMsg_Accessibility_In_Params {
  // Identifier to uniquely distinguish which instance of IAccessible is being
  // called upon on the renderer side.
  int iaccessible_id;

  // Identifier to resolve which IAccessible interface function is being called.
  int iaccessible_function_id;

  // Function input parameters.
  // Input VARIANT structure's LONG field to specify requested object.
  long input_variant_lval;

  // LONG input parameters, used differently depending on the function called.
  long input_long1;
  long input_long2;
};

// Parameters structure to hold a union of the possible IAccessible function
// OUTPUT variables, with the unused fields always set to default value. Used in
// ViewHostMsg_GetAccessibilityInfoResponse, as only parameter.
struct ViewHostMsg_Accessibility_Out_Params {
  // Identifier to uniquely distinguish which instance of IAccessible is being
  // called upon on the renderer side.
  int iaccessible_id;

  // Function output parameters.
  // Output VARIANT structure's LONG field to specify requested object.
  long output_variant_lval;

  // LONG output parameters, used differently depending on the function called.
  // output_long1 can in some cases be set to -1 to indicate that the child
  // object found by the called IAccessible function is not a simple object.
  long output_long1;
  long output_long2;
  long output_long3;
  long output_long4;

  // String output parameter.
  std::wstring output_string;

  // Return code, either S_OK (true) or S_FALSE (false). WebKit MSAA error
  // return codes (E_POINTER, E_INVALIDARG, E_FAIL, E_NOTIMPL) must be handled
  // on the browser side by input validation.
  bool return_code;
};

// The first parameter for the ViewHostMsg_ImeUpdateStatus message.
enum ViewHostMsg_ImeControl {
  IME_DISABLE = 0,
  IME_MOVE_WINDOWS,
  IME_COMPLETE_COMPOSITION,
};

// Multi-pass include of render_messages_internal.  Preprocessor magic allows
// us to use 1 header to define the enums and classes for our render messages.
#define IPC_MESSAGE_MACROS_ENUMS
#include "chrome/common/render_messages_internal.h"

#ifdef IPC_MESSAGE_MACROS_LOG_ENABLED
// When we are supposed to create debug strings, we run it through twice, once
// with debug strings on, and once with only CLASSES on to generate both types
// of messages.
#  undef IPC_MESSAGE_MACROS_LOG
#  define IPC_MESSAGE_MACROS_CLASSES
#  include "chrome/common/render_messages_internal.h"

#  undef IPC_MESSAGE_MACROS_CLASSES
#  define IPC_MESSAGE_MACROS_LOG
#  include "chrome/common/render_messages_internal.h"
#else
// No debug strings requested, just define the classes
#  define IPC_MESSAGE_MACROS_CLASSES
#  include "chrome/common/render_messages_internal.h"
#endif


namespace IPC {

template <>
struct ParamTraits<ResourceType::Type> {
  typedef ResourceType::Type param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type) || !ResourceType::ValidType(type))
      return false;
    *p = ResourceType::FromInt(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring type;
    switch (p) {
      case ResourceType::MAIN_FRAME:
        type = L"MAIN_FRAME";
       break;
     case ResourceType::SUB_FRAME:
       type = L"SUB_FRAME";
       break;
     case ResourceType::SUB_RESOURCE:
       type = L"SUB_RESOURCE";
       break;
     case ResourceType::OBJECT:
       type = L"OBJECT";
       break;
     default:
       type = L"UNKNOWN";
       break;
    }

    LogParam(type, l);
  }
};

template <>
struct ParamTraits<FilterPolicy::Type> {
  typedef FilterPolicy::Type param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type) || !FilterPolicy::ValidType(type))
      return false;
    *p = FilterPolicy::FromInt(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring type;
    switch (p) {
      case FilterPolicy::DONT_FILTER:
        type = L"DONT_FILTER";
        break;
      case FilterPolicy::FILTER_ALL:
        type = L"FILTER_ALL";
        break;
      case FilterPolicy::FILTER_ALL_EXCEPT_IMAGES:
        type = L"FILTER_ALL_EXCEPT_IMAGES";
        break;
      default:
        type = L"UNKNOWN";
        break;
    }

    LogParam(type, l);
  }
};

template <>
struct ParamTraits<ContextNode::Type> {
  typedef ContextNode::Type param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = ContextNode::FromInt(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring type;
    switch (p) {
     case WebInputEvent::MOUSE_DOWN:
      type = L"MOUSE_DOWN";
      break;
     case WebInputEvent::MOUSE_UP:
      type = L"MOUSE_UP";
      break;
     case WebInputEvent::MOUSE_MOVE:
      type = L"MOUSE_MOVE";
      break;
     case WebInputEvent::MOUSE_LEAVE:
      type = L"MOUSE_LEAVE";
      break;
     case WebInputEvent::MOUSE_DOUBLE_CLICK:
      type = L"MOUSE_DOUBLE_CLICK";
      break;
     case WebInputEvent::MOUSE_WHEEL:
      type = L"MOUSE_WHEEL";
      break;
     case WebInputEvent::KEY_DOWN:
      type = L"KEY_DOWN";
      break;
     case WebInputEvent::KEY_UP:
      type = L"KEY_UP";
      break;
     default:
      type = L"UNKNOWN";
      break;
    }

    LogParam(type, l);
  }
};

template <>
struct ParamTraits<WebInputEvent::Type> {
  typedef WebInputEvent::Type param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<WebInputEvent::Type>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring event;
    switch (p) {
     case ContextNode::NONE:
      event = L"NONE";
      break;
     case ContextNode::PAGE:
      event = L"PAGE";
      break;
     case ContextNode::FRAME:
      event = L"FRAME";
      break;
     case ContextNode::LINK:
      event = L"LINK";
      break;
     case ContextNode::IMAGE:
      event = L"IMAGE";
      break;
     case ContextNode::IMAGE_LINK:
      event = L"IMAGE_LINK";
      break;
     case ContextNode::SELECTION:
      event = L"SELECTION";
      break;
     case ContextNode::EDITABLE:
      event = L"EDITABLE";
      break;
     case ContextNode::MISPELLED_WORD:
      event = L"MISPELLED_WORD";
      break;
     default:
      event = L"UNKNOWN";
      break;
    }

    LogParam(event, l);
  }
};

// Traits for ViewMsg_Accessibility_In_Params structure to pack/unpack.
template <>
struct ParamTraits<ViewMsg_Accessibility_In_Params> {
  typedef ViewMsg_Accessibility_In_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.iaccessible_id);
    WriteParam(m, p.iaccessible_function_id);
    WriteParam(m, p.input_variant_lval);
    WriteParam(m, p.input_long1);
    WriteParam(m, p.input_long2);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->iaccessible_id) &&
      ReadParam(m, iter, &p->iaccessible_function_id) &&
      ReadParam(m, iter, &p->input_variant_lval) &&
      ReadParam(m, iter, &p->input_long1) &&
      ReadParam(m, iter, &p->input_long2);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.iaccessible_id, l);
    l->append(L", ");
    LogParam(p.iaccessible_function_id, l);
    l->append(L", ");
    LogParam(p.input_variant_lval, l);
    l->append(L", ");
    LogParam(p.input_long1, l);
    l->append(L", ");
    LogParam(p.input_long2, l);
    l->append(L")");
  }
};

// Traits for ViewHostMsg_Accessibility_Out_Params structure to pack/unpack.
template <>
struct ParamTraits<ViewHostMsg_Accessibility_Out_Params> {
  typedef ViewHostMsg_Accessibility_Out_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.iaccessible_id);
    WriteParam(m, p.output_variant_lval);
    WriteParam(m, p.output_long1);
    WriteParam(m, p.output_long2);
    WriteParam(m, p.output_long3);
    WriteParam(m, p.output_long4);
    WriteParam(m, p.output_string);
    WriteParam(m, p.return_code);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->iaccessible_id) &&
      ReadParam(m, iter, &p->output_variant_lval) &&
      ReadParam(m, iter, &p->output_long1) &&
      ReadParam(m, iter, &p->output_long2) &&
      ReadParam(m, iter, &p->output_long3) &&
      ReadParam(m, iter, &p->output_long4) &&
      ReadParam(m, iter, &p->output_string) &&
      ReadParam(m, iter, &p->return_code);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.iaccessible_id, l);
    l->append(L", ");
    LogParam(p.output_variant_lval, l);
    l->append(L", ");
    LogParam(p.output_long1, l);
    l->append(L", ");
    LogParam(p.output_long2, l);
    l->append(L", ");
    LogParam(p.output_long3, l);
    l->append(L", ");
    LogParam(p.output_long4, l);
    l->append(L", ");
    LogParam(p.output_string, l);
    l->append(L", ");
    LogParam(p.return_code, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<ViewHostMsg_ImeControl> {
  typedef ViewHostMsg_ImeControl param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<ViewHostMsg_ImeControl>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring control;
    switch (p) {
     case IME_DISABLE:
      control = L"IME_DISABLE";
      break;
     case IME_MOVE_WINDOWS:
      control = L"IME_MOVE_WINDOWS";
      break;
     case IME_COMPLETE_COMPOSITION:
      control = L"IME_COMPLETE_COMPOSITION";
      break;
     default:
      control = L"UNKNOWN";
      break;
    }

    LogParam(control, l);
  }
};

// Traits for ViewMsg_Navigate_Params structure to pack/unpack.
template <>
struct ParamTraits<ViewMsg_Navigate_Params> {
  typedef ViewMsg_Navigate_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.page_id);
    WriteParam(m, p.url);
    WriteParam(m, p.referrer);
    WriteParam(m, p.transition);
    WriteParam(m, p.state);
    WriteParam(m, p.reload);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->page_id) &&
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->referrer) &&
      ReadParam(m, iter, &p->transition) &&
      ReadParam(m, iter, &p->state) &&
      ReadParam(m, iter, &p->reload);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.page_id, l);
    l->append(L", ");
    LogParam(p.url, l);
    l->append(L", ");
    LogParam(p.transition, l);
    l->append(L", ");
    LogParam(p.state, l);
    l->append(L", ");
    LogParam(p.reload, l);
    l->append(L")");
  }
};

// Traits for PasswordForm_Params structure to pack/unpack.
template <>
struct ParamTraits<PasswordForm> {
  typedef PasswordForm param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.signon_realm);
    WriteParam(m, p.origin);
    WriteParam(m, p.action);
    WriteParam(m, p.submit_element);
    WriteParam(m, p.username_element);
    WriteParam(m, p.username_value);
    WriteParam(m, p.password_element);
    WriteParam(m, p.password_value);
    WriteParam(m, p.old_password_element);
    WriteParam(m, p.old_password_value);
    WriteParam(m, p.ssl_valid);
    WriteParam(m, p.preferred);
    WriteParam(m, p.blacklisted_by_user);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->signon_realm) &&
      ReadParam(m, iter, &p->origin) &&
      ReadParam(m, iter, &p->action) &&
      ReadParam(m, iter, &p->submit_element) &&
      ReadParam(m, iter, &p->username_element) &&
      ReadParam(m, iter, &p->username_value) &&
      ReadParam(m, iter, &p->password_element) &&
      ReadParam(m, iter, &p->password_value) &&
      ReadParam(m, iter, &p->old_password_element) &&
      ReadParam(m, iter, &p->old_password_value) &&
      ReadParam(m, iter, &p->ssl_valid) &&
      ReadParam(m, iter, &p->preferred) &&
      ReadParam(m, iter, &p->blacklisted_by_user);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<PasswordForm>");
  }
};

// Traits for AutofillForm_Params structure to pack/unpack.
template <>
struct ParamTraits<AutofillForm> {
  typedef AutofillForm param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.elements.size());
    for (std::vector<AutofillForm::Element>::const_iterator itr =
        p.elements.begin();
        itr != p.elements.end();
        itr++) {
      WriteParam(m, itr->name);
      WriteParam(m, itr->value);
    }
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
      bool result = true;
      size_t elements_size = 0;
      result = result && ReadParam(m, iter, &elements_size);
      p->elements.resize(elements_size);
      for (size_t i = 0; i < elements_size; i++) {
        std::wstring s;
        result = result && ReadParam(m, iter, &(p->elements[i].name));
        result = result && ReadParam(m, iter, &(p->elements[i].value));
      }
      return result;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<AutofillForm>");
  }
};

// Traits for ViewHostMsg_FrameNavigate_Params structure to pack/unpack.
template <>
struct ParamTraits<ViewHostMsg_FrameNavigate_Params> {
  typedef ViewHostMsg_FrameNavigate_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.page_id);
    WriteParam(m, p.url);
    WriteParam(m, p.referrer);
    WriteParam(m, p.transition);
    WriteParam(m, p.redirects);
    WriteParam(m, p.should_update_history);
    WriteParam(m, p.searchable_form_url);
    WriteParam(m, p.searchable_form_element_name);
    WriteParam(m, p.searchable_form_encoding);
    WriteParam(m, p.password_form);
    WriteParam(m, p.security_info);
    WriteParam(m, p.gesture);
    WriteParam(m, p.contents_mime_type);
    WriteParam(m, p.is_post);
    WriteParam(m, p.is_content_filtered);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->page_id) &&
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->referrer) &&
      ReadParam(m, iter, &p->transition) &&
      ReadParam(m, iter, &p->redirects) &&
      ReadParam(m, iter, &p->should_update_history) &&
      ReadParam(m, iter, &p->searchable_form_url) &&
      ReadParam(m, iter, &p->searchable_form_element_name) &&
      ReadParam(m, iter, &p->searchable_form_encoding) &&
      ReadParam(m, iter, &p->password_form) &&
      ReadParam(m, iter, &p->security_info) &&
      ReadParam(m, iter, &p->gesture) &&
      ReadParam(m, iter, &p->contents_mime_type) &&
      ReadParam(m, iter, &p->is_post) &&
      ReadParam(m, iter, &p->is_content_filtered);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.page_id, l);
    l->append(L", ");
    LogParam(p.url, l);
    l->append(L", ");
    LogParam(p.referrer, l);
    l->append(L", ");
    LogParam(p.transition, l);
    l->append(L", ");
    LogParam(p.redirects, l);
    l->append(L", ");
    LogParam(p.should_update_history, l);
    l->append(L", ");
    LogParam(p.searchable_form_url, l);
    l->append(L", ");
    LogParam(p.searchable_form_element_name, l);
    l->append(L", ");
    LogParam(p.searchable_form_encoding, l);
    l->append(L", ");
    LogParam(p.password_form, l);
    l->append(L", ");
    LogParam(p.security_info, l);
    l->append(L", ");
    LogParam(p.gesture, l);
    l->append(L", ");
    LogParam(p.contents_mime_type, l);
    l->append(L", ");
    LogParam(p.is_post, l);
    l->append(L", ");
    LogParam(p.is_content_filtered, l);
    l->append(L")");
  }
};

// Traits for ViewHostMsg_ContextMenu_Params structure to pack/unpack.
template <>
struct ParamTraits<ViewHostMsg_ContextMenu_Params> {
  typedef ViewHostMsg_ContextMenu_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.type);
    WriteParam(m, p.x);
    WriteParam(m, p.y);
    WriteParam(m, p.link_url);
    WriteParam(m, p.image_url);
    WriteParam(m, p.page_url);
    WriteParam(m, p.frame_url);
    WriteParam(m, p.selection_text);
    WriteParam(m, p.misspelled_word);
    WriteParam(m, p.dictionary_suggestions);
    WriteParam(m, p.spellcheck_enabled);
    WriteParam(m, p.edit_flags);
    WriteParam(m, p.security_info);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->type) &&
      ReadParam(m, iter, &p->x) &&
      ReadParam(m, iter, &p->y) &&
      ReadParam(m, iter, &p->link_url) &&
      ReadParam(m, iter, &p->image_url) &&
      ReadParam(m, iter, &p->page_url) &&
      ReadParam(m, iter, &p->frame_url) &&
      ReadParam(m, iter, &p->selection_text) &&
      ReadParam(m, iter, &p->misspelled_word) &&
      ReadParam(m, iter, &p->dictionary_suggestions) &&
      ReadParam(m, iter, &p->spellcheck_enabled) &&
      ReadParam(m, iter, &p->edit_flags) &&
      ReadParam(m, iter, &p->security_info);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ViewHostMsg_ContextMenu_Params>");
  }
};

#if defined(OS_WIN)
// TODO(port): Make these messages portable.

// Traits for ViewHostMsg_PaintRect_Params structure to pack/unpack.
template <>
struct ParamTraits<ViewHostMsg_PaintRect_Params> {
  typedef ViewHostMsg_PaintRect_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.bitmap);
    WriteParam(m, p.bitmap_rect);
    WriteParam(m, p.view_size);
    WriteParam(m, p.plugin_window_moves);
    WriteParam(m, p.flags);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->bitmap) &&
      ReadParam(m, iter, &p->bitmap_rect) &&
      ReadParam(m, iter, &p->view_size) &&
      ReadParam(m, iter, &p->plugin_window_moves) &&
      ReadParam(m, iter, &p->flags);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.bitmap, l);
    l->append(L", ");
    LogParam(p.bitmap_rect, l);
    l->append(L", ");
    LogParam(p.view_size, l);
    l->append(L", ");
    LogParam(p.plugin_window_moves, l);
    l->append(L", ");
    LogParam(p.flags, l);
    l->append(L")");
  }
};

// Traits for ViewHostMsg_ScrollRect_Params structure to pack/unpack.
template <>
struct ParamTraits<ViewHostMsg_ScrollRect_Params> {
  typedef ViewHostMsg_ScrollRect_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.bitmap);
    WriteParam(m, p.bitmap_rect);
    WriteParam(m, p.dx);
    WriteParam(m, p.dy);
    WriteParam(m, p.clip_rect);
    WriteParam(m, p.view_size);
    WriteParam(m, p.plugin_window_moves);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->bitmap) &&
      ReadParam(m, iter, &p->bitmap_rect) &&
      ReadParam(m, iter, &p->dx) &&
      ReadParam(m, iter, &p->dy) &&
      ReadParam(m, iter, &p->clip_rect) &&
      ReadParam(m, iter, &p->view_size) &&
      ReadParam(m, iter, &p->plugin_window_moves);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.bitmap, l);
    l->append(L", ");
    LogParam(p.bitmap_rect, l);
    l->append(L", ");
    LogParam(p.dx, l);
    l->append(L", ");
    LogParam(p.dy, l);
    l->append(L", ");
    LogParam(p.clip_rect, l);
    l->append(L", ");
    LogParam(p.view_size, l);
    l->append(L", ");
    LogParam(p.plugin_window_moves, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<WebPluginGeometry> {
  typedef WebPluginGeometry param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.window);
    WriteParam(m, p.window_rect);
    WriteParam(m, p.clip_rect);
    WriteParam(m, p.cutout_rects);
    WriteParam(m, p.visible);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->window) &&
      ReadParam(m, iter, &p->window_rect) &&
      ReadParam(m, iter, &p->clip_rect) &&
      ReadParam(m, iter, &p->cutout_rects) &&
      ReadParam(m, iter, &p->visible);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.window, l);
    l->append(L", ");
    LogParam(p.window_rect, l);
    l->append(L", ");
    LogParam(p.clip_rect, l);
    l->append(L", ");
    LogParam(p.cutout_rects, l);
    l->append(L", ");
    LogParam(p.visible, l);
    l->append(L")");
  }
};
#endif  // defined(OS_WIN)

// Traits for ViewMsg_GetPlugins_Reply structure to pack/unpack.
template <>
struct ParamTraits<WebPluginMimeType> {
  typedef WebPluginMimeType param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.mime_type);
    WriteParam(m, p.file_extensions);
    WriteParam(m, p.description);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->mime_type) &&
      ReadParam(m, iter, &r->file_extensions) &&
      ReadParam(m, iter, &r->description);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.mime_type, l);
    l->append(L", ");
    LogParam(p.file_extensions, l);
    l->append(L", ");
    LogParam(p.description, l);
    l->append(L")");
  }
};


template <>
struct ParamTraits<WebPluginInfo> {
  typedef WebPluginInfo param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.name);
    WriteParam(m, p.path);
    WriteParam(m, p.version);
    WriteParam(m, p.desc);
    WriteParam(m, p.mime_types);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->name) &&
      ReadParam(m, iter, &r->path) &&
      ReadParam(m, iter, &r->version) &&
      ReadParam(m, iter, &r->desc) &&
      ReadParam(m, iter, &r->mime_types);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.name, l);
    l->append(L", ");
    l->append(L", ");
    LogParam(p.path, l);
    l->append(L", ");
    LogParam(p.version, l);
    l->append(L", ");
    LogParam(p.desc, l);
    l->append(L", ");
    LogParam(p.mime_types, l);
    l->append(L")");
  }
};

// Traits for ViewMsg_UploadFile_Params structure to pack/unpack.
template <>
struct ParamTraits<ViewMsg_UploadFile_Params> {
  typedef ViewMsg_UploadFile_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.file_path);
    WriteParam(m, p.form);
    WriteParam(m, p.file);
    WriteParam(m, p.submit);
    WriteParam(m, p.other_values);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->file_path) &&
      ReadParam(m, iter, &p->form) &&
      ReadParam(m, iter, &p->file) &&
      ReadParam(m, iter, &p->submit) &&
      ReadParam(m, iter, &p->other_values);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ViewMsg_UploadFile_Params>");
  }
};

// Traits for net::UploadData::Element.
template <>
struct ParamTraits<net::UploadData::Element> {
  typedef net::UploadData::Element param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.type()));
    if (p.type() == net::UploadData::TYPE_BYTES) {
      m->WriteData(&p.bytes()[0], static_cast<int>(p.bytes().size()));
    } else {
      WriteParam(m, p.file_path());
      WriteParam(m, p.file_range_offset());
      WriteParam(m, p.file_range_length());
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int type;
    if (!ReadParam(m, iter, &type))
      return false;
    if (type == net::UploadData::TYPE_BYTES) {
      const char* data;
      int len;
      if (!m->ReadData(iter, &data, &len))
        return false;
      r->SetToBytes(data, len);
    } else {
      DCHECK(type == net::UploadData::TYPE_FILE);
      std::wstring file_path;
      uint64 offset, length;
      if (!ReadParam(m, iter, &file_path))
        return false;
      if (!ReadParam(m, iter, &offset))
        return false;
      if (!ReadParam(m, iter, &length))
        return false;
      r->SetToFilePathRange(file_path, offset, length);
    }
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<net::UploadData::Element>");
  }
};

// Traits for CacheManager::UsageStats
template <>
struct ParamTraits<CacheManager::UsageStats> {
  typedef CacheManager::UsageStats param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.min_dead_capacity);
    WriteParam(m, p.max_dead_capacity);
    WriteParam(m, p.capacity);
    WriteParam(m, p.live_size);
    WriteParam(m, p.dead_size);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->min_dead_capacity) &&
      ReadParam(m, iter, &r->max_dead_capacity) &&
      ReadParam(m, iter, &r->capacity) &&
      ReadParam(m, iter, &r->live_size) &&
      ReadParam(m, iter, &r->dead_size);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<CacheManager::UsageStats>");
  }
};

// Traits for PasswordFormDomManager::FillData.
template <>
struct ParamTraits<PasswordFormDomManager::FillData> {
  typedef PasswordFormDomManager::FillData param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.basic_data);
    WriteParam(m, p.additional_logins);
    WriteParam(m, p.wait_for_username);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->basic_data) &&
      ReadParam(m, iter, &r->additional_logins) &&
      ReadParam(m, iter, &r->wait_for_username);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<PasswordFormDomManager::FillData>");
  }
};

template<>
struct ParamTraits<NavigationGesture> {
  typedef NavigationGesture param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<NavigationGesture>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring event;
    switch (p) {
      case NavigationGestureUser:
        event = L"GESTURE_USER";
        break;
      case NavigationGestureAuto:
        event = L"GESTURE_AUTO";
        break;
      default:
        event = L"GESTURE_UNKNOWN";
        break;
    }
    LogParam(event, l);
  }
};

// Traits for ViewHostMsg_Resource_Request
template <>
struct ParamTraits<ViewHostMsg_Resource_Request> {
  typedef ViewHostMsg_Resource_Request param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.method);
    WriteParam(m, p.url);
    WriteParam(m, p.policy_url);
    WriteParam(m, p.referrer);
    WriteParam(m, p.headers);
    WriteParam(m, p.load_flags);
    WriteParam(m, p.origin_pid);
    WriteParam(m, p.resource_type);
    WriteParam(m, p.mixed_content);
    WriteParam(m, p.request_context);
    WriteParam(m, p.upload_content);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->method) &&
      ReadParam(m, iter, &r->url) &&
      ReadParam(m, iter, &r->policy_url) &&
      ReadParam(m, iter, &r->referrer) &&
      ReadParam(m, iter, &r->headers) &&
      ReadParam(m, iter, &r->load_flags) &&
      ReadParam(m, iter, &r->origin_pid) &&
      ReadParam(m, iter, &r->resource_type) &&
      ReadParam(m, iter, &r->mixed_content) &&
      ReadParam(m, iter, &r->request_context) &&
      ReadParam(m, iter, &r->upload_content);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.method, l);
    l->append(L", ");
    LogParam(p.url, l);
    l->append(L", ");
    LogParam(p.referrer, l);
    l->append(L", ");
    LogParam(p.load_flags, l);
    l->append(L", ");
    LogParam(p.origin_pid, l);
    l->append(L", ");
    LogParam(p.resource_type, l);
    l->append(L", ");
    LogParam(p.mixed_content, l);
    l->append(L", ");
    LogParam(p.request_context, l);
    l->append(L")");
  }
};

// Traits for URLRequestStatus
template <>
struct ParamTraits<URLRequestStatus> {
  typedef URLRequestStatus param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.status()));
    WriteParam(m, p.os_error());
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int status, os_error;
    if (!ReadParam(m, iter, &status) ||
        !ReadParam(m, iter, &os_error))
      return false;
    r->set_status(static_cast<URLRequestStatus::Status>(status));
    r->set_os_error(os_error);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring status;
    switch (p.status()) {
     case URLRequestStatus::SUCCESS:
      status = L"SUCCESS";
      break;
     case URLRequestStatus::IO_PENDING:
      status = L"IO_PENDING ";
      break;
     case URLRequestStatus::HANDLED_EXTERNALLY:
      status = L"HANDLED_EXTERNALLY";
      break;
     case URLRequestStatus::CANCELED:
      status = L"CANCELED";
      break;
     case URLRequestStatus::FAILED:
      status = L"FAILED";
      break;
     default:
      status = L"UNKNOWN";
      break;
    }
    if (p.status() == URLRequestStatus::FAILED)
      l->append(L"(");

    LogParam(status, l);

    if (p.status() == URLRequestStatus::FAILED) {
      l->append(L", ");
      LogParam(p.os_error(), l);
      l->append(L")");
    }
  }
};

template <>
struct ParamTraits<scoped_refptr<net::HttpResponseHeaders> > {
  typedef scoped_refptr<net::HttpResponseHeaders> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.get() != NULL);
    if (p) {
      // Do not disclose Set-Cookie headers over IPC.
      p->Persist(m, net::HttpResponseHeaders::PERSIST_SANS_COOKIES);
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    bool has_object;
    if (!ReadParam(m, iter, &has_object))
      return false;
    if (has_object)
      *r = new net::HttpResponseHeaders(*m, iter);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<HttpResponseHeaders>");
  }
};

// Traits for webkit_glue::ResourceLoaderBridge::ResponseInfo
template <>
struct ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo> {
  typedef webkit_glue::ResourceLoaderBridge::ResponseInfo param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.request_time);
    WriteParam(m, p.response_time);
    WriteParam(m, p.headers);
    WriteParam(m, p.mime_type);
    WriteParam(m, p.charset);
    WriteParam(m, p.security_info);
    WriteParam(m, p.content_length);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->request_time) &&
      ReadParam(m, iter, &r->response_time) &&
      ReadParam(m, iter, &r->headers) &&
      ReadParam(m, iter, &r->mime_type) &&
      ReadParam(m, iter, &r->charset) &&
      ReadParam(m, iter, &r->security_info) &&
      ReadParam(m, iter, &r->content_length);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.request_time, l);
    l->append(L", ");
    LogParam(p.response_time, l);
    l->append(L", ");
    LogParam(p.headers, l);
    l->append(L", ");
    LogParam(p.mime_type, l);
    l->append(L", ");
    LogParam(p.charset, l);
    l->append(L", ");
    LogParam(p.security_info, l);
    l->append(L")");
  }
};

// Traits for ViewMsg_Resource_ResponseHead
template <>
struct ParamTraits<ViewMsg_Resource_ResponseHead> {
  typedef ViewMsg_Resource_ResponseHead param_type;
  static void Write(Message* m, const param_type& p) {
    ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo>::Write(m, p);
    WriteParam(m, p.status);
    WriteParam(m, p.filter_policy);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo>::Read(m, iter, r) &&
      ReadParam(m, iter, &r->status) &&
      ReadParam(m, iter, &r->filter_policy);
  }
  static void Log(const param_type& p, std::wstring* l) {
    // log more?
    ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo>::Log(p, l);
  }
};

// Traits for ViewHostMsg_Resource_SyncLoad_Response
template <>
struct ParamTraits<ViewHostMsg_SyncLoad_Result> {
  typedef ViewHostMsg_SyncLoad_Result param_type;
  static void Write(Message* m, const param_type& p) {
    ParamTraits<ViewMsg_Resource_ResponseHead>::Write(m, p);
    WriteParam(m, p.final_url);
    WriteParam(m, p.data);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ParamTraits<ViewMsg_Resource_ResponseHead>::Read(m, iter, r) &&
      ReadParam(m, iter, &r->final_url) &&
      ReadParam(m, iter, &r->data);
  }
  static void Log(const param_type& p, std::wstring* l) {
    // log more?
    ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo>::Log(p, l);
  }
};

// Traits for FormData structure to pack/unpack.
template <>
struct ParamTraits<FormData> {
  typedef FormData param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.origin);
    WriteParam(m, p.action);
    WriteParam(m, p.elements);
    WriteParam(m, p.values);
    WriteParam(m, p.submit);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->origin) &&
      ReadParam(m, iter, &p->action) &&
      ReadParam(m, iter, &p->elements) &&
      ReadParam(m, iter, &p->values) &&
      ReadParam(m, iter, &p->submit);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<FormData>");
  }
};

// Traits for ViewMsg_Print_Params
template <>
struct ParamTraits<ViewMsg_Print_Params> {
  typedef ViewMsg_Print_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.printable_size);
    WriteParam(m, p.dpi);
    WriteParam(m, p.min_shrink);
    WriteParam(m, p.max_shrink);
    WriteParam(m, p.desired_dpi);
    WriteParam(m, p.document_cookie);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->printable_size) &&
           ReadParam(m, iter, &p->dpi) &&
           ReadParam(m, iter, &p->min_shrink) &&
           ReadParam(m, iter, &p->max_shrink) &&
           ReadParam(m, iter, &p->desired_dpi) &&
           ReadParam(m, iter, &p->document_cookie);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ViewMsg_Print_Params>");
  }
};

// Traits for ViewMsg_PrintPage_Params
template <>
struct ParamTraits<ViewMsg_PrintPage_Params> {
  typedef ViewMsg_PrintPage_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.params);
    WriteParam(m, p.page_number);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->params) &&
           ReadParam(m, iter, &p->page_number);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ViewMsg_PrintPage_Params>");
  }
};

// Traits for ViewMsg_PrintPages_Params
template <>
struct ParamTraits<ViewMsg_PrintPages_Params> {
  typedef ViewMsg_PrintPages_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.params);
    WriteParam(m, p.pages);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->params) &&
           ReadParam(m, iter, &p->pages);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ViewMsg_PrintPages_Params>");
  }
};

// Traits for ViewHostMsg_DidPrintPage_Params
template <>
struct ParamTraits<ViewHostMsg_DidPrintPage_Params> {
  typedef ViewHostMsg_DidPrintPage_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.emf_data_handle);
    WriteParam(m, p.data_size);
    WriteParam(m, p.document_cookie);
    WriteParam(m, p.page_number);
    WriteParam(m, p.actual_shrink);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->emf_data_handle) &&
           ReadParam(m, iter, &p->data_size) &&
           ReadParam(m, iter, &p->document_cookie) &&
           ReadParam(m, iter, &p->page_number) &&
           ReadParam(m, iter, &p->actual_shrink);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ViewHostMsg_DidPrintPage_Params>");
  }
};

// Traits for WebPreferences structure to pack/unpack.
template <>
struct ParamTraits<WebPreferences> {
  typedef WebPreferences param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.standard_font_family);
    WriteParam(m, p.fixed_font_family);
    WriteParam(m, p.serif_font_family);
    WriteParam(m, p.sans_serif_font_family);
    WriteParam(m, p.cursive_font_family);
    WriteParam(m, p.fantasy_font_family);
    WriteParam(m, p.default_font_size);
    WriteParam(m, p.default_fixed_font_size);
    WriteParam(m, p.minimum_font_size);
    WriteParam(m, p.minimum_logical_font_size);
    WriteParam(m, p.default_encoding);
    WriteParam(m, p.javascript_enabled);
    WriteParam(m, p.javascript_can_open_windows_automatically);
    WriteParam(m, p.loads_images_automatically);
    WriteParam(m, p.plugins_enabled);
    WriteParam(m, p.dom_paste_enabled);
    WriteParam(m, p.developer_extras_enabled);
    WriteParam(m, p.shrinks_standalone_images_to_fit);
    WriteParam(m, p.uses_universal_detector);
    WriteParam(m, p.text_areas_are_resizable);
    WriteParam(m, p.java_enabled);
    WriteParam(m, p.user_style_sheet_enabled);
    WriteParam(m, p.user_style_sheet_location);
    WriteParam(m, p.uses_page_cache);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
        ReadParam(m, iter, &p->standard_font_family) &&
        ReadParam(m, iter, &p->fixed_font_family) &&
        ReadParam(m, iter, &p->serif_font_family) &&
        ReadParam(m, iter, &p->sans_serif_font_family) &&
        ReadParam(m, iter, &p->cursive_font_family) &&
        ReadParam(m, iter, &p->fantasy_font_family) &&
        ReadParam(m, iter, &p->default_font_size) &&
        ReadParam(m, iter, &p->default_fixed_font_size) &&
        ReadParam(m, iter, &p->minimum_font_size) &&
        ReadParam(m, iter, &p->minimum_logical_font_size) &&
        ReadParam(m, iter, &p->default_encoding) &&
        ReadParam(m, iter, &p->javascript_enabled) &&
        ReadParam(m, iter, &p->javascript_can_open_windows_automatically) &&
        ReadParam(m, iter, &p->loads_images_automatically) &&
        ReadParam(m, iter, &p->plugins_enabled) &&
        ReadParam(m, iter, &p->dom_paste_enabled) &&
        ReadParam(m, iter, &p->developer_extras_enabled) &&
        ReadParam(m, iter, &p->shrinks_standalone_images_to_fit) &&
        ReadParam(m, iter, &p->uses_universal_detector) &&
        ReadParam(m, iter, &p->text_areas_are_resizable) &&
        ReadParam(m, iter, &p->java_enabled) &&
        ReadParam(m, iter, &p->user_style_sheet_enabled) &&
        ReadParam(m, iter, &p->user_style_sheet_location) &&
        ReadParam(m, iter, &p->uses_page_cache);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<WebPreferences>");
  }
};

// Traits for WebDropData
template <>
struct ParamTraits<WebDropData> {
  typedef WebDropData param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.url);
    WriteParam(m, p.url_title);
    WriteParam(m, p.filenames);
    WriteParam(m, p.plain_text);
    WriteParam(m, p.text_html);
    WriteParam(m, p.html_base_url);
    WriteParam(m, p.file_description_filename);
    WriteParam(m, p.file_contents);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->url_title) &&
      ReadParam(m, iter, &p->filenames) &&
      ReadParam(m, iter, &p->plain_text) &&
      ReadParam(m, iter, &p->text_html) &&
      ReadParam(m, iter, &p->html_base_url) &&
      ReadParam(m, iter, &p->file_description_filename) &&
      ReadParam(m, iter, &p->file_contents);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<WebDropData>");
  }
};

// Traits for ScreenInfo
template <>
struct ParamTraits<webkit_glue::ScreenInfo> {
  typedef webkit_glue::ScreenInfo param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.depth);
    WriteParam(m, p.depth_per_component);
    WriteParam(m, p.is_monochrome);
    WriteParam(m, p.rect);
    WriteParam(m, p.available_rect);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->depth) &&
      ReadParam(m, iter, &p->depth_per_component) &&
      ReadParam(m, iter, &p->is_monochrome) &&
      ReadParam(m, iter, &p->rect) &&
      ReadParam(m, iter, &p->available_rect);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<webkit_glue::ScreenInfo>");
  }
};

template<>
struct ParamTraits<ModalDialogEvent> {
  typedef ModalDialogEvent param_type;
#if defined(OS_WIN)
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.event);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->event);
  }
#else
  static void Write(Message* m, const param_type& p) {
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return true;
  }
#endif

  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ModalDialogEvent>");
  }
};

}  // namespace IPC

#endif  // CHROME_COMMON_RENDER_MESSAGES_H_
