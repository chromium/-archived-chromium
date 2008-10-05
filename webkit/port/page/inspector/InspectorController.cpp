/*
 * Copyright (C) 2007 Google Inc. All rights reserved.
 * Authors: Collin Jackson, Adam Barth
 *
 * This is the V8 version of the KJS InspectorController, which is located in
 * webkit/pending.
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorController.h"

#include "NotImplemented.h"

#include "CString.h"
#include "CachedCSSStyleSheet.h"
#include "CachedResource.h"
#include "CachedScript.h"
#include "CachedXSLStyleSheet.h"
#include "Console.h"
#include "DOMWindow.h"
#include "DocLoader.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFrameOwnerElement.h"
#include "InspectorClient.h"
#if USE(JSC)
#include "JSDOMWindow.h"
#include "JSInspectedObjectWrapper.h"
#include "JSInspectorCallbackWrapper.h"
#include "JSNode.h"
#include "JSRange.h"
#elif USE(V8)
#include "v8_proxy.h"
#include "v8_binding.h"
#endif
// TODO(ojan): Import this and enable the JavaScriptDebugServer in the code below.
// We need to do this once we start adding debugger hooks or when we do the next
// full webkit merge, whichever comes first.
// #include "JavaScriptDebugServer.h"
#include "Page.h"
#include "Range.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "SystemTime.h"
#include "TextEncoding.h"
#include "TextIterator.h"
#if USE(JSC)
#include "kjs_proxy.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <kjs/ustring.h>
#endif
#include <wtf/RefCounted.h>

#if ENABLE(DATABASE)
#include "Database.h"
#include "JSDatabase.h"
#endif

#if USE(JSC)
using namespace KJS;
using namespace std;
#endif

namespace WebCore {

// Maximum size of the console message cache.
static const int MAX_CONSOLE_MESSAGES = 250;

namespace bug1228513 {
  // TODO(ericroman): Temporary hacks to help diagnose http://b/1228513

  // To remove all these hacks, search for "bug1228513"
  // in InspectorController.{cpp,h}

  // The goal is to push useful data onto the stack, so it is available in
  // the minidump (bug repros occasionally on chrome-bot) to:
  // (1) distinguish whether "InspectorController* this" is:
  //    {a valid InspectorController, a freed InspectorController, other}
  // (2) know whether an inspector window was previously opened.
  //     We shouldn't see this happening on chrome-bot, yet it appears
  //     to be the case.
  
  enum InspectorControllerState {
      VALID = 0x18565F18,
      DELETED = 0x2B197D29
  };
  
  static int g_totalNumShow = 0;
  static int g_totalNumClose = 0;

  struct Info {
    int totalNumShow;
    int totalNumClose;
    int inspectorState;
  };

  void getInfo(Info& info, const InspectorController* inspector) {
    info.totalNumShow = g_totalNumShow;
    info.totalNumClose = g_totalNumClose;
    info.inspectorState = inspector->m_bug1228513_inspectorState;
  }
} // namespace bug1228513

#if USE(JSC)
// TODO(ojan): We probably need to implement these functions to get the wrapped JS calls
// to the DOM working.
static JSRetainPtr<JSStringRef> jsStringRef(const char* str)
{
    return JSRetainPtr<JSStringRef>(Adopt, JSStringCreateWithUTF8CString(str));
}

static JSRetainPtr<JSStringRef> jsStringRef(const String& str)
{
    return JSRetainPtr<JSStringRef>(Adopt, JSStringCreateWithCharacters(str.characters(), str.length()));
}

#define HANDLE_EXCEPTION(exception) handleException((exception), __LINE__)

JSValueRef InspectorController::callSimpleFunction(JSContextRef context, JSObjectRef thisObject, const char* functionName) const
{
    ASSERT_ARG(context, context);
    ASSERT_ARG(thisObject, thisObject);

    JSValueRef exception = 0;

    JSValueRef functionProperty = JSObjectGetProperty(context, thisObject, jsStringRef(functionName).get(), &exception);
    if (HANDLE_EXCEPTION(exception))
        return JSValueMakeUndefined(context);

    JSObjectRef function = JSValueToObject(context, functionProperty, &exception);
    if (HANDLE_EXCEPTION(exception))
        return JSValueMakeUndefined(context);

    JSValueRef result = JSObjectCallAsFunction(context, function, thisObject, 0, 0, &exception);
    if (HANDLE_EXCEPTION(exception))
        return JSValueMakeUndefined(context);

    return result;
}

#endif

#pragma mark -
#pragma mark ConsoleMessage Struct


struct ConsoleMessage {
    ConsoleMessage(MessageSource s, MessageLevel l, const String& m, unsigned li, const String& u)
        : source(s)
        , level(l)
        , message(m)
        , line(li)
        , url(u)
    {
    }

#if USE(JSC)
    // TODO(ojan): I think we'll need something like this when we wrap JS calls to the DOM
    ConsoleMessage(MessageSource s, MessageLevel l, ExecState* exec, const List& args, unsigned li, const String& u)
        : source(s)
        , level(l)
        , wrappedArguments(args.size())
        , line(li)
        , url(u)
    {
        JSLock lock;
        for (unsigned i = 0; i < args.size(); ++i)
            wrappedArguments[i] = JSInspectedObjectWrapper::wrap(exec, args[i]);
    }
#endif

    MessageSource source;
    MessageLevel level;
    String message;
#if USE(JSC)
    Vector<ProtectedPtr<JSValue> > wrappedArguments;
#endif
    unsigned line;
    String url;
};

#pragma mark -
#pragma mark XMLHttpRequestResource Class

#if USE(JSC)
struct XMLHttpRequestResource {
    XMLHttpRequestResource(KJS::UString& sourceString)
    {
        KJS::JSLock lock;
        this->sourceString = sourceString.rep();
    }

    ~XMLHttpRequestResource()
    {
        KJS::JSLock lock;
        sourceString.clear();
    }

    RefPtr<KJS::UString::Rep> sourceString;
};
#elif USE(V8)
struct XMLHttpRequestResource {
    XMLHttpRequestResource(const String& str)
    {
        sourceString = str;
    }

    ~XMLHttpRequestResource() { }

    String sourceString;
};
#endif

#pragma mark -
#pragma mark InspectorResource Struct

struct InspectorResource : public RefCounted<InspectorResource> {
    // Keep these in sync with WebInspector.Resource.Type
    enum Type {
        Doc,
        Stylesheet,
        Image,
        Font,
        Script,
        XHR,
        Other
    };

    static PassRefPtr<InspectorResource> create(unsigned long identifier, DocumentLoader* documentLoader, Frame* frame)
    {
      // Apple changed the default refcount to 1: http://trac.webkit.org/changeset/30406
      // We default it to 1 in the protected constructor below to match Apple,
      // so adoptRef is the right thing.
      return adoptRef(new InspectorResource(identifier, documentLoader, frame));
    }
   
    ~InspectorResource()
    {
#if USE(JSC)
        setScriptObject(0, 0);
#elif USE(V8)
        setScriptObject(v8::Handle<v8::Object>());
#endif
    }

    Type type() const
    {
        if (xmlHttpRequestResource)
            return XHR;

        if (requestURL == loader->requestURL())
            return Doc;

        if (loader->frameLoader() && requestURL == loader->frameLoader()->iconURL())
            return Image;

        CachedResource* cachedResource = frame->document()->docLoader()->cachedResource(requestURL.string());
        if (!cachedResource)
            return Other;

        switch (cachedResource->type()) {
            case CachedResource::ImageResource:
                return Image;
            case CachedResource::FontResource:
                return Font;
            case CachedResource::CSSStyleSheet:
#if ENABLE(XSLT)
            case CachedResource::XSLStyleSheet:
#endif
                return Stylesheet;
            case CachedResource::Script:
                return Script;
            default:
                return Other;
        }
    }

#if USE(JSC)
    void setScriptObject(JSContextRef context, JSObjectRef newScriptObject)
    {
        if (scriptContext && scriptObject)
            JSValueUnprotect(scriptContext, scriptObject);

        scriptObject = newScriptObject;
        scriptContext = context;

        ASSERT((context && newScriptObject) || (!context && !newScriptObject));
        if (context && newScriptObject)
            JSValueProtect(context, newScriptObject);
    }
#elif USE(V8)
    void setScriptObject(v8::Handle<v8::Object> newScriptObject)
    {
        //XXXMB - the InspectorController and InspectorResource both maintain persistent handles
        //        to this object (I think!).  If so, calling dispose could clobber the other.
        if (!scriptObject.IsEmpty()) {
            scriptObject.Dispose();
            scriptObject.Clear();
        }
        if (!newScriptObject.IsEmpty())
            scriptObject = v8::Persistent<v8::Object>::New(newScriptObject);
    }
#endif

// TODO(ojan): XHR requests show up in the inspector, but not their contents.
// Something is wrong obviously, but not sure what. Not the highest priority
// thing the inspector needs fixed right now though.
#if USE(JSC)
    void setXMLHttpRequestProperties(KJS::UString& data)
    {
        xmlHttpRequestResource.set(new XMLHttpRequestResource(data));
    }
#elif USE(V8)
    void setXMLHttpRequestProperties(String& data)
    {
        xmlHttpRequestResource.set(new XMLHttpRequestResource(data));
    }
#endif

    String sourceString() const
    {
       if (xmlHttpRequestResource) {
#if USE(JSC)
             return KJS::UString(xmlHttpRequestResource->sourceString);
#elif USE(V8)
             return xmlHttpRequestResource->sourceString;
#endif
       }

        String sourceString;

        if (requestURL == loader->requestURL()) {
            RefPtr<SharedBuffer> buffer = loader->mainResourceData();
            String textEncodingName = frame->document()->inputEncoding();
            if (buffer) {
                TextEncoding encoding(textEncodingName);
                if (!encoding.isValid())
                   encoding = WindowsLatin1Encoding();
                return encoding.decode(buffer->data(), buffer->size());
            }
            return String();
        } else {
            CachedResource* cachedResource = frame->document()->docLoader()->cachedResource(requestURL.string());
            if (!cachedResource)
                return String();

            // Try to get the decoded source.  Only applies to some CachedResource
            // types.
            switch(cachedResource->type()) {
                case CachedResource::CSSStyleSheet:
                    {
                        CachedCSSStyleSheet *sheet = 
                            reinterpret_cast<CachedCSSStyleSheet*>(cachedResource);
                        sourceString = sheet->sheetText();
                    }
                    break;
                case CachedResource::Script:
                    {
                        CachedScript *script = 
                            reinterpret_cast<CachedScript*>(cachedResource);
                        sourceString = script->script();
                    }
                    break;
#if ENABLE(XSLT)
                case CachedResource::XSLStyleSheet:
                    {
                        CachedXSLStyleSheet *sheet = 
                            reinterpret_cast<CachedXSLStyleSheet*>(cachedResource);
                        sourceString = sheet->sheet();
                    }
                    break;
#endif
            }
        }

        return sourceString;
    }

    unsigned long identifier;
    RefPtr<DocumentLoader> loader;
    RefPtr<Frame> frame;
    OwnPtr<XMLHttpRequestResource> xmlHttpRequestResource;
    KURL requestURL;
    HTTPHeaderMap requestHeaderFields;
    HTTPHeaderMap responseHeaderFields;
    String mimeType;
    String suggestedFilename;
#if USE(JSC)
    JSContextRef scriptContext;
    JSObjectRef scriptObject;
#elif USE(V8)
    v8::Persistent<v8::Object> scriptObject;
#endif
    long long expectedContentLength;
    bool cached;
    bool finished;
    bool failed;
    int length;
    int responseStatusCode;
    double startTime;
    double responseReceivedTime;
    double endTime;

    // Helper function to determine when the script object is initialized
#if USE(JSC)
    inline bool hasScriptObject() const { return scriptObject; }
#elif USE(V8)
    inline bool hasScriptObject() { return !scriptObject.IsEmpty(); }
#endif

protected:
    // TODO(ojan): Get rid of the need to set the initialRefCount the next time we do a 
    // full webkit merge. Apple changed the default refcount to 1: http://trac.webkit.org/changeset/30406
    InspectorResource(unsigned long identifier, DocumentLoader* documentLoader, Frame* frame)
        : RefCounted<InspectorResource>(1)
        , identifier(identifier)
        , loader(documentLoader)
        , frame(frame)
        , xmlHttpRequestResource(0)
#if USE(JSC)
        , scriptContext(0)
        , scriptObject(0)
#endif
        , expectedContentLength(0)
        , cached(false)
        , finished(false)
        , failed(false)
        , length(0)
        , responseStatusCode(0)
        , startTime(-1.0)
        , responseReceivedTime(-1.0)
        , endTime(-1.0)
    {
    }
};

#pragma mark -
#pragma mark InspectorDatabaseResource Struct

#if ENABLE(DATABASE)
struct InspectorDatabaseResource : public RefCounted<InspectorDatabaseResource> {
    static PassRefPtr<InspectorDatabaseResource> create(Database* database, const String& domain, const String& name, const String& version)
    {
        // Apple changed the default refcount to 1: http://trac.webkit.org/changeset/30406
        // We default it to 1 in the protected constructor below to match Apple,
        // so adoptRef is the right thing.
        return adoptRef(new InspectorDatabaseResource(database, domain, name, version));
    }

    void setScriptObject(JSContextRef context, JSObjectRef newScriptObject)
    {
        if (scriptContext && scriptObject)
            JSValueUnprotect(scriptContext, scriptObject);

        scriptObject = newScriptObject;
        scriptContext = context;

        ASSERT((context && newScriptObject) || (!context && !newScriptObject));
        if (context && newScriptObject)
            JSValueProtect(context, newScriptObject);
    }

    RefPtr<Database> database;
    String domain;
    String name;
    String version;
    JSContextRef scriptContext;
    JSObjectRef scriptObject;
   
private:
    // TODO(ojan): Get rid of the need to set the initialRefCount the next time we do a 
    // full webkit merge. Apple changed the default refcount to 1: http://trac.webkit.org/changeset/30406
    InspectorDatabaseResource(Database* database, const String& domain, const String& name, const String& version)
        : RefCounted<InspectorDatabaseResource>(1)
        , database(database)
        , domain(domain)
        , name(name)
        , version(version)
        , scriptContext(0)
        , scriptObject(0)
    {
    }
};
#endif

#pragma mark -
#pragma mark JavaScript Callbacks

#if USE(JSC)
static JSValueRef addSourceToFrame(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSValueRef undefined = JSValueMakeUndefined(ctx);

    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (argumentCount < 2 || !controller)
        return undefined;

    JSValueRef identifierValue = arguments[0];
    if (!JSValueIsNumber(ctx, identifierValue))
        return undefined;

    unsigned long identifier = static_cast<unsigned long>(JSValueToNumber(ctx, identifierValue, exception));
    if (exception && *exception)
        return undefined;

    RefPtr<InspectorResource> resource = controller->resources().get(identifier);
    ASSERT(resource);
    if (!resource)
        return undefined;

    String sourceString = resource->sourceString();
    if (sourceString.isEmpty())
        return undefined;

    Node* node = toNode(toJS(arguments[1]));
    ASSERT(node);
    if (!node)
        return undefined;

    if (!node->attached()) {
        ASSERT_NOT_REACHED();
        return undefined;
    }

    ASSERT(node->isElementNode());
    if (!node->isElementNode())
        return undefined;

    Element* element = static_cast<Element*>(node);
    ASSERT(element->isFrameOwnerElement());
    if (!element->isFrameOwnerElement())
        return undefined;

    HTMLFrameOwnerElement* frameOwner = static_cast<HTMLFrameOwnerElement*>(element);
    ASSERT(frameOwner->contentFrame());
    if (!frameOwner->contentFrame())
        return undefined;

    FrameLoader* loader = frameOwner->contentFrame()->loader();

    loader->setResponseMIMEType(resource->mimeType);
    loader->begin();
    loader->write(sourceString);
    loader->end();

    return undefined;
}
#elif USE(V8)
void InspectorController::addSourceToFrame(unsigned long identifier, Node* node) 
{    
    RefPtr<InspectorResource> resource = this->resources().get(identifier);
    ASSERT(resource);
    if (!resource)
        return;

    String sourceString = resource->sourceString();
    if (sourceString.isEmpty())
        return;

    ASSERT(node);
    if (!node)
        return;

    if (!node->attached()) {
        ASSERT_NOT_REACHED();
        return;
    }

    ASSERT(node->isElementNode());
    if (!node->isElementNode())
        return;

    Element* element = static_cast<Element*>(node);
    ASSERT(element->isFrameOwnerElement());
    if (!element->isFrameOwnerElement())
        return;

    HTMLFrameOwnerElement* frameOwner = static_cast<HTMLFrameOwnerElement*>(element);
    ASSERT(frameOwner->contentFrame());
    if (!frameOwner->contentFrame())
        return;

    FrameLoader* loader = frameOwner->contentFrame()->loader();

    loader->setResponseMIMEType(resource->mimeType);
    loader->begin();
    loader->write(sourceString);
    loader->end();
}
#endif

#if USE(JSC)
static JSValueRef getResourceDocumentNode(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSValueRef undefined = JSValueMakeUndefined(ctx);

    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!argumentCount || argumentCount > 1 || !controller)
        return undefined;

    JSValueRef identifierValue = arguments[0];
    if (!JSValueIsNumber(ctx, identifierValue))
        return undefined;

    unsigned long identifier = static_cast<unsigned long>(JSValueToNumber(ctx, identifierValue, exception));
    if (exception && *exception)
        return undefined;

    RefPtr<InspectorResource> resource = controller->resources().get(identifier);
    ASSERT(resource);
    if (!resource)
        return undefined;

    Frame* frame = resource->frame.get();

    Document* document = frame->document();
    if (!document)
        return undefined;

    if (document->isPluginDocument() || document->isImageDocument())
        return undefined;

    ExecState* exec = toJSDOMWindowWrapper(resource->frame.get())->window()->globalExec();

    KJS::JSLock lock;
    JSValueRef documentValue = toRef(JSInspectedObjectWrapper::wrap(exec, toJS(exec, document)));
    return documentValue;
}
#elif USE(V8)
Node* InspectorController::getResourceDocumentNode(unsigned long identifier) { 
    RefPtr<InspectorResource> resource = this->resources().get(identifier);
    ASSERT(resource);
    if (!resource)
        return 0;

    Frame* frame = resource->frame.get();

    Document* document = frame->document();
    if (!document)
        return 0;

    if (document->isPluginDocument() || document->isImageDocument())
        return 0;

    return document;
}
#endif

#if USE(JSC)
static JSValueRef highlightDOMNode(JSContextRef context, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    JSValueRef undefined = JSValueMakeUndefined(context);

    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (argumentCount < 1 || !controller)
        return undefined;

    JSQuarantinedObjectWrapper* wrapper = JSQuarantinedObjectWrapper::asWrapper(toJS(arguments[0]));
    if (!wrapper)
        return undefined;
    Node* node = toNode(wrapper->unwrappedObject());
    if (!node)
        return undefined;

    controller->highlight(node);

    return undefined;
}
#elif USE(V8)
void InspectorController::highlightDOMNode(Node* node)
{
    if (!enabled())
        return;
    
    ASSERT_ARG(node, node);
    m_client->highlight(node);
}
#endif

#if USE(JSC)
static JSValueRef hideDOMNodeHighlight(JSContextRef context, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    JSValueRef undefined = JSValueMakeUndefined(context);

    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (argumentCount || !controller)
        return undefined;

    controller->hideHighlight();

    return undefined;
}
#elif USE(V8)
void InspectorController::hideDOMNodeHighlight()
{
    if (!enabled())
        return;

    m_client->hideHighlight();
}
#endif

#if USE(JSC)
static JSValueRef loaded(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->scriptObjectReady();
    return JSValueMakeUndefined(ctx);
}
#elif USE(V8)
void InspectorController::loaded() { 
    scriptObjectReady();
}
#endif

#if USE(JSC)
static JSValueRef unloading(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->close();
    return JSValueMakeUndefined(ctx);
}
#elif USE(V8)
// We don't need to implement this because we just map windowUnloading to
// InspectorController::close in the IDL file.
#endif

#if USE(JSC)
static JSValueRef attach(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->attachWindow();
    return JSValueMakeUndefined(ctx);
}
#elif USE(V8)
void InspectorController::attach() {
    attachWindow();
}
#endif

#if USE(JSC)
static JSValueRef detach(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->detachWindow();
    return JSValueMakeUndefined(ctx);
}
#elif USE(V8)
void InspectorController::detach() {
    detachWindow();
}
#endif

#if USE(JSC)
static JSValueRef search(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 2 || !JSValueIsString(ctx, arguments[1]))
        return JSValueMakeUndefined(ctx);

    Node* node = toNode(toJS(arguments[0]));
    if (!node)
        return JSValueMakeUndefined(ctx);

    JSRetainPtr<JSStringRef> searchString(Adopt, JSValueToStringCopy(ctx, arguments[1], exception));
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    String target(JSStringGetCharactersPtr(searchString.get()), JSStringGetLength(searchString.get()));

    JSObjectRef global = JSContextGetGlobalObject(ctx);

    JSValueRef arrayProperty = JSObjectGetProperty(ctx, global, jsStringRef("Array").get(), exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef arrayConstructor = JSValueToObject(ctx, arrayProperty, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef result = JSObjectCallAsConstructor(ctx, arrayConstructor, 0, 0, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSValueRef pushProperty = JSObjectGetProperty(ctx, result, jsStringRef("push").get(), exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef pushFunction = JSValueToObject(ctx, pushProperty, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    RefPtr<Range> searchRange(rangeOfContents(node));

    ExceptionCode ec = 0;
    do {
        RefPtr<Range> resultRange(findPlainText(searchRange.get(), target, true, false));
        if (resultRange->collapsed(ec))
            break;

        // A non-collapsed result range can in some funky whitespace cases still not
        // advance the range's start position (4509328). Break to avoid infinite loop.
        VisiblePosition newStart = endVisiblePosition(resultRange.get(), DOWNSTREAM);
        if (newStart == startVisiblePosition(searchRange.get(), DOWNSTREAM))
            break;

        KJS::JSLock lock;
        JSValueRef arg0 = toRef(toJS(toJS(ctx), resultRange.get()));
        JSObjectCallAsFunction(ctx, pushFunction, result, 1, &arg0, exception);
        if (exception && *exception)
            return JSValueMakeUndefined(ctx);

        setStart(searchRange.get(), newStart);
    } while (true);

    return result;
}
#elif USE(V8)
// TODO(ojan): See when/if this works. We should either make it work or remove it.
void InspectorController::search(Node* node, const String& target) { 
    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
    v8::Context::Scope scope(context);

    v8::Handle<v8::Object> global = context->Global();
    v8::Handle<v8::Array> array = v8::Array::New();

    v8::Handle<v8::Value> push = array->Get(v8::String::New("push"));
    ASSERT(push->IsFunction());

    RefPtr<Range> searchRange(rangeOfContents(node));

    int exception = 0;
    do {
        RefPtr<Range> resultRange(findPlainText(searchRange.get(), target, true, false));
        if (resultRange->collapsed(exception))
            break;

        // A non-collapsed result range can in some funky whitespace cases still not
        // advance the range's start position (4509328). Break to avoid infinite loop.
        VisiblePosition newStart = endVisiblePosition(resultRange.get(), DOWNSTREAM);
        if (newStart == startVisiblePosition(searchRange.get(), DOWNSTREAM))
            break;

        v8::Handle<v8::Value> arg0 = V8Proxy::ToV8Object(V8ClassIndex::RANGE, resultRange.get());
        v8::Handle<v8::Value> args[] = { arg0 };

        (v8::Function::Cast(*push))->Call(array, 1, args);

        setStart(searchRange.get(), newStart);
    } while (true);

    // TODO(jackson): Figure out how to return array
}
#endif

#if ENABLE(DATABASE)
static JSValueRef databaseTableNames(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 1)
        return JSValueMakeUndefined(ctx);

    JSQuarantinedObjectWrapper* wrapper = JSQuarantinedObjectWrapper::asWrapper(toJS(arguments[0]));
    if (!wrapper)
        return JSValueMakeUndefined(ctx);

    Database* database = toDatabase(wrapper->unwrappedObject());
    if (!database)
        return JSValueMakeUndefined(ctx);

    JSObjectRef global = JSContextGetGlobalObject(ctx);

    JSValueRef arrayProperty = JSObjectGetProperty(ctx, global, jsStringRef("Array").get(), exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef arrayConstructor = JSValueToObject(ctx, arrayProperty, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef result = JSObjectCallAsConstructor(ctx, arrayConstructor, 0, 0, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSValueRef pushProperty = JSObjectGetProperty(ctx, result, jsStringRef("push").get(), exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef pushFunction = JSValueToObject(ctx, pushProperty, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    Vector<String> tableNames = database->tableNames();
    unsigned length = tableNames.size();
    for (unsigned i = 0; i < length; ++i) {
        String tableName = tableNames[i];
        JSValueRef tableNameValue = JSValueMakeString(ctx, jsStringRef(tableName).get());

        JSValueRef pushArguments[] = { tableNameValue };
        JSObjectCallAsFunction(ctx, pushFunction, result, 1, pushArguments, exception);
        if (exception && *exception)
            return JSValueMakeUndefined(ctx);
    }

    return result;
}
#endif

#if USE(JSC)
static JSValueRef inspectedWindow(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    JSDOMWindow* inspectedWindow = toJSDOMWindow(controller->inspectedPage()->mainFrame());
    JSLock lock;
    return toRef(JSInspectedObjectWrapper::wrap(inspectedWindow->globalExec(), inspectedWindow));
}
#elif USE(V8)
DOMWindow* InspectorController::inspectedWindow() {
    // Can be null if page was already destroyed.
    if (!m_inspectedPage)
        return NULL;
    return m_inspectedPage->mainFrame()->domWindow();
}
#endif

#if USE(JSC)
static JSValueRef localizedStrings(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    String url = controller->localizedStringsURL();
    if (url.isNull())
        return JSValueMakeNull(ctx);

    return JSValueMakeString(ctx, jsStringRef(url).get());
}
#elif USE(V8)
// TODO(ojan): Figure out how/if to implement this function.
#endif

#if USE(JSC)
static JSValueRef platform(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
#if PLATFORM(MAC)
#ifdef BUILDING_ON_TIGER
    static const String platform = "mac-tiger";
#else
    static const String platform = "mac-leopard";
#endif
#elif PLATFORM(WIN_OS)
    static const String platform = "windows";
#elif PLATFORM(QT)
    static const String platform = "qt";
#elif PLATFORM(GTK)
    static const String platform = "gtk";
#elif PLATFORM(WX)
    static const String platform = "wx";
#else
    static const String platform = "unknown";
#endif

    JSValueRef platformValue = JSValueMakeString(ctx, jsStringRef(platform).get());

    return platformValue;
}
#elif USE(V8)
String InspectorController::platform() const {
  return String("windows");
}
#endif

#if USE(JSC)
static JSValueRef moveByUnrestricted(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 2)
        return JSValueMakeUndefined(ctx);

    double x = JSValueToNumber(ctx, arguments[0], exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    double y = JSValueToNumber(ctx, arguments[1], exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    controller->moveWindowBy(narrowPrecisionToFloat(x), narrowPrecisionToFloat(y));

    return JSValueMakeUndefined(ctx);
}
#elif USE(V8)
// TODO(ojan): Figure out how/if to implement this function.
#endif

#if USE(JSC)
static JSValueRef wrapCallback(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 1)
        return JSValueMakeUndefined(ctx);

    JSLock lock;
    return toRef(JSInspectorCallbackWrapper::wrap(toJS(ctx), toJS(arguments[0])));
}
#elif USE(V8)
// TODO(ojan): Figure out how to wrap JS calls to the DOM with V8.
// Eventually, get it working over IPC.
#endif

#if USE(JSC)
static JSValueRef startDebuggingAndReloadInspectedPage(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->startDebuggingAndReloadInspectedPage();

    return JSValueMakeUndefined(ctx);
}
#elif USE(V8)
// TODO(ojan): Figure out how/if to implement this function.
#endif

#if USE(JSC)
static JSValueRef stopDebugging(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->stopDebugging();

    return JSValueMakeUndefined(ctx);
}
#elif USE(V8)
// TODO(ojan): Figure out how/if to implement this function.
#endif

#if USE(JSC)
static JSValueRef debuggerAttached(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);
    return JSValueMakeBoolean(ctx, controller->debuggerAttached());
}
#elif USE(V8)
// TODO(ojan): Figure out how/if to implement this function.
#endif

#pragma mark -
#pragma mark InspectorController Class

InspectorController::InspectorController(Page* page, InspectorClient* client)
    :
#if USE(V8)
      // The V8 version of InspectorController is RefCounted while the JSC
      // version uses an OwnPtr (http://b/904340).  However, since we're not
      // using a create method to initialize the InspectorController, we need
      // to start the RefCount at 0.
      RefCounted(0),
#endif
      m_bug1228513_inspectorState(bug1228513::VALID)
    , m_inspectedPage(page)
    , m_client(client)
    , m_page(0)
#if USE(JSC)
    , m_scriptObject(0) // is an uninitialized V8 object
    , m_controllerScriptObject(0) // is equivalent to |this|
    , m_scriptContext(0) // isn't necessary for V8
#endif
    , m_windowVisible(false)
    , m_debuggerAttached(false)
    , m_showAfterVisible(ElementsPanel)
    , m_nextIdentifier(-2)
    , m_trackResources(false)
{
    ASSERT_ARG(page, page);
    ASSERT_ARG(client, client);
}

InspectorController::~InspectorController()
{
    m_bug1228513_inspectorState = bug1228513::DELETED;
    m_client->inspectorDestroyed();

#if USE(JSC)
    if (m_scriptContext) {
        JSValueRef exception = 0;

        JSObjectRef global = JSContextGetGlobalObject(m_scriptContext);
        JSValueRef controllerProperty = JSObjectGetProperty(m_scriptContext, global, jsStringRef("InspectorController").get(), &exception);
        if (!HANDLE_EXCEPTION(exception)) {
            if (JSObjectRef controller = JSValueToObject(m_scriptContext, controllerProperty, &exception)) {
                if (!HANDLE_EXCEPTION(exception))
                    JSObjectSetPrivate(controller, 0);
            }
        }
    }
#endif

    if (m_page)
        m_page->setParentInspectorController(0);

    // m_inspectedPage should have been cleared by inspectedPageDestroyed
    ASSERT(!m_inspectedPage);

    deleteAllValues(m_frameResources);
    deleteAllValues(m_consoleMessages);
}

void InspectorController::inspectedPageDestroyed()
{
    ASSERT(m_inspectedPage);
    stopDebugging();
    m_inspectedPage = 0;
}

bool InspectorController::enabled() const
{
    // Copy some data onto the stack in case we crash on line
    // "m_inspectedPage->settings()->developerExtrasEnabled();"
    bug1228513::Info bug1228513_info;
    bug1228513::getInfo(bug1228513_info, this);

    if (!m_inspectedPage)
        return false;

    bool b = m_inspectedPage->settings()->developerExtrasEnabled();

    if (bug1228513_info.inspectorState != bug1228513::VALID) {
        CRASH();
    }

    return b;
}

String InspectorController::localizedStringsURL()
{
    if (!enabled())
        return String();
    return m_client->localizedStringsURL();
}

void InspectorController::inspect(Node* node)
{
    if (!node || !enabled())
        return;

    show();

    if (node->nodeType() != Node::ELEMENT_NODE && node->nodeType() != Node::DOCUMENT_NODE)
        node = node->parentNode();
    m_nodeToFocus = node;

    if (!hasScriptObject()) {
        m_showAfterVisible = ElementsPanel;
        return;
    }

    if (windowVisible())
        focusNode();
}

#if USE(JSC)
void InspectorController::focusNode()
{
    if (!enabled())
        return;

    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    ASSERT(m_nodeToFocus);

    Frame* frame = m_nodeToFocus->document()->frame();
    if (!frame)
        return;

    ExecState* exec = toJSDOMWindow(frame)->globalExec();

    JSValueRef arg0;

    {
        KJS::JSLock lock;
        arg0 = toRef(JSInspectedObjectWrapper::wrap(exec, toJS(exec, m_nodeToFocus.get())));
    }

    m_nodeToFocus = 0;

    JSValueRef exception = 0;

    JSValueRef functionProperty = JSObjectGetProperty(m_scriptContext, m_scriptObject, jsStringRef("updateFocusedNode").get(), &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectRef function = JSValueToObject(m_scriptContext, functionProperty, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    ASSERT(function);

    JSObjectCallAsFunction(m_scriptContext, function, m_scriptObject, 1, &arg0, &exception);
    HANDLE_EXCEPTION(exception);
}
#elif USE(V8)
void InspectorController::focusNode()
{
    if (!enabled() || !m_nodeToFocus)
        return;

    ASSERT(hasScriptObject());

    Frame* frame = m_nodeToFocus->document()->frame();
    if (!frame)
        return;

    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
    v8::Context::Scope scope(context);

    v8::Handle<v8::Value> nodeToFocus = V8Proxy::ToV8Object(V8ClassIndex::NODE, m_nodeToFocus.get());
    v8::Handle<v8::Value> updateFocusedNode = m_scriptObject->Get(v8::String::New("updateFocusedNode"));
    ASSERT(updateFocusedNode->IsFunction());

    v8::Handle<v8::Function> func(v8::Function::Cast(*updateFocusedNode));
    v8::Handle<v8::Value> args[] = { nodeToFocus };
    func->Call(m_scriptObject, 1, args);
}
#endif

void InspectorController::highlight(Node* node)
{
    if (!enabled())
        return;
    ASSERT_ARG(node, node);
    m_highlightedNode = node;
    m_client->highlight(node);
}

void InspectorController::hideHighlight()
{
    if (!enabled())
        return;
    m_client->hideHighlight();
}

bool InspectorController::windowVisible()
{
    return m_windowVisible;
}

void InspectorController::setWindowVisible(bool visible)
{
    // Policy: only log resources while the inspector window is visible.
    enableTrackResources(visible);

    if (visible == m_windowVisible)
        return;

    m_windowVisible = visible;

#if USE(JSC)
    if (!m_scriptContext || !m_scriptObject)
        return;
#elif USE(V8)
    if (!hasScriptObject())
        return;
#endif

    if (m_windowVisible) {
        populateScriptObjects();
        if (m_nodeToFocus)
            focusNode();
        if (m_showAfterVisible == ConsolePanel)
            showConsole();
        else if (m_showAfterVisible == ResourcesPanel)
            showTimeline();
    } else
        resetScriptObjects();

    m_showAfterVisible = ElementsPanel;
}

void InspectorController::enableTrackResources(bool trackResources)
{
    if (m_trackResources == trackResources)
        return;

    m_trackResources = trackResources;

    // Clear the current resources.
    deleteAllValues(m_frameResources);
    m_mainResource = NULL;
    m_frameResources.clear();
    m_resources.clear();
}


#if USE(JSC)
void InspectorController::addMessageToConsole(MessageSource source, MessageLevel level, ExecState* exec, const List& arguments, unsigned lineNumber, const String& sourceURL)
{
    if (!enabled())
        return;

    addConsoleMessage(new ConsoleMessage(source, level, exec, arguments, lineNumber, sourceURL));
}
#endif

void InspectorController::addMessageToConsole(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceID)
{
    if (!enabled())
        return;

    addConsoleMessage(new ConsoleMessage(source, level, message, lineNumber, sourceID));
}

void InspectorController::addConsoleMessage(ConsoleMessage* consoleMessage)
{
    ASSERT(enabled());
    ASSERT_ARG(consoleMessage, consoleMessage);

    // Limit the number of console messages we keep in memory so a poorly
    // behaving script doesn't cause unbounded memory growth.  We remove the
    // oldest messages so that the most recent errors are preserved.
    // TODO(erikkay): this is not very efficient since Vector has to do a copy
    // when you remove from anywhere other than the end.  Unfortunately, WTF
    // doesn't appear to have a double-ended list we could use instead.  The
    // extra CPU cost is definitely better than the memory cost.
    if (m_consoleMessages.size() >= MAX_CONSOLE_MESSAGES) {
        ConsoleMessage* msg = m_consoleMessages[0];
        m_consoleMessages.remove(0);
        delete msg;
    }
    m_consoleMessages.append(consoleMessage);

    if (windowVisible())
        addScriptConsoleMessage(consoleMessage);
}

void InspectorController::attachWindow()
{
    if (!enabled())
        return;
    m_client->attachWindow();
}

void InspectorController::detachWindow()
{
    if (!enabled())
        return;
    m_client->detachWindow();
}

#if USE(V8)
void InspectorController::setScriptObject(v8::Handle<v8::Object> newScriptObject)
{
    if (hasScriptObject()) {
        m_scriptObject.Dispose();
        m_scriptObject.Clear();
    }

    if (!newScriptObject.IsEmpty())
        m_scriptObject = v8::Persistent<v8::Object>::New(newScriptObject);
}
#endif

#if USE(JSC)
void InspectorController::inspectedWindowScriptObjectCleared(Frame* frame)
{
    if (!enabled() || !m_scriptContext || !m_scriptObject)
        return;

    JSDOMWindow* win = toJSDOMWindow(frame);
    ExecState* exec = win->globalExec();

    JSValueRef arg0;

    {
        KJS::JSLock lock(false);
        arg0 = toRef(JSInspectedObjectWrapper::wrap(exec, win));
    }

    JSValueRef exception = 0;
    callFunction(m_scriptContext, m_scriptObject, "inspectedWindowCleared", 1, &arg0, exception);
}
#elif USE(V8)
void InspectorController::inspectedWindowScriptObjectCleared(Frame* frame)
{
    // TODO(tc): We need to call inspectedWindowCleared, but that won't matter
    // until we merge in inspector.js as well.
    notImplemented();
}
#endif

#if USE(JSC)
void InspectorController::windowScriptObjectAvailable()
{
    if (!m_page || !enabled())
        return;

    m_scriptContext = toRef(m_page->mainFrame()->scriptProxy()->globalObject()->globalExec());

    JSObjectRef global = JSContextGetGlobalObject(m_scriptContext);
    ASSERT(global);

    static JSStaticFunction staticFunctions[] = {
        { "addSourceToFrame", addSourceToFrame, kJSPropertyAttributeNone },
        { "getResourceDocumentNode", getResourceDocumentNode, kJSPropertyAttributeNone },
        { "highlightDOMNode", highlightDOMNode, kJSPropertyAttributeNone },
        { "hideDOMNodeHighlight", hideDOMNodeHighlight, kJSPropertyAttributeNone },
        { "loaded", loaded, kJSPropertyAttributeNone },
        { "windowUnloading", unloading, kJSPropertyAttributeNone },
        { "attach", attach, kJSPropertyAttributeNone },
        { "detach", detach, kJSPropertyAttributeNone },
        { "search", search, kJSPropertyAttributeNone },
#if ENABLE(DATABASE)
        { "databaseTableNames", databaseTableNames, kJSPropertyAttributeNone },
#endif
        { "inspectedWindow", inspectedWindow, kJSPropertyAttributeNone },
        { "localizedStringsURL", localizedStrings, kJSPropertyAttributeNone },
        { "platform", platform, kJSPropertyAttributeNone },
        { "moveByUnrestricted", moveByUnrestricted, kJSPropertyAttributeNone },
        { "wrapCallback", wrapCallback, kJSPropertyAttributeNone },
        { "startDebuggingAndReloadInspectedPage", WebCore::startDebuggingAndReloadInspectedPage, kJSPropertyAttributeNone },
        { "stopDebugging", WebCore::stopDebugging, kJSPropertyAttributeNone },
        { "debuggerAttached", WebCore::debuggerAttached, kJSPropertyAttributeNone },
        { 0, 0, 0 }
    };

    JSClassDefinition inspectorControllerDefinition = {
        0, kJSClassAttributeNone, "InspectorController", 0, 0, staticFunctions,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    JSClassRef controllerClass = JSClassCreate(&inspectorControllerDefinition);
    ASSERT(controllerClass);

    m_controllerScriptObject = JSObjectMake(m_scriptContext, controllerClass, reinterpret_cast<void*>(this));
    ASSERT(m_controllerScriptObject);

    JSObjectSetProperty(m_scriptContext, global, jsStringRef("InspectorController").get(), m_controllerScriptObject, kJSPropertyAttributeNone, 0);
}
#elif USE(V8)
void InspectorController::windowScriptObjectAvailable()
{
    if (!m_page || !enabled())
        return;
    
    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
    v8::Context::Scope scope(context);

    // InspectorController.idl exposes the methods of InspectorController to JavaScript
    v8::Handle<v8::Object> global = context->Global();
    v8::Handle<v8::Value> inspectorController = V8Proxy::ToV8Object(V8ClassIndex::INSPECTORCONTROLLER, this);
    global->Set(v8::String::New("InspectorController"), inspectorController);
}
#endif

#if USE(JSC)
void InspectorController::scriptObjectReady()
{
    ASSERT(m_scriptContext);
    if (!m_scriptContext)
        return;

    JSObjectRef global = JSContextGetGlobalObject(m_scriptContext);
    ASSERT(global);

    JSValueRef exception = 0;

    JSValueRef inspectorValue = JSObjectGetProperty(m_scriptContext, global, jsStringRef("WebInspector").get(), &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    ASSERT(inspectorValue);
    if (!inspectorValue)
        return;

    m_scriptObject = JSValueToObject(m_scriptContext, inspectorValue, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    ASSERT(m_scriptObject);

    JSValueProtect(m_scriptContext, m_scriptObject);

    // Make sure our window is visible now that the page loaded
    showWindow();
}
#elif USE(V8)
void InspectorController::scriptObjectReady()
{
    if (!m_page || !enabled())
        return;

    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
    v8::Context::Scope scope(context);

    v8::Handle<v8::Object> global = context->Global();
    v8::Handle<v8::Object> inspector = v8::Handle<v8::Object>(v8::Object::Cast(*global->Get(v8::String::New("WebInspector"))));
    setScriptObject(inspector);
    
    // Make sure our window is visible now that the page loaded
    m_client->showWindow();
}
#endif

void InspectorController::show()
{
    if (!enabled())
        return;

    ++bug1228513::g_totalNumShow;

    if (!m_page) {
        m_page = m_client->createPage();
        if (!m_page)
            return;
        m_page->setParentInspectorController(this);

        // showWindow() will be called after the page loads in scriptObjectReady()
        return;
    }

    showWindow();
}

void InspectorController::showConsole()
{
    if (!enabled())
        return;

    show();

    if (!hasScriptObject()) {
        m_showAfterVisible = ConsolePanel;
        return;
    }

#if USE(JSC)
    callSimpleFunction(m_scriptContext, m_scriptObject, "showConsole");
#elif USE(V8)
    if (windowVisible()) {
        v8::HandleScope handle_scope;
        v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
        v8::Context::Scope scope(context);

        v8::Handle<v8::Value> showConsole = m_scriptObject->Get(v8::String::New("showConsole"));
        ASSERT(showConsole->IsFunction());

        v8::Handle<v8::Function> func(v8::Function::Cast(*showConsole));
        func->Call(m_scriptObject, 0, NULL);
    } else {
        m_client->showWindow();
    }
#endif
}


void InspectorController::showTimeline()
{
    if (!enabled())
        return;

    show();

    if (!hasScriptObject()) {
        m_showAfterVisible = ResourcesPanel;
        return;
    }

#if USE(JSC)
    callSimpleFunction(m_scriptContext, m_scriptObject, "showTimeline");
#elif USE(V8)
    if (windowVisible()) {
        v8::HandleScope handle_scope;
        v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
        v8::Context::Scope scope(context);

        v8::Handle<v8::Value> showTimeline = m_scriptObject->Get(v8::String::New("showTimeline"));
        ASSERT(showTimeline->IsFunction());

        v8::Handle<v8::Function> func(v8::Function::Cast(*showTimeline));
        func->Call(m_scriptObject, 0, NULL);
    } else {
        m_client->showWindow();
    }
#endif
}

void InspectorController::close()
{
    if (!enabled())
        return;

    ++bug1228513::g_totalNumClose;

    closeWindow();
    if (m_page) {
        m_page->setParentInspectorController(0);
#if USE(V8)
        v8::HandleScope handle_scope;
        v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
        v8::Context::Scope scope(context);
        setScriptObject(v8::Handle<v8::Object>());
#endif
    }

#if USE(JSC)
    ASSERT(m_scriptContext && m_scriptObject);
    JSValueUnprotect(m_scriptContext, m_scriptObject);
#endif

    m_page = 0;
#if USE(JSC)
    m_scriptObject = 0;
    m_scriptContext = 0;
#endif
}

void InspectorController::showWindow()
{
    ASSERT(enabled());

    m_client->showWindow();
}

void InspectorController::closeWindow()
{
    stopDebugging();
    m_client->closeWindow();
}

#if USE(JSC)
static void addHeaders(JSContextRef context, JSObjectRef object, const HTTPHeaderMap& headers, JSValueRef* exception)
{
    ASSERT_ARG(context, context);
    ASSERT_ARG(object, object);

    HTTPHeaderMap::const_iterator end = headers.end();
    for (HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it) {
        JSValueRef value = JSValueMakeString(context, jsStringRef(it->second).get());
        JSObjectSetProperty(context, object, jsStringRef(it->first).get(), value, kJSPropertyAttributeNone, exception);
        if (exception && *exception)
            return;
    }
}
#elif USE(V8)
static void addHeaders(v8::Handle<v8::Object> object, const HTTPHeaderMap& headers)
{
    ASSERT_ARG(object, !object.IsEmpty());
    HTTPHeaderMap::const_iterator end = headers.end();
    for (HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it) {
        v8::Handle<v8::String> field = v8::String::New(FromWebCoreString(it->first), it->first.length());
        object->Set(field, v8StringOrNull(it->second));
    }
}
#endif

#if USE(JSC)
static JSObjectRef scriptObjectForRequest(JSContextRef context, const InspectorResource* resource, JSValueRef* exception)
{
    ASSERT_ARG(context, context);

    JSObjectRef object = JSObjectMake(context, 0, 0);
    addHeaders(context, object, resource->requestHeaderFields, exception);

    return object;
}
#elif USE(V8)
static v8::Handle<v8::Object> scriptObjectForRequest(const InspectorResource* resource)
{
    v8::Handle<v8::Object> object = v8::Object::New();
    addHeaders(object, resource->requestHeaderFields);
    return object;
}
#endif

#if USE(JSC)
static JSObjectRef scriptObjectForResponse(JSContextRef context, const InspectorResource* resource, JSValueRef* exception)
{
    ASSERT_ARG(context, context);

    JSObjectRef object = JSObjectMake(context, 0, 0);
    addHeaders(context, object, resource->responseHeaderFields, exception);

    return object;
}
#elif USE(V8)
static v8::Handle<v8::Object> scriptObjectForResponse(const InspectorResource* resource)
{
    v8::Handle<v8::Object> object = v8::Object::New();
    addHeaders(object, resource->responseHeaderFields);
    return object;
}
#endif

#if USE(JSC)
JSObjectRef InspectorController::addScriptResource(InspectorResource* resource)
{
    ASSERT_ARG(resource, resource);

    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    if (!m_scriptContext || !m_scriptObject)
        return 0;

    if (!resource->scriptObject) {
        JSValueRef exception = 0;

        JSValueRef resourceProperty = JSObjectGetProperty(m_scriptContext, m_scriptObject, jsStringRef("Resource").get(), &exception);
        if (HANDLE_EXCEPTION(exception))
            return 0;

        JSObjectRef resourceConstructor = JSValueToObject(m_scriptContext, resourceProperty, &exception);
        if (HANDLE_EXCEPTION(exception))
            return 0;

        JSValueRef urlValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.string()).get());
        JSValueRef domainValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.host()).get());
        JSValueRef pathValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.path()).get());
        JSValueRef lastPathComponentValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.lastPathComponent()).get());

        JSValueRef identifier = JSValueMakeNumber(m_scriptContext, resource->identifier);
        JSValueRef mainResource = JSValueMakeBoolean(m_scriptContext, m_mainResource == resource);
        JSValueRef cached = JSValueMakeBoolean(m_scriptContext, resource->cached);

        JSObjectRef scriptObject = scriptObjectForRequest(m_scriptContext, resource, &exception);
        if (HANDLE_EXCEPTION(exception))
            return 0;

        JSValueRef arguments[] = { scriptObject, urlValue, domainValue, pathValue, lastPathComponentValue, identifier, mainResource, cached };
        JSObjectRef result = JSObjectCallAsConstructor(m_scriptContext, resourceConstructor, 8, arguments, &exception);
        if (HANDLE_EXCEPTION(exception))
            return 0;

        ASSERT(result);

        resource->setScriptObject(m_scriptContext, result);
    }

    JSValueRef exception = 0;

    JSValueRef addResourceProperty = JSObjectGetProperty(m_scriptContext, m_scriptObject, jsStringRef("addResource").get(), &exception);
    if (HANDLE_EXCEPTION(exception))
        return 0;

    JSObjectRef addResourceFunction = JSValueToObject(m_scriptContext, addResourceProperty, &exception);
    if (HANDLE_EXCEPTION(exception))
        return 0;

    JSValueRef addArguments[] = { resource->scriptObject };
    JSObjectCallAsFunction(m_scriptContext, addResourceFunction, m_scriptObject, 1, addArguments, &exception);
    if (HANDLE_EXCEPTION(exception))
        return 0;

    return resource->scriptObject;
}
#elif USE(V8)
void InspectorController::addScriptResource(InspectorResource* resource)
{
    ASSERT_ARG(resource, resource);
    ASSERT(trackResources());
    ASSERT(hasScriptObject());
    if (!hasScriptObject()) 
        return;

    // Acquire the context so we can create V8 objects
    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
    v8::Context::Scope scope(context);

    v8::Handle<v8::Value> resourceConstructor = m_scriptObject->Get(v8::String::New("Resource"));
    ASSERT(resourceConstructor->IsFunction());

    v8::Handle<v8::Value> arguments[] = {
        scriptObjectForRequest(resource),
        v8StringOrNull(resource->requestURL.string()),
        v8StringOrNull(resource->requestURL.host()),
        v8StringOrNull(resource->requestURL.path()),
        v8StringOrNull(resource->requestURL.lastPathComponent()),
        v8::Number::New(resource->identifier),
        (m_mainResource == resource)?v8::True():v8::False(),
        (resource->cached)?v8::True():v8::False(),
    };

    v8::Handle<v8::Object> object = SafeAllocation::NewInstance(v8::Handle<v8::Function>::Cast(resourceConstructor), 8, arguments);

    resource->setScriptObject(object);

    ASSERT(!object.IsEmpty());

    v8::Handle<v8::Value> addResourceFunction = m_scriptObject->Get(v8::String::New("addResource"));
    ASSERT(addResourceFunction->IsFunction());

    v8::Handle<v8::Value> addArguments[] = { object };
    (v8::Function::Cast(*addResourceFunction))->Call(m_scriptObject, 1, addArguments);
}
#endif

#if USE(JSC)
JSObjectRef InspectorController::addAndUpdateScriptResource(InspectorResource* resource)
{
    ASSERT_ARG(resource, resource);

    JSObjectRef scriptResource = addScriptResource(resource);
    if (!scriptResource)
        return 0;

    updateScriptResourceResponse(resource);
    updateScriptResource(resource, resource->length);
    updateScriptResource(resource, resource->startTime, resource->responseReceivedTime, resource->endTime);
    updateScriptResource(resource, resource->finished, resource->failed);
    return scriptResource;
}
#elif USE(V8)
void InspectorController::addAndUpdateScriptResource(InspectorResource* resource)
{
    ASSERT_ARG(resource, resource);
    ASSERT(trackResources());

    addScriptResource(resource);
    updateScriptResourceResponse(resource);
    updateScriptResource(resource, resource->length);
    updateScriptResource(resource, resource->startTime, resource->responseReceivedTime, resource->endTime);
    updateScriptResource(resource, resource->finished, resource->failed);
}
#endif

#if USE(JSC)
void InspectorController::removeScriptResource(InspectorResource* resource)
{
    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    if (!m_scriptContext || !m_scriptObject)
        return;

    ASSERT(resource);
    ASSERT(resource->scriptObject);
    if (!resource || !resource->scriptObject)
        return;

    JSObjectRef scriptObject = resource->scriptObject;
    resource->setScriptObject(0, 0);

    JSValueRef exception = 0;

    JSValueRef removeResourceProperty = JSObjectGetProperty(m_scriptContext, m_scriptObject, jsStringRef("removeResource").get(), &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectRef removeResourceFunction = JSValueToObject(m_scriptContext, removeResourceProperty, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSValueRef arguments[] = { scriptObject };
    JSObjectCallAsFunction(m_scriptContext, removeResourceFunction, m_scriptObject, 1, arguments, &exception);
    HANDLE_EXCEPTION(exception);
}
#elif USE(V8)
void InspectorController::removeScriptResource(InspectorResource* resource)
{
    ASSERT(hasScriptObject());
    if (!hasScriptObject())
        return;

    ASSERT(resource);
    ASSERT(resource->hasScriptObject());
    if (!resource || !resource->hasScriptObject())
        return;

    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
    v8::Context::Scope scope(context);

    v8::Handle<v8::Value> removeResourceFunction = m_scriptObject->Get(v8::String::New("removeResource"));
    ASSERT(removeResourceFunction->IsFunction());
    v8::Handle<v8::Value> args[] = { resource->scriptObject };
    (v8::Function::Cast(*removeResourceFunction))->Call(m_scriptObject, 1, args);

    resource->setScriptObject(v8::Handle<v8::Object>());
}
#endif

static void updateResourceRequest(InspectorResource* resource, const ResourceRequest& request)
{
    resource->requestHeaderFields = request.httpHeaderFields();
    resource->requestURL = request.url();
}

static void updateResourceResponse(InspectorResource* resource, const ResourceResponse& response)
{
    resource->expectedContentLength = response.expectedContentLength();
    resource->mimeType = response.mimeType();
    resource->responseHeaderFields = response.httpHeaderFields();
    resource->responseStatusCode = response.httpStatusCode();
    resource->suggestedFilename = response.suggestedFilename();
}

#if USE(JSC)
void InspectorController::updateScriptResourceRequest(InspectorResource* resource)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef urlValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.string()).get());
    JSValueRef domainValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.host()).get());
    JSValueRef pathValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.path()).get());
    JSValueRef lastPathComponentValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.lastPathComponent()).get());

    JSValueRef mainResourceValue = JSValueMakeBoolean(m_scriptContext, m_mainResource == resource);

    JSValueRef exception = 0;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("url").get(), urlValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("domain").get(), domainValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("path").get(), pathValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("lastPathComponent").get(), lastPathComponentValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectRef scriptObject = scriptObjectForRequest(m_scriptContext, resource, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("requestHeaders").get(), scriptObject, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("mainResource").get(), mainResourceValue, kJSPropertyAttributeNone, &exception);
    HANDLE_EXCEPTION(exception);
}
#elif USE(V8)
void InspectorController::updateScriptResourceRequest(InspectorResource* resource)
{
    ASSERT(resource->hasScriptObject());
    ASSERT(trackResources());
    if (!resource->hasScriptObject())
        return;

    // Acquire the context so we can create V8 objects
    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
    v8::Context::Scope scope(context);

    resource->scriptObject->Set(v8::String::New("url"), v8StringOrNull(resource->requestURL.string()));
    resource->scriptObject->Set(v8::String::New("domain"), v8StringOrNull(resource->requestURL.host()));
    resource->scriptObject->Set(v8::String::New("path"), v8StringOrNull(resource->requestURL.path()));
    resource->scriptObject->Set(v8::String::New("lastPathComponent"), v8StringOrNull(resource->requestURL.lastPathComponent()));
    resource->scriptObject->Set(v8::String::New("requestHeaders"), scriptObjectForRequest(resource));
    resource->scriptObject->Set(v8::String::New("mainResource"), (m_mainResource == resource)?v8::True():v8::False());
}
#endif

#if USE(JSC)
void InspectorController::updateScriptResourceResponse(InspectorResource* resource)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef mimeTypeValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->mimeType).get());

    JSValueRef suggestedFilenameValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->suggestedFilename).get());

    JSValueRef expectedContentLengthValue = JSValueMakeNumber(m_scriptContext, static_cast<double>(resource->expectedContentLength));
    JSValueRef statusCodeValue = JSValueMakeNumber(m_scriptContext, resource->responseStatusCode);

    JSValueRef exception = 0;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("mimeType").get(), mimeTypeValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("suggestedFilename").get(), suggestedFilenameValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("expectedContentLength").get(), expectedContentLengthValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("statusCode").get(), statusCodeValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectRef scriptObject = scriptObjectForResponse(m_scriptContext, resource, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("responseHeaders").get(), scriptObject, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSValueRef typeValue = JSValueMakeNumber(m_scriptContext, resource->type());
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("type").get(), typeValue, kJSPropertyAttributeNone, &exception);
    HANDLE_EXCEPTION(exception);
}
#elif USE(V8)
void InspectorController::updateScriptResourceResponse(InspectorResource* resource)
{
    ASSERT(resource->hasScriptObject());
    ASSERT(trackResources());
    if (!resource->hasScriptObject())
        return;

    // Acquire the context so we can create V8 objects
    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
    v8::Context::Scope scope(context);

    resource->scriptObject->Set(v8::String::New("mimeType"), v8StringOrNull(resource->mimeType));
    resource->scriptObject->Set(v8::String::New("suggestedFilename"), v8StringOrNull(resource->suggestedFilename));
    resource->scriptObject->Set(v8::String::New("expectedContentLength"), v8::Number::New(resource->expectedContentLength));
    resource->scriptObject->Set(v8::String::New("statusCode"), v8::Number::New(resource->responseStatusCode));
    resource->scriptObject->Set(v8::String::New("responseHeaders"), scriptObjectForResponse(resource));
    resource->scriptObject->Set(v8::String::New("type"), v8::Number::New(resource->type()));
}
#endif

#if USE(JSC)
void InspectorController::updateScriptResource(InspectorResource* resource, int length)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef lengthValue = JSValueMakeNumber(m_scriptContext, length);

    JSValueRef exception = 0;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("contentLength").get(), lengthValue, kJSPropertyAttributeNone, &exception);
    HANDLE_EXCEPTION(exception);
}
#elif USE(V8)
void InspectorController::updateScriptResource(InspectorResource* resource, int length)
{
    ASSERT(resource->hasScriptObject());
    ASSERT(trackResources());
    if (!resource->hasScriptObject())
        return;

    // Acquire the context so we can create V8 objects
    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
    v8::Context::Scope scope(context);

    m_scriptObject->Set(v8::String::New("contentLength"), v8::Number::New(length));
}
#endif

#if USE(JSC)
void InspectorController::updateScriptResource(InspectorResource* resource, bool finished, bool failed)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef failedValue = JSValueMakeBoolean(m_scriptContext, failed);
    JSValueRef finishedValue = JSValueMakeBoolean(m_scriptContext, finished);

    JSValueRef exception = 0;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("failed").get(), failedValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("finished").get(), finishedValue, kJSPropertyAttributeNone, &exception);
    HANDLE_EXCEPTION(exception);
}
#elif USE(V8)
void InspectorController::updateScriptResource(InspectorResource* resource, bool finished, bool failed)
{
    ASSERT(resource->hasScriptObject());
    ASSERT(trackResources());
    if (!resource->hasScriptObject())
        return;

    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
    v8::Context::Scope scope(context);

    resource->scriptObject->Set(v8::String::New("failed"), (failed)?v8::True():v8::False());
    resource->scriptObject->Set(v8::String::New("finished"), (finished)?v8::True():v8::False());
}
#endif

#if USE(JSC)
void InspectorController::updateScriptResource(InspectorResource* resource, double startTime, double responseReceivedTime, double endTime)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef startTimeValue = JSValueMakeNumber(m_scriptContext, startTime);
    JSValueRef responseReceivedTimeValue = JSValueMakeNumber(m_scriptContext, responseReceivedTime);
    JSValueRef endTimeValue = JSValueMakeNumber(m_scriptContext, endTime);

    JSValueRef exception = 0;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("startTime").get(), startTimeValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("responseReceivedTime").get(), responseReceivedTimeValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("endTime").get(), endTimeValue, kJSPropertyAttributeNone, &exception);
    HANDLE_EXCEPTION(exception);
}
#elif USE(V8)
void InspectorController::updateScriptResource(InspectorResource* resource, double startTime, double responseReceivedTime, double endTime)
{
    ASSERT(resource->hasScriptObject());
    ASSERT(trackResources());
    if (!resource->hasScriptObject())
        return;

    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
    v8::Context::Scope scope(context);

    resource->scriptObject->Set(v8::String::New("startTime"), v8::Number::New(startTime));
    resource->scriptObject->Set(v8::String::New("responseReceivedTime"), v8::Number::New(responseReceivedTime));
    resource->scriptObject->Set(v8::String::New("endTime"), v8::Number::New(endTime));
}
#endif

#if USE(JSC)
void InspectorController::populateScriptObjects()
{
    ASSERT(m_scriptContext);
    if (!m_scriptContext)
        return;

    ResourcesMap::iterator resourcesEnd = m_resources.end();
    for (ResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it)
        addAndUpdateScriptResource(it->second.get());

    unsigned messageCount = m_consoleMessages.size();
    for (unsigned i = 0; i < messageCount; ++i)
        addScriptConsoleMessage(m_consoleMessages[i]);

#if ENABLE(DATABASE)
    DatabaseResourcesSet::iterator databasesEnd = m_databaseResources.end();
    for (DatabaseResourcesSet::iterator it = m_databaseResources.begin(); it != databasesEnd; ++it)
        addDatabaseScriptResource((*it).get());
#endif
}
#elif USE(V8)
void InspectorController::populateScriptObjects()
{
    ResourcesMap::iterator resourcesEnd = m_resources.end();
    for (ResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it)
        addAndUpdateScriptResource(it->second.get());

    unsigned messageCount = m_consoleMessages.size();
    for (unsigned i = 0; i < messageCount; ++i)
        addScriptConsoleMessage(m_consoleMessages[i]);
}
#endif

#if ENABLE(DATABASE)
JSObjectRef InspectorController::addDatabaseScriptResource(InspectorDatabaseResource* resource)
{
    ASSERT_ARG(resource, resource);

    if (resource->scriptObject)
        return resource->scriptObject;

    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    if (!m_scriptContext || !m_scriptObject)
        return 0;

    Frame* frame = resource->database->document()->frame();
    if (!frame)
        return 0;

    JSValueRef exception = 0;

    JSValueRef databaseProperty = JSObjectGetProperty(m_scriptContext, m_scriptObject, jsStringRef("Database").get(), &exception);
    if (HANDLE_EXCEPTION(exception))
        return 0;

    JSObjectRef databaseConstructor = JSValueToObject(m_scriptContext, databaseProperty, &exception);
    if (HANDLE_EXCEPTION(exception))
        return 0;

    ExecState* exec = toJSDOMWindow(frame)->globalExec();

    JSValueRef database;

    {
        KJS::JSLock lock;
        database = toRef(JSInspectedObjectWrapper::wrap(exec, toJS(exec, resource->database.get())));
    }

    JSValueRef domainValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->domain).get());
    JSValueRef nameValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->name).get());
    JSValueRef versionValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->version).get());

    JSValueRef arguments[] = { database, domainValue, nameValue, versionValue };
    JSObjectRef result = JSObjectCallAsConstructor(m_scriptContext, databaseConstructor, 4, arguments, &exception);
    if (HANDLE_EXCEPTION(exception))
        return 0;

    ASSERT(result);

    JSValueRef addDatabaseProperty = JSObjectGetProperty(m_scriptContext, m_scriptObject, jsStringRef("addDatabase").get(), &exception);
    if (HANDLE_EXCEPTION(exception))
        return 0;

    JSObjectRef addDatabaseFunction = JSValueToObject(m_scriptContext, addDatabaseProperty, &exception);
    if (HANDLE_EXCEPTION(exception))
        return 0;

    JSValueRef addArguments[] = { result };
    JSObjectCallAsFunction(m_scriptContext, addDatabaseFunction, m_scriptObject, 1, addArguments, &exception);
    if (HANDLE_EXCEPTION(exception))
        return 0;

    resource->setScriptObject(m_scriptContext, result);

    return result;
}

void InspectorController::removeDatabaseScriptResource(InspectorDatabaseResource* resource)
{
    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    if (!m_scriptContext || !m_scriptObject)
        return;

    ASSERT(resource);
    ASSERT(resource->scriptObject);
    if (!resource || !resource->scriptObject)
        return;

    JSObjectRef scriptObject = resource->scriptObject;
    resource->setScriptObject(0, 0);

    JSValueRef exception = 0;

    JSValueRef removeDatabaseProperty = JSObjectGetProperty(m_scriptContext, m_scriptObject, jsStringRef("removeDatabase").get(), &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectRef removeDatabaseFunction = JSValueToObject(m_scriptContext, removeDatabaseProperty, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSValueRef arguments[] = { scriptObject };
    JSObjectCallAsFunction(m_scriptContext, removeDatabaseFunction, m_scriptObject, 1, arguments, &exception);
    HANDLE_EXCEPTION(exception);
}
#endif

#if USE(JSC)
void InspectorController::addScriptConsoleMessage(const ConsoleMessage* message)
{
    ASSERT_ARG(message, message);

    if (!m_scriptContext || !m_scriptObject)
        return;

    JSValueRef exception = 0;

    JSValueRef messageConstructorProperty = JSObjectGetProperty(m_scriptContext, m_scriptObject, jsStringRef("ConsoleMessage").get(), &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectRef messageConstructor = JSValueToObject(m_scriptContext, messageConstructorProperty, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSValueRef addMessageProperty = JSObjectGetProperty(m_scriptContext, m_scriptObject, jsStringRef("addMessageToConsole").get(), &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectRef addMessage = JSValueToObject(m_scriptContext, addMessageProperty, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSValueRef sourceValue = JSValueMakeNumber(m_scriptContext, message->source);
    JSValueRef levelValue = JSValueMakeNumber(m_scriptContext, message->level);
    JSValueRef lineValue = JSValueMakeNumber(m_scriptContext, message->line);
    JSValueRef urlValue = JSValueMakeString(m_scriptContext, jsStringRef(message->url).get());

    static const unsigned maximumMessageArguments = 256;
    JSValueRef arguments[maximumMessageArguments];
    unsigned argumentCount = 0;
    arguments[argumentCount++] = sourceValue;
    arguments[argumentCount++] = levelValue;
    arguments[argumentCount++] = lineValue;
    arguments[argumentCount++] = urlValue;

    if (!message->wrappedArguments.isEmpty()) {
        unsigned remainingSpaceInArguments = maximumMessageArguments - argumentCount;
        unsigned argumentsToAdd = min(remainingSpaceInArguments, static_cast<unsigned>(message->wrappedArguments.size()));
        for (unsigned i = 0; i < argumentsToAdd; ++i)
            arguments[argumentCount++] = toRef(message->wrappedArguments[i]);
    } else {
        JSValueRef messageValue = JSValueMakeString(m_scriptContext, jsStringRef(message->message).get());
        arguments[argumentCount++] = messageValue;
    }

    JSObjectRef messageObject = JSObjectCallAsConstructor(m_scriptContext, messageConstructor, argumentCount, arguments, &exception);
    if (HANDLE_EXCEPTION(exception))
        return;

    JSObjectCallAsFunction(m_scriptContext, addMessage, m_scriptObject, 1, &messageObject, &exception);
    HANDLE_EXCEPTION(exception);
}
#elif USE(V8)
void InspectorController::addScriptConsoleMessage(const ConsoleMessage* message)
{
    ASSERT_ARG(message, message);

    if (!hasScriptObject()) 
        return;

    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
    
    if (context.IsEmpty())
        return;

    v8::Context::Scope scope(context);

    v8::Handle<v8::Value> consoleMessageProperty = 
        m_scriptObject->Get(v8::String::New("ConsoleMessage"));
    ASSERT(!consoleMessageProperty.IsEmpty() && consoleMessageProperty->IsFunction());
    
    if (consoleMessageProperty.IsEmpty() || !consoleMessageProperty->IsFunction())
        return;
    v8::Handle<v8::Function> consoleMessageConstructor = 
        v8::Handle<v8::Function>::Cast(consoleMessageProperty);
    
    v8::Handle<v8::Value> addMessageToConsole = 
        m_scriptObject->Get(v8::String::New("addMessageToConsole"));
    ASSERT(!addMessageToConsole.IsEmpty() && addMessageToConsole->IsFunction());
    
    if (addMessageToConsole.IsEmpty() || !addMessageToConsole->IsFunction())
        return;

    v8::Handle<v8::Value> args[] = {
        v8::Number::New(message->source),
        v8::Number::New(message->level),
        v8::Number::New(message->line),
        v8StringOrNull(message->url),
        v8StringOrNull(message->message),
    };

    v8::Handle<v8::Object> consoleMessage = 
        SafeAllocation::NewInstance(consoleMessageConstructor, 5, args);
    if (consoleMessage.IsEmpty())
        return;

    v8::Handle<v8::Value> args2[] = { consoleMessage };
    (v8::Function::Cast(*addMessageToConsole))->Call(m_scriptObject, 1, args2);
}
#endif

void InspectorController::resetScriptObjects()
{
#if USE(JSC)
    if (!m_scriptContext || !m_scriptObject)
        return;
#elif USE(V8)
    if (!hasScriptObject())
        return;
#endif

    ResourcesMap::iterator resourcesEnd = m_resources.end();
    for (ResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it) {
        InspectorResource* resource = it->second.get();
#if USE(JSC)
        resource->setScriptObject(0, 0);
#elif USE(V8)
        resource->setScriptObject(v8::Handle<v8::Object>());
#endif
    }

#if ENABLE(DATABASE)
    DatabaseResourcesSet::iterator databasesEnd = m_databaseResources.end();
    for (DatabaseResourcesSet::iterator it = m_databaseResources.begin(); it != databasesEnd; ++it) {
        InspectorDatabaseResource* resource = (*it).get();
        resource->setScriptObject(0, 0);
    }
#endif

#if USE(JSC)
    callSimpleFunction(m_scriptContext, m_scriptObject, "reset");
#elif USE(V8)
    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
    v8::Context::Scope scope(context);

    v8::Handle<v8::Value> reset = m_scriptObject->Get(v8::String::New("reset"));
    ASSERT(reset->IsFunction());

    v8::Handle<v8::Function> func(v8::Function::Cast(*reset));
    func->Call(m_scriptObject, 0, NULL);
#endif
}

void InspectorController::pruneResources(ResourcesMap* resourceMap, DocumentLoader* loaderToKeep)
{
    ASSERT_ARG(resourceMap, resourceMap);

    ResourcesMap mapCopy(*resourceMap);
    ResourcesMap::iterator end = mapCopy.end();
    for (ResourcesMap::iterator it = mapCopy.begin(); it != end; ++it) {
        InspectorResource* resource = (*it).second.get();
        if (resource == m_mainResource)
            continue;

        if (!loaderToKeep || resource->loader != loaderToKeep) {
            removeResource(resource);
            if (windowVisible() && resource->hasScriptObject())
                removeScriptResource(resource);
        }
    }
}

void InspectorController::didCommitLoad(DocumentLoader* loader)
{
    if (!enabled())
        return;

    if (loader->frame() == m_inspectedPage->mainFrame()) {
        m_client->inspectedURLChanged(loader->url().string());
        deleteAllValues(m_consoleMessages);
        m_consoleMessages.clear();

#if ENABLE(DATABASE)
        m_databaseResources.clear();
#endif

        if (windowVisible()) {
            resetScriptObjects();

            if (!loader->isLoadingFromCachedPage()) {
                // We don't add the main resource until its load is committed. This is
                // needed to keep the load for a user-entered URL from showing up in the
                // list of resources for the page they are navigating away from.
                if (trackResources() && m_mainResource)
                    addAndUpdateScriptResource(m_mainResource.get());
            } else {
                // Pages loaded from the page cache are committed before
                // m_mainResource is the right resource for this load, so we
                // clear it here. It will be re-assigned in
                // identifierForInitialRequest.
                m_mainResource = 0;
            }
        }
    }

    if (trackResources()) {
      for (Frame* frame = loader->frame(); frame; frame = frame->tree()->traverseNext(loader->frame()))
          if (ResourcesMap* resourceMap = m_frameResources.get(frame))
              pruneResources(resourceMap, loader);
    }
}

void InspectorController::frameDetachedFromParent(Frame* frame)
{
    if (!enabled())
        return;
    if (ResourcesMap* resourceMap = m_frameResources.get(frame))
        removeAllResources(resourceMap);
}

void InspectorController::addResource(InspectorResource* resource)
{
    ASSERT(trackResources());
    m_resources.set(resource->identifier, resource);

    Frame* frame = resource->frame.get();
    ResourcesMap* resourceMap = m_frameResources.get(frame);
    if (resourceMap)
        resourceMap->set(resource->identifier, resource);
    else {
        resourceMap = new ResourcesMap;
        resourceMap->set(resource->identifier, resource);
        m_frameResources.set(frame, resourceMap);
    }
}

void InspectorController::removeResource(InspectorResource* resource)
{
    m_resources.remove(resource->identifier);

    Frame* frame = resource->frame.get();
    ResourcesMap* resourceMap = m_frameResources.get(frame);
    if (!resourceMap) {
        ASSERT_NOT_REACHED();
        return;
    }

    resourceMap->remove(resource->identifier);
    if (resourceMap->isEmpty()) {
        m_frameResources.remove(frame);
        delete resourceMap;
    }
}

void InspectorController::didLoadResourceFromMemoryCache(DocumentLoader* loader, const ResourceRequest& request, const ResourceResponse& response, int length)
{
    if (!enabled() || !trackResources())
        return;

    RefPtr<InspectorResource> resource = InspectorResource::create(m_nextIdentifier--, loader, loader->frame());
    resource->finished = true;

    updateResourceRequest(resource.get(), request);
    updateResourceResponse(resource.get(), response);

    resource->length = length;
    resource->cached = true;
    resource->startTime = currentTime();
    resource->responseReceivedTime = resource->startTime;
    resource->endTime = resource->startTime;

    if (loader->frame() == m_inspectedPage->mainFrame() && request.url() == loader->requestURL())
        m_mainResource = resource;

    addResource(resource.get());

    if (windowVisible())
        addAndUpdateScriptResource(resource.get());
}

void InspectorController::identifierForInitialRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    if (!enabled() || !trackResources())
        return;

    RefPtr<InspectorResource> resource = InspectorResource::create(identifier, loader, loader->frame());

    updateResourceRequest(resource.get(), request);

    if (loader->frame() == m_inspectedPage->mainFrame() && request.url() == loader->requestURL())
        m_mainResource = resource;

    addResource(resource.get());

    if (windowVisible() && loader->isLoadingFromCachedPage() && resource == m_mainResource)
        addAndUpdateScriptResource(resource.get());
}

void InspectorController::willSendRequest(DocumentLoader* loader, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    if (!enabled() || !trackResources())
        return;

    InspectorResource* resource = m_resources.get(identifier).get();
    if (!resource)
        return;

    resource->startTime = currentTime();

    if (!redirectResponse.isNull()) {
        updateResourceRequest(resource, request);
        updateResourceResponse(resource, redirectResponse);
    }

    if (resource != m_mainResource && windowVisible()) {
        if (!resource->hasScriptObject())
            addScriptResource(resource);
        else
            updateScriptResourceRequest(resource);

        updateScriptResource(resource, resource->startTime, resource->responseReceivedTime, resource->endTime);

        if (!redirectResponse.isNull())
            updateScriptResourceResponse(resource);
    }
}

void InspectorController::didReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse& response)
{
    if (!enabled() || !trackResources())
        return;

    InspectorResource* resource = m_resources.get(identifier).get();
    if (!resource)
        return;

    updateResourceResponse(resource, response);

    resource->responseReceivedTime = currentTime();

    if (windowVisible() && resource->hasScriptObject()) {
        updateScriptResourceResponse(resource);
        updateScriptResource(resource, resource->startTime, resource->responseReceivedTime, resource->endTime);
    }
}

void InspectorController::didReceiveContentLength(DocumentLoader*, unsigned long identifier, int lengthReceived)
{
    if (!enabled() || !trackResources())
        return;

    InspectorResource* resource = m_resources.get(identifier).get();
    if (!resource)
        return;

    resource->length += lengthReceived;

    if (windowVisible() && resource->hasScriptObject())
        updateScriptResource(resource, resource->length);
}

void InspectorController::didFinishLoading(DocumentLoader* loader, unsigned long identifier)
{
    if (!enabled() || !trackResources())
        return;

    RefPtr<InspectorResource> resource = m_resources.get(identifier);
    if (!resource)
        return;

    removeResource(resource.get());

    resource->finished = true;
    resource->endTime = currentTime();

    addResource(resource.get());

    if (windowVisible() && resource->hasScriptObject()) {
        updateScriptResource(resource.get(), resource->startTime, resource->responseReceivedTime, resource->endTime);
        updateScriptResource(resource.get(), resource->finished);
    }
}

void InspectorController::didFailLoading(DocumentLoader* loader, unsigned long identifier, const ResourceError& /*error*/)
{
    if (!enabled() || !trackResources())
        return;

    RefPtr<InspectorResource> resource = m_resources.get(identifier);
    if (!resource)
        return;

    removeResource(resource.get());

    resource->finished = true;
    resource->failed = true;
    resource->endTime = currentTime();

    addResource(resource.get());

    if (windowVisible() && resource->hasScriptObject()) {
        updateScriptResource(resource.get(), resource->startTime, resource->responseReceivedTime, resource->endTime);
        updateScriptResource(resource.get(), resource->finished, resource->failed);
    }
}

