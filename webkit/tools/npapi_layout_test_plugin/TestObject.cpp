/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#include "TestObject.h"
#include "PluginObject.h"

#include <stdlib.h>

static bool testEnumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count);
static bool testInvokeDefault(NPObject *obj, const NPVariant *args, uint32_t argCount, NPVariant *result);
static bool testHasProperty(NPObject *obj, NPIdentifier name);
static bool testGetProperty(NPObject *obj, NPIdentifier name, NPVariant *variant);
static NPObject *testAllocate(NPP npp, NPClass *theClass);
static void testDeallocate(NPObject *obj);

static NPClass testClass = {
    NP_CLASS_STRUCT_VERSION,
    testAllocate,
    testDeallocate,
    0,
    0,
    0,
    testInvokeDefault,
    testHasProperty,
    testGetProperty,
    0,
    0,
    testEnumerate
};

NPClass *getTestClass(void)
{
    return &testClass;
}

int testObjectCount = 0;

int getTestObjectCount(void) {
  return testObjectCount;
}

static bool identifiersInitialized = false;

#define NUM_TEST_IDENTIFIERS      4
#define ID_PROPERTY_FOO           0
#define ID_PROPERTY_BAR           1
#define ID_PROPERTY_TEST_OBJECT   2
#define ID_PROPERTY_REF_COUNT     3

static NPIdentifier testIdentifiers[NUM_TEST_IDENTIFIERS];
static const NPUTF8 *testIdentifierNames[NUM_TEST_IDENTIFIERS] = {
    "foo",
    "bar",
    "testObject",
    "refCount",
};

static void initializeIdentifiers(void)
{
    browser->getstringidentifiers(testIdentifierNames, NUM_TEST_IDENTIFIERS, testIdentifiers);
}

static NPObject *testAllocate(NPP npp, NPClass *theClass)
{
    TestObject *newInstance =
        static_cast<TestObject*>(malloc(sizeof(TestObject)));
    newInstance->testObject = NULL;
    ++testObjectCount;

    if (!identifiersInitialized) {
        identifiersInitialized = true;
        initializeIdentifiers();
    }

    return reinterpret_cast<NPObject*>(newInstance);
}

static void testDeallocate(NPObject *obj)
{
    TestObject *testObject = reinterpret_cast<TestObject*>(obj);
    if (testObject->testObject)
      browser->releaseobject(testObject->testObject);
    --testObjectCount;
    free(obj);
}

static bool testInvokeDefault(NPObject *obj, const NPVariant *args,
                              uint32_t argCount, NPVariant *result)
{
    INT32_TO_NPVARIANT(2, *result);
    return true;
}

static bool testHasProperty(NPObject *obj, NPIdentifier name)
{
    for (unsigned i = 0; i < NUM_TEST_IDENTIFIERS; i++) {
        if (testIdentifiers[i] == name)
            return true;
    }

    return false;
}

static bool testGetProperty(NPObject *obj, NPIdentifier name,
                            NPVariant *variant)
{
    if (name == testIdentifiers[ID_PROPERTY_FOO]) {
        char* mem = static_cast<char*>(browser->memalloc(4));
        strcpy(mem, "foo");
        STRINGZ_TO_NPVARIANT(mem, *variant);
        return true;
    } else if (name == testIdentifiers[ID_PROPERTY_BAR]) {
        BOOLEAN_TO_NPVARIANT(true, *variant);
        return true;
    } else if (name == testIdentifiers[ID_PROPERTY_TEST_OBJECT]) {
        TestObject* testObject = reinterpret_cast<TestObject*>(obj);
        if (testObject->testObject == NULL)
          testObject->testObject = browser->createobject(NULL, &testClass);
        browser->retainobject(testObject->testObject);
        OBJECT_TO_NPVARIANT(testObject->testObject, *variant);
        return true;
    } else if (name == testIdentifiers[ID_PROPERTY_REF_COUNT]) {
        INT32_TO_NPVARIANT(obj->referenceCount, *variant);
        return true;
    }
    return false;
}

static bool testEnumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count)
{
    *count = NUM_TEST_IDENTIFIERS;

    *value = (NPIdentifier*)browser->memalloc(NUM_TEST_IDENTIFIERS * sizeof(NPIdentifier));
    memcpy(*value, testIdentifiers, sizeof(NPIdentifier) * NUM_TEST_IDENTIFIERS);

    return true;
}


