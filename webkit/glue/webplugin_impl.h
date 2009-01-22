// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBPLUGIN_IMPL_H_
#define WEBKIT_GLUE_WEBPLUGIN_IMPL_H_

#include <string>
#include <map>
#include <vector>

#include "config.h"
#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceRequest.h"
#include "Widget.h"
MSVC_POP_WARNING();

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/webplugin_delegate.h"

class WebFrameImpl;
class WebPluginDelegate;
class WebPluginImpl;
class MultipartResponseDelegate;

namespace WebCore {
  class Element;
  class Event;
  class Frame;
  class IntRect;
  class KeyboardEvent;
  class KURL;
  class MouseEvent;
  class ResourceHandle;
  class ResourceError;
  class ResourceResponse;
  class ScrollView;
  class String;
  class Widget;
}

// Implements WebCore::Widget functions that WebPluginImpl needs.  This class
// exists because it is possible for the plugin widget to be deleted at any
// time because of a delegate javascript call.  However we don't want the
// WebPluginImpl to be deleted from under us because it could be lower in the
// call stack.
class WebPluginContainer : public WebCore::Widget {
 public:
  WebPluginContainer(WebPluginImpl* impl);
  virtual ~WebPluginContainer();
  NPObject* GetPluginScriptableObject();
  
  // Widget methods:
  virtual void setFrameRect(const WebCore::IntRect& rect);
  virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect& rect);
  virtual void invalidateRect(const WebCore::IntRect&);
  virtual void setFocus();
  virtual void show();
  virtual void hide();
  virtual void handleEvent(WebCore::Event* event);
  virtual void frameRectsChanged();
  virtual void setParent(WebCore::ScrollView* view);

#if USE(JSC)
  virtual bool isPluginView() const;
#endif

  // Returns window-relative rectangles that should clip this widget.
  // Only rects that intersect the given bounds are relevant.
  // Use this to implement iframe shim behavior.
  //
  // TODO(tulrich): add this method to WebCore/platform/Widget.h so it
  // can be used by any platform.
  void windowCutoutRects(const WebCore::IntRect& bounds,
                         WTF::Vector<WebCore::IntRect>* cutouts) const;

  // These methods are invoked from webkit when it has data to be sent to the
  // plugin. The plugin in this case does not initiate a download for the data.
  void didReceiveResponse(const WebCore::ResourceResponse& response);
  void didReceiveData(const char *buffer, int length);
  void didFinishLoading();
  void didFail(const WebCore::ResourceError&);

  void set_ignore_response_error(bool ignore_response_error) {
    ignore_response_error_ = ignore_response_error;
  }

  struct HttpResponseInfo {
    std::string url;
    std::wstring mime_type;
    uint32 last_modified;
    uint32 expected_length;
  };
  // Helper function to read fields in a HTTP response structure.
  // These fields are written to the HttpResponseInfo structure passed in.
  static void ReadHttpResponseInfo(const WebCore::ResourceResponse& response,
                                   HttpResponseInfo* http_response);

 private:
  WebPluginImpl* impl_;
  // Set to true if the next response error should be ignored.
  bool ignore_response_error_;
};

