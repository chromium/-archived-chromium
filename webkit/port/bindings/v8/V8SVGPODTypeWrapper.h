// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8SVGPODTypeWrapper_h
#define V8SVGPODTypeWrapper_h

#if ENABLE(SVG)

#include "config.h"
#include "SVGElement.h"
#include "SVGList.h"
#include <wtf/Assertions.h>
#include <wtf/RefCounted.h>
#include <wtf/HashMap.h>

namespace WebCore {

template<typename PODType>
class V8SVGPODTypeWrapper : public RefCounted<V8SVGPODTypeWrapper<PODType> > {
public:
    V8SVGPODTypeWrapper() : RefCounted<V8SVGPODTypeWrapper<PODType> >(0) {
    }
    virtual ~V8SVGPODTypeWrapper() { 
    }

    // Getter wrapper
    virtual operator PODType() = 0;

    // Setter wrapper
    virtual void commitChange(PODType, SVGElement*) = 0;
};

template<typename PODType>
class V8SVGPODTypeWrapperCreatorForList : public V8SVGPODTypeWrapper<PODType>
{
public:
    typedef PODType (SVGPODListItem<PODType>::*GetterMethod)() const; 
    typedef void (SVGPODListItem<PODType>::*SetterMethod)(PODType);

    V8SVGPODTypeWrapperCreatorForList(SVGPODListItem<PODType>* creator, const QualifiedName& attributeName)
        : m_creator(creator)
        , m_getter(&SVGPODListItem<PODType>::value)
        , m_setter(&SVGPODListItem<PODType>::setValue)
        , m_associatedAttributeName(attributeName)
    {
        ASSERT(m_creator);
        ASSERT(m_getter);
        ASSERT(m_setter);
    }

    virtual ~V8SVGPODTypeWrapperCreatorForList() { }

    // Getter wrapper
    virtual operator PODType() { return (m_creator.get()->*m_getter)(); }

    // Setter wrapper
    virtual void commitChange(PODType type, SVGElement* context)
    {
        if (!m_setter)
            return;

        (m_creator.get()->*m_setter)(type);

        if (context)
            context->svgAttributeChanged(m_associatedAttributeName);
    }

private:
    // Update callbacks
    RefPtr<SVGPODListItem<PODType> > m_creator;
    GetterMethod m_getter;
    SetterMethod m_setter;
    const QualifiedName& m_associatedAttributeName;
};

template<typename PODType>
class V8SVGPODTypeWrapperCreatorReadOnly : public V8SVGPODTypeWrapper<PODType>
{
public:
    V8SVGPODTypeWrapperCreatorReadOnly(PODType type)
        : m_podType(type)
    { }

    virtual ~V8SVGPODTypeWrapperCreatorReadOnly() { }

    // Getter wrapper
    virtual operator PODType() { return m_podType; }

    // Setter wrapper
    virtual void commitChange(PODType type, SVGElement*)
    {
        m_podType = type;
    }

private:
    PODType m_podType;
};

template<typename PODType, typename PODTypeCreator>
class V8SVGPODTypeWrapperCreatorReadWrite : public V8SVGPODTypeWrapper<PODType>
{
public:
    typedef PODType (PODTypeCreator::*GetterMethod)() const; 
    typedef void (PODTypeCreator::*SetterMethod)(PODType);
    typedef void (*CacheRemovalCallback)(V8SVGPODTypeWrapper<PODType>*);

    V8SVGPODTypeWrapperCreatorReadWrite(PODTypeCreator* creator, GetterMethod getter, SetterMethod setter, CacheRemovalCallback cacheRemovalCallback)
        : m_creator(creator)
        , m_getter(getter)
        , m_setter(setter)
        , m_cacheRemovalCallback(cacheRemovalCallback)
    {
        ASSERT(creator);
        ASSERT(getter);
        ASSERT(setter);
        ASSERT(cacheRemovalCallback);
    }

    virtual ~V8SVGPODTypeWrapperCreatorReadWrite() { 
        ASSERT(m_cacheRemovalCallback);

        (*m_cacheRemovalCallback)(this);
    }

    // Getter wrapper
    virtual operator PODType() { return (m_creator.get()->*m_getter)(); }

    // Setter wrapper
    virtual void commitChange(PODType type, SVGElement* context)
    {
        if (!m_setter)
            return;

        (m_creator.get()->*m_setter)(type);

        if (context)
            context->svgAttributeChanged(m_creator->associatedAttributeName());
    }

private:
    // Update callbacks
    RefPtr<PODTypeCreator> m_creator;
    GetterMethod m_getter;
    SetterMethod m_setter;
    CacheRemovalCallback m_cacheRemovalCallback;
};

// Caching facilities
template<typename PODType, typename PODTypeCreator>
struct PODTypeReadWriteHashInfo {
    typedef PODType (PODTypeCreator::*GetterMethod)() const; 
    typedef void (PODTypeCreator::*SetterMethod)(PODType);

