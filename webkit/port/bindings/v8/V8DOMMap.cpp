/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <v8.h>

#include "DOMObjectsInclude.h"
#include "V8DOMMap.h"

#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/Threading.h>
#include <wtf/ThreadSpecific.h>

namespace WebCore {

// DOM binding algorithm:
//
// There are two kinds of DOM objects:
// 1. DOM tree nodes, such as Document, HTMLElement, ...
//    there classes implements TreeShared<T> interface;
// 2. Non-node DOM objects, such as CSSRule, Location, etc.
//    these classes implement a ref-counted scheme.
//
// A DOM object may have a JS wrapper object. If a tree node
// is alive, its JS wrapper must be kept alive even it is not
// reachable from JS roots.
// However, JS wrappers of non-node objects can go away if
// not reachable from other JS objects. It works like a cache.
//
// DOM objects are ref-counted, and JS objects are traced from
// a set of root objects. They can create a cycle. To break
// cycles, we do following:
//   Handles from DOM objects to JS wrappers are always weak,
// so JS wrappers of non-node object cannot create a cycle.
//   Before starting a global GC, we create a virtual connection
// between nodes in the same tree in the JS heap. If the wrapper
// of one node in a tree is alive, wrappers of all nodes in
// the same tree are considered alive. This is done by creating
// object groups in GC prologue callbacks. The mark-compact
// collector will remove these groups after each GC.
//
// DOM objects should be deref-ed from the owning thread, not the GC thread
// that does not own them. In V8, GC can kick in from any thread. To ensure
// that DOM objects are always deref-ed from the owning thread when running
// V8 in multi-threading environment, we do following:
// 1. Maintain a thread specific DOM wrapper map for each object map.
//    (We're using TLS support from WTF instead of base since V8Bindings
//     does not depend on base. We further assume that all child threads
//     running V8 instances are created by WTF and thus a destructor will
//     be called to clean up all thread specific data.)
// 2. When GC happens:
//    2.1. If the dead object is in GC thread's map, remove the JS reference
//         and deref the DOM object.
//    2.2. Otherwise, go through all thread maps to find the owning thread.
//         Remove the JS reference from the owning thread's map and move the
//         DOM object to a delayed queue. Post a task to the owning thread
//         to have it deref-ed from the owning thread at later time.
// 3. When a thread is tearing down, invoke a cleanup routine to go through
//    all objects in the delayed queue and the thread map and deref all of
//    them.

static void weakDOMObjectCallback(v8::Persistent<v8::Value> obj, void* param);
static void weakNodeCallback(v8::Persistent<v8::Value> obj, void* param);

#if ENABLE(SVG)
static void weakSVGElementInstanceCallback(v8::Persistent<v8::Value> obj, void* param);

// SVG non-node elements may have a reference to a context node which should be notified when the element is change
static void weakSVGObjectWithContextCallback(v8::Persistent<v8::Value> obj, void* domObj);
#endif

// This is to ensure that we will deref DOM objects from the owning thread, not the GC thread.
// The helper function will be scheduled by the GC thread to get called from the owning thread.
static void derefDelayedObjectsInCurrentThread(void*);

// This should be called to remove all DOM objects associated with the current thread when it is tearing down.
static void removeAllDOMObjectsInCurrentThread();

// A map from a thread ID to thread's specific data.
class ThreadSpecificDOMData;
typedef WTF::HashMap<WTF::ThreadIdentifier, ThreadSpecificDOMData*> DOMThreadMap;
static DOMThreadMap domThreadMap;

// Mutex to protect against concurrent access of domThreadMap.
static WTF::Mutex domThreadMapMutex;

class ThreadSpecificDOMData {
public:
    enum DOMWrapperMapType
    {
        DOMNodeMap,
        DOMObjectMap,
        ActiveDOMObjectMap,
#if ENABLE(SVG)
        DOMSVGElementInstanceMap,
        DOMSVGObjectWithContextMap
#endif
    };

    typedef WTF::HashMap<void*, V8ClassIndex::V8WrapperType> DelayedObjectMap;

