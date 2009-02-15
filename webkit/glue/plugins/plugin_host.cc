// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// HACK: we need this #define in place before npapi.h is included for
// plugins to work. However, all sorts of headers include npapi.h, so
// the only way to be certain the define is in place is to put it
// here.  You might ask, "Why not set it in npapi.h directly, or in
// this directory's SConscript, then?"  but it turns out this define
// makes npapi.h include Xlib.h, which in turn defines a ton of symbols
// like None and Status, causing conflicts with the aforementioned
// many headers that include npapi.h.  Ugh.
// See also webplugin_delegate_impl.cc.
#define MOZ_X11 1

#include "config.h"

#include "webkit/glue/plugins/plugin_host.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "net/base/net_util.h"
#include "webkit/default_plugin/default_plugin_shared.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/plugin_stream_url.h"
#include "third_party/npapi/bindings/npruntime.h"


namespace NPAPI
{
scoped_refptr<PluginHost> PluginHost::singleton_;

PluginHost::PluginHost() {
  InitializeHostFuncs();
}

PluginHost::~PluginHost() {
}

PluginHost *PluginHost::Singleton() {
  if (singleton_.get() == NULL) {
    singleton_ = new PluginHost();
  }

  DCHECK(singleton_.get() != NULL);
  return singleton_;
}

void PluginHost::InitializeHostFuncs() {
  memset(&host_funcs_, 0, sizeof(host_funcs_));
  host_funcs_.size = sizeof(host_funcs_);
  host_funcs_.version = (NP_VERSION_MAJOR << 8) | (NP_VERSION_MINOR);

  // The "basic" functions
  host_funcs_.geturl = &NPN_GetURL;
  host_funcs_.posturl = &NPN_PostURL;
  host_funcs_.requestread = &NPN_RequestRead;
  host_funcs_.newstream = &NPN_NewStream;
  host_funcs_.write = &NPN_Write;
  host_funcs_.destroystream = &NPN_DestroyStream;
  host_funcs_.status = &NPN_Status;
  host_funcs_.uagent = &NPN_UserAgent;
  host_funcs_.memalloc = &NPN_MemAlloc;
  host_funcs_.memfree = &NPN_MemFree;
  host_funcs_.memflush = &NPN_MemFlush;
  host_funcs_.reloadplugins = &NPN_ReloadPlugins;

  // We don't implement java yet
  host_funcs_.getJavaEnv = &NPN_GetJavaEnv;
  host_funcs_.getJavaPeer = &NPN_GetJavaPeer;

  // Advanced functions we implement
  host_funcs_.geturlnotify = &NPN_GetURLNotify;
  host_funcs_.posturlnotify = &NPN_PostURLNotify;
  host_funcs_.getvalue = &NPN_GetValue;
  host_funcs_.setvalue = &NPN_SetValue;
  host_funcs_.invalidaterect = &NPN_InvalidateRect;
  host_funcs_.invalidateregion = &NPN_InvalidateRegion;
  host_funcs_.forceredraw = &NPN_ForceRedraw;

  // These come from the Javascript Engine
  host_funcs_.getstringidentifier = NPN_GetStringIdentifier;
  host_funcs_.getstringidentifiers = NPN_GetStringIdentifiers;
  host_funcs_.getintidentifier = NPN_GetIntIdentifier;
  host_funcs_.identifierisstring = NPN_IdentifierIsString;
  host_funcs_.utf8fromidentifier = NPN_UTF8FromIdentifier;
  host_funcs_.intfromidentifier = NPN_IntFromIdentifier;
  host_funcs_.createobject = NPN_CreateObject;
  host_funcs_.retainobject = NPN_RetainObject;
  host_funcs_.releaseobject = NPN_ReleaseObject;
  host_funcs_.invoke = NPN_Invoke;
  host_funcs_.invokeDefault = NPN_InvokeDefault;
  host_funcs_.evaluate = NPN_Evaluate;
  host_funcs_.getproperty = NPN_GetProperty;
  host_funcs_.setproperty = NPN_SetProperty;
  host_funcs_.removeproperty = NPN_RemoveProperty;
  host_funcs_.hasproperty = NPN_HasProperty;
  host_funcs_.hasmethod = NPN_HasMethod;
  host_funcs_.releasevariantvalue = NPN_ReleaseVariantValue;
  host_funcs_.setexception = NPN_SetException;
  host_funcs_.pushpopupsenabledstate = &NPN_PushPopupsEnabledState;
  host_funcs_.poppopupsenabledstate = &NPN_PopPopupsEnabledState;
  host_funcs_.enumerate = &NPN_Enumerate;
  host_funcs_.pluginthreadasynccall = &NPN_PluginThreadAsyncCall;
  host_funcs_.construct = &NPN_Construct;

}

void PluginHost::PatchNPNetscapeFuncs(NPNetscapeFuncs* overrides) {
  // When running in the plugin process, we need to patch the NPN functions
  // that the plugin calls to interact with NPObjects that we give.  Otherwise
  // the plugin will call the v8 NPN functions, which won't work since we have
  // an NPObjectProxy and not a real v8 implementation.
  if (overrides->invoke)
    host_funcs_.invoke = overrides->invoke;

  if (overrides->invokeDefault)
    host_funcs_.invokeDefault = overrides->invokeDefault;

  if (overrides->evaluate)
    host_funcs_.evaluate = overrides->evaluate;

  if (overrides->getproperty)
    host_funcs_.getproperty = overrides->getproperty;

  if (overrides->setproperty)
    host_funcs_.setproperty = overrides->setproperty;

  if (overrides->removeproperty)
    host_funcs_.removeproperty = overrides->removeproperty;

  if (overrides->hasproperty)
    host_funcs_.hasproperty = overrides->hasproperty;

  if (overrides->hasmethod)
    host_funcs_.hasmethod = overrides->hasmethod;

  if (overrides->setexception)
    host_funcs_.setexception = overrides->setexception;

  if (overrides->enumerate)
    host_funcs_.enumerate = overrides->enumerate;
}

bool PluginHost::SetPostData(const char *buf,
                             uint32 length,
                             std::vector<std::string>* names,
                             std::vector<std::string>* values,
                             std::vector<char>* body) {
  // Use a state table to do the parsing.  Whitespace must be
  // trimmed after the fact if desired.  In our case, we actually
  // don't care about the whitespace, because we're just going to
  // pass this back into another POST.  This function strips out the
  // "Content-length" header and does not append it to the request.

  //
  // This parser takes action only on state changes.
  //
  // Transition table:
  //                  :       \n  NULL    Other
  // 0 GetHeader      1       2   4       0
  // 1 GetValue       1       0   3       1
  // 2 GetData        2       2   3       2
  // 3 DONE
  // 4 ERR
  //
  enum { INPUT_COLON=0, INPUT_NEWLINE, INPUT_NULL, INPUT_OTHER };
  enum { GETNAME, GETVALUE, GETDATA, DONE, ERR };
  int statemachine[3][4] = { { GETVALUE, GETDATA, GETDATA, GETNAME },
                             { GETVALUE, GETNAME, DONE, GETVALUE },
                             { GETDATA,  GETDATA, DONE, GETDATA } };
  std::string name, value;
  char *ptr = (char*)buf;
  char *start = ptr;
  int state = GETNAME;  // initial state
  bool done = false;
  bool err = false;
  do {
    int input;

    // Translate the current character into an input
    // for the state table.
    switch (*ptr) {
      case ':' :
        input = INPUT_COLON;
        break;
      case '\n':
        input = INPUT_NEWLINE;
        break;
      case 0   :
        input = INPUT_NULL;
        break;
      default  :
        input = INPUT_OTHER;
        break;
    }

    int newstate = statemachine[state][input];

    // Take action based on the new state.
    if (state != newstate) {
      switch (newstate) {
      case GETNAME:
        // Got a value.
        value = std::string(start, ptr - start);
        TrimWhitespace(value, TRIM_ALL, &value);
        // If the name field is empty, we'll skip this header
        // but we won't error out.
        if (!name.empty() && name != "content-length") {
          names->push_back(name);
          values->push_back(value);
        }
        start = ptr + 1;
        break;
      case GETVALUE:
        // Got a header.
        name = StringToLowerASCII(std::string(start, ptr - start));
        TrimWhitespace(name, TRIM_ALL, &name);
        start = ptr + 1;
        break;
      case GETDATA:
      {
        // Finished headers, now get body
        if (*ptr)
          start = ptr + 1;
        size_t previous_size = body->size();
        size_t new_body_size = length - static_cast<int>(start - buf);
        body->resize(previous_size + new_body_size);
        if (!body->empty())
          memcpy(&body->front() + previous_size, start, new_body_size);
        done = true;
        break;
      }
      case ERR:
        // error
        err = true;
        done = true;
        break;
      }
    }
    state = newstate;
    ptr++;
  } while (!done);

  return !err;
}

} // namespace NPAPI