// This is the WebKit side of the plugin implementation that forwards calls,
// after changing out of WebCore types, to a delegate.  The delegate will
// be in a different process.
class WebPluginImpl : public WebPlugin,
                      public WebCore::ResourceHandleClient {
 public:
  // Creates a WebPlugin instance, as long as the delegate's initialization
  // succeeds.  If it fails, the delegate is deleted and NULL is returned.
  // Note that argn and argv are UTF8.
  static WebCore::Widget* Create(const GURL& url,
                                 char** argn,
                                 char** argv,
                                 int argc,
                                 WebCore::Element* element,
                                 WebFrameImpl* frame,
                                 WebPluginDelegate* delegate,
                                 bool load_manually,
                                 const std::string& mime_type);
  virtual ~WebPluginImpl();

  virtual NPObject* GetPluginScriptableObject();

  // Helper function for sorting post data.
  static bool SetPostData(WebCore::ResourceRequest* request,
                          const char *buf,
                          uint32 length);

 private:
  friend class WebPluginContainer;

  WebPluginImpl(WebCore::Element *element, WebFrameImpl *frame,
                WebPluginDelegate* delegate, const GURL& plugin_url,
                bool load_manually, const std::string& mime_type,
                int arg_count, char** arg_names, char** arg_values);

  // WebPlugin implementation:
  void SetWindow(HWND window, HANDLE pump_messages_event);

  // Given a (maybe partial) url, completes using the base url.
  bool CompleteURL(const std::string& url_in, std::string* url_out);

  // Executes the script passed in. The notify_needed and notify_data arguments
  // are passed in by the plugin process. These indicate whether the plugin
  // expects a notification on script execution. We pass them back to the
  // plugin as is. This avoids having to track the notification arguments
  // in the plugin process.
  bool ExecuteScript(const std::string& url, const std::wstring& script,
                     bool notify_needed, int notify_data, bool popups_allowed);

  // Given a download request, check if we need to route the output
  // to a frame.  Returns ROUTED if the load is done and routed to
  // a frame, NOT_ROUTED or corresponding error codes otherwise.
  RoutingStatus RouteToFrame(const char *method, bool is_javascript_url,
                             const char* target, unsigned int len,
                             const char* buf, bool is_file_data, bool notify,
                             const char* url, GURL* completeURL);

  // Cancels a pending request.
  void CancelResource(int id);

  // Returns the next avaiable resource id.
  int GetNextResourceId();

  // Initiates HTTP GET/POST requests.
  // Returns true on success.
  bool InitiateHTTPRequest(int resource_id, WebPluginResourceClient* client,
                           const char* method, const char* buf, int buf_len,
                           const GURL& url, const char* range_info,
                           bool use_plugin_src_as_referer);

  gfx::Rect GetWindowClipRect(const gfx::Rect& rect);

  NPObject* GetWindowScriptNPObject();
  NPObject* GetPluginElement();
  virtual WebFrame* GetWebFrame() { return webframe_; }

  void SetCookie(const GURL& url,
                 const GURL& policy_url,
                 const std::string& cookie);
  std::string GetCookies(const GURL& url,
                         const GURL& policy_url);

  void ShowModalHTMLDialog(const GURL& url, int width, int height,
                           const std::string& json_arguments,
                           std::string* json_retval);
  void OnMissingPluginStatus(int status);
  void Invalidate();
  void InvalidateRect(const gfx::Rect& rect);

  // Widget implementation:
  virtual WebCore::IntRect windowClipRect() const;

  // Returns window-relative rectangles that should clip this widget.
  // Only rects that intersect the given bounds are relevant.
  // Use this to implement iframe shim behavior.
  //
  // TODO(tulrich): windowCutoutRects() is not in WebCore::Widgets
  // yet; need to add it.
  void windowCutoutRects(const WebCore::IntRect& bounds,
                         WTF::Vector<WebCore::IntRect>* rects) const;

  // Override for when our window changes size or position.
  // Used to notify the plugin when the size or position changes.
  virtual void setFrameRect(const WebCore::IntRect& rect);

  // Overrides paint so we can notify the underlying widget to repaint.
  virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect& rect);
  virtual void print(WebCore::GraphicsContext*);

  // Override setFocus so we can notify the Plugin.
  virtual void setFocus();

  // Override show and hide to be able to control the visible state of the
  // plugin window.
  virtual void show();
  virtual void hide();

  // Handle widget events.
  virtual void handleEvent(WebCore::Event* event);
  void handleMouseEvent(WebCore::MouseEvent* event);
  void handleKeyboardEvent(WebCore::KeyboardEvent* event);

  // Sets the actual Widget for the plugin.
  void SetContainer(WebPluginContainer* container);

  // Destroys the plugin instance.
  // The response_handle_to_ignore parameter if not NULL indicates the
  // resource handle to be left valid during plugin shutdown.
  void TearDownPluginInstance(
      WebCore::ResourceHandle* response_handle_to_ignore);

  WebCore::ScrollView* parent() const;

  // ResourceHandleClient implementation.  We implement this interface in the
  // renderer process, and then use the simple WebPluginResourceClient interface
  // to relay the callbacks to the plugin.
  void willSendRequest(WebCore::ResourceHandle* handle,
                       WebCore::ResourceRequest& request,
                       const WebCore::ResourceResponse&);

  void didReceiveResponse(WebCore::ResourceHandle* handle,
                          const WebCore::ResourceResponse& response);
  void didReceiveData(WebCore::ResourceHandle* handle, const char *buffer,
                      int length, int);
  void didFinishLoading(WebCore::ResourceHandle* handle);
  void didFail(WebCore::ResourceHandle* handle, const WebCore::ResourceError&);

  // Helper function
  WebPluginResourceClient* GetClientFromHandle(WebCore::ResourceHandle* handle);

  // Helper function to remove the stored information about a resource
  // request given its index in m_clients.
  void RemoveClient(size_t i);

  // Helper function to remove the stored information about a resource
  // request given a handle.
  void RemoveClient(WebCore::ResourceHandle* handle);

  // Returns all the response headers in one string, including the status code.
  std::wstring GetAllHeaders(const WebCore::ResourceResponse& response);

  WebCore::Frame* frame() { return webframe_ ? webframe_->frame() : NULL; }

  // Calculates the bounds of the plugin widget based on the frame rect passed in.
  void CalculateBounds(const WebCore::IntRect& frame_rect,
                       WebCore::IntRect* window_rect,
                       WebCore::IntRect* clip_rect,
                       std::vector<gfx::Rect>* cutout_rects);

  void HandleURLRequest(const char *method,
                        bool is_javascript_url,
                        const char* target, unsigned int len,
                        const char* buf, bool is_file_data,
                        bool notify, const char* url,
                        void* notify_data, bool popups_allowed);

  void CancelDocumentLoad();

  void InitiateHTTPRangeRequest(const char* url, const char* range_info,
                                HANDLE existing_stream, bool notify_needed,
                                HANDLE notify_data);

  // Handles HTTP multipart responses, i.e. responses received with a HTTP
  // status code of 206.
  void HandleHttpMultipartResponse(const WebCore::ResourceResponse& response,
                                   WebPluginResourceClient* client);

  void HandleURLRequestInternal(const char *method, bool is_javascript_url,
                                const char* target, unsigned int len,
                                const char* buf, bool is_file_data,
                                bool notify, const char* url,
                                void* notify_data, bool popups_allowed,
                                bool use_plugin_src_as_referrer);

  // Tears down the existing plugin instance and creates a new plugin instance
  // to handle the response identified by the response_handle parameter.
  bool ReinitializePluginForResponse(WebCore::ResourceHandle* response_handle);

  // Helper functions to convert an array of names/values to a vector.
  static void ArrayToVector(int total_values, char** values, 
                            std::vector<std::string>* value_vector);

  struct ClientInfo {
    int id;
    WebPluginResourceClient* client;
    WebCore::ResourceRequest request;
    RefPtr<WebCore::ResourceHandle> handle;
  };

  std::vector<ClientInfo> clients_;

  bool windowless_;
  HWND window_;
  WebCore::Element* element_;
  WebFrameImpl* webframe_;

  WebPluginDelegate* delegate_;
  bool visible_;

  WebPluginContainer* widget_;

  typedef std::map<WebPluginResourceClient*, MultipartResponseDelegate*>
      MultiPartResponseHandlerMap;
  // Tracks HTTP multipart response handlers instantiated for
  // a WebPluginResourceClient instance.
  MultiPartResponseHandlerMap multi_part_response_map_;

  // The plugin source URL.
  GURL plugin_url_;

  // Indicates if the download would be initiated by the plugin or us.
  bool load_manually_;

  // Indicates if this is the first geometry update received by the plugin.
  bool first_geometry_update_;

  // The mime type of the plugin.
  std::string mime_type_;

  // Holds the list of argument names passed to the plugin.
  std::vector<std::string> arg_names_;

  // Holds the list of argument values passed to the plugin.
  std::vector<std::string> arg_values_;

  DISALLOW_COPY_AND_ASSIGN(WebPluginImpl);
};

#endif  // #ifndef WEBKIT_GLUE_WEBPLUGIN_IMPL_H_