#if USE(JSC)
void InspectorController::resourceRetrievedByXMLHttpRequest(unsigned long identifier, KJS::UString& sourceString)
{
    if (!enabled())
        return;

    InspectorResource* resource = m_resources.get(identifier).get();
    if (!resource)
        return;

    resource->setXMLHttpRequestProperties(sourceString);
}
#elif USE(V8)
// TODO(ojan): Implement!
void InspectorController::resourceRetrievedByXMLHttpRequest(unsigned long identifier, String& sourceString)
{
}
#endif


#if ENABLE(DATABASE)
void InspectorController::didOpenDatabase(Database* database, const String& domain, const String& name, const String& version)
{
    if (!enabled())
        return;

    RefPtr<InspectorDatabaseResource> resource = InspectorDatabaseResource::create(database, domain, name, version);

    m_databaseResources.add(resource);

    if (windowVisible())
        addDatabaseScriptResource(resource.get());
}
#endif

void InspectorController::moveWindowBy(float x, float y) const
{
    if (!m_page || !enabled())
        return;

    FloatRect frameRect = m_page->chrome()->windowRect();
    frameRect.move(x, y);
    m_page->chrome()->setWindowRect(frameRect);
}

void InspectorController::startDebuggingAndReloadInspectedPage()
{
    /*
    JavaScriptDebugServer::shared().addListener(this, m_inspectedPage);
    m_debuggerAttached = true;
    m_inspectedPage->mainFrame()->loader()->reload();
    */
    notImplemented();
}