    template <class KeyType>
    class InternalDOMWrapperMap : public DOMWrapperMap<KeyType> {
    public:
        InternalDOMWrapperMap(v8::WeakReferenceCallback callback)
            : DOMWrapperMap<KeyType>(callback) { }

        virtual void forget(KeyType* obj);

        void forgetOnly(KeyType* obj)
        {
            DOMWrapperMap<KeyType>::forget(obj);
        }
    };

    ThreadSpecificDOMData()
        : m_domNodeMap(new InternalDOMWrapperMap<Node>(&weakNodeCallback))
        , m_domObjectMap(new InternalDOMWrapperMap<void>(weakDOMObjectCallback))
        , m_activeDomObjectMap(new InternalDOMWrapperMap<void>(weakActiveDOMObjectCallback))
#if ENABLE(SVG)
        , m_domSvgElementInstanceMap(new InternalDOMWrapperMap<SVGElementInstance>(weakSVGElementInstanceCallback))
        , m_domSvgObjectWithContextMap(new InternalDOMWrapperMap<void>(weakSVGObjectWithContextCallback))
#endif
        , m_delayedProcessingScheduled(false)
        , m_isMainThread(WTF::isMainThread())
    {
        WTF::MutexLocker locker(domThreadMapMutex);
        domThreadMap.set(WTF::currentThread(), this);
    }

    // This is called when WTF thread is tearing down.
    // We assume that all child threads running V8 instances are created by WTF.
    ~ThreadSpecificDOMData()
    {
        removeAllDOMObjectsInCurrentThread();

        delete m_domNodeMap;
        delete m_domObjectMap;
        delete m_activeDomObjectMap;
#if ENABLE(SVG)
        delete m_domSvgElementInstanceMap;
        delete m_domSvgObjectWithContextMap;
#endif

        WTF::MutexLocker locker(domThreadMapMutex);
        domThreadMap.remove(WTF::currentThread());
    }

    void* getDOMWrapperMap(DOMWrapperMapType type)
    {
        switch (type) {
            case DOMNodeMap:
                return m_domNodeMap;
            case DOMObjectMap:
                return m_domObjectMap;
            case ActiveDOMObjectMap:
                return m_activeDomObjectMap;
#if ENABLE(SVG)
            case DOMSVGElementInstanceMap:
                return m_domSvgElementInstanceMap;
            case DOMSVGObjectWithContextMap:
                return m_domSvgObjectWithContextMap;
#endif
            default:
                ASSERT(false);
                return NULL;
        }
    }

    InternalDOMWrapperMap<Node>& domNodeMap() { return *m_domNodeMap; }
    InternalDOMWrapperMap<void>& domObjectMap() { return *m_domObjectMap; }
    InternalDOMWrapperMap<void>& activeDomObjectMap() { return *m_activeDomObjectMap; }
#if ENABLE(SVG)
    InternalDOMWrapperMap<SVGElementInstance>& domSvgElementInstanceMap() { return *m_domSvgElementInstanceMap; }
    InternalDOMWrapperMap<void>& domSvgObjectWithContextMap() { return *m_domSvgObjectWithContextMap; }
#endif

    DelayedObjectMap& delayedObjectMap() { return m_delayedObjectMap; }
    bool delayedProcessingScheduled() const { return m_delayedProcessingScheduled; }
    void setDelayedProcessingScheduled(bool value) { m_delayedProcessingScheduled = value; }
    bool isMainThread() const { return m_isMainThread; }

private:
    InternalDOMWrapperMap<Node>* m_domNodeMap;
    InternalDOMWrapperMap<void>* m_domObjectMap;
    InternalDOMWrapperMap<void>* m_activeDomObjectMap;
#if ENABLE(SVG)
    InternalDOMWrapperMap<SVGElementInstance>* m_domSvgElementInstanceMap;
    InternalDOMWrapperMap<void>* m_domSvgObjectWithContextMap;
#endif

    // Stores all the DOM objects that are delayed to be processed when the owning thread gains control.
    DelayedObjectMap m_delayedObjectMap;