extern "C" {

// FindInstance()
// Finds a PluginInstance from an NPP.
// The caller must take a reference if needed.
NPAPI::PluginInstance* FindInstance(NPP id) {
  if (id == NULL) {
    NOTREACHED();
    return NULL;
  }

  return (NPAPI::PluginInstance *)id->ndata;
}

// Allocates memory from the host's memory space.
void* NPN_MemAlloc(uint32 size) {
  scoped_refptr<NPAPI::PluginHost> host = NPAPI::PluginHost::Singleton();
  if (host != NULL) {
    // Note: We must use the same allocator/deallocator
    // that is used by the javascript library, as some of the
    // JS APIs will pass memory to the plugin which the plugin
    // will attempt to free.
    return malloc(size);
  }
  return NULL;
}

// Deallocates memory from the host's memory space
void NPN_MemFree(void* ptr) {
  scoped_refptr<NPAPI::PluginHost> host = NPAPI::PluginHost::Singleton();
  if (host != NULL) {
    if (ptr != NULL && ptr != (void*)-1) {
      free(ptr);
    }
  }
}

// Requests that the host free a specified amount of memory.
uint32 NPN_MemFlush(uint32 size) {
  // This is not relevant on Windows; MAC specific
  return size;
}

// This is for dynamic discovery of new plugins.
// Should force a re-scan of the plugins directory to load new ones.
void  NPN_ReloadPlugins(NPBool reloadPages) {
  // TODO: implement me
  DLOG(INFO) << "NPN_ReloadPlugin is not implemented yet.";
}

// Requests a range of bytes for a seekable stream.
NPError  NPN_RequestRead(NPStream* stream, NPByteRange* range_list) {
  if (!stream || !range_list) {
    return NPERR_GENERIC_ERROR;
  }

  scoped_refptr<NPAPI::PluginInstance> plugin =
      reinterpret_cast<NPAPI::PluginInstance*>(stream->ndata);
  if (!plugin.get()) {
    return NPERR_GENERIC_ERROR;
  }

  plugin->RequestRead(stream, range_list);
  return NPERR_NO_ERROR;
}

static bool IsJavaScriptUrl(const std::string& url) {
  return StartsWithASCII(url, "javascript:", false);
}

// Generic form of GetURL for common code between
// GetURL() and GetURLNotify().
static NPError GetURLNotify(NPP id,
                            const char* url,
                            const char* target,
                            bool notify,
                            void* notify_data) {
  if (!url)
    return NPERR_INVALID_URL;

  bool is_javascript_url = IsJavaScriptUrl(url);

  scoped_refptr<NPAPI::PluginInstance> plugin = FindInstance(id);
  if (plugin.get()) {
    plugin->webplugin()->HandleURLRequest("GET",
                                          is_javascript_url,
                                          target,
                                          0,
                                          0,
                                          false,
                                          notify,
                                          url,
                                          notify_data,
                                          plugin->popups_allowed());
  } else {
    NOTREACHED();
    return NPERR_GENERIC_ERROR;
  }
  return NPERR_NO_ERROR;
}

// Requests creation of a new stream with the contents of the
// specified URL; gets notification of the result.
NPError NPN_GetURLNotify(NPP id,
                         const char* url,
                         const char* target,
                         void* notify_data) {
  // This is identical to NPN_GetURL, but after finishing, the
  // browser will call NPP_URLNotify to inform the plugin that
  // it has completed.

  // According to the NPAPI documentation, if target == _self
  // or a parent to _self, the browser should return NPERR_INVALID_PARAM,
  // because it can't notify the plugin once deleted.  This is
  // absolutely false; firefox doesn't do this, and Flash relies on
  // being able to use this.

  // Also according to the NPAPI documentation, we should return
  // NPERR_INVALID_URL if the url requested is not valid.  However,
  // this would require that we synchronously start fetching the
  // URL.  That just isn't practical.  As such, there really is
  // no way to return this error.  From looking at the Firefox
  // implementation, it doesn't look like Firefox does this either.

  return GetURLNotify(id, url, target, true, notify_data);
}

NPError  NPN_GetURL(NPP id, const char* url, const char* target) {
  // Notes:
  //    Request from the Plugin to fetch content either for the plugin
  //    or to be placed into a browser window.
  //
  // If target == null, the browser fetches content and streams to plugin.
  //    otherwise, the browser loads content into an existing browser frame.
  // If the target is the window/frame containing the plugin, the plugin
  //    may be destroyed.
  // If the target is _blank, a mailto: or news: url open content in a new
  //    browser window
  // If the target is _self, no other instance of the plugin is created.  The
  //    plugin continues to operate in its own window

  return GetURLNotify(id, url, target, false, 0);
}

// Generic form of PostURL for common code between
// PostURL() and PostURLNotify().
static NPError PostURLNotify(NPP id,
                             const char* url,
                             const char* target,
                             uint32 len,
                             const char* buf,
                             NPBool file,
                             bool notify,
                             void* notify_data) {
  if (!url)
    return NPERR_INVALID_URL;

  scoped_refptr<NPAPI::PluginInstance> plugin = FindInstance(id);
  if (!plugin.get()) {
    NOTREACHED();
    return NPERR_GENERIC_ERROR;
  }

  std::string post_file_contents;

  if (file) {
    // Post data to be uploaded from a file. This can be handled in two
    // ways.
    // 1. Read entire file and send the contents as if it was a post data
    //    specified in the argument
    // 2. Send just the file details and read them in the browser at the
    //    time of sending the request.
    // Approach 2 is more efficient but complicated. Approach 1 has a major
    // drawback of sending potentially large data over two IPC hops.  In a way
    // 'large data over IPC' problem exists as it is in case of plugin giving
    // the data directly instead of in a file.
    // Currently we are going with the approach 1 to get the feature working.
    // We can optimize this later with approach 2.

    // TODO(joshia): Design a scheme to send a file descriptor instead of
    // entire file contents across.

    // Security alert:
    // ---------------
    // Here we are blindly uploading whatever file requested by a plugin.
    // This is risky as someone could exploit a plugin to send private
    // data in arbitrary locations.
    // A malicious (non-sandboxed) plugin has unfeterred access to OS
    // resources and can do this anyway without using browser's HTTP stack.
    // FWIW, Firefox and Safari don't perform any security checks.

    if (!buf)
      return NPERR_FILE_NOT_FOUND;

    std::string file_path_ascii(buf);
    std::wstring file_path;
    static const char kFileUrlPrefix[] = "file:";
    if (StartsWithASCII(file_path_ascii, kFileUrlPrefix, false)) {
      GURL file_url(file_path_ascii);
      DCHECK(file_url.SchemeIsFile());
      net::FileURLToFilePath(file_url, &file_path);
    } else {
      file_path = base::SysNativeMBToWide(file_path_ascii);
    }

    file_util::FileInfo post_file_info = {0};
    if (!file_util::GetFileInfo(file_path.c_str(), &post_file_info) ||
        post_file_info.is_directory)
      return NPERR_FILE_NOT_FOUND;

    if (!file_util::ReadFileToString(file_path, &post_file_contents))
      return NPERR_FILE_NOT_FOUND;

    buf = post_file_contents.c_str();
    len = post_file_contents.size();
  }

  bool is_javascript_url = IsJavaScriptUrl(url);

  // The post data sent by a plugin contains both headers
  // and post data.  Example:
  //      Content-type: text/html
  //      Content-length: 200
  //
  //      <200 bytes of content here>
  //
  // Unfortunately, our stream needs these broken apart,
  // so we need to parse the data and set headers and data
  // separately.
  plugin->webplugin()->HandleURLRequest("POST",
                                        is_javascript_url,
                                        target,
                                        len,
                                        buf,
                                        false,
                                        notify,
                                        url,
                                        notify_data,
                                        plugin->popups_allowed());
  return NPERR_NO_ERROR;
}

NPError  NPN_PostURLNotify(NPP id,
                           const char* url,
                           const char* target,
                           uint32 len,
                           const char* buf,
                           NPBool file,
                           void* notify_data) {
  return PostURLNotify(id, url, target, len, buf, file, true, notify_data);
}

NPError  NPN_PostURL(NPP id,
                     const char* url,
                     const char* target,
                     uint32 len,
                     const char* buf,
                     NPBool file) {
  // POSTs data to an URL, either from a temp file or a buffer.
  // If file is true, buf contains a temp file (which host will delete after
  //   completing), and len contains the length of the filename.
  // If file is false, buf contains the data to send, and len contains the
  //   length of the buffer
  //
  // If target is null,
  //   server response is returned to the plugin
  // If target is _current, _self, or _top,
  //   server response is written to the plugin window and plugin is unloaded.
  // If target is _new or _blank,
  //   server response is written to a new browser window
  // If target is an existing frame,
  //   server response goes to that frame.
  //
  // For protocols other than FTP
  //   file uploads must be line-end converted from \r\n to \n
  //
  // Note:  you cannot specify headers (even a blank line) in a memory buffer,
  //        use NPN_PostURLNotify

  return PostURLNotify(id, url, target, len, buf, file, false, 0);
}

NPError NPN_NewStream(NPP id,
                      NPMIMEType type,
                      const char* target,
                      NPStream** stream) {
  // Requests creation of a new data stream produced by the plugin,
  // consumed by the browser.
  //
  // Browser should put this stream into a window target.
  //
  // TODO: implement me
  DLOG(INFO) << "NPN_NewStream is not implemented yet.";
  return NPERR_GENERIC_ERROR;
}

int32 NPN_Write(NPP id, NPStream* stream, int32 len, void* buffer) {
  // Writes data to an existing Plugin-created stream.

  // TODO: implement me
  DLOG(INFO) << "NPN_Write is not implemented yet.";
  return NPERR_GENERIC_ERROR;
}

NPError NPN_DestroyStream(NPP id, NPStream* stream, NPReason reason) {
  // Destroys a stream (could be created by plugin or browser).
  //
  // Reasons:
  //    NPRES_DONE          - normal completion
  //    NPRES_USER_BREAK    - user terminated
  //    NPRES_NETWORK_ERROR - network error (all errors fit here?)
  //
  //

  scoped_refptr<NPAPI::PluginInstance> plugin = FindInstance(id);
  if (plugin.get() == NULL) {
    NOTREACHED();
    return NPERR_GENERIC_ERROR;
  }

  return plugin->NPP_DestroyStream(stream, reason);
}

const char* NPN_UserAgent(NPP id) {
#if defined(OS_WIN)
  // Flash passes in a null id during the NP_initialize call.  We need to
  // default to the Mozilla user agent if we don't have an NPP instance or
  // else Flash won't request windowless mode.
  if (id) {
    scoped_refptr<NPAPI::PluginInstance> plugin = FindInstance(id);
    if (plugin.get() && !plugin->use_mozilla_user_agent())
      return webkit_glue::GetUserAgent(GURL()).c_str();
  }

  return "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.9a1) Gecko/20061103 Firefox/2.0a1";
#else
  // TODO(port): For now we always use our real useragent on Mac and Linux.
  // We might eventually need to spoof for some plugins.
  return webkit_glue::GetUserAgent(GURL()).c_str();
#endif
}

void NPN_Status(NPP id, const char* message) {
  // Displays a message on the status line of the browser window.

  // TODO: implement me
  DLOG(INFO) << "NPN_Status is not implemented yet.";
}

void NPN_InvalidateRect(NPP id, NPRect *invalidRect) {
  // Invalidates specified drawing area prior to repainting or refreshing a
  // windowless plugin

  // Before a windowless plugin can refresh part of its drawing area, it must
  // first invalidate it.  This function causes the NPP_HandleEvent method to
  // pass an update event or a paint message to the plug-in.  After calling
  // this method, the plug-in recieves a paint message asynchronously.

  // The browser redraws invalid areas of the document and any windowless
  // plug-ins at regularly timed intervals. To force a paint message, the
  // plug-in can call NPN_ForceRedraw after calling this method.

  scoped_refptr<NPAPI::PluginInstance> plugin = FindInstance(id);
  DCHECK(plugin.get() != NULL);
  if (plugin.get() && plugin->webplugin()) {
    if (invalidRect) {
      if (!plugin->windowless()) {
#if defined(OS_WIN)
        RECT rect = {0};
        rect.left = invalidRect->left;
        rect.right = invalidRect->right;
        rect.top = invalidRect->top;
        rect.bottom = invalidRect->bottom;
        ::InvalidateRect(plugin->window_handle(), &rect, FALSE);
#elif defined(OS_MACOSX)
        NOTIMPLEMENTED();
#else
        NOTIMPLEMENTED();
#endif
        return;
      }

      gfx::Rect rect(invalidRect->left,
                     invalidRect->top,
                     invalidRect->right - invalidRect->left,
                     invalidRect->bottom - invalidRect->top);
      plugin->webplugin()->InvalidateRect(rect);
    } else {
      plugin->webplugin()->Invalidate();
    }
  }
}

void NPN_InvalidateRegion(NPP id, NPRegion invalidRegion) {
  // Invalidates a specified drawing region prior to repainting
  // or refreshing a window-less plugin.
  //
  // Similar to NPN_InvalidateRect.

  // TODO: implement me
  DLOG(INFO) << "NPN_InvalidateRegion is not implemented yet.";
}

void NPN_ForceRedraw(NPP id) {
  // Forces repaint for a windowless plug-in.
  //
  // Once a value has been invalidated with NPN_InvalidateRect/
  // NPN_InvalidateRegion, ForceRedraw can be used to force a paint message.
  //
  // The plugin will receive a WM_PAINT message, the lParam of the WM_PAINT
  // message holds a pointer to an NPRect that is the bounding box of the
  // update area.
  // Since the plugin and browser share the same HDC, before drawing, the
  // plugin is responsible fro saving the current HDC settings, setting up
  // its own environment, drawing, and restoring the HDC to the previous
  // settings.  The HDC settings must be restored whenever control returns
  // back to the browser, either before returning from NPP_HandleEvent or
  // before calling a drawing-related netscape method.
  //

  // TODO: implement me
  DLOG(INFO) << "NPN_ForceRedraw is not implemented yet.";
}

NPError NPN_GetValue(NPP id, NPNVariable variable, void *value) {
  // Allows the plugin to query the browser for information
  //
  // Variables:
  //    NPNVxDisplay (unix only)
  //    NPNVxtAppContext (unix only)
  //    NPNVnetscapeWindow (win only) - Gets the native window on which the
  //              plug-in drawing occurs, returns HWND
  //    NPNVjavascriptEnabledBool:  tells whether Javascript is enabled
  //    NPNVasdEnabledBool:  tells whether SmartUpdate is enabled
  //    NPNVOfflineBool: tells whether offline-mode is enabled

  NPError rv = NPERR_GENERIC_ERROR;

  switch (variable) {
  case NPNVWindowNPObject:
  {
    scoped_refptr<NPAPI::PluginInstance> plugin = FindInstance(id);
    NPObject *np_object = plugin->webplugin()->GetWindowScriptNPObject();
    // Return value is expected to be retained, as
    // described here:
    // <http://www.mozilla.org/projects/plugins/npruntime.html#browseraccess>
    if (np_object) {
      NPN_RetainObject(np_object);
      void **v = (void **)value;
      *v = np_object;
      rv = NPERR_NO_ERROR;
    } else {
      NOTREACHED();
    }
    break;
  }
  case NPNVPluginElementNPObject:
  {
    scoped_refptr<NPAPI::PluginInstance> plugin = FindInstance(id);
    NPObject *np_object = plugin->webplugin()->GetPluginElement();
    // Return value is expected to be retained, as
    // described here:
    // <http://www.mozilla.org/projects/plugins/npruntime.html#browseraccess>
    if (np_object) {
      NPN_RetainObject(np_object);
      void **v = (void **)value;
      *v = np_object;
      rv = NPERR_NO_ERROR;
    } else {
      NOTREACHED();
    }
    break;
  }
  case NPNVnetscapeWindow:
  {
#if defined(OS_WIN)
    scoped_refptr<NPAPI::PluginInstance> plugin = FindInstance(id);
    HWND handle = plugin->window_handle();
    *((void**)value) = (void*)handle;
    rv = NPERR_NO_ERROR;
#else
    NOTIMPLEMENTED();
#endif
    break;
  }
  case NPNVjavascriptEnabledBool:
  {
    // yes, JS is enabled.
    *((void**)value) = (void*)1;
    rv = NPERR_NO_ERROR;
    break;
  }
  case NPNVserviceManager:
  {
    NPAPI::PluginInstance* instance =
        NPAPI::PluginInstance::GetInitializingInstance();
    if (instance) {
      instance->GetServiceManager(reinterpret_cast<void**>(value));
    } else {
      NOTREACHED();
    }

    rv = NPERR_NO_ERROR;
    break;
  }
#if defined(OS_LINUX)
  case NPNVToolkit:
    // Tell them we are GTK2.  (The alternative is GTK 1.2.)
    *reinterpret_cast<int*>(value) = NPNVGtk2;
    rv = NPERR_NO_ERROR;
    break;

  case NPNVSupportsXEmbedBool:
    // Yes, we support XEmbed.
    *reinterpret_cast<NPBool*>(value) = TRUE;
    rv = NPERR_NO_ERROR;
    break;
#endif
  case NPNVSupportsWindowless:
  {
    NPBool* supports_windowless = reinterpret_cast<NPBool*>(value);
    *supports_windowless = TRUE;
    rv = NPERR_NO_ERROR;
    break;
  }
  case default_plugin::kMissingPluginStatusStart +
       default_plugin::MISSING_PLUGIN_AVAILABLE:
  // fall through
  case default_plugin::kMissingPluginStatusStart +
       default_plugin::MISSING_PLUGIN_USER_STARTED_DOWNLOAD:
  {
    // This is a hack for the default plugin to send notification to renderer.
    // Because we will check if the plugin is default plugin, we don't need
    // to worry about future standard change that may conflict with the
    // variable definition.
    scoped_refptr<NPAPI::PluginInstance> plugin = FindInstance(id);
    if (plugin->plugin_lib()->plugin_info().path.value() ==
          kDefaultPluginLibraryName) {
      plugin->webplugin()->OnMissingPluginStatus(
          variable - default_plugin::kMissingPluginStatusStart);
    }
    break;
  }
#if defined(OS_MACOSX)
  case NPNVsupportsQuickDrawBool:
  {
    // we do not support the QuickDraw drawing model
    NPBool* supports_qd = reinterpret_cast<NPBool*>(value);
    *supports_qd = FALSE;
    rv = NPERR_NO_ERROR;
    break;
  }
  case NPNVsupportsCoreGraphicsBool:
  {
    // we do support (and in fact require) the CoreGraphics drawing model
    NPBool* supports_cg = reinterpret_cast<NPBool*>(value);
    *supports_cg = TRUE;
    rv = NPERR_NO_ERROR;
    break;
  }
#endif
  default:
  {
    // TODO: implement me
    DLOG(INFO) << "NPN_GetValue(" << variable << ") is not implemented yet.";
    break;
  }
  }
  return rv;
}

NPError  NPN_SetValue(NPP id, NPPVariable variable, void *value) {
  // Allows the plugin to set various modes

  scoped_refptr<NPAPI::PluginInstance> plugin = FindInstance(id);
  switch(variable)
  {
  case NPPVpluginWindowBool:
  {
    // Sets windowless mode for display of the plugin
    // Note: the documentation at http://developer.mozilla.org/en/docs/NPN_SetValue
    // is wrong.  When value is NULL, the mode is set to true.  This is the same
    // way Mozilla works.
    plugin->set_windowless(value == 0);
    return NPERR_NO_ERROR;
  }
  case NPPVpluginTransparentBool:
  {
    // Sets transparent mode for display of the plugin
    //
    // Transparent plugins require the browser to paint the background
    // before having the plugin paint.  By default, windowless plugins
    // are transparent.  Making a windowless plugin opaque means that
    // the plugin does not require the browser to paint the background.
    //
    bool mode = (value != 0);
    plugin->set_transparent(mode);
    return NPERR_NO_ERROR;
  }
  case NPPVjavascriptPushCallerBool:
    // Specifies whether you are pushing or popping the JSContext off
    // the stack
    // TODO: implement me
    DLOG(INFO) << "NPN_SetValue(NPPVJavascriptPushCallerBool) is not implemented.";
    return NPERR_GENERIC_ERROR;
  case NPPVpluginKeepLibraryInMemory:
    // Tells browser that plugin library should live longer than usual.
    // TODO: implement me
    DLOG(INFO) << "NPN_SetValue(NPPVpluginKeepLibraryInMemory) is not implemented.";
    return NPERR_GENERIC_ERROR;
#if defined(OS_MACOSX)
  case NPNVpluginDrawingModel:
    // we only support the CoreGraphics drawing model
    if (reinterpret_cast<int>(value) == NPDrawingModelCoreGraphics)
      return NPERR_NO_ERROR;
    return NPERR_GENERIC_ERROR;    
#endif
  default:
    // TODO: implement me
    DLOG(INFO) << "NPN_SetValue(" << variable << ") is not implemented.";
    break;
  }

  NOTREACHED();
  return NPERR_GENERIC_ERROR;
}

void *NPN_GetJavaEnv() {
  // TODO: implement me
  DLOG(INFO) << "NPN_GetJavaEnv is not implemented.";
  return NULL;
}

void *NPN_GetJavaPeer(NPP) {
  // TODO: implement me
  DLOG(INFO) << "NPN_GetJavaPeer is not implemented.";
  return NULL;
}

void NPN_PushPopupsEnabledState(NPP id, NPBool enabled) {
  scoped_refptr<NPAPI::PluginInstance> plugin = FindInstance(id);
  if (plugin) {
    plugin->PushPopupsEnabledState(enabled);
  }
}

void NPN_PopPopupsEnabledState(NPP id) {
  scoped_refptr<NPAPI::PluginInstance> plugin = FindInstance(id);
  if (plugin) {
    plugin->PopPopupsEnabledState();
  }
}

void NPN_PluginThreadAsyncCall(NPP id,
                         void (*func)(void *),
                         void *userData) {
  scoped_refptr<NPAPI::PluginInstance> plugin = FindInstance(id);
  if (plugin) {
    plugin->PluginThreadAsyncCall(func, userData);
  }
}

} // extern "C"

