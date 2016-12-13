/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "PluginObject.h"

#include "TestObject.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef WIN32
#define snprintf sprintf_s
#endif

static void pluginInvalidate(NPObject*);
static bool pluginHasProperty(NPObject*, NPIdentifier name);
static bool pluginHasMethod(NPObject*, NPIdentifier name);
static bool pluginGetProperty(NPObject*, NPIdentifier name, NPVariant*);
static bool pluginSetProperty(NPObject*, NPIdentifier name, const NPVariant*);
static bool pluginInvoke(NPObject*, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool pluginInvokeDefault(NPObject*, const NPVariant* args, uint32_t argCount, NPVariant* result);
static NPObject* pluginAllocate(NPP npp, NPClass*);
static void pluginDeallocate(NPObject*);

NPNetscapeFuncs* browser;

static NPClass pluginClass = {
    NP_CLASS_STRUCT_VERSION,
    pluginAllocate,
    pluginDeallocate,
    pluginInvalidate,
    pluginHasMethod,
    pluginInvoke,
    pluginInvokeDefault,
    pluginHasProperty,
    pluginGetProperty,
    pluginSetProperty,
};

NPClass *getPluginClass(void)
{
    return &pluginClass;
}

static bool identifiersInitialized = false;

#define ID_PROPERTY_PROPERTY                    0
#define ID_PROPERTY_EVENT_LOGGING               1
#define ID_PROPERTY_HAS_STREAM                  2
#define ID_PROPERTY_TEST_OBJECT                 3
#define ID_PROPERTY_LOG_DESTROY                 4
#define ID_PROPERTY_RETURN_ERROR_FROM_NEWSTREAM 5
#define ID_PROPERTY_TEST_OBJECT_COUNT           6
#define NUM_PROPERTY_IDENTIFIERS                7

static NPIdentifier pluginPropertyIdentifiers[NUM_PROPERTY_IDENTIFIERS];
static const NPUTF8 *pluginPropertyIdentifierNames[NUM_PROPERTY_IDENTIFIERS] = {
    "property",
    "eventLoggingEnabled",
    "hasStream",
    "testObject",
    "logDestroy",
    "returnErrorFromNewStream",
    "testObjectCount",
};

#define ID_TEST_CALLBACK_METHOD     0
#define ID_TEST_GETURL              1
#define ID_REMOVE_DEFAULT_METHOD    2
#define ID_TEST_DOM_ACCESS          3
#define ID_TEST_GET_URL_NOTIFY      4
#define ID_TEST_INVOKE_DEFAULT      5
#define ID_DESTROY_STREAM           6
#define ID_TEST_ENUMERATE           7
#define ID_TEST_GETINTIDENTIFIER    8
#define ID_TEST_GET_PROPERTY        9
#define ID_TEST_EVALUATE            10
#define ID_TEST_GET_PROPERTY_RETURN_VALUE 11
#define ID_TEST_CALLBACK_METHOD_RET 12
#define ID_TEST_CREATE_TEST_OBJECT  13
#define ID_TEST_PASS_TEST_OBJECT    14
#define ID_TEST_CLONE_OBJECT        15
#define ID_TEST_SCRIPT_OBJECT_INVOKE 16
#define ID_TEST_IDENTIFIER_TO_STRING 17
#define ID_TEST_IDENTIFIER_TO_INT   18
#define ID_TEST_POSTURL_FILE        19
#define ID_TEST_CALLBACK_AND_GET_VALUE 20
#define ID_TEST_CONSTRUCT           21
#define ID_DESTROY_NULL_STREAM      22
#define NUM_METHOD_IDENTIFIERS      23

static NPIdentifier pluginMethodIdentifiers[NUM_METHOD_IDENTIFIERS];
static const NPUTF8 *pluginMethodIdentifierNames[NUM_METHOD_IDENTIFIERS] = {
    "testCallback",
    "getURL",
    "removeDefaultMethod",
    "testDOMAccess",
    "getURLNotify",
    "testInvokeDefault",
    "destroyStream",
    "testEnumerate",
    "testGetIntIdentifier",
    "testGetProperty",
    "testEvaluate",
    "testGetPropertyReturnValue",
    "testCallbackRet",       // Chrome bug 897451
    "testCreateTestObject",  // Chrome bug 1093606
    "testPassTestObject",    // Chrome bug 1093606
    "testCloneObject",
    "testScriptObjectInvoke", // Chrome bug 1175346
    "testIdentifierToString",
    "testIdentifierToInt",
    "testPostURLFile",
    // Chrome bug http://code.google.com/p/chromium/issues/detail?id=4270
    "testCallbackAndGetValue",
    "testConstruct",
    "destroyNullStream",
};

static NPUTF8* createCStringFromNPVariant(const NPVariant* variant)
{
    size_t length = NPVARIANT_TO_STRING(*variant).UTF8Length;
    NPUTF8* result = (NPUTF8*)malloc(length + 1);
    memcpy(result, NPVARIANT_TO_STRING(*variant).UTF8Characters, length);
    result[length] = '\0';
    return result;
}

static void initializeIdentifiers(void)
{
    browser->getstringidentifiers(pluginPropertyIdentifierNames, NUM_PROPERTY_IDENTIFIERS, pluginPropertyIdentifiers);
    browser->getstringidentifiers(pluginMethodIdentifierNames, NUM_METHOD_IDENTIFIERS, pluginMethodIdentifiers);
}

static bool pluginHasProperty(NPObject *obj, NPIdentifier name)
{
    for (int i = 0; i < NUM_PROPERTY_IDENTIFIERS; i++)
        if (name == pluginPropertyIdentifiers[i])
            return true;
    return false;
}

static bool pluginHasMethod(NPObject *obj, NPIdentifier name)
{
    for (int i = 0; i < NUM_METHOD_IDENTIFIERS; i++)
        if (name == pluginMethodIdentifiers[i])
            return true;
    return false;
}

static bool pluginGetProperty(NPObject* obj, NPIdentifier name, NPVariant* result)
{
    PluginObject* plugin = reinterpret_cast<PluginObject*>(obj);
    if (name == pluginPropertyIdentifiers[ID_PROPERTY_PROPERTY]) {
        STRINGZ_TO_NPVARIANT("property", *result);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_EVENT_LOGGING]) {
        BOOLEAN_TO_NPVARIANT(plugin->eventLogging, *result);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_LOG_DESTROY]) {
        BOOLEAN_TO_NPVARIANT(plugin->logDestroy, *result);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_HAS_STREAM]) {
        BOOLEAN_TO_NPVARIANT(plugin->stream != 0, *result);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_TEST_OBJECT]) {
        NPObject* testObject = plugin->testObject;
        browser->retainobject(testObject);
        OBJECT_TO_NPVARIANT(testObject, *result);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_RETURN_ERROR_FROM_NEWSTREAM]) {
        BOOLEAN_TO_NPVARIANT(plugin->returnErrorFromNewStream, *result);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_TEST_OBJECT_COUNT]) {
        INT32_TO_NPVARIANT(getTestObjectCount(), *result);
        return true;
    }
    return false;
}

