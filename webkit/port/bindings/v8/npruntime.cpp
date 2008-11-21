/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Google, Inc.  All rights reserved.
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

#include <map>
#include <set>
#include <string>
#include <v8.h>

#include "base/string_piece.h"
#include "bindings/npruntime.h"
#include "ChromiumBridge.h"
#include "np_v8object.h"
#include "npruntime_priv.h"
#include "v8_npobject.h"

#include <wtf/Assertions.h>

using namespace v8;


// TODO: Consider removing locks if we're singlethreaded already.
// The static initializer here should work okay, but we want to avoid
// static initialization in general.
//
// Commenting out the locks to avoid dependencies on chrome for now.
// Need a platform abstraction which we can use.
// static Lock StringIdentifierMapLock;

typedef std::map<StringPiece, PrivateIdentifier*> StringIdentifierMap;

static StringIdentifierMap* getStringIdentifierMap() {
    static StringIdentifierMap* stringIdentifierMap = 0;
    if (!stringIdentifierMap)
        stringIdentifierMap = new StringIdentifierMap();
    return stringIdentifierMap;
}

// TODO: Consider removing locks if we're singlethreaded already.
// static Lock IntIdentifierMapLock;

typedef std::map<int, PrivateIdentifier*> IntIdentifierMap;

static IntIdentifierMap* getIntIdentifierMap() {
    static IntIdentifierMap* intIdentifierMap = 0;
    if (!intIdentifierMap)
        intIdentifierMap = new IntIdentifierMap;
    return intIdentifierMap;
}

