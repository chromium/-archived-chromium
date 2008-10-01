/*
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

#ifndef InspectorController_h
#define InspectorController_h

#if USE(V8)
// NOTE: The revision of the inspector JS and C++ that we are merged to
// is stored in trunk/webkit/port/page/inspector/BASE_REVISION.

// TODO(ojan): When we merge to webkit trunk, get rid of the need for this.
// This part of this file contains the version of InspectorController.h that
// we are synced to in trunk WebKit along with our (extensive) changes for V8.
// The block below for #elif USE(JAVSCRIPTCORE_BINDINGS) has the old version
// of InspectorController.h from the 3.1 branch, which we need to keep for the
// KJS build. We unfortunately can't just use a different header file for
// each build because a bunch of different files pull in this header file.

#include "JavaScriptDebugListener.h"

#include "Console.h"
#include "PlatformString.h"
#include "StringHash.h"
#include "DOMWindow.h"
#include <wtf/RefCounted.h>
#if USE(JSC)
#include <JavaScriptCore/JSContextRef.h>
#include <profiler/Profiler.h>
#elif USE(V8)
#include <v8.h>
#endif
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

#if USE(JSC)
namespace KJS {
    class Profile;
    class UString;
}
#endif

namespace WebCore {

class Database;
class DocumentLoader;
class GraphicsContext;
class InspectorClient;
class JavaScriptCallFrame;
class Node;
class Page;
class ResourceResponse;
class ResourceError;
class SharedBuffer;

struct ConsoleMessage;
struct InspectorDatabaseResource;
struct InspectorResource;
class ResourceRequest;

// TODO(ojan): Webkit's version of this inherits from JavaScriptDebugListener.
// We need to do this once we start adding debugger hooks or when we do the next
// full webkit merge, whichever comes first.
class InspectorController : public RefCounted<InspectorController> {
public:
    int m_bug1228513_inspectorState;

    typedef HashMap<unsigned long, RefPtr<InspectorResource> > ResourcesMap;
    typedef HashMap<RefPtr<Frame>, ResourcesMap*> FrameResourcesMap;
    typedef HashSet<RefPtr<InspectorDatabaseResource> > DatabaseResourcesSet;

    typedef enum {
        CurrentPanel,
        ConsolePanel,
        DatabasesPanel,
        ElementsPanel,
        ProfilesPanel,
        ResourcesPanel,
        ScriptsPanel
    } SpecialPanels;

    InspectorController(Page*, InspectorClient*);
    ~InspectorController();

    void inspectedPageDestroyed();
    void pageDestroyed() { m_page = 0; }

    bool enabled() const;

    Page* inspectedPage() const { return m_inspectedPage; }

    String localizedStringsURL();

    void inspect(Node*);
    void highlight(Node*);
    void hideHighlight();

    void show();
    void showConsole();
    void showTimeline();
    void close();

    bool windowVisible();
    void setWindowVisible(bool visible = true);

#if USE(JSC)
    void addMessageToConsole(MessageSource, MessageLevel, KJS::ExecState*, const KJS::List& arguments, unsigned lineNumber, const String& sourceID);
#elif USE(V8)
    // TODO(ojan): Do we need to implement this version?
#endif
    void addMessageToConsole(MessageSource, MessageLevel, const String& message, unsigned lineNumber, const String& sourceID);

    void attachWindow();
    void detachWindow();

#if USE(JSC)
    JSContextRef scriptContext() const { return m_scriptContext; };
    void setScriptContext(JSContextRef context) { m_scriptContext = context; };
#elif USE(V8)
    void setScriptObject(v8::Handle<v8::Object> newScriptObject);
#endif

    void inspectedWindowScriptObjectCleared(Frame*);
    void windowScriptObjectAvailable();

    void scriptObjectReady();

    void populateScriptObjects();
    void resetScriptObjects();

    void didCommitLoad(DocumentLoader*);
    void frameDetachedFromParent(Frame*);

    void didLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int length);

    void identifierForInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&);
    void willSendRequest(DocumentLoader*, unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse);
    void didReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    void didReceiveContentLength(DocumentLoader*, unsigned long identifier, int lengthReceived);
    void didFinishLoading(DocumentLoader*, unsigned long identifier);
    void didFailLoading(DocumentLoader*, unsigned long identifier, const ResourceError&);
#if USE(JSC)
    void resourceRetrievedByXMLHttpRequest(unsigned long identifier, KJS::UString& sourceString);
#elif USE(V8)
    void resourceRetrievedByXMLHttpRequest(unsigned long identifier, String& sourceString);
#endif

#if ENABLE(DATABASE)
    void didOpenDatabase(Database*, const String& domain, const String& name, const String& version);
#endif

    const ResourcesMap& resources() const { return m_resources; }

    void moveWindowBy(float x, float y) const;

    void startDebuggingAndReloadInspectedPage();
    void stopDebugging();
    bool debuggerAttached() const { return m_debuggerAttached; }

    void drawNodeHighlight(GraphicsContext&) const;


#if USE(V8)
    // InspectorController.idl
    void addSourceToFrame(unsigned long identifier, Node* frame);
    Node* getResourceDocumentNode(unsigned long identifier);
    void highlightDOMNode(Node* node);
    void hideDOMNodeHighlight();
    void loaded();
    void attach();
    void detach();
    // TODO(jackson): search should return an array of JSRanges
    void search(Node* node, const String& query);
    DOMWindow* inspectedWindow();
    String platform() const;
#endif

private:
    void focusNode();

    void addConsoleMessage(ConsoleMessage*);
    void addScriptConsoleMessage(const ConsoleMessage*);

    void addResource(InspectorResource*);
    void removeResource(InspectorResource*);

#if USE(JSC)
    JSObjectRef addScriptResource(InspectorResource*);
#elif USE(V8)
    void addScriptResource(InspectorResource*);
#endif
    void removeScriptResource(InspectorResource*);

#if USE(JSC)
    JSObjectRef addAndUpdateScriptResource(InspectorResource*);
#elif USE(V8)
    void addAndUpdateScriptResource(InspectorResource*);
#endif
    void updateScriptResourceRequest(InspectorResource*);
    void updateScriptResourceResponse(InspectorResource*);
    void updateScriptResource(InspectorResource*, int length);
    void updateScriptResource(InspectorResource*, bool finished, bool failed = false);
    void updateScriptResource(InspectorResource*, double startTime, double responseReceivedTime, double endTime);

    void pruneResources(ResourcesMap*, DocumentLoader* loaderToKeep = 0);
    void removeAllResources(ResourcesMap* map) { pruneResources(map); }

    // Return true if the inspector should track request/response activity.
    // Chrome's policy is to only log requests if the inspector is already open.
    // This reduces the passive bloat from InspectorController: http://b/1113875
    bool trackResources() const { return m_trackResources; }

    // Start/stop resource tracking.
    void enableTrackResources(bool trackResources);

    bool m_trackResources;

    // Helper function to determine when the script object is initialized
#if USE(JSC)
    inline bool hasScriptObject() const { return m_scriptObject; }
#elif USE(V8)
    inline bool hasScriptObject() { return !m_scriptObject.IsEmpty(); }
#endif

#if ENABLE(DATABASE)
#if USE(JSC)
    JSObjectRef addDatabaseScriptResource(InspectorDatabaseResource*);
    void removeDatabaseScriptResource(InspectorDatabaseResource*);
#elif USE(V8)
    // TODO(ojan): implement when we turn on databases.
#endif
#endif

#if USE(JSC)
    JSValueRef callSimpleFunction(JSContextRef, JSObjectRef thisObject, const char* functionName) const;
    JSValueRef callFunction(JSContextRef, JSObjectRef thisObject, const char* functionName, size_t argumentCount, const JSValueRef arguments[], JSValueRef& exception) const;
    bool handleException(JSContextRef, JSValueRef exception, unsigned lineNumber) const;
#endif

    void showWindow();
    void closeWindow();

#if USE(JSC)
    virtual void didParseSource(KJS::ExecState*, const KJS::UString& source, int startingLineNumber, const KJS::UString& sourceURL, int sourceID);
    virtual void failedToParseSource(KJS::ExecState*, const KJS::UString& source, int startingLineNumber, const KJS::UString& sourceURL, int errorLine, const KJS::UString& errorMessage);
    virtual void didEnterCallFrame(KJS::ExecState*, int sourceID, int lineNumber);
    virtual void willExecuteStatement(KJS::ExecState*, int sourceID, int lineNumber);
    virtual void willLeaveCallFrame(KJS::ExecState*, int sourceID, int lineNumber);
    virtual void exceptionWasRaised(KJS::ExecState*, int sourceID, int lineNumber);
#elif USE(V8)
    // TODO(ojan): implement when we start integrating in the debugger.
#endif

    Page* m_inspectedPage;
    InspectorClient* m_client;
    Page* m_page;
    RefPtr<Node> m_nodeToFocus;
    RefPtr<InspectorResource> m_mainResource;
    ResourcesMap m_resources;
    FrameResourcesMap m_frameResources;
    Vector<ConsoleMessage*> m_consoleMessages;
#if ENABLE(DATABASE)
    DatabaseResourcesSet m_databaseResources;
#endif
#if USE(JSC)
    JSObjectRef m_scriptObject;
    JSObjectRef m_controllerScriptObject;
    JSContextRef m_scriptContext;
#elif USE(V8)
    v8::Persistent<v8::Object> m_scriptObject;
#endif
    bool m_windowVisible;
    bool m_debuggerAttached;
    SpecialPanels m_showAfterVisible;
    unsigned long m_nextIdentifier;
    RefPtr<Node> m_highlightedNode;
};

} // namespace WebCore

#elif USE(JSC)
// TODO(ojan): When we merge to webkit trunk, get rid of all the code below.
// This part of this file contains the version of InspectorController.h that
// we are synced to in the 3.1 branch, which we need to keep for the
// KJS build. We unfortunately can't just use a different header file for
// each build because a bunch of different files pull in this header file.

#include "Chrome.h"
#include <wtf/RefCounted.h>
#if USE(JSC)
#include <JavaScriptCore/JSContextRef.h>
#elif USE(V8)
#include <v8.h>
#endif
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>


namespace WebCore {

class Database;
class DocumentLoader;
class GraphicsContext;
class DOMWindow;
class InspectorClient;
class Node;
class ResourceResponse;
class ResourceError;

struct ConsoleMessage;
struct InspectorDatabaseResource;
struct InspectorResource;
class ResourceRequest;

class InspectorController : public RefCounted<InspectorController> {
public:
    
    typedef HashMap<unsigned long, RefPtr<InspectorResource> > ResourcesMap;
    typedef HashMap<RefPtr<Frame>, ResourcesMap*> FrameResourcesMap;
    typedef HashSet<RefPtr<InspectorDatabaseResource> > DatabaseResourcesSet;

    typedef enum {
        CurrentPanel,
        ConsolePanel,
        DatabasesPanel,
        ElementsPanel,
        ProfilesPanel,
        ResourcesPanel,
        ScriptsPanel
    } SpecialPanels;

    InspectorController(Page*, InspectorClient*);
    ~InspectorController();

    void inspectedPageDestroyed();
    void pageDestroyed() { m_page = 0; }
    void inspectedPageDestroyed();

    bool enabled() const;

    Page* inspectedPage() const { return m_inspectedPage; }

    String localizedStringsURL();

    void inspect(Node*);
    void highlight(Node*);
    void hideHighlight();

    void show();
    void showPanel(SpecialPanels);
    void close();

    bool isRecordingUserInitiatedProfile() const { return m_recordingUserInitiatedProfile; }
    void startUserInitiatedProfiling();
    void stopUserInitiatedProfiling();
    void finishedProfiling(PassRefPtr<KJS::Profile>);

    bool windowVisible();
    void setWindowVisible(bool visible = true, bool attached = false);

    void addMessageToConsole(MessageSource, MessageLevel, KJS::ExecState*, const KJS::ArgList& arguments, unsigned lineNumber, const String& sourceID);
    void addMessageToConsole(MessageSource, MessageLevel, const String& message, unsigned lineNumber, const String& sourceID);
    void clearConsoleMessages();
    void toggleRecordButton(bool);

    void addProfile(PassRefPtr<KJS::Profile>, int lineNumber, const KJS::UString& sourceURL);
    void addProfileMessageToConsole(PassRefPtr<KJS::Profile> prpProfile, int lineNumber, const KJS::UString& sourceURL);
    void addScriptProfile(KJS::Profile* profile);
    const Vector<RefPtr<KJS::Profile> >& profiles() const { return m_profiles; }

    void attachWindow();
    void detachWindow();

    void setAttachedWindow(bool);
    void setAttachedWindowHeight(unsigned height);

#if USE(JSC)
    JSContextRef scriptContext() const { return m_scriptContext; };
    void setScriptContext(JSContextRef context) { m_scriptContext = context; };
#endif

    void inspectedWindowScriptObjectCleared(Frame*);
    void windowScriptObjectAvailable();

    void scriptObjectReady();

    void populateScriptObjects();
    void resetScriptObjects();

    void didCommitLoad(DocumentLoader*);
    void frameDetachedFromParent(Frame*);

    void didLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int length);

    void identifierForInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&);
    void willSendRequest(DocumentLoader*, unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse);
    void didReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    void didReceiveContentLength(DocumentLoader*, unsigned long identifier, int lengthReceived);
    void didFinishLoading(DocumentLoader*, unsigned long identifier);
    void didFailLoading(DocumentLoader*, unsigned long identifier, const ResourceError&);
    void resourceRetrievedByXMLHttpRequest(unsigned long identifier, KJS::UString& sourceString);

#if ENABLE(DATABASE)
    void didOpenDatabase(Database*, const String& domain, const String& name, const String& version);
#endif

    const ResourcesMap& resources() const { return m_resources; }

    void moveWindowBy(float x, float y) const;
    void closeWindow();

    void startDebuggingAndReloadInspectedPage();
    void stopDebugging();
    bool debuggerAttached() const { return m_debuggerAttached; }

    JavaScriptCallFrame* currentCallFrame() const;

    void addBreakpoint(int sourceID, unsigned lineNumber);
    void removeBreakpoint(int sourceID, unsigned lineNumber);

    bool pauseOnExceptions();
    void setPauseOnExceptions(bool pause);

    void pauseInDebugger();
    void resumeDebugger();

    void stepOverStatementInDebugger();
    void stepIntoStatementInDebugger();
    void stepOutOfFunctionInDebugger();

    void drawNodeHighlight(GraphicsContext&) const;
    
    void startTiming(const KJS::UString& title);
    bool stopTiming(const KJS::UString& title, double& elapsed);

    void startGroup(MessageSource source, KJS::ExecState* exec, const KJS::ArgList& arguments, unsigned lineNumber, const String& sourceURL);
    void endGroup(MessageSource source, unsigned lineNumber, const String& sourceURL);
#if USE(V8)
    // InspectorController.idl
    void addSourceToFrame(unsigned long identifier, Node* frame);
    Node* getResourceDocumentNode(unsigned long identifier);
    void highlightDOMNode(Node* node);
    void hideDOMNodeHighlight();
    void loaded();
    void attach();
    void detach();
    void log(const String& message);
    // TODO(jackson): search should return an array of JSRanges
    void search(Node* node, const String& query);
    DOMWindow* inspectedWindow();
    String platform() const;
#endif

private:
    void focusNode();

    void addConsoleMessage(ConsoleMessage*);
    void addScriptConsoleMessage(const ConsoleMessage*);

#if USE(V8)
    void setScriptObject(v8::Handle<v8::Object> newScriptObject)
    {
        if (!m_scriptObject.IsEmpty()) {
            m_scriptObject.Dispose();
            m_scriptObject.Clear();
        }

        if (!newScriptObject.IsEmpty())
            m_scriptObject = v8::Persistent<v8::Object>::New(newScriptObject);
    }
#endif

    void addResource(InspectorResource*);
    void removeResource(InspectorResource*);

#if USE(JSC)
    JSObjectRef addScriptResource(InspectorResource*);
#elif USE(V8)
    void addScriptResource(InspectorResource*);
#endif
    void removeScriptResource(InspectorResource*);

#if USE(JSC)
    JSObjectRef addAndUpdateScriptResource(InspectorResource*);
#elif USE(V8)
    void addAndUpdateScriptResource(InspectorResource*);
#endif
    void updateScriptResourceRequest(InspectorResource*);
    void updateScriptResourceResponse(InspectorResource*);
    void updateScriptResourceType(InspectorResource*);
    void updateScriptResource(InspectorResource*, int length);
    void updateScriptResource(InspectorResource*, bool finished, bool failed = false);
    void updateScriptResource(InspectorResource*, double startTime, double responseReceivedTime, double endTime);

    void pruneResources(ResourcesMap*, DocumentLoader* loaderToKeep = 0);
    void removeAllResources(ResourcesMap* map) { pruneResources(map); }

    // Return true if the inspector should track request/response activity.
    // Chrome's policy is to only log requests if the inspector is already open.
    // This reduces the passive bloat from InspectorController: http://b/1113875
    bool trackResources() const { return m_trackResources; }

    // Start/stop resource tracking.
    void enableTrackResources(bool trackResources);

    bool m_trackResources;

#if ENABLE(DATABASE)
    JSObjectRef addDatabaseScriptResource(InspectorDatabaseResource*);
    void removeDatabaseScriptResource(InspectorDatabaseResource*);
#endif

    JSValueRef callSimpleFunction(JSContextRef, JSObjectRef thisObject, const char* functionName) const;
    JSValueRef callFunction(JSContextRef, JSObjectRef thisObject, const char* functionName, size_t argumentCount, const JSValueRef arguments[], JSValueRef& exception) const;

    bool handleException(JSContextRef, JSValueRef exception, unsigned lineNumber) const;

    void showWindow();
    void closeWindow();

    virtual void didParseSource(KJS::ExecState*, const KJS::SourceProvider& source, int startingLineNumber, const KJS::UString& sourceURL, int sourceID);
    virtual void failedToParseSource(KJS::ExecState*, const KJS::SourceProvider& source, int startingLineNumber, const KJS::UString& sourceURL, int errorLine, const KJS::UString& errorMessage);
    virtual void didPause();

    Page* m_inspectedPage;
    InspectorClient* m_client;
    Page* m_page;
    RefPtr<Node> m_nodeToFocus;
    RefPtr<InspectorResource> m_mainResource;
    ResourcesMap m_resources;
    HashSet<String> m_knownResources;
    FrameResourcesMap m_frameResources;
    Vector<ConsoleMessage*> m_consoleMessages;
    Vector<RefPtr<KJS::Profile> > m_profiles;
    HashMap<String, double> m_times;
#if ENABLE(DATABASE)
    DatabaseResourcesSet m_databaseResources;
#endif
#if USE(JSC)
    JSObjectRef m_scriptObject;
    JSObjectRef m_controllerScriptObject;
    JSContextRef m_scriptContext;
#elif USE(V8)
    v8::Persistent<v8::Object> m_scriptObject;
#endif
    bool m_windowVisible;
    bool m_debuggerAttached;
    bool m_attachDebuggerWhenShown;
    bool m_recordingUserInitiatedProfile;
    SpecialPanels m_showAfterVisible;
    // TODO(ojan): Come up with a solution for this that avoids collisions.
    unsigned long m_nextIdentifier;
    RefPtr<Node> m_highlightedNode;
    unsigned m_groupLevel;
};

} // namespace WebCore

#endif

#endif // !defined(InspectorController_h)
