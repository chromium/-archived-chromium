/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef BackForwardList_h
#define BackForwardList_h

#include <wtf/RefCounted.h>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class HistoryItem;
class Page;

typedef Vector<RefPtr<HistoryItem> > HistoryItemVector;
typedef HashSet<RefPtr<HistoryItem> > HistoryItemHashSet;

// Assumptions:
// - When BackForwardList::addItem is called, that means WebCore wants a new
// item in session history.  We will add a new entry in our corresponding
// browser history.
// - When the first frame load is committed during a navigation, m_previousItem
// will be the HistoryItem corresponding to the page we have just left.
// [m_previousItem refers to the item passed to addItem() or goToItem() 2 calls
// ago.] Also, WebCore will have already updated it with saved document state.

// NOTE: We don't have all the methods in the trunk version of BackForwardList.
// It seems some are not used except in Mac-specific code, so we can get away
// without them.  They've been completely removed from our version so that we
// can immediately tell from compile failures if WebCore starts using them.

// A delegate class that's responsible for doing most of the work of the
// back-forward list.
class BackForwardListClient {
public:
    virtual void didAddHistoryItem(HistoryItem* item) = 0;
    virtual void willGoToHistoryItem(HistoryItem* item) = 0;
    virtual HistoryItem* itemAtIndex(int index) = 0;
    virtual void goToItemAtIndexAsync(int index) = 0;
    virtual int backListCount() = 0;
    virtual int forwardListCount() = 0;
};

class BackForwardList : public RefCounted<BackForwardList> {
public: 
    BackForwardList(Page*);
    ~BackForwardList();
    
    Page* page() { return m_page; }
    
    // Must be called before any other methods.
    void setClient(BackForwardListClient* client);

    void addItem(PassRefPtr<HistoryItem>);
    void goToItem(HistoryItem*);
        
    HistoryItem* backItem();
    HistoryItem* currentItem();
    HistoryItem* forwardItem();
    HistoryItem* previousItem();
    HistoryItem* itemAtIndex(int);
    void goToItemAtIndexAsync(int);

    // Returns an iterable container of all history items, which will be
    // traversed in order to clear the page cache when it's disabled (see
    // Settings::setUsesPageCache).  For now, this is a stub that returns an
    // empty HistoryItemVector, since we don't currently allow disabling of the
    // page cache. If we do, we'll have to add a corresponding way to clear
    // it when it's disabled.
    HistoryItemVector& entries() { return m_entries_not_used; }

    // Only here for recovery after a crash.  We don't want to use addItem
    // because that calls client->didAddHistoryItem().
    void setCurrentItem(HistoryItem* item);

    void backListWithLimit(int, HistoryItemVector&);
    void forwardListWithLimit(int, HistoryItemVector&);

    int capacity();
    void setCapacity(int);
    bool enabled();
    void setEnabled(bool);
    int backListCount();
    int forwardListCount();

    void close();
    bool closed();

    // Is the current/previous item fake? Webkit requires a previous item when
    // navigating to a specific HistoryItem. In certain situations we end up
    // creating a HistoryItem to satisfy webkit and mark it as fake. See
    // WebFrameImpl::InternalLoadRequest for more details.
    void setCurrentItemFake(bool value) { m_currentItemFake = value; }
    bool isPreviousItemFake() const { return m_previousItemFake; }
    
private:
    // Sets m_previousItemFake to the value of m_currentItemFake and
    // m_currentItemFake to false. This is called internally at various points
    // when m_currenItem is being updated.
    void updateFakeState();

    Page* m_page;
    BackForwardListClient* m_client;
    RefPtr<HistoryItem> m_currentItem;  // most recently visited item
    // NOTE: this is NOT the same as the "back" item.
    RefPtr<HistoryItem> m_previousItem;  // second-most recently visited item
    unsigned m_current;
    unsigned m_capacity;
    bool m_closed;
    bool m_enabled;

    // See comment above setCurrentItemFake.
    bool m_currentItemFake;
    bool m_previousItemFake;

    // Settings.cpp requires an entries() method that returns a
    // HistoryItemVector reference, but we don't actually use it.
    HistoryItemVector m_entries_not_used;

}; //class BackForwardList
    
}; //namespace WebCore

#endif //BACKFORWARDLIST_H