void InspectorController::stopDebugging()
{
    /*
    JavaScriptDebugServer::shared().removeListener(this, m_inspectedPage);
    m_debuggerAttached = false;
    */
    notImplemented();
}

static void drawOutlinedRect(GraphicsContext& context, const IntRect& rect, const Color& fillColor)
{
    static const int outlineThickness = 1;
    static const Color outlineColor(62, 86, 180, 228);

    IntRect outline = rect;
    outline.inflate(outlineThickness);

    context.clearRect(outline);

    context.save();
    context.clipOut(rect);
    context.fillRect(outline, outlineColor);
    context.restore();

    context.fillRect(rect, fillColor);
}

static void drawHighlightForBoxes(GraphicsContext& context, const Vector<IntRect>& lineBoxRects, const IntRect& contentBox, const IntRect& paddingBox, const IntRect& borderBox, const IntRect& marginBox)
{
    static const Color contentBoxColor(125, 173, 217, 128);
    static const Color paddingBoxColor(125, 173, 217, 160);
    static const Color borderBoxColor(125, 173, 217, 192);
    static const Color marginBoxColor(125, 173, 217, 228);

    if (!lineBoxRects.isEmpty()) {
        for (size_t i = 0; i < lineBoxRects.size(); ++i)
            drawOutlinedRect(context, lineBoxRects[i], contentBoxColor);
        return;
    }

    if (marginBox != borderBox)
        drawOutlinedRect(context, marginBox, marginBoxColor);
    if (borderBox != paddingBox)
        drawOutlinedRect(context, borderBox, borderBoxColor);
    if (paddingBox != contentBox)
        drawOutlinedRect(context, paddingBox, paddingBoxColor);
    drawOutlinedRect(context, contentBox, contentBoxColor);
}