extern "C" {

NPIdentifier NPN_GetStringIdentifier(const NPUTF8* name) {
    ASSERT(name);

    if (name) {
        // AutoLock safeLock(StringIdentifierMapLock);

        StringIdentifierMap* identMap = getStringIdentifierMap();

        // We use StringPiece here as the key-type to avoid a string copy to
        // construct the map key.
        StringPiece nameStr(name);
        StringIdentifierMap::iterator iter = identMap->find(nameStr);
        if (iter != identMap->end())
            return static_cast<NPIdentifier>(iter->second);

        size_t nameLen = nameStr.length();

        // We never release identifier names, so this dictionary will grow, as
        // will the memory for the identifier name strings.
        PrivateIdentifier* identifier = static_cast<PrivateIdentifier*>(
            malloc(sizeof(PrivateIdentifier) + nameLen + 1));
        memcpy(identifier + 1, name, nameLen + 1);
        identifier->isString = true;
        identifier->value.string = reinterpret_cast<NPUTF8*>(identifier + 1);
        (*identMap)[StringPiece(identifier->value.string, nameLen)] =
            identifier;
        return (NPIdentifier)identifier;
    }

    return 0;
}

void NPN_GetStringIdentifiers(const NPUTF8** names, int32_t nameCount,
                              NPIdentifier* identifiers) {
    ASSERT(names);
    ASSERT(identifiers);

    if (names && identifiers)
        for (int i = 0; i < nameCount; i++)
            identifiers[i] = NPN_GetStringIdentifier(names[i]);
}

NPIdentifier NPN_GetIntIdentifier(int32_t intid) {
    // AutoLock safeLock(IntIdentifierMapLock);

    IntIdentifierMap* identMap = getIntIdentifierMap();

    IntIdentifierMap::iterator iter = identMap->find(intid);
    if (iter != identMap->end())
        return static_cast<NPIdentifier>(iter->second);

    PrivateIdentifier* identifier = reinterpret_cast<PrivateIdentifier*>(
        malloc(sizeof(PrivateIdentifier)));
    // We never release identifier names, so this dictionary will grow.
    identifier->isString = false;
    identifier->value.number = intid;
    (*identMap)[intid] = identifier;
    return (NPIdentifier)identifier;
}

bool NPN_IdentifierIsString(NPIdentifier identifier) {
    PrivateIdentifier* i = reinterpret_cast<PrivateIdentifier*>(identifier);
    return i->isString;
}

NPUTF8 *NPN_UTF8FromIdentifier(NPIdentifier identifier) {
    PrivateIdentifier* i = reinterpret_cast<PrivateIdentifier*>(identifier);
    if (!i->isString || !i->value.string)
        return NULL;

    return (NPUTF8 *)strdup(i->value.string);
}

int32_t NPN_IntFromIdentifier(NPIdentifier identifier) {
    PrivateIdentifier* i = reinterpret_cast<PrivateIdentifier*>(identifier);
    if (i->isString)
        return 0;
    return i->value.number;
}

void NPN_ReleaseVariantValue(NPVariant* variant) {
    ASSERT(variant);

    if (variant->type == NPVariantType_Object) {
        NPN_ReleaseObject(variant->value.objectValue);
        variant->value.objectValue = 0;
    } else if (variant->type == NPVariantType_String) {
        free((void*)variant->value.stringValue.UTF8Characters);
        variant->value.stringValue.UTF8Characters = 0;
        variant->value.stringValue.UTF8Length = 0;
    }

    variant->type = NPVariantType_Void;
}

static const char* kCounterNPObjects = "NPObjects";

NPObject *NPN_CreateObject(NPP npp, NPClass* aClass) {
    ASSERT(aClass);

    if (aClass) {
        NPObject* obj;
        if (aClass->allocate != NULL)
            obj = aClass->allocate(npp, aClass);
        else
            obj = reinterpret_cast<NPObject*>(malloc(sizeof(NPObject)));

        obj->_class = aClass;
        obj->referenceCount = 1;

        WebCore::ChromiumBridge::incrementStatsCounter(kCounterNPObjects);
        return obj;
    }

    return 0;
}

NPObject* NPN_RetainObject(NPObject* obj) {
    ASSERT(obj);
    ASSERT(obj->referenceCount > 0);

    if (obj)
        obj->referenceCount++;

    return obj;
}

// _NPN_DeallocateObject actually deletes the object.  Technically,
// callers should use NPN_ReleaseObject.  Webkit exposes this function
// to kill objects which plugins may not have properly released.
void _NPN_DeallocateObject(NPObject *obj) {
    ASSERT(obj);
    ASSERT(obj->referenceCount >= 0);

    if (obj) {
        WebCore::ChromiumBridge::decrementStatsCounter(kCounterNPObjects);

        // NPObjects that remain in pure C++ may never have wrappers.
        // Hence, if it's not already alive, don't unregister it.
        // If it is alive, unregister it as the *last* thing we do
        // so that it can do as much cleanup as possible on its own.
        if (_NPN_IsAlive(obj))
            _NPN_UnregisterObject(obj);

        obj->referenceCount = -1;
        if (obj->_class->deallocate)
            obj->_class->deallocate(obj);
        else
            free(obj);
    }
}

void NPN_ReleaseObject(NPObject* obj) {
    ASSERT(obj);
    ASSERT(obj->referenceCount >= 1);

    if (obj && obj->referenceCount >= 1) {
        if (--obj->referenceCount == 0)
            _NPN_DeallocateObject(obj);
    }
}

void _NPN_InitializeVariantWithStringCopy(NPVariant* variant,
                                          const NPString* value) {
    variant->type = NPVariantType_String;
    variant->value.stringValue.UTF8Length = value->UTF8Length;
    variant->value.stringValue.UTF8Characters =
        reinterpret_cast<NPUTF8*>(malloc(sizeof(NPUTF8) * value->UTF8Length));
    memcpy((void*)variant->value.stringValue.UTF8Characters,
           value->UTF8Characters,
           sizeof(NPUTF8) * value->UTF8Length);
}


// NPN_Registry
//
// The registry is designed for quick lookup of NPObjects.
// JS needs to be able to quickly lookup a given NPObject to determine
// if it is alive or not.
// The browser needs to be able to quickly lookup all NPObjects which are
// "owned" by an object.
//
// The g_live_objects is a hash table of all live objects to their owner
// objects.  Presence in this table is used primarily to determine if
// objects are live or not.
//
// The g_root_objects is a hash table of root objects to a set of
// objects that should be deactivated in sync with the root.  A
// root is defined as a top-level owner object.  This is used on
// Frame teardown to deactivate all objects associated
// with a particular plugin.

typedef std::set<NPObject*> NPObjectSet;
typedef std::map<NPObject*, NPObject*> NPObjectMap;
typedef std::map<NPObject*, NPObjectSet*> NPRootObjectMap;

// A map of live NPObjects with pointers to their Roots.
NPObjectMap g_live_objects;

// A map of the root objects and the list of NPObjects
// associated with that object.
NPRootObjectMap g_root_objects;

void _NPN_RegisterObject(NPObject* obj, NPObject* owner) {
    ASSERT(obj);

    // Check if already registered.
    if (g_live_objects.find(obj) != g_live_objects.end()) {
      return;
    }

    if (!owner) {
      // Registering a new owner object.
      ASSERT(g_root_objects.find(obj) == g_root_objects.end());
      g_root_objects[obj] = new NPObjectSet();
    } else {
      // Always associate this object with it's top-most parent.
      // Since we always flatten, we only have to look up one level.
      NPObjectMap::iterator owner_entry = g_live_objects.find(owner);
      NPObject* parent = NULL;
      if (g_live_objects.end() != owner_entry)
          parent = owner_entry->second;

      if (parent) {
        owner = parent;
      }
      ASSERT(g_root_objects.find(obj) == g_root_objects.end());
      if (g_root_objects.find(owner) != g_root_objects.end())
        (g_root_objects[owner])->insert(obj);
    }

    ASSERT(g_live_objects.find(obj) == g_live_objects.end());
    g_live_objects[obj] = owner;
}

void _NPN_UnregisterObject(NPObject* obj) {
    ASSERT(obj);
    ASSERT(g_live_objects.find(obj) != g_live_objects.end());

    NPObject* owner = NULL;
    if (g_live_objects.find(obj) != g_live_objects.end())
      owner = g_live_objects.find(obj)->second;

    if (owner == NULL) {
        // Unregistering a owner object; also unregister it's descendants.
        ASSERT(g_root_objects.find(obj) != g_root_objects.end());
        NPObjectSet* set = g_root_objects[obj];
        while (set->size() > 0) {
#ifndef NDEBUG
            size_t size = set->size();
#endif
            NPObject* sub_object = *(set->begin());
            // The sub-object should not be a owner!
            ASSERT(g_root_objects.find(sub_object) == g_root_objects.end());

            // First, unregister the object.
            set->erase(sub_object);
            g_live_objects.erase(sub_object);

            // Remove the JS references to the object.
            ForgetV8ObjectForNPObject(sub_object);

            ASSERT(set->size() < size);
        }
        delete set;
        g_root_objects.erase(obj);
    } else {
        NPRootObjectMap::iterator owner_entry = g_root_objects.find(owner);
        if (owner_entry != g_root_objects.end()) {
          NPObjectSet* list = owner_entry->second;
          ASSERT(list->find(obj) != list->end());
          list->erase(obj);
        }
    }
    ForgetV8ObjectForNPObject(obj);

    g_live_objects.erase(obj);
}

bool _NPN_IsAlive(NPObject* obj) {
    return g_live_objects.find(obj) != g_live_objects.end();
}

}  // extern "C"