static bool pluginSetProperty(NPObject* obj, NPIdentifier name, const NPVariant* variant)
{
    PluginObject* plugin = reinterpret_cast<PluginObject*>(obj);
    if (name == pluginPropertyIdentifiers[ID_PROPERTY_EVENT_LOGGING]) {
        plugin->eventLogging = NPVARIANT_TO_BOOLEAN(*variant);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_LOG_DESTROY]) {
        plugin->logDestroy = NPVARIANT_TO_BOOLEAN(*variant);
        return true;
    } else if (name == pluginPropertyIdentifiers[ID_PROPERTY_RETURN_ERROR_FROM_NEWSTREAM]) {
        plugin->returnErrorFromNewStream = NPVARIANT_TO_BOOLEAN(*variant);
        return true;
    }

    return false;
}

static bool testDOMAccess(PluginObject* obj, const NPVariant*, uint32_t, NPVariant* result)
{
    // Get plug-in's DOM element
    NPObject* elementObject;
    if (browser->getvalue(obj->npp, NPNVPluginElementNPObject, &elementObject) == NPERR_NO_ERROR) {
        // Get style
        NPVariant styleVariant;
        NPIdentifier styleIdentifier = browser->getstringidentifier("style");
        if (browser->getproperty(obj->npp, elementObject, styleIdentifier, &styleVariant) && NPVARIANT_IS_OBJECT(styleVariant)) {
            // Set style.border
            NPIdentifier borderIdentifier = browser->getstringidentifier("border");
            NPVariant borderVariant;
            STRINGZ_TO_NPVARIANT("3px solid red", borderVariant);
            browser->setproperty(obj->npp, NPVARIANT_TO_OBJECT(styleVariant), borderIdentifier, &borderVariant);
            browser->releasevariantvalue(&styleVariant);
        }

        browser->releaseobject(elementObject);
    }
    VOID_TO_NPVARIANT(*result);
    return true;
}

