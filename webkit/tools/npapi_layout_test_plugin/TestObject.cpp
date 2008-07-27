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


