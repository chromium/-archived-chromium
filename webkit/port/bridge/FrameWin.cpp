/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include <windows.h>

#pragma warning(push, 0)
#include "Document.h"
#include "EditorClient.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameLoadRequest.h"
#include "FramePrivate.h"
#include "FrameView.h"
#include "Frame.h"
#include "FrameWin.h"
#include "NotImplemented.h"
#include "RenderView.h"

#if USE(JAVASCRIPTCORE_BINDINGS)
#include "JSLock.h"
#include "kjs_proxy.h"
#include "kjs_window.h"
#include "NP_jsobject.h"
#include "NotImplemented.h"
#include "bindings/npruntime.h"
#endif
#if USE(V8_BINDING)
#include "v8_npobject.h"
#include "npruntime_priv.h"
#endif
#include "Page.h"
#include "RenderFrame.h"
#include "ResourceHandle.h"
#if USE(JAVASCRIPTCORE_BINDINGS)
#include "runtime_root.h"
#include "runtime.h"
#endif

#include "Settings.h"
#include "TextResourceDecoder.h"
#include "webkit/glue/webplugin_impl.h"
#pragma warning(pop)

// So we can twiddle event member vars
#define private public
#include "PlatformKeyboardEvent.h"
#undef private

#include "webkit/glue/cache_manager.h"

namespace WebCore {

void Frame::clearPlatformScriptObjects()
{
}

#if USE(JAVASCRIPTCORE_BINDINGS) || USE(V8_BINDING)
JSInstance Frame::createScriptInstanceForWidget(Widget* widget)
{
    ASSERT(widget != 0);

    if (widget->isFrameView())
        return JSInstanceHolder::EmptyInstance();

    // Note:  We have to trust that the widget passed to us here
    // is a WebPluginImpl.  There isn't a way to dynamically verify
    // it, since the derived class (Widget) has no identifier.
    WebPluginContainer* container = static_cast<WebPluginContainer*>(widget);
    if (!container)
        return JSInstanceHolder::EmptyInstance();

    NPObject *npObject = container->GetPluginScriptableObject();
    if (!npObject)
        return JSInstanceHolder::EmptyInstance();

#if USE(JAVASCRIPTCORE_BINDINGS)
    // Register 'widget' with the frame so that we can teardown
    // subobjects when the container goes away.
    RefPtr<KJS::Bindings::RootObject> root = 
        createRootObject(widget, scriptProxy()->globalObject());
    KJS::Bindings::Instance *instance = 
        KJS::Bindings::Instance::createBindingForLanguageInstance(
            KJS::Bindings::Instance::CLanguage, npObject,
            root.release());
    // GetPluginScriptableObject returns a retained NPObject.  
    // The caller is expected to release it.
    NPN_ReleaseObject(npObject);
    return instance;
#else
    // Frame Memory Management for NPObjects
    // -------------------------------------
    // NPObjects are treated differently than other objects wrapped by JS.
    // NPObjects are not Peerable, and cannot be made peerable, since NPObjects
    // can be created either by the browser (e.g. the main window object) or by
    // the plugin (the main plugin object for a HTMLEmbedElement).  Further,
    // unlike most DOM Objects, the frame is especially careful to ensure 
    // NPObjects terminate at frame teardown because if a plugin leaks a 
    // reference, it could leak its objects (or the browser's objects).
    // 
    // The Frame maintains a list of plugin objects (m_pluginObjects)
    // which it can use to quickly find the wrapped embed object.
    // 
    // Inside the NPRuntime, we've added a few methods for registering 
    // wrapped NPObjects.  The purpose of the registration is because 
    // javascript garbage collection is non-deterministic, yet we need to
    // be able to tear down the plugin objects immediately.  When an object
    // is registered, javascript can use it.  When the object is destroyed,
    // or when the object's "owning" object is destroyed, the object will
    // be un-registered, and the javascript engine must not use it.
    //  
    // Inside the javascript engine, the engine can keep a reference to the
    // NPObject as part of its wrapper.  However, before accessing the object
    // it must consult the NPN_Registry.

    v8::Local<v8::Object> wrapper = CreateV8ObjectForNPObject(npObject, NULL);

    // Track the plugin object.  We've been given a reference to the object.
    d->m_pluginObjects.set(widget, npObject);

    JSInstance instance = wrapper;
    return instance;
#endif
}
#endif  // JAVASCRIPTCORE_BINDINGS

void computePageRectsForFrame(WebCore::Frame* frame, const WebCore::IntRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, Vector<IntRect>& pages, int& outPageHeight)
{
    ASSERT(frame);

    pages.clear();
    outPageHeight = 0;

    if (!frame->document() || !frame->view() || !frame->document()->renderer())
        return;
 
    RenderView* root = static_cast<RenderView *>(frame->document()->renderer());

    if (!root) {
        LOG_ERROR("document to be printed has no renderer");
        return;
    }

    if (userScaleFactor <= 0) {
        LOG_ERROR("userScaleFactor has bad value %.2f", userScaleFactor);
        return;
    }
    
    float ratio = static_cast<float>(printRect.height()) / static_cast<float>(printRect.width());
 
    float pageWidth  = (float) root->docWidth();
    float pageHeight = pageWidth * ratio;
    outPageHeight = (int) pageHeight;   // this is the height of the page adjusted by margins
    pageHeight -= (headerHeight + footerHeight);

    if (pageHeight <= 0) {
        LOG_ERROR("pageHeight has bad value %.2f", pageHeight);
        return;
    }

    float currPageHeight = pageHeight / userScaleFactor;
    float docHeight      = root->layer()->height();
    float docWidth       = root->layer()->width();
    float currPageWidth  = pageWidth / userScaleFactor;

    
    // always return at least one page, since empty files should print a blank page
    float printedPagesHeight = 0.0;
    do {
        float proposedBottom = std::min(docHeight, printedPagesHeight + pageHeight);
        frame->adjustPageHeight(&proposedBottom, printedPagesHeight, proposedBottom, printedPagesHeight);
        currPageHeight = max(1.0f, proposedBottom - printedPagesHeight);
       
        pages.append(IntRect(0,printedPagesHeight,currPageWidth,currPageHeight));
        printedPagesHeight += currPageHeight;
    } while (printedPagesHeight < docHeight);
}

DragImageRef Frame::dragImageForSelection()
{    
    if (selectionController()->isRange())
        return 0;  // TODO(pkasting): http://b/119669 Implement me!

    return 0;
}

void Frame::dashboardRegionsChanged()
{
}

} // namespace WebCore