    // Empty value
    PODTypeReadWriteHashInfo()
        : creator(0)
        , getter(0)
        , setter(0)
    { }

    // Deleted value
    explicit PODTypeReadWriteHashInfo(bool)
        : creator(reinterpret_cast<PODTypeCreator*>(-1))
        , getter(0)
        , setter(0)
    { }

    PODTypeReadWriteHashInfo(PODTypeCreator* _creator, GetterMethod _getter, SetterMethod _setter)
        : creator(_creator)
        , getter(_getter)
        , setter(_setter)
    {
        ASSERT(creator);
        ASSERT(getter);
    }

    bool operator==(const PODTypeReadWriteHashInfo& other) const
    {
        return creator == other.creator && getter == other.getter && setter == other.setter;
    }

    PODTypeCreator* creator;
    GetterMethod getter;
    SetterMethod setter;
};

template<typename PODType, typename PODTypeCreator>
struct PODTypeReadWriteHashInfoHash {
    static unsigned hash(const PODTypeReadWriteHashInfo<PODType, PODTypeCreator>& info)
    {
        return StringImpl::computeHash((::UChar*) &info, sizeof(PODTypeReadWriteHashInfo<PODType, PODTypeCreator>) / sizeof(::UChar));
    }

    static bool equal(const PODTypeReadWriteHashInfo<PODType, PODTypeCreator>& a, const PODTypeReadWriteHashInfo<PODType, PODTypeCreator>& b)
    {
        return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
};

template<typename PODType, typename PODTypeCreator>
struct PODTypeReadWriteHashInfoTraits : WTF::GenericHashTraits<PODTypeReadWriteHashInfo<PODType, PODTypeCreator> > {
    static const bool emptyValueIsZero = true;
    static const bool needsDestruction = false;

    static const PODTypeReadWriteHashInfo<PODType, PODTypeCreator>& deletedValue()
    {
        static PODTypeReadWriteHashInfo<PODType, PODTypeCreator> key(true);
        return key;
    }

    static const PODTypeReadWriteHashInfo<PODType, PODTypeCreator>& emptyValue()
    {
        static PODTypeReadWriteHashInfo<PODType, PODTypeCreator> key;
        return key;
    }
};

template<typename PODType, typename PODTypeCreator>
class V8SVGPODTypeWrapperCache
{
public:
    typedef PODType (PODTypeCreator::*GetterMethod)() const; 
    typedef void (PODTypeCreator::*SetterMethod)(PODType);

    typedef HashMap<PODTypeReadWriteHashInfo<PODType, PODTypeCreator>, V8SVGPODTypeWrapperCreatorReadWrite<PODType, PODTypeCreator>*, PODTypeReadWriteHashInfoHash<PODType, PODTypeCreator>, PODTypeReadWriteHashInfoTraits<PODType, PODTypeCreator> > ReadWriteHashMap;
    typedef typename ReadWriteHashMap::const_iterator ReadWriteHashMapIterator;

    static ReadWriteHashMap& readWriteHashMap()
    {
        static ReadWriteHashMap _readWriteHashMap;
        return _readWriteHashMap;
    }

    // Used for readwrite attributes only
    static V8SVGPODTypeWrapper<PODType>* lookupOrCreateWrapper(PODTypeCreator* creator, GetterMethod getter, SetterMethod setter)
    {
        ReadWriteHashMap& map(readWriteHashMap());
        PODTypeReadWriteHashInfo<PODType, PODTypeCreator> info(creator, getter, setter);

        if (map.contains(info))
            return map.get(info);

        V8SVGPODTypeWrapperCreatorReadWrite<PODType, PODTypeCreator>* wrapper = new V8SVGPODTypeWrapperCreatorReadWrite<PODType, PODTypeCreator>(
          creator, getter, setter, forgetWrapper);
        map.set(info, wrapper);
        return wrapper;
    }

    static void forgetWrapper(V8SVGPODTypeWrapper<PODType>* wrapper)
    {
        ReadWriteHashMap& map(readWriteHashMap());

        ReadWriteHashMapIterator it = map.begin();
        ReadWriteHashMapIterator end = map.end();

        for (; it != end; ++it) {
            if (it->second != wrapper)
                continue;

            // It's guaruanteed that there's just one object we need to take care of.
            map.remove(it->first);
            break;
        }
    }
};


} // namespace WebCore

#endif // ENABLE(SVG)
#endif // V8SVGPODTypeWrapper_h