static NPIdentifier stringVariantToIdentifier(NPVariant variant)
{
    assert(NPVARIANT_IS_STRING(variant));
    NPUTF8* utf8String = createCStringFromNPVariant(&variant);
    NPIdentifier identifier = browser->getstringidentifier(utf8String);
    free(utf8String);
    return identifier;
}

static NPIdentifier int32VariantToIdentifier(NPVariant variant)
{
    assert(NPVARIANT_IS_INT32(variant));
    int32 integer = NPVARIANT_TO_INT32(variant);
    return browser->getintidentifier(integer);
}

static NPIdentifier doubleVariantToIdentifier(NPVariant variant)
{
    assert(NPVARIANT_IS_DOUBLE(variant));
    double value = NPVARIANT_TO_DOUBLE(variant);
    // Sadly there is no "getdoubleidentifier"
    int32 integer = static_cast<int32>(value);
    return browser->getintidentifier(integer);
}

static NPIdentifier variantToIdentifier(NPVariant variant)
{
    if (NPVARIANT_IS_STRING(variant))
        return stringVariantToIdentifier(variant);
    else if (NPVARIANT_IS_INT32(variant))
        return int32VariantToIdentifier(variant);
    else if (NPVARIANT_IS_DOUBLE(variant))
        return doubleVariantToIdentifier(variant);
    return 0;
}

static bool testIdentifierToString(PluginObject*, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount != 1)
        return false;
    NPIdentifier identifier = variantToIdentifier(args[0]);
    if (!identifier)
        return false;
    NPUTF8* utf8String = browser->utf8fromidentifier(identifier);
    if (!utf8String)
        return false;
    STRINGZ_TO_NPVARIANT(utf8String, *result);
    return true;
}

static bool testIdentifierToInt(PluginObject*, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount != 1)
        return false;
    NPIdentifier identifier = variantToIdentifier(args[0]);
    if (!identifier)
        return false;
    int32 integer = browser->intfromidentifier(identifier);
    INT32_TO_NPVARIANT(integer, *result);
    return true;
}

static bool testCallback(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount == 0 || !NPVARIANT_IS_STRING(args[0]))
        return false;

    NPObject* windowScriptObject;
    browser->getvalue(obj->npp, NPNVWindowNPObject, &windowScriptObject);

    NPUTF8* callbackString = createCStringFromNPVariant(&args[0]);
    NPIdentifier callbackIdentifier = browser->getstringidentifier(callbackString);
    free(callbackString);

    NPVariant browserResult;
    browser->invoke(obj->npp, windowScriptObject, callbackIdentifier, 0, 0, &browserResult);
    browser->releasevariantvalue(&browserResult);

    VOID_TO_NPVARIANT(*result);
    return true;
}