    // The flag to indicate if the task to do the delayed process has already been posted.
    bool m_delayedProcessingScheduled;

    bool m_isMainThread;
};

static WTF::ThreadSpecific<ThreadSpecificDOMData> threadSpecificDOMData;

template<typename T>
static void handleWeakObjectInOwningThread(ThreadSpecificDOMData::DOMWrapperMapType mapType, V8ClassIndex::V8WrapperType objType, T* obj);

template <class KeyType>
void ThreadSpecificDOMData::InternalDOMWrapperMap<KeyType>::forget(KeyType* obj)
{
    DOMWrapperMap<KeyType>::forget(obj);

    ThreadSpecificDOMData::DelayedObjectMap& delayedObjectMap = (*threadSpecificDOMData).delayedObjectMap();
    delayedObjectMap.take(obj);
}

DOMWrapperMap<Node>& getDOMNodeMap()
{
    return (*threadSpecificDOMData).domNodeMap();
}

DOMWrapperMap<void>& getDOMObjectMap()
{
    return (*threadSpecificDOMData).domObjectMap();
}

DOMWrapperMap<void>& getActiveDOMObjectMap()
{
    return (*threadSpecificDOMData).activeDomObjectMap();
}

#if ENABLE(SVG)
DOMWrapperMap<SVGElementInstance>& getDOMSVGElementInstanceMap()
{
    return (*threadSpecificDOMData).domSvgElementInstanceMap();
}

static void weakSVGElementInstanceCallback(v8::Persistent<v8::Value> obj, void* param)
{
    SVGElementInstance* instance = static_cast<SVGElementInstance*>(param);

    ThreadSpecificDOMData::InternalDOMWrapperMap<SVGElementInstance>& map = static_cast<ThreadSpecificDOMData::InternalDOMWrapperMap<SVGElementInstance>&>(getDOMSVGElementInstanceMap());
    if (map.contains(instance)) {
        instance->deref();
        map.forgetOnly(instance);
    } else {
        handleWeakObjectInOwningThread(ThreadSpecificDOMData::DOMSVGElementInstanceMap, V8ClassIndex::SVGELEMENTINSTANCE, instance);
    }
}

// Map of SVG objects with contexts to V8 objects
DOMWrapperMap<void>& getDOMSVGObjectWithContextMap()
{
    return (*threadSpecificDOMData).domSvgObjectWithContextMap();
}

static void weakSVGObjectWithContextCallback(v8::Persistent<v8::Value> obj, void* domObj)
{
    v8::HandleScope scope;
    ASSERT(obj->IsObject());

    V8ClassIndex::V8WrapperType type = V8Proxy::GetDOMWrapperType(v8::Handle<v8::Object>::Cast(obj));

    ThreadSpecificDOMData::InternalDOMWrapperMap<void>& map = static_cast<ThreadSpecificDOMData::InternalDOMWrapperMap<void>&>(getDOMSVGObjectWithContextMap());
    if (map.contains(domObj)) {
        // Forget function removes object from the map and dispose the wrapper.
        map.forgetOnly(domObj);

        switch (type) {
#define MakeCase(TYPE, NAME)     \
            case V8ClassIndex::TYPE: static_cast<NAME*>(domObj)->deref(); break;
            SVG_OBJECT_TYPES(MakeCase)
#undef MakeCase
#define MakeCase(TYPE, NAME)     \
            case V8ClassIndex::TYPE:    \
                static_cast<V8SVGPODTypeWrapper<NAME>*>(domObj)->deref(); break;
            SVG_POD_NATIVE_TYPES(MakeCase)
#undef MakeCase
            default:
                ASSERT(false);
        }
    } else {
        handleWeakObjectInOwningThread(ThreadSpecificDOMData::DOMSVGObjectWithContextMap, type, domObj);
    }
}
#endif

// Called when the dead object is not in GC thread's map. Go through all thread maps to find the one containing it.
// Then clear the JS reference and push the DOM object into the delayed queue for it to be deref-ed at later time from the owning thread.
// * This is called when the GC thread is not the owning thread.
// * This can be called on any thread that has GC running.
// * Only one V8 instance is running at a time due to V8::Locker. So we don't need to worry about concurrency.
template<typename T>
static void handleWeakObjectInOwningThread(ThreadSpecificDOMData::DOMWrapperMapType mapType, V8ClassIndex::V8WrapperType objType, T* obj)
{
    WTF::MutexLocker locker(domThreadMapMutex);
    for (typename DOMThreadMap::iterator iter(domThreadMap.begin()); iter != domThreadMap.end(); ++iter) {
        WTF::ThreadIdentifier threadID = iter->first;
        ThreadSpecificDOMData* threadData = iter->second;

        // Skip the current thread that is GC thread.
        if (threadID == WTF::currentThread()) {
            ASSERT(!static_cast<DOMWrapperMap<T>*>(threadData->getDOMWrapperMap(mapType))->contains(obj));
            continue;
        }

        ThreadSpecificDOMData::InternalDOMWrapperMap<T>* domMap = static_cast<ThreadSpecificDOMData::InternalDOMWrapperMap<T>*>(threadData->getDOMWrapperMap(mapType));
        if (domMap->contains(obj)) {
            // Clear the JS reference.
            domMap->forgetOnly(obj);

            // Push into the delayed queue.
            threadData->delayedObjectMap().set(obj, objType);

            // Post a task to the owning thread in order to process the delayed queue.
            // FIXME(jianli): For now, we can only post to main thread due to WTF task posting limitation. We will fix this when we work on nested worker.
            if (!threadData->delayedProcessingScheduled()) {
                threadData->setDelayedProcessingScheduled(true);
                if (threadData->isMainThread())
                    WTF::callOnMainThread(&derefDelayedObjectsInCurrentThread, NULL);
            }

            break;
        }
    }
}

// Called when obj is near death (not reachable from JS roots).
// It is time to remove the entry from the table and dispose the handle.
static void weakDOMObjectCallback(v8::Persistent<v8::Value> obj, void* domObj)
{
    v8::HandleScope scope;
    ASSERT(obj->IsObject());

    V8ClassIndex::V8WrapperType type = V8Proxy::GetDOMWrapperType(v8::Handle<v8::Object>::Cast(obj));

    ThreadSpecificDOMData::InternalDOMWrapperMap<void>& map = static_cast<ThreadSpecificDOMData::InternalDOMWrapperMap<void>&>(getDOMObjectMap());
    if (map.contains(domObj)) {
        // Forget function removes object from the map and dispose the wrapper.
        map.forgetOnly(domObj);

        switch (type) {
#define MakeCase(TYPE, NAME)   \
            case V8ClassIndex::TYPE: static_cast<NAME*>(domObj)->deref(); break;
            DOM_OBJECT_TYPES(MakeCase)
#undef MakeCase
          default:
            ASSERT(false);
        }
    } else {
        handleWeakObjectInOwningThread(ThreadSpecificDOMData::DOMObjectMap, type, domObj);
    }
}

void weakActiveDOMObjectCallback(v8::Persistent<v8::Value> obj, void* domObj)
{
    v8::HandleScope scope;
    ASSERT(obj->IsObject());

    V8ClassIndex::V8WrapperType type = V8Proxy::GetDOMWrapperType(v8::Handle<v8::Object>::Cast(obj));

    ThreadSpecificDOMData::InternalDOMWrapperMap<void>& map = static_cast<ThreadSpecificDOMData::InternalDOMWrapperMap<void>&>(getActiveDOMObjectMap());
    if (map.contains(domObj)) {
        // Forget function removes object from the map and dispose the wrapper.
        map.forgetOnly(domObj);

        switch (type) {
#define MakeCase(TYPE, NAME)   \
            case V8ClassIndex::TYPE: static_cast<NAME*>(domObj)->deref(); break;
            ACTIVE_DOM_OBJECT_TYPES(MakeCase)
#undef MakeCase
            default:
              ASSERT(false);
        }
    } else {
        handleWeakObjectInOwningThread(ThreadSpecificDOMData::ActiveDOMObjectMap, type, domObj);
    }
}

static void weakNodeCallback(v8::Persistent<v8::Value> obj, void* param)
{
    Node* node = static_cast<Node*>(param);

    ThreadSpecificDOMData::InternalDOMWrapperMap<Node>& map = static_cast<ThreadSpecificDOMData::InternalDOMWrapperMap<Node>&>(getDOMNodeMap());
    if (map.contains(node)) {
        map.forgetOnly(node);
        node->deref();
    } else {
        handleWeakObjectInOwningThread<Node>(ThreadSpecificDOMData::DOMNodeMap, V8ClassIndex::NODE, node);
    }
}

static void derefObject(V8ClassIndex::V8WrapperType type, void* domObj)
{
    switch (type) {
        case V8ClassIndex::NODE:
            static_cast<Node*>(domObj)->deref();
            break;

#define MakeCase(TYPE, NAME)   \
        case V8ClassIndex::TYPE: static_cast<NAME*>(domObj)->deref(); break;
        DOM_OBJECT_TYPES(MakeCase)   // This includes both active and non-active.
#undef MakeCase

#if ENABLE(SVG)
#define MakeCase(TYPE, NAME)     \
        case V8ClassIndex::TYPE: static_cast<NAME*>(domObj)->deref(); break;
        SVG_OBJECT_TYPES(MakeCase)   // This also includes SVGElementInstance.
#undef MakeCase

#define MakeCase(TYPE, NAME)     \
        case V8ClassIndex::TYPE:    \
            static_cast<V8SVGPODTypeWrapper<NAME>*>(domObj)->deref(); break;
        SVG_POD_NATIVE_TYPES(MakeCase)
#undef MakeCase
#endif

        default:
            ASSERT(false);
    }
}

static void derefDelayedObjects()
{
    WTF::MutexLocker locker(domThreadMapMutex);
    ThreadSpecificDOMData::DelayedObjectMap& delayedObjectMap = (*threadSpecificDOMData).delayedObjectMap();
    for (ThreadSpecificDOMData::DelayedObjectMap::iterator iter(delayedObjectMap.begin()); iter != delayedObjectMap.end(); ++iter) {
        derefObject(iter->second, iter->first);
    }
    delayedObjectMap.clear();
}

static void derefDelayedObjectsInCurrentThread(void*)
{
    (*threadSpecificDOMData).setDelayedProcessingScheduled(false);
    derefDelayedObjects();
}

template<typename T>
static void removeObjectsFromWrapperMap(DOMWrapperMap<T>& domMap)
{
    for (typename WTF::HashMap<T*, v8::Object*>::iterator iter(domMap.impl().begin()); iter != domMap.impl().end(); ++iter) {
        T* domObj = static_cast<T*>(iter->first);
        v8::Persistent<v8::Object> obj(iter->second);

        V8ClassIndex::V8WrapperType type = V8Proxy::GetDOMWrapperType(v8::Handle<v8::Object>::Cast(obj));

        // Deref the DOM object.
        derefObject(type, domObj);

        // Clear the JS wrapper.
        obj.Dispose();
    }
    domMap.impl().clear();
}

static void removeAllDOMObjectsInCurrentThread()
{
    v8::Locker locker;
    v8::HandleScope scope;

    // Deref all objects in the delayed queue.
    derefDelayedObjects();

    // Remove all DOM nodes.
    removeObjectsFromWrapperMap<Node>(getDOMNodeMap());

    // Remove all DOM objects in the wrapper map.
    removeObjectsFromWrapperMap<void>(getDOMObjectMap());

    // Remove all active DOM objects in the wrapper map.
    removeObjectsFromWrapperMap<void>(getActiveDOMObjectMap());

#if ENABLE(SVG)
    // Remove all SVG element instances in the wrapper map.
    removeObjectsFromWrapperMap<SVGElementInstance>(getDOMSVGElementInstanceMap());

    // Remove all SVG objects with context in the wrapper map.
    removeObjectsFromWrapperMap<void>(getDOMSVGObjectWithContextMap());
#endif
}

} // namespace WebCore