void InspectorController::drawNodeHighlight(GraphicsContext& context) const
{
    if (!m_highlightedNode)
        return;

    RenderObject* renderer = m_highlightedNode->renderer();
    if (!renderer)
        return;

    IntRect contentBox = renderer->absoluteContentBox();
    // FIXME: Should we add methods to RenderObject to obtain these rects?
    IntRect paddingBox(contentBox.x() - renderer->paddingLeft(), contentBox.y() - renderer->paddingTop(), contentBox.width() + renderer->paddingLeft() + renderer->paddingRight(), contentBox.height() + renderer->paddingTop() + renderer->paddingBottom());
    IntRect borderBox(paddingBox.x() - renderer->borderLeft(), paddingBox.y() - renderer->borderTop(), paddingBox.width() + renderer->borderLeft() + renderer->borderRight(), paddingBox.height() + renderer->borderTop() + renderer->borderBottom());
    IntRect marginBox(borderBox.x() - renderer->marginLeft(), borderBox.y() - renderer->marginTop(), borderBox.width() + renderer->marginLeft() + renderer->marginRight(), borderBox.height() + renderer->marginTop() + renderer->marginBottom());

    IntRect boundingBox = renderer->absoluteBoundingBoxRect();

    Vector<IntRect> lineBoxRects;
    if (renderer->isInline() || (renderer->isText() && !m_highlightedNode->isSVGElement())) {
        // FIXME: We should show margins/padding/border for inlines.
        renderer->addLineBoxRects(lineBoxRects);
    }
    if (lineBoxRects.isEmpty() && contentBox.isEmpty()) {
        // If we have no line boxes and our content box is empty, we'll just draw our bounding box.
        // This can happen, e.g., with an <a> enclosing an <img style="float:right">.
        // FIXME: Can we make this better/more accurate? The <a> in the above case has no
        // width/height but the highlight makes it appear to be the size of the <img>.
        lineBoxRects.append(boundingBox);
    }

    FrameView* view = m_inspectedPage->mainFrame()->view();
    FloatRect overlayRect = view->visibleContentRect();

    if (!overlayRect.contains(boundingBox) && !boundingBox.contains(enclosingIntRect(overlayRect))) {
        Element* element;
        if (m_highlightedNode->isElementNode())
            element = static_cast<Element*>(m_highlightedNode.get());
        else
            element = static_cast<Element*>(m_highlightedNode->parent());
        element->scrollIntoViewIfNeeded();
        overlayRect = view->visibleContentRect();
    }

    context.translate(-overlayRect.x(), -overlayRect.y());

    drawHighlightForBoxes(context, lineBoxRects, contentBox, paddingBox, borderBox, marginBox);
}