static bool testCallbackAndGetValue(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    NPP npp = obj->npp;
    if (!testCallback(obj, args, argCount, result))
        return false;

    NPObject *global;
    browser->getvalue(npp, NPNVWindowNPObject, &global);

    VOID_TO_NPVARIANT(*result);
    return true;
}

static bool getURL(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount == 2 && NPVARIANT_IS_STRING(args[0]) && NPVARIANT_IS_STRING(args[1])) {
        NPUTF8* urlString = createCStringFromNPVariant(&args[0]);
        NPUTF8* targetString = createCStringFromNPVariant(&args[1]);
        NPError npErr = browser->geturl(obj->npp, urlString, targetString);
        free(urlString);
        free(targetString);

        INT32_TO_NPVARIANT(npErr, *result);
        return true;
    } else if (argCount == 1 && NPVARIANT_IS_STRING(args[0])) {
        NPUTF8* urlString = createCStringFromNPVariant(&args[0]);
        NPError npErr = browser->geturl(obj->npp, urlString, 0);
        free(urlString);

        INT32_TO_NPVARIANT(npErr, *result);
        return true;
    }
    return false;
}

static bool removeDefaultMethod(PluginObject*, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    pluginClass.invokeDefault = 0;
    VOID_TO_NPVARIANT(*result);
    return true;
}

static bool getURLNotify(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount != 3 || !NPVARIANT_IS_STRING(args[0])
        || (!NPVARIANT_IS_STRING(args[1]) && !NPVARIANT_IS_NULL(args[1]))
        || !NPVARIANT_IS_STRING(args[2]))
        return false;

    NPUTF8* urlString = createCStringFromNPVariant(&args[0]);
    NPUTF8* targetString = (NPVARIANT_IS_STRING(args[1]) ? createCStringFromNPVariant(&args[1]) : NULL);
    NPUTF8* callbackString = createCStringFromNPVariant(&args[2]);

    NPIdentifier callbackIdentifier = browser->getstringidentifier(callbackString);
    browser->geturlnotify(obj->npp, urlString, targetString, callbackIdentifier);

    free(urlString);
    free(targetString);
    free(callbackString);

    VOID_TO_NPVARIANT(*result);
    return true;
}

static bool testInvokeDefault(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (!NPVARIANT_IS_OBJECT(args[0]))
        return false;

    NPObject *callback = NPVARIANT_TO_OBJECT(args[0]);

    NPVariant invokeArgs[1];
    NPVariant browserResult;

    STRINGZ_TO_NPVARIANT("test", invokeArgs[0]);
    bool retval = browser->invokeDefault(obj->npp, callback, invokeArgs, 1, &browserResult);

    if (retval)
        browser->releasevariantvalue(&browserResult);

    BOOLEAN_TO_NPVARIANT(retval, *result);
    return true;
}

static bool destroyStream(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    NPError npError = browser->destroystream(obj->npp, obj->stream, NPRES_USER_BREAK);
    INT32_TO_NPVARIANT(npError, *result);
    return true;
}

static bool destroyNullStream(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    NPError npError = browser->destroystream(obj->npp, 0, NPRES_USER_BREAK);
    INT32_TO_NPVARIANT(npError, *result);
    return true;
}

static bool testEnumerate(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount == 2 && NPVARIANT_IS_OBJECT(args[0]) && NPVARIANT_IS_OBJECT(args[1])) {
        uint32_t count;
        NPIdentifier* identifiers;

        if (browser->enumerate(obj->npp, NPVARIANT_TO_OBJECT(args[0]), &identifiers, &count)) {
            NPObject* outArray = NPVARIANT_TO_OBJECT(args[1]);
            NPIdentifier pushIdentifier = browser->getstringidentifier("push");

            for (uint32_t i = 0; i < count; i++) {
                NPUTF8* string = browser->utf8fromidentifier(identifiers[i]);

                if (!string)
                    continue;

                NPVariant args[1];
                STRINGZ_TO_NPVARIANT(string, args[0]);
                NPVariant browserResult;
                browser->invoke(obj->npp, outArray, pushIdentifier, args, 1, &browserResult);
                browser->releasevariantvalue(&browserResult);
                browser->memfree(string);
            }

            browser->memfree(identifiers);
        }

        VOID_TO_NPVARIANT(*result);
        return true;
    }
    return false;
}

