/*
 IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. ("Apple") in
 consideration of your agreement to the following terms, and your use, installation,
 modification or redistribution of this Apple software constitutes acceptance of these
 terms.  If you do not agree with these terms, please do not use, install, modify or
 redistribute this Apple software.

 In consideration of your agreement to abide by the following terms, and subject to these
 terms, Apple grants you a personal, non-exclusive license, under Apple’s copyrights in
 this original Apple software (the "Apple Software"), to use, reproduce, modify and
 redistribute the Apple Software, with or without modifications, in source and/or binary
 forms; provided that if you redistribute the Apple Software in its entirety and without
 modifications, you must retain this notice and the following text and disclaimers in all
 such redistributions of the Apple Software.  Neither the name, trademarks, service marks
 or logos of Apple Computer, Inc. may be used to endorse or promote products derived from
 the Apple Software without specific prior written permission from Apple. Except as expressly
 stated in this notice, no other rights or licenses, express or implied, are granted by Apple
 herein, including but not limited to any patent rights that may be infringed by your
 derivative works or by other works in which the Apple Software may be incorporated.

 The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES,
 EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT,
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS
 USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.

 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
          OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE,
 REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND
 WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR
 OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <wtf/Platform.h>
#include "PluginObject.h"

#ifdef WIN32
#define strcasecmp _stricmp
#define NPAPI WINAPI
#else
#define NPAPI
#endif

#if defined(OS_LINUX)
#include <X11/Xlib.h>
#endif

static void log(NPP instance, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    char message[2048] = "PLUGIN: ";
    vsprintf(message + strlen(message), format, args);
    va_end(args);

    NPObject* windowObject = 0;
    NPError error = browser->getvalue(instance, NPNVWindowNPObject, &windowObject);
    if (error != NPERR_NO_ERROR) {
        fprintf(stderr, "Failed to retrieve window object while logging: %s\n", message);
        return;
    }

    NPVariant consoleVariant;
    if (!browser->getproperty(instance, windowObject, browser->getstringidentifier("console"), &consoleVariant)) {
        fprintf(stderr, "Failed to retrieve console object while logging: %s\n", message);
        browser->releaseobject(windowObject);
        return;
    }

    NPObject* consoleObject = NPVARIANT_TO_OBJECT(consoleVariant);

    NPVariant messageVariant;
    STRINGZ_TO_NPVARIANT(message, messageVariant);

    NPVariant result;
    if (!browser->invoke(instance, consoleObject, browser->getstringidentifier("log"), &messageVariant, 1, &result)) {
        fprintf(stderr, "Failed to invoke console.log while logging: %s\n", message);
        browser->releaseobject(consoleObject);
        browser->releaseobject(windowObject);
        return;
    }

    browser->releasevariantvalue(&result);
    browser->releaseobject(consoleObject);
    browser->releaseobject(windowObject);
}

// Plugin entry points
extern "C" {
    NPError NPAPI NP_Initialize(NPNetscapeFuncs *browserFuncs
#if defined(OS_LINUX)
                                , NPPluginFuncs *pluginFuncs
#endif
                                );
    NPError NPAPI NP_GetEntryPoints(NPPluginFuncs *pluginFuncs);
    void NPAPI NP_Shutdown(void);

#if defined(OS_LINUX)
    NPError NP_GetValue(NPP instance, NPPVariable variable, void *value);
    const char* NP_GetMIMEDescription(void);
#endif
}

// Plugin entry points
NPError NPAPI NP_Initialize(NPNetscapeFuncs *browserFuncs
#if defined(OS_LINUX)
                            , NPPluginFuncs *pluginFuncs
#endif
)
{
    browser = browserFuncs;
#if defined(OS_LINUX)
    return NP_GetEntryPoints(pluginFuncs);
#else
    return NPERR_NO_ERROR;
#endif
}

NPError NPAPI NP_GetEntryPoints(NPPluginFuncs *pluginFuncs)
{
    pluginFuncs->version = 11;
    pluginFuncs->size = sizeof(pluginFuncs);
    pluginFuncs->newp = NPP_New;
    pluginFuncs->destroy = NPP_Destroy;
    pluginFuncs->setwindow = NPP_SetWindow;
    pluginFuncs->newstream = NPP_NewStream;
    pluginFuncs->destroystream = NPP_DestroyStream;
    pluginFuncs->asfile = NPP_StreamAsFile;
    pluginFuncs->writeready = NPP_WriteReady;
    pluginFuncs->write = (NPP_WriteProcPtr)NPP_Write;
    pluginFuncs->print = NPP_Print;
    pluginFuncs->event = NPP_HandleEvent;
    pluginFuncs->urlnotify = NPP_URLNotify;
    pluginFuncs->getvalue = NPP_GetValue;
    pluginFuncs->setvalue = NPP_SetValue;

    return NPERR_NO_ERROR;
}

void NPAPI NP_Shutdown(void)
{
}

NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char *argn[], char *argv[], NPSavedData *saved)
{
    if (browser->version >= 14) {
        PluginObject* obj = (PluginObject*)browser->createobject(instance, getPluginClass());

        for (int i = 0; i < argc; i++) {
            if (strcasecmp(argn[i], "onstreamload") == 0 && !obj->onStreamLoad)
                obj->onStreamLoad = strdup(argv[i]);
            else if (strcasecmp(argn[i], "onStreamDestroy") == 0 && !obj->onStreamDestroy)
                obj->onStreamDestroy = strdup(argv[i]);
            else if (strcasecmp(argn[i], "onURLNotify") == 0 && !obj->onURLNotify)
                obj->onURLNotify = strdup(argv[i]);
            else if (strcasecmp(argn[i], "logfirstsetwindow") == 0)
                obj->logSetWindow = TRUE;
            else if (strcasecmp(argn[i], "logSrc") == 0) {
                for (int i = 0; i < argc; i++) {
                    if (strcasecmp(argn[i], "src") == 0) {
                        log(instance, "src: %s", argv[i]);
                        fflush(stdout);
                    }
                }
            }
        }

        instance->pdata = obj;
    }

    // On Windows and Unix, plugins only get events if they are windowless.
    return browser->setvalue(instance, NPPVpluginWindowBool, NULL);
}

NPError NPP_Destroy(NPP instance, NPSavedData **save)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
    if (obj) {
        if (obj->onStreamLoad)
            free(obj->onStreamLoad);

        if (obj->onURLNotify)
            free(obj->onURLNotify);

        if (obj->onStreamDestroy)
            free(obj->onStreamDestroy);

        if (obj->logDestroy)
            log(instance, "NPP_Destroy");

        browser->releaseobject(&obj->header);
    }

    fflush(stdout);

    return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow *window)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);

    if (obj) {
        if (obj->logSetWindow) {
            log(instance, "NPP_SetWindow: %d %d", (int)window->width, (int)window->height);
            fflush(stdout);
            obj->logSetWindow = false;
        }
    }

    return NPERR_NO_ERROR;
}

static void executeScript(const PluginObject* obj, const char* script)
{
    NPObject *windowScriptObject;
    browser->getvalue(obj->npp, NPNVWindowNPObject, &windowScriptObject);

    NPString npScript;
    npScript.UTF8Characters = script;
    npScript.UTF8Length = strlen(script);

    NPVariant browserResult;
    browser->evaluate(obj->npp, windowScriptObject, &npScript, &browserResult);
    browser->releasevariantvalue(&browserResult);
}

NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream *stream, NPBool seekable, uint16 *stype)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);

    if (obj->returnErrorFromNewStream)
        return NPERR_GENERIC_ERROR;

    obj->stream = stream;
    *stype = NP_ASFILEONLY;

    if (browser->version >= NPVERS_HAS_RESPONSE_HEADERS)
        notifyStream(obj, stream->url, stream->headers);

    if (obj->onStreamLoad)
        executeScript(obj, obj->onStreamLoad);

    return NPERR_NO_ERROR;
}

NPError NPP_DestroyStream(NPP instance, NPStream *stream, NPReason reason)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);

    if (obj->onStreamDestroy)
        executeScript(obj, obj->onStreamDestroy);

    return NPERR_NO_ERROR;
}

int32 NPP_WriteReady(NPP instance, NPStream *stream)
{
    return 0;
}

int32 NPP_Write(NPP instance, NPStream *stream, int32 offset, int32 len, void *buffer)
{
    return 0;
}

void NPP_StreamAsFile(NPP instance, NPStream *stream, const char *fname)
{
}

void NPP_Print(NPP instance, NPPrint *platformPrint)
{
}

int16 NPP_HandleEvent(NPP instance, void *event)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
    if (!obj->eventLogging)
        return 0;

#ifdef WIN32
    // Below is the event handling code.  Per the NPAPI spec, the events don't
    // map directly between operating systems:
    // http://devedge-temp.mozilla.org/library/manuals/2002/plugin/1.0/structures5.html#1000000
    NPEvent* evt = static_cast<NPEvent*>(event);
    short x = static_cast<short>(evt->lParam & 0xffff);
    short y = static_cast<short>(evt->lParam >> 16);
    switch (evt->event) {
        case WM_PAINT:
            log(instance, "updateEvt");
            break;
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
            log(instance, "mouseDown at (%d, %d)", x, y);
            break;
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
            log(instance, "mouseUp at (%d, %d)", x, y);
            break;
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
            break;
        case WM_MOUSEMOVE:
            log(instance, "adjustCursorEvent");
            break;
        case WM_KEYUP:
            // TODO(tc): We need to convert evt->wParam from virtual-key code
            // to key code.
            log(instance, "NOTIMPLEMENTED: keyUp '%c'", ' ');
            break;
        case WM_KEYDOWN:
            // TODO(tc): We need to convert evt->wParam from virtual-key code
            // to key code.
            log(instance, "NOTIMPLEMENTED: keyDown '%c'", ' ');
            break;
        case WM_SETCURSOR:
            break;
        case WM_SETFOCUS:
            log(instance, "getFocusEvent");
            break;
        case WM_KILLFOCUS:
            log(instance, "loseFocusEvent");
            break;
        default:
            log(instance, "event %d", evt->event);
    }

    fflush(stdout);

#elif defined(OS_LINUX)
    XEvent* evt = static_cast<XEvent*>(event);
    XButtonPressedEvent* bpress_evt = reinterpret_cast<XButtonPressedEvent*>(evt);
    XButtonReleasedEvent* brelease_evt = reinterpret_cast<XButtonReleasedEvent*>(evt);
    switch (evt->type) {
        case ButtonPress:
            log(instance, "mouseDown at (%d, %d)", bpress_evt->x, bpress_evt->y);
            break;
        case ButtonRelease:
            log(instance, "mouseUp at (%d, %d)", brelease_evt->x, brelease_evt->y);
            break;
        case KeyPress:
            // TODO: extract key code
            log(instance, "NOTIMPLEMENTED: keyDown '%c'", ' ');
            break;
        case KeyRelease:
            // TODO: extract key code
            log(instance, "NOTIMPLEMENTED: keyUp '%c'", ' ');
            break;
        case GraphicsExpose:
            log(instance, "updateEvt");
            break;
        // NPAPI events
        case FocusIn:
            log(instance, "getFocusEvent");
            break;
        case FocusOut:
            log(instance, "loseFocusEvent");
            break;
        case EnterNotify:
        case LeaveNotify:
        case MotionNotify:
            log(instance, "adjustCursorEvent");
            break;
        default:
            log(instance, "event %d", evt->type);
    }

    fflush(stdout);
#else

#ifdef MAC_EVENT_CODE_DISABLED_DUE_TO_ERRORS
// This code apparently never built on Mac, but Mac was previously
// using the Linux branch.  It doesn't quite build.
// warning: 'GlobalToLocal' is deprecated (declared at
// .../Frameworks/QD.framework/Headers/QuickdrawAPI.h:2181)
    EventRecord* evt = static_cast<EventRecord*>(event);
    Point pt = { evt->where.v, evt->where.h };
    switch (evt->what) {
        case nullEvent:
            // these are delivered non-deterministically, don't log.
            break;
        case mouseDown:
            GlobalToLocal(&pt);
            log(instance, "mouseDown at (%d, %d)", pt.h, pt.v);
            break;
        case mouseUp:
            GlobalToLocal(&pt);
            log(instance, "mouseUp at (%d, %d)", pt.h, pt.v);
            break;
        case keyDown:
            log(instance, "keyDown '%c'", (char)(evt->message & 0xFF));
            break;
        case keyUp:
            log(instance, "keyUp '%c'", (char)(evt->message & 0xFF));
            break;
        case autoKey:
            log(instance, "autoKey '%c'", (char)(evt->message & 0xFF));
            break;
        case updateEvt:
            log(instance, "updateEvt");
            break;
        case diskEvt:
            log(instance, "diskEvt");
            break;
        case activateEvt:
            log(instance, "activateEvt");
            break;
        case osEvt:
            switch ((evt->message & 0xFF000000) >> 24) {
                case suspendResumeMessage:
                    log(instance, "osEvt - %s", (evt->message & 0x1) ? "resume" : "suspend");
                    break;
                case mouseMovedMessage:
                    log(instance, "osEvt - mouseMoved");
                    break;
                default:
                    log(instance, "osEvt - %08lX", evt->message);
            }
            break;
        case kHighLevelEvent:
            log(instance, "kHighLevelEvent");
            break;
        // NPAPI events
        case getFocusEvent:
            log(instance, "getFocusEvent");
            break;
        case loseFocusEvent:
            log(instance, "loseFocusEvent");
            break;
        case adjustCursorEvent:
            log(instance, "adjustCursorEvent");
            break;
        default:
            log(instance, "event %d", evt->what);
    }
#endif  // MAC_EVENT_CODE_DISABLED_DUE_TO_ERRORS

#endif

    return 0;
}

void NPP_URLNotify(NPP instance, const char *url, NPReason reason, void *notifyData)
{
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
    if (obj->onURLNotify)
        executeScript(obj, obj->onURLNotify);

    handleCallback(obj, url, reason, notifyData);
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value)
{
    NPError err = NPERR_NO_ERROR;

    switch (variable) {
#if defined(OS_LINUX)
        case NPPVpluginNameString:
            *((const char **)value) = "WebKit Test PlugIn";
            break;
        case NPPVpluginDescriptionString:
            *((const char **)value) = "Simple Netscape plug-in that handles test content for WebKit";
            break;
        case NPPVpluginNeedsXEmbed:
            *((NPBool *)value) = TRUE;
            break;
#endif
        case NPPVpluginScriptableNPObject: {
            void **v = (void **)value;
            PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
            // Return value is expected to be retained
            browser->retainobject((NPObject *)obj);
            *v = obj;
            break;
        }
        default:
            fprintf(stderr, "Unhandled variable to NPP_GetValue\n");
            err = NPERR_GENERIC_ERROR;
            break;
    }

    return err;
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value)
{
    return NPERR_GENERIC_ERROR;
}

#if defined(OS_LINUX)
NPError NP_GetValue(NPP instance, NPPVariable variable, void *value)
{
    return NPP_GetValue(instance, variable, value);
}

const char* NP_GetMIMEDescription(void) {
    // The layout test LayoutTests/fast/js/navigator-mimeTypes-length.html
    // asserts that the number of mimetypes handled by plugins should be
    // greater than the number of plugins.  This isn't true if we're
    // the only plugin and we only handle one mimetype, so specify
    // multiple mimetypes here.
    return "application/x-webkit-test-netscape:testnetscape:test netscape content;"
           "application/x-webkit-test-netscape2:testnetscape2:test netscape content2";
}
#endif