#if USE(JSC)
bool InspectorController::handleException(JSValueRef exception, unsigned lineNumber) const
{
    if (!exception)
        return false;

    if (!m_page)
        return true;

    JSRetainPtr<JSStringRef> messageString(Adopt, JSValueToStringCopy(m_scriptContext, exception, 0));
    String message(JSStringGetCharactersPtr(messageString.get()), JSStringGetLength(messageString.get()));

    m_page->mainFrame()->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, message, lineNumber, __FILE__);
    return true;
}

#pragma mark -
#pragma mark JavaScriptDebugListener functions

void InspectorController::didParseSource(ExecState*, const UString& /*source*/, int /*startingLineNumber*/, const UString& /*sourceURL*/, int /*sourceID*/)
{
}

void InspectorController::failedToParseSource(ExecState*, const UString& /*source*/, int /*startingLineNumber*/, const UString& /*sourceURL*/, int /*errorLine*/, const UString& /*errorMessage*/)
{
}

void InspectorController::didEnterCallFrame(ExecState*, int /*sourceID*/, int /*lineNumber*/)
{
}

void InspectorController::willExecuteStatement(ExecState*, int /*sourceID*/, int /*lineNumber*/)
{
}

void InspectorController::willLeaveCallFrame(ExecState*, int /*sourceID*/, int /*lineNumber*/)
{
}

void InspectorController::exceptionWasRaised(ExecState*, int /*sourceID*/, int /*lineNumber*/)
{
}

#elif USE(V8)
// TODO(ojan): Implement!
#endif

} // namespace WebCore