static bool testGetIntIdentifier(PluginObject*, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount != 1)
        return false;

    NPIdentifier identifier;

    if (NPVARIANT_IS_DOUBLE(args[0])) {
        identifier = browser->getintidentifier((int)NPVARIANT_TO_DOUBLE(args[0]));
        INT32_TO_NPVARIANT((int32)identifier, *result);
        return true;
    } else if (NPVARIANT_IS_INT32(args[0])) {
        identifier = browser->getintidentifier((int)NPVARIANT_TO_INT32(args[0]));
        INT32_TO_NPVARIANT((int32)identifier, *result);
        return true;
    }
    return false;
}

static bool testGetProperty(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount == 0)
        return false;

    NPObject *object;
    browser->getvalue(obj->npp, NPNVWindowNPObject, &object);

    for (uint32_t i = 0; i < argCount; i++) {
        assert(NPVARIANT_IS_STRING(args[i]));
        NPUTF8* propertyString = createCStringFromNPVariant(&args[i]);
        NPIdentifier propertyIdentifier = browser->getstringidentifier(propertyString);
        free(propertyString);

        NPVariant variant;
        bool retval = browser->getproperty(obj->npp, object, propertyIdentifier, &variant);
        browser->releaseobject(object);

        if (!retval)
            break;

        if (i + 1 < argCount) {
            assert(NPVARIANT_IS_OBJECT(variant));
            object = NPVARIANT_TO_OBJECT(variant);
        } else {
            *result = variant;
            return true;
        }
    }

    VOID_TO_NPVARIANT(*result);
    return false;
}

static bool testEvaluate(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount != 1 || !NPVARIANT_IS_STRING(args[0]))
        return false;
    NPObject* windowScriptObject;
    browser->getvalue(obj->npp, NPNVWindowNPObject, &windowScriptObject);

    NPString s = NPVARIANT_TO_STRING(args[0]);

    bool retval = browser->evaluate(obj->npp, windowScriptObject, &s, result);
    browser->releaseobject(windowScriptObject);
    return retval;
}

static bool testGetPropertyReturnValue(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount != 2 || !NPVARIANT_IS_OBJECT(args[0]) || !NPVARIANT_IS_STRING(args[1]))
        return false;

    NPUTF8* propertyString = createCStringFromNPVariant(&args[1]);
    NPIdentifier propertyIdentifier = browser->getstringidentifier(propertyString);
    free(propertyString);

    NPVariant variant;
    bool retval = browser->getproperty(obj->npp, NPVARIANT_TO_OBJECT(args[0]), propertyIdentifier, &variant);
    if (retval)
        browser->releasevariantvalue(&variant);

    BOOLEAN_TO_NPVARIANT(retval, *result);
    return true;
}

static char* toCString(const NPString& string)
{
    char* result = static_cast<char*>(malloc(string.UTF8Length + 1));
    memcpy(result, string.UTF8Characters, string.UTF8Length);
    result[string.UTF8Length] = '\0';

    return result;
}

