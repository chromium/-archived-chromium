/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef TreeShared_h
#define TreeShared_h

#include "RefCounted.h"

namespace WebCore {

#if USE(V8_BINDING)

template<class T> class TreeShared : public Peerable, Noncopyable {
public:
    TreeShared() : m_refCount(0), m_parent(0), m_peer(0)
    {
#ifndef NDEBUG
        m_deletionHasBegun = false;
        m_inRemovedLastRefFunction = false;
#endif
    }

    TreeShared(T* parent) : m_refCount(0), m_parent(parent), m_peer(0)
    {
#ifndef NDEBUG
        m_deletionHasBegun = false;
        m_inRemovedLastRefFunction = false;
#endif
    }

    virtual ~TreeShared()
    { 
        ASSERT(m_deletionHasBegun);
        ASSERT(!m_peer);
    }

    void ref()
    {
        ASSERT(!m_deletionHasBegun);
        ASSERT(!m_inRemovedLastRefFunction);
        ++m_refCount;
    }

    void deref() 
    { 
        ASSERT(!m_deletionHasBegun);
        ASSERT(!m_inRemovedLastRefFunction);

        if (--m_refCount <= 0 && !m_parent && !m_peer) {
#ifndef NDEBUG
            m_inRemovedLastRefFunction = true;
#endif
            removedLastRef();
        } 
    }

    void setPeer(void* peer) 
    {
        m_peer = peer;
        if (m_refCount <= 0 && !m_parent && !m_peer) {
#ifndef NDEBUG
            m_inRemovedLastRefFunction = true;
#endif
            removedLastRef();
        }
    }

    void* peer() const { return m_peer; }

    bool hasOneRef()
    {
        ASSERT(!m_deletionHasBegun);
        ASSERT(!m_inRemovedLastRefFunction);
        if (m_peer)
            return m_refCount == 0;
        return m_refCount == 1; 
    }

    int refCount() const
    {
        if (m_peer)
            return m_refCount + 1;
        return m_refCount; 
    }

    // setParent never deletes the node even if the node only has a
    // parent and no other references.  For DOM nodes the deletion
    // done in ContainerNode::removeAllChildren.
    void setParent(T* parent) { m_parent = parent; }
    T* parent() const { return m_parent; }

#ifndef NDEBUG
    bool m_deletionHasBegun;
    bool m_inRemovedLastRefFunction;
#endif

private:
    virtual void removedLastRef()
    {
#ifndef NDEBUG
        m_deletionHasBegun = true;
#endif
        delete static_cast<T*>(this);
    }

    int m_refCount;
    T* m_parent;
    void* m_peer;
};

#elif USE(JAVASCRIPTCORE_BINDINGS)

template<class T> class TreeShared : Noncopyable {
public:
    TreeShared()
        : m_refCount(0)
        , m_parent(0)
    {
#ifndef NDEBUG
        m_deletionHasBegun = false;
        m_inRemovedLastRefFunction = false;
#endif
    }
    TreeShared(T* parent)
        : m_refCount(0)
        , m_parent(0)
    {
#ifndef NDEBUG
        m_deletionHasBegun = false;
        m_inRemovedLastRefFunction = false;
#endif
    }
    virtual ~TreeShared()
    {
        ASSERT(m_deletionHasBegun);
    }

    void ref()
    {
        ASSERT(!m_deletionHasBegun);
        ASSERT(!m_inRemovedLastRefFunction);
        ++m_refCount;
    }

    void deref()
    {
        ASSERT(!m_deletionHasBegun);
        ASSERT(!m_inRemovedLastRefFunction);
        if (--m_refCount <= 0 && !m_parent) {
#ifndef NDEBUG
            m_inRemovedLastRefFunction = true;
#endif
            removedLastRef();
        }
    }

    bool hasOneRef() const
    {
        ASSERT(!m_deletionHasBegun);
        ASSERT(!m_inRemovedLastRefFunction);
        return m_refCount == 1;
    }

    int refCount() const
    {
        return m_refCount;
    }

    void setParent(T* parent) { m_parent = parent; }
    T* parent() const { return m_parent; }

#ifndef NDEBUG
    bool m_deletionHasBegun;
    bool m_inRemovedLastRefFunction;
#endif

private:
    virtual void removedLastRef()
    {
#ifndef NDEBUG
        m_deletionHasBegun = true;
#endif
        delete this;
    }

    int m_refCount;
    T* m_parent;
};

#else
#error "You must include config.h before TreeShared.h"
#endif

}

#endif // TreeShared.h
