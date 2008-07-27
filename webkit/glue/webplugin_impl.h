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

#ifndef WEBKIT_GLUE_WEBPLUGIN_IMPL_H__
#define WEBKIT_GLUE_WEBPLUGIN_IMPL_H__

#include <string>
#include <vector>

#include "config.h"
#pragma warning(push, 0)
#include "ResourceHandleClient.h"
#include "ResourceRequest.h"
#include "Widget.h"
#pragma warning(pop)

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/webplugin_delegate.h"

class WebFrameImpl;
class WebPluginDelegate;
class WebPluginImpl;

namespace WebCore {
  class DeprecatedString;
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
  virtual WebCore::IntRect windowClipRect() const;
  virtual void geometryChanged() const;
  virtual void setFrameGeometry(const WebCore::IntRect& rect);
  virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect& rect);
  virtual void setFocus();
  virtual void show();
  virtual void hide();
  virtual void handleEvent(WebCore::Event* event);
  virtual void attachToWindow();
  virtual void detachFromWindow();

  // These methods are invoked from webkit when it has data to be sent to the 
  // plugin. The plugin in this case does not initiate a download for the data.
  void didReceiveResponse(const WebCore::ResourceResponse& response);
  void didReceiveData(const char *buffer, int length);
  void didFinishLoading();
  void didFail(const WebCore::ResourceError&);

 private:
  WebPluginImpl* impl_;
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
                                 bool load_manually);
  virtual ~WebPluginImpl();

  virtual NPObject* GetPluginScriptableObject();

  // Helper function for sorting post data. 
  static bool SetPostData(WebCore::ResourceRequest* request,
                          const char *buf,
                          uint32 length);

 private:
  friend class WebPluginContainer;

  WebPluginImpl(WebCore::Element *element, WebFrameImpl *frame,
                WebPluginDelegate* delegate);

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
                           const GURL& complete_url_string);

  gfx::Rect GetWindowClipRect(const gfx::Rect& rect);

  NPObject* GetWindowScriptNPObject();
  NPObject* GetPluginElement();

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
  virtual void geometryChanged() const;

  // Override for when our window changes size or position.
  // Used to notify the plugin when the size or position changes.
  virtual void setFrameGeometry(const WebCore::IntRect& rect);

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
                       WebCore::IntRect* clip_rect);

  void HandleURLRequest(const char *method, 
                        bool is_javascript_url,
                        const char* target, unsigned int len,
                        const char* buf, bool is_file_data,
                        bool notify, const char* url,
                        void* notify_data, bool popups_allowed);

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
  bool force_geometry_update_;
  bool visible_;
  // Set when we receive the first paint notification for the plugin widget.
  bool received_first_paint_notification_;

  WebPluginContainer* widget_;

  DISALLOW_EVIL_CONSTRUCTORS(WebPluginImpl);
};

#endif  // #ifndef WEBKIT_GLUE_WEBPLUGIN_IMPL_H__
