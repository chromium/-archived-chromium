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
#include "HitTestResult.h"
#include "HTMLFrameOwnerElement.h"
#include "InspectorClient.h"
#include "v8_proxy.h"
#include "v8_binding.h"
#include "Page.h"
#include "Range.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptCallStack.h"
#include "ScriptController.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "SystemTime.h"
#include "TextEncoding.h"
#include "TextIterator.h"
#include <wtf/RefCounted.h>

#if ENABLE(DATABASE)
#include "Database.h"
#if USE(JSC)
#include "JSDatabase.h"
#endif
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER)
#include "JavaScriptDebugServer.h"
#endif

namespace WebCore {

// Maximum size of the console message cache.
static const size_t MAX_CONSOLE_MESSAGES = 250;

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

struct ConsoleMessage {	
    ConsoleMessage(MessageSource s, MessageLevel l, const String& m, unsigned li, const String& u, unsigned g)	
        : source(s)	
        , level(l)	
        , message(m)	
        , line(li)	
        , url(u)	
        , groupLevel(g) 
        , repeatCount(1) 
    {	
    }

    ConsoleMessage(MessageSource s, MessageLevel l, ScriptCallStack* callStack, unsigned g)
        : source(s)
        , level(l)
#if USE(JSC)
        , wrappedArguments(callStack->at(0).argumentCount())
#elif USE(V8)
        , arguments(callStack->at(0).argumentCount())
#endif
        , groupLevel(g)
        , repeatCount(1)
    {
        const ScriptCallFrame& lastCaller = callStack->at(0);
        line = lastCaller.lineNumber();
        url = lastCaller.sourceURL().string();

#if USE(JSC)
        JSLock lock(false);
        for (unsigned i = 0; i < lastCaller.argumentCount(); ++i)
            wrappedArguments[i] = JSInspectedObjectWrapper::wrap(callStack->state(), lastCaller.argumentAt(i).jsValue());
#elif USE(V8)
        for (unsigned i = 0; i < lastCaller.argumentCount(); ++i) {
            arguments[i] = lastCaller.argumentAt(i);
        }
#endif
    }

    bool operator==(ConsoleMessage msg) const 
    { 
        return msg.source == this->source
            && msg.level == this->level
            && msg.message == this->message
#if USE(JSC)
            && msg.wrappedArguments == this->wrappedArguments
#elif USE(V8)
            && msg.arguments == this->arguments
#endif
            && msg.line == this->line
            && msg.url == this->url
            && msg.groupLevel == this->groupLevel;
    }

    MessageSource source;	
    MessageLevel level;	
    String message;	
#if USE(JSC)
    Vector<ProtectedPtr<JSValue> > wrappedArguments;
#elif USE(V8)
    Vector<ScriptValue> arguments;
#endif
    unsigned line;	
    String url;	
    unsigned groupLevel;
    unsigned repeatCount;
};

struct XMLHttpRequestResource {
    XMLHttpRequestResource(const String& str)
    {
        sourceString = str;
    }

    ~XMLHttpRequestResource() { }

    String sourceString;
};

// InspectorResource Struct

struct InspectorResource : public RefCounted<InspectorResource> {
    // Keep these in sync with WebInspector.Resource.Type
    enum Type {
        Doc,
        Stylesheet,
        Image,
        Font,
        Script,
        XHR,
        Media,
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
        setScriptObject(v8::Handle<v8::Object>());
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

    // TODO(ojan): XHR requests show up in the inspector, but not their contents.
    // Something is wrong obviously, but not sure what. Not the highest priority
    // thing the inspector needs fixed right now though.
    void setXMLHttpRequestProperties(String& data)
    {
        xmlHttpRequestResource.set(new XMLHttpRequestResource(data));
    }