static bool testPostURLFile(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (argCount != 4 || !NPVARIANT_IS_STRING(args[0]) || !NPVARIANT_IS_STRING(args[1]) || !NPVARIANT_IS_STRING(args[2]) || !NPVARIANT_IS_STRING(args[3]))
        return false;

    NPString urlString = NPVARIANT_TO_STRING(args[0]);
    char* url = toCString(urlString);

    NPString targetString = NPVARIANT_TO_STRING(args[1]);
    char* target = toCString(targetString);

    NPString pathString = NPVARIANT_TO_STRING(args[2]);
    char* path = toCString(pathString);

    NPString contentsString = NPVARIANT_TO_STRING(args[3]);

    FILE* tempFile = fopen(path, "w");
    if (!tempFile)
        return false;

    int written = fwrite(contentsString.UTF8Characters, contentsString.UTF8Length, 1, tempFile);
    fclose(tempFile);
    if (written != 1)
        return false;

    NPError error = browser->posturl(obj->npp, url, target, pathString.UTF8Length, path, TRUE);

    free(path);
    free(target);
    free(url);

    BOOLEAN_TO_NPVARIANT(error == NPERR_NO_ERROR, *result);
    return true;
}

static bool testConstruct(PluginObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (!argCount || !NPVARIANT_IS_OBJECT(args[0]))
        return false;

    return browser->construct(obj->npp, NPVARIANT_TO_OBJECT(args[0]), args + 1, argCount - 1, result);
}

