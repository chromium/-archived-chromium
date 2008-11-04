/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "BackForwardList.h"

#include "HistoryItem.h"
#include "Logging.h"
#include "PageCache.h"

using namespace std;

namespace WebCore {

static const unsigned DefaultCapacity = 100;
static const unsigned NoCurrentItemIndex = UINT_MAX;

BackForwardList::BackForwardList(Page* page)
    : m_page(page)
    , m_current(NoCurrentItemIndex)
    , m_capacity(DefaultCapacity)
    , m_closed(true)
    , m_enabled(true)
    , m_currentItemFake(false)
    , m_previousItemFake(false)
{
}

BackForwardList::~BackForwardList()
{
    ASSERT(m_closed);
}

void BackForwardList::setClient(BackForwardListClient* client)
{
    m_client = client;
}

void BackForwardList::addItem(PassRefPtr<HistoryItem> prpItem)
{
    ASSERT(prpItem);
    if (m_capacity == 0 || !m_enabled)
        return;
    
    updateFakeState();
    m_previousItem = m_currentItem;
    m_currentItem = prpItem;

    m_client->didAddHistoryItem(m_currentItem.get());
}

void BackForwardList::goToItem(HistoryItem* item)
{
    updateFakeState();

    m_previousItem = m_currentItem;
    m_currentItem = item;

    m_client->willGoToHistoryItem(item);
}

HistoryItem* BackForwardList::backItem()
{
    ASSERT_NOT_REACHED();
    return 0;
}

HistoryItem* BackForwardList::currentItem()
{
    return m_currentItem.get();
}

HistoryItem* BackForwardList::forwardItem()
{
    ASSERT_NOT_REACHED();
    return 0;
}

HistoryItem* BackForwardList::previousItem()
{
    return m_previousItem.get();
}

void BackForwardList::setCurrentItem(HistoryItem* item) {
    m_currentItem = item;
    m_currentItemFake = false;
}

void BackForwardList::backListWithLimit(int limit, HistoryItemVector& list)
{
    ASSERT_NOT_REACHED();
    list.clear();
}

void BackForwardList::forwardListWithLimit(int limit, HistoryItemVector& list)
{
    ASSERT_NOT_REACHED();
    list.clear();
}

int BackForwardList::capacity()
{
    return m_capacity;
}

void BackForwardList::setCapacity(int size)
{    
    m_capacity = size;
}

bool BackForwardList::enabled()
{
    return m_enabled;
}

void BackForwardList::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled) {
        int capacity = m_capacity;
        setCapacity(0);
        setCapacity(capacity);
    }
}

int BackForwardList::backListCount()
{
    return m_client->backListCount();
}

int BackForwardList::forwardListCount()
{
    return m_client->forwardListCount();
}

HistoryItem* BackForwardList::itemAtIndex(int index)
{
    return m_client->itemAtIndex(index);
}

void BackForwardList::close()
{
    m_closed = true;
    m_previousItem = 0;
    m_currentItem = 0;
}

bool BackForwardList::closed()
{
    return m_closed;
}

void BackForwardList::updateFakeState()
{
    m_previousItemFake = m_currentItemFake;
    m_currentItemFake = false;
}

}; // namespace WebCore