    String sourceString() const
    {
       if (xmlHttpRequestResource) {
             return xmlHttpRequestResource->sourceString;
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
                default:
                    break;
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
    v8::Persistent<v8::Object> scriptObject;
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
    inline bool hasScriptObject() { return !scriptObject.IsEmpty(); }

protected:
    InspectorResource(unsigned long identifier, DocumentLoader* documentLoader, Frame* frame)
       : identifier(identifier)
        , loader(documentLoader)
        , frame(frame)
        , xmlHttpRequestResource(0)
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

// InspectorDatabaseResource Struct

#if ENABLE(DATABASE)
struct InspectorDatabaseResource : public RefCounted<InspectorDatabaseResource> {
    static PassRefPtr<InspectorDatabaseResource> create(Database* database, const String& domain, const String& name, const String& version)
    {
        // Apple changed the default refcount to 1: http://trac.webkit.org/changeset/30406
        // We default it to 1 in the protected constructor below to match Apple,
        // so adoptRef is the right thing.
        return adoptRef(new InspectorDatabaseResource(database, domain, name, version));
    }

    void setScriptObject()
    {
        // TODO(aa): Implement this.
    }

    RefPtr<Database> database;
    String domain;
    String name;
    String version;
   
private:
    InspectorDatabaseResource(Database* database, const String& domain, const String& name, const String& version)
        : database(database)
        , domain(domain)
        , name(name)
        , version(version)
    {
    }
};
#endif

// JavaScript Callbacks 

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
void InspectorController::highlightDOMNode(Node* node)
{
    if (!enabled())
        return;
    
    ASSERT_ARG(node, node);
    m_client->highlight(node);
}
void InspectorController::hideDOMNodeHighlight()
{
    if (!enabled())
        return;

    m_client->hideHighlight();
}

void InspectorController::loaded() { 
    scriptObjectReady();
}

// We don't need to implement this because we just map windowUnloading to
// InspectorController::close in the IDL file.

void InspectorController::attach() {
    attachWindow();
}

void InspectorController::detach() {
    detachWindow();
}

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

#if ENABLE(DATABASE)
#if USE(JSC)
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
#elif USE(V8)
// TODO(aa): Implement inspector database support
#endif
#endif

DOMWindow* InspectorController::inspectedWindow() {
    // Can be null if page was already destroyed.
    if (!m_inspectedPage)
        return NULL;
    return m_inspectedPage->mainFrame()->domWindow();
}

String InspectorController::platform() const {
  return String("windows");
}

// InspectorController Class

InspectorController::InspectorController(Page* page, InspectorClient* client)
    :
      // The V8 version of InspectorController is RefCounted while the JSC
      // version uses an OwnPtr (http://b/904340).  However, since we're not
      // using a create method to initialize the InspectorController, we need
      // to start the RefCount at 0.
      RefCounted<InspectorController>(0)
    , m_bug1228513_inspectorState(bug1228513::VALID)
    , m_trackResources(false)
    , m_inspectedPage(page)
    , m_client(client)
    , m_page(0)
    , m_windowVisible(false)
#if ENABLE(JAVASCRIPT_DEBUGGER)
    , m_debuggerAttached(false)
    , m_attachDebuggerWhenShown(false)
#endif
    , m_recordingUserInitiatedProfile(false)
    , m_showAfterVisible(ElementsPanel)
    , m_nextIdentifier(-2)
    , m_groupLevel(0)
    , m_searchingForNode(false)
    , m_currentUserInitiatedProfileNumber(-1)
    , m_nextUserInitiatedProfileNumber(1)
    , m_previousMessage(0)
{
    ASSERT_ARG(page, page);
    ASSERT_ARG(client, client);
}

InspectorController::~InspectorController()
{
    m_bug1228513_inspectorState = bug1228513::DELETED;
    m_client->inspectorDestroyed();

    if (m_page)
        m_page->setParentInspectorController(0);

    // m_inspectedPage should have been cleared by inspectedPageDestroyed
    ASSERT(!m_inspectedPage);

    deleteAllValues(m_frameResources);
    deleteAllValues(m_consoleMessages);
}

void InspectorController::inspectedPageDestroyed()
{
    close();

    ASSERT(m_inspectedPage);
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

void InspectorController::setWindowVisible(bool visible, bool attached)
{
    // Policy: only log resources while the inspector window is visible.
    enableTrackResources(visible);

    if (visible == m_windowVisible)
        return;

    m_windowVisible = visible;

    if (!hasScriptObject())
        return;

    if (m_windowVisible) {
        setAttachedWindow(attached); 
        populateScriptObjects();
        if (m_nodeToFocus)
            focusNode();
#if ENABLE(JAVASCRIPT_DEBUGGER)
        if (m_attachDebuggerWhenShown) 
            startDebuggingAndReloadInspectedPage();
#endif
        if (m_showAfterVisible != CurrentPanel) 
            showPanel(m_showAfterVisible); 
    } else { 
#if ENABLE(JAVASCRIPT_DEBUGGER)
        stopDebugging();
#endif
        resetScriptObjects();
    }

    m_showAfterVisible = CurrentPanel;
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

void InspectorController::addDatabaseScriptResource(InspectorDatabaseResource*)
{
    // TODO(aa): Implement database support for inspector.
}

void InspectorController::addMessageToConsole(MessageSource source, MessageLevel level, ScriptCallStack* callStack)
{
    if (!enabled())
        return;

    addConsoleMessage(0, new ConsoleMessage(source, level, callStack, m_groupLevel));
}

void InspectorController::addMessageToConsole(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceID)
{
    if (!enabled())
        return;

    addConsoleMessage(0, new ConsoleMessage(source, level, message, lineNumber, sourceID, m_groupLevel));
}

void InspectorController::addConsoleMessage(ScriptState*, ConsoleMessage* consoleMessage)
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
    m_previousMessage = consoleMessage;
    m_consoleMessages.append(consoleMessage);
    if (windowVisible())
        addScriptConsoleMessage(m_previousMessage);
}

void InspectorController::clearConsoleMessages()
{
    deleteAllValues(m_consoleMessages);
    m_consoleMessages.clear();
    m_previousMessage = 0;
}

void InspectorController::startGroup(MessageSource source, ScriptCallStack* callStack)
{    
    ++m_groupLevel;

    addConsoleMessage(0, new ConsoleMessage(source, StartGroupMessageLevel, callStack, m_groupLevel));
}

void InspectorController::endGroup(MessageSource source, unsigned lineNumber, const String& sourceURL)
{
    if (m_groupLevel == 0)
        return;

    --m_groupLevel;

    addConsoleMessage(0, new ConsoleMessage(source, EndGroupMessageLevel, String(), lineNumber, sourceURL, m_groupLevel));
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

void InspectorController::setScriptObject(v8::Handle<v8::Object> newScriptObject)
{
    if (hasScriptObject()) {
        m_scriptObject.Dispose();
        m_scriptObject.Clear();
    }

    if (!newScriptObject.IsEmpty())
        m_scriptObject = v8::Persistent<v8::Object>::New(newScriptObject);
}

void InspectorController::inspectedWindowScriptObjectCleared(Frame* frame)
{
    // TODO(tc): We need to call inspectedWindowCleared, but that won't matter
    // until we merge in inspector.js as well.
    notImplemented();
}

void InspectorController::setAttachedWindow(bool attached)
{
    notImplemented();
}

void InspectorController::setAttachedWindowHeight(unsigned height)
{
    notImplemented();
}

void InspectorController::toggleSearchForNodeInPage()
{
    if (!enabled())
        return;

    m_searchingForNode = !m_searchingForNode;
    if (!m_searchingForNode)
        hideHighlight();
}

void InspectorController::mouseDidMoveOverElement(const HitTestResult& result, unsigned modifierFlags)
{
    if (!enabled() || !m_searchingForNode)
        return;

    Node* node = result.innerNode();
    if (node)
        highlight(node);
}

void InspectorController::handleMousePressOnNode(Node* node)
{
    if (!enabled())
        return;

    ASSERT(m_searchingForNode);
    ASSERT(node);
    if (!node)
        return;

    // inspect() will implicitly call ElementsPanel's focusedNodeChanged() and the hover feedback will be stopped there.
    inspect(node);
}


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

void InspectorController::showPanel(SpecialPanels panel)
{
    if (!enabled())
        return;

    show();

    if (!hasScriptObject()) {
        m_showAfterVisible = panel;
        return;
    }

    if (panel == CurrentPanel)
        return;

    const char* showFunctionName;
    switch (panel) {
        case ConsolePanel:
            showFunctionName = "showConsole";
            break;
        case DatabasesPanel:
            showFunctionName = "showDatabasesPanel";
            break;
        case ElementsPanel:
            showFunctionName = "showElementsPanel";
            break;
        case ProfilesPanel:
            showFunctionName = "showProfilesPanel";
            break;
        case ResourcesPanel:
            showFunctionName = "showResourcesPanel";
            break;
        case ScriptsPanel:
            showFunctionName = "showScriptsPanel";
            break;
        default:
            ASSERT_NOT_REACHED();
            showFunctionName = 0;
    }

    if (windowVisible() && showFunctionName) {
        v8::HandleScope handle_scope;
        v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
        v8::Context::Scope scope(context);

        // TODO(ojan): Use showFunctionName here. For some reason some of these
        // are not functions (e.g. showElementsPanel). 
        v8::Handle<v8::Value> showFunction = m_scriptObject->Get(v8::String::New("showConsole"));
        ASSERT(showFunction->IsFunction());

        v8::Handle<v8::Function> func(v8::Function::Cast(*showFunction));
        func->Call(m_scriptObject, 0, NULL);
    } else {
        m_client->showWindow();
    }
}

void InspectorController::close()
{
    if (!enabled())
        return;

    ++bug1228513::g_totalNumClose;

#if ENABLE(JAVASCRIPT_DEBUGGER)
    stopDebugging();
#endif
    closeWindow();
    if (m_page) {
        v8::HandleScope handle_scope;
        v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
        v8::Context::Scope scope(context);
        setScriptObject(v8::Handle<v8::Object>());
    }

    m_page = 0;
}

void InspectorController::showWindow()
{
    ASSERT(enabled());
    m_client->showWindow();
}

void InspectorController::closeWindow()
{
    m_client->closeWindow();
}

static void addHeaders(v8::Handle<v8::Object> object, const HTTPHeaderMap& headers)
{
    ASSERT_ARG(object, !object.IsEmpty());
    HTTPHeaderMap::const_iterator end = headers.end();
    for (HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it) {
        v8::Handle<v8::String> field = v8::String::New(FromWebCoreString(it->first), it->first.length());
        object->Set(field, v8StringOrNull(it->second));
    }
}

static v8::Handle<v8::Object> scriptObjectForRequest(const InspectorResource* resource)
{
    v8::Handle<v8::Object> object = v8::Object::New();
    addHeaders(object, resource->requestHeaderFields);
    return object;
}

static v8::Handle<v8::Object> scriptObjectForResponse(const InspectorResource* resource)
{
    v8::Handle<v8::Object> object = v8::Object::New();
    addHeaders(object, resource->responseHeaderFields);
    return object;
}

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

void InspectorController::updateScriptResourceType(InspectorResource* resource)
{
    notImplemented();
}

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

void InspectorController::populateScriptObjects()
{
    ResourcesMap::iterator resourcesEnd = m_resources.end();
    for (ResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it)
        addAndUpdateScriptResource(it->second.get());

    unsigned messageCount = m_consoleMessages.size();
    for (unsigned i = 0; i < messageCount; ++i)
        addScriptConsoleMessage(m_consoleMessages[i]);

    // TODO(ojan): Call populateInterface javascript function here.
    // Need to add it to the IDL and whatnot.
}

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

    // Create an instance of WebInspector.ConsoleMessage passing the variable
    // number of arguments available.
    static unsigned kArgcFixed = 6;
    unsigned argc = kArgcFixed + message->arguments.size();
    v8::Handle<v8::Value> *args = new v8::Handle<v8::Value>[argc];
    if (args == 0)
        return;
    unsigned i = 0;
    args[i++] = v8::Number::New(message->source);
    args[i++] = v8::Number::New(message->level);
    args[i++] = v8::Number::New(message->line);
    args[i++] = v8StringOrNull(message->url);
    args[i++] = v8::Number::New(message->groupLevel);
    args[i++] = v8::Number::New(message->repeatCount);
    ASSERT(kArgcFixed == i);
    for (unsigned i = 0; i < message->arguments.size(); ++i) {
      args[kArgcFixed + i] = message->arguments[i].v8Value();
    }

    v8::Handle<v8::Object> consoleMessage = 
        SafeAllocation::NewInstance(consoleMessageConstructor, argc, args);
    delete[] args;
    if (consoleMessage.IsEmpty())
        return;

    v8::Handle<v8::Value> args2[] = { consoleMessage };
    (v8::Function::Cast(*addMessageToConsole))->Call(m_scriptObject, 1, args2);
}

void InspectorController::resetScriptObjects()
{
    if (!hasScriptObject())
        return;

    ResourcesMap::iterator resourcesEnd = m_resources.end();
    for (ResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it) {
        InspectorResource* resource = it->second.get();
        resource->setScriptObject(v8::Handle<v8::Object>());
    }

#if ENABLE(DATABASE)
    DatabaseResourcesSet::iterator databasesEnd = m_databaseResources.end();
    for (DatabaseResourcesSet::iterator it = m_databaseResources.begin(); it != databasesEnd; ++it) {
        InspectorDatabaseResource* resource = (*it).get();
        resource->setScriptObject();
    }
#endif

    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_page->mainFrame());
    v8::Context::Scope scope(context);

    v8::Handle<v8::Value> reset = m_scriptObject->Get(v8::String::New("reset"));
    ASSERT(reset->IsFunction());

    v8::Handle<v8::Function> func(v8::Function::Cast(*reset));
    func->Call(m_scriptObject, 0, NULL);
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

    ASSERT(m_inspectedPage);

    if (loader->frame() == m_inspectedPage->mainFrame()) {
        m_client->inspectedURLChanged(loader->url().string());
        deleteAllValues(m_consoleMessages);
        m_consoleMessages.clear();
        m_groupLevel = 0;

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
    m_knownResources.add(resource->requestURL.string());

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

    // If the resource URL is already known, we don't need to add it again since this is just a cached load. 
    if (m_knownResources.contains(request.url().string()))
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

    ASSERT(m_inspectedPage); 

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

void InspectorController::resourceRetrievedByXMLHttpRequest(unsigned long identifier, const String& sourceString)
{
    notImplemented();
}

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

static inline void convertFromFrameToMainFrame(Frame* frame, IntRect& rect) 
{ 
    rect = frame->page()->mainFrame()->view()->windowToContents(frame->view()->contentsToWindow(rect)); 
} 

void InspectorController::drawNodeHighlight(GraphicsContext& context) const
{
    if (!m_highlightedNode)
        return;

    RenderObject* renderer = m_highlightedNode->renderer();
    Frame* containingFrame = m_highlightedNode->document()->frame(); 
    if (!renderer || !containingFrame) 
        return;

    IntRect contentBox = renderer->absoluteContentBox();
    IntRect boundingBox = renderer->absoluteBoundingBoxRect();

    // FIXME: Should we add methods to RenderObject to obtain these rects?
    IntRect paddingBox(contentBox.x() - renderer->paddingLeft(), contentBox.y() - renderer->paddingTop(), contentBox.width() + renderer->paddingLeft() + renderer->paddingRight(), contentBox.height() + renderer->paddingTop() + renderer->paddingBottom());
    IntRect borderBox(paddingBox.x() - renderer->borderLeft(), paddingBox.y() - renderer->borderTop(), paddingBox.width() + renderer->borderLeft() + renderer->borderRight(), paddingBox.height() + renderer->borderTop() + renderer->borderBottom());
    IntRect marginBox(borderBox.x() - renderer->marginLeft(), borderBox.y() - renderer->marginTop(), borderBox.width() + renderer->marginLeft() + renderer->marginRight(), borderBox.height() + renderer->marginTop() + renderer->marginBottom());

    convertFromFrameToMainFrame(containingFrame, contentBox); 
    convertFromFrameToMainFrame(containingFrame, paddingBox); 
    convertFromFrameToMainFrame(containingFrame, borderBox); 
    convertFromFrameToMainFrame(containingFrame, marginBox); 
    convertFromFrameToMainFrame(containingFrame, boundingBox);
    
    Vector<IntRect> lineBoxRects;
    if (renderer->isInline() || (renderer->isText() && !m_highlightedNode->isSVGElement())) {
        // FIXME: We should show margins/padding/border for inlines.
        renderer->addLineBoxRects(lineBoxRects);
    }

    for (unsigned i = 0; i < lineBoxRects.size(); ++i) 
        convertFromFrameToMainFrame(containingFrame, lineBoxRects[i]); 

    if (lineBoxRects.isEmpty() && contentBox.isEmpty()) {
        // If we have no line boxes and our content box is empty, we'll just draw our bounding box.
        // This can happen, e.g., with an <a> enclosing an <img style="float:right">.
        // FIXME: Can we make this better/more accurate? The <a> in the above case has no
        // width/height but the highlight makes it appear to be the size of the <img>.
        lineBoxRects.append(boundingBox);
    }

    ASSERT(m_inspectedPage); 

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

// TODO(dglazkov): These next three methods  should be easy to implement or gain
// for free when we unfork inspector

void InspectorController::count(const String& title, unsigned lineNumber, const String& sourceID)
{
    notImplemented();
}

void InspectorController::startTiming(const String& title)
{
    notImplemented();
}

bool InspectorController::stopTiming(const String& title, double& elapsed)
{
    elapsed = 0;
    notImplemented();
    return false;
}

} // namespace WebCore