static bool pluginInvoke(NPObject* header, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    PluginObject* plugin = reinterpret_cast<PluginObject*>(header);
    if (name == pluginMethodIdentifiers[ID_TEST_CALLBACK_METHOD])
        return testCallback(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_GETURL])
        return getURL(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_REMOVE_DEFAULT_METHOD])
        return removeDefaultMethod(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_DOM_ACCESS])
        return testDOMAccess(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_GET_URL_NOTIFY])
        return getURLNotify(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_INVOKE_DEFAULT])
        return testInvokeDefault(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_ENUMERATE])
        return testEnumerate(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_DESTROY_STREAM])
        return destroyStream(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_GETINTIDENTIFIER])
        return testGetIntIdentifier(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_EVALUATE])
        return testEvaluate(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_GET_PROPERTY])
        return testGetProperty(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_GET_PROPERTY_RETURN_VALUE])
        return testGetPropertyReturnValue(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_IDENTIFIER_TO_STRING])
        return testIdentifierToString(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_IDENTIFIER_TO_INT])
        return testIdentifierToInt(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_POSTURL_FILE])
        return testPostURLFile(plugin, args, argCount, result);
    else if (name == pluginMethodIdentifiers[ID_TEST_CALLBACK_METHOD_RET]) {
        // call whatever method name we're given, and pass it the 'window' obj.
        // we expect the function to return its argument.
        if (argCount > 0 && NPVARIANT_IS_STRING(args[0])) {
            NPObject *windowScriptObject;
            browser->getvalue(plugin->npp, NPNVWindowNPObject, &windowScriptObject);

            NPUTF8* callbackString = createCStringFromNPVariant(&args[0]);
            NPIdentifier callbackIdentifier = browser->getstringidentifier(callbackString);
            free(callbackString);

            NPVariant callbackArgs[1];
            OBJECT_TO_NPVARIANT(windowScriptObject, callbackArgs[0]);

            NPVariant browserResult;
            browser->invoke(plugin->npp, windowScriptObject, callbackIdentifier,
                            callbackArgs, 1, &browserResult);

            if (NPVARIANT_IS_OBJECT(browserResult)) {
                // Now return the callbacks return value back to our caller.
                // BUG 897451: This should be the same as the
                // windowScriptObject, but its not (in Chrome) - or at least, it
                // has a different refcount. This means Chrome will delete the
                // object before returning it and the calling JS gets a garbage
                // value.  Firefox handles it fine.
                OBJECT_TO_NPVARIANT(NPVARIANT_TO_OBJECT(browserResult), *result);
            } else {
                browser->releasevariantvalue(&browserResult);
                VOID_TO_NPVARIANT(*result);
            }

            return true;
        }
    } else if (name == pluginMethodIdentifiers[ID_TEST_CREATE_TEST_OBJECT]) {
        NPObject *testObject = browser->createobject(plugin->npp, getTestClass());
        assert(testObject->referenceCount == 1);
        OBJECT_TO_NPVARIANT(testObject, *result);
        return true;
    } else if (name == pluginMethodIdentifiers[ID_TEST_PASS_TEST_OBJECT]) {
        // call whatever method name we're given, and pass it our second
        // argument.
        if (argCount > 1 && NPVARIANT_IS_STRING(args[0])) {
            NPObject *windowScriptObject;
            browser->getvalue(plugin->npp, NPNVWindowNPObject, &windowScriptObject);

            NPUTF8* callbackString = createCStringFromNPVariant(&args[0]);
            NPIdentifier callbackIdentifier = browser->getstringidentifier(callbackString);
            free(callbackString);

            NPVariant browserResult;
            browser->invoke(plugin->npp, windowScriptObject, callbackIdentifier, &args[1], 1, &browserResult);
            browser->releasevariantvalue(&browserResult);

            VOID_TO_NPVARIANT(*result);
            return true;
        }
    } else if (name == pluginMethodIdentifiers[ID_TEST_CLONE_OBJECT]) {
        // Create another instance of the same class
        NPObject *new_object = browser->createobject(plugin->npp, &pluginClass);
        assert(new_object->referenceCount == 1);
        OBJECT_TO_NPVARIANT(new_object, *result);
        return true;
    } else if (name == pluginMethodIdentifiers[ID_TEST_SCRIPT_OBJECT_INVOKE]) {
        if (argCount > 1 && NPVARIANT_IS_STRING(args[0])) {
            // Invoke a script callback to get a script NPObject. Then call
            // a method on the script NPObject passing it a freshly created
            // NPObject.
            // Arguments:
            // arg1:  Callback that returns a script object.
            // arg2:  Name of the method to call on the script object returned
            //        from the callback
            NPObject *windowScriptObject;
            browser->getvalue(plugin->npp, NPNVWindowNPObject,
                              &windowScriptObject);

            // Arg1 is the name of the callback
            NPUTF8* callbackString = createCStringFromNPVariant(&args[0]);
            NPIdentifier callbackIdentifier =
                  browser->getstringidentifier(callbackString);
            free(callbackString);

            // Invoke a callback that returns a script object
            NPVariant object_result;
            browser->invoke(plugin->npp, windowScriptObject, callbackIdentifier,
                            &args[1], 1, &object_result);

            // Script object returned
            NPObject *script_object = object_result.value.objectValue;

            // Arg2 is the name of the method to be called on the script object
            NPUTF8* object_mehod_string = createCStringFromNPVariant(&args[1]);
            NPIdentifier object_method =
                browser->getstringidentifier(object_mehod_string);
            free(object_mehod_string);

            // Create a fresh NPObject to be passed as an argument
            NPObject *object_arg = browser->createobject(plugin->npp, &pluginClass);
            NPVariant invoke_args[1];
            OBJECT_TO_NPVARIANT(object_arg, invoke_args[0]);

            // Invoke the script method
            NPVariant object_method_result;
            browser->invoke(plugin->npp, script_object, object_method,
                            invoke_args, 1, &object_method_result);

            browser->releasevariantvalue(&object_result);
            VOID_TO_NPVARIANT(*result);
            if (NPVARIANT_IS_OBJECT(object_method_result)) {
                // Now return the callbacks return value back to our caller.
                // BUG 897451: This should be the same as the
                // windowScriptObject, but its not (in Chrome) - or at least, it
                // has a different refcount. This means Chrome will delete the
                // object before returning it and the calling JS gets a garbage
                // value.  Firefox handles it fine.
                OBJECT_TO_NPVARIANT(NPVARIANT_TO_OBJECT(object_method_result),
                                    *result);
            } else {
                browser->releasevariantvalue(&object_method_result);
                VOID_TO_NPVARIANT(*result);
            }
            return true;
        }
    } else if (name == pluginMethodIdentifiers[ID_TEST_CALLBACK_AND_GET_VALUE]) {
          return testCallbackAndGetValue(plugin, args, argCount, result);
    } else if (name == pluginMethodIdentifiers[ID_TEST_CONSTRUCT]) {
          return testConstruct(plugin, args, argCount, result);
    } else if (name == pluginMethodIdentifiers[ID_DESTROY_NULL_STREAM]) 
          return destroyNullStream(plugin, args, argCount, result);
    
    return false;
}

static bool pluginInvokeDefault(NPObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    INT32_TO_NPVARIANT(1, *result);
    return true;
}

static void pluginInvalidate(NPObject* obj)
{
}

static NPObject *pluginAllocate(NPP npp, NPClass *theClass)
{
    PluginObject* newInstance = (PluginObject*)malloc(sizeof(PluginObject));

    if (!identifiersInitialized) {
        identifiersInitialized = true;
        initializeIdentifiers();
    }

    newInstance->npp = npp;
    newInstance->testObject = browser->createobject(npp, getTestClass());
    newInstance->eventLogging = FALSE;
    newInstance->onStreamLoad = 0;
    newInstance->onStreamDestroy = 0;
    newInstance->onURLNotify = 0;
    newInstance->logDestroy = FALSE;
    newInstance->logSetWindow = FALSE;
    newInstance->returnErrorFromNewStream = FALSE;
    newInstance->stream = 0;

    newInstance->firstUrl = NULL;
    newInstance->firstHeaders = NULL;
    newInstance->lastUrl = NULL;
    newInstance->lastHeaders = NULL;

    return (NPObject*)newInstance;
}

static void pluginDeallocate(NPObject* header)
{
    PluginObject* plugin = reinterpret_cast<PluginObject*>(header);
    browser->releaseobject(plugin->testObject);

    free(plugin->firstUrl);
    free(plugin->firstHeaders);
    free(plugin->lastUrl);
    free(plugin->lastHeaders);
    free(plugin);
}

void handleCallback(PluginObject* object, const char *url, NPReason reason, void *notifyData)
{
    assert(object);

    NPVariant args[2];

    NPObject *windowScriptObject;
    browser->getvalue(object->npp, NPNVWindowNPObject, &windowScriptObject);

    NPIdentifier callbackIdentifier = notifyData;

    INT32_TO_NPVARIANT(reason, args[0]);

    char *strHdr = NULL;
    if (object->firstUrl && object->firstHeaders && object->lastUrl && object->lastHeaders) {
        // Format expected by JavaScript validator: four fields separated by \n\n:
        // First URL; first header block; last URL; last header block.
        // Note that header blocks already end with \n due to how NPStream::headers works.
        int len = strlen(object->firstUrl) + 2
            + strlen(object->firstHeaders) + 1
            + strlen(object->lastUrl) + 2
            + strlen(object->lastHeaders) + 1;
        strHdr = (char*)malloc(len + 1);
        snprintf(strHdr, len + 1, "%s\n\n%s\n%s\n\n%s\n",
                 object->firstUrl, object->firstHeaders, object->lastUrl, object->lastHeaders);
        STRINGN_TO_NPVARIANT(strHdr, len, args[1]);
    } else
        NULL_TO_NPVARIANT(args[1]);

    NPVariant browserResult;
    browser->invoke(object->npp, windowScriptObject, callbackIdentifier, args, 2, &browserResult);
    browser->releasevariantvalue(&browserResult);

    free(strHdr);
}

void notifyStream(PluginObject* object, const char *url, const char *headers)
{
    if (object->firstUrl == NULL) {
        if (url)
            object->firstUrl = strdup(url);
        if (headers)
            object->firstHeaders = strdup(headers);
    } else {
        free(object->lastUrl);
        free(object->lastHeaders);
        object->lastUrl = (url ? strdup(url) : NULL);
        object->lastHeaders = (headers ? strdup(headers) : NULL);
    }
}
