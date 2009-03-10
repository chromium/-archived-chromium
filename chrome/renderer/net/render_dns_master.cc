// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See header file for description of RenderDnsMaster class


#include "chrome/renderer/net/render_dns_master.h"

#include <ctype.h>

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/common/net/dns.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/net/render_dns_queue.h"
#include "chrome/renderer/render_thread.h"

// This API is used in the render process by renderer_glue.cc.
// IF you are in the render process, you MUST be on the renderer thread to call.
void DnsPrefetchCString(const char* hostname, size_t length) {
  RenderThread::current()->Resolve(hostname, length);
}

// The number of hostnames submitted to Browser DNS resolver per call to
// SubmitHostsnames() (which reads names from our queue).
static const size_t kMAX_SUBMISSION_PER_TASK = 30;

RenderDnsMaster::RenderDnsMaster()
    : c_string_queue_(1000),
      ALLOW_THIS_IN_INITIALIZER_LIST(render_dns_factory_(this)) {
  Reset();
}

void RenderDnsMaster::Reset() {
  domain_map_.clear();
  c_string_queue_.Clear();
  buffer_full_discard_count_ = 0;
  numeric_ip_discard_count_ = 0;
  new_name_count_ = 0;
}

// Push names into queue quickly!
void RenderDnsMaster::Resolve(const char* name, size_t length) {
  if (!length)
    return;  // Don't store empty strings in buffer.
  if (is_numeric_ip(name, length))
    return;  // Numeric IPs have no DNS lookup significance.

  size_t old_size =  c_string_queue_.Size();
  DnsQueue::PushResult result = c_string_queue_.Push(name, length);
  if (DnsQueue::SUCCESSFUL_PUSH == result) {
    if (1 == c_string_queue_.Size()) {
      DCHECK(0 == old_size);
      if (0 != old_size)
        return;  // Overkill safety net: Don't send too many InvokeLater's.
      render_dns_factory_.RevokeAll();
      RenderThread::current()->message_loop()->PostDelayedTask(FROM_HERE,
          render_dns_factory_.NewRunnableMethod(
              &RenderDnsMaster::SubmitHostnames), 10);
    }
    return;
  }
  if (DnsQueue::OVERFLOW_PUSH == result) {
    buffer_full_discard_count_++;
    return;
  }
  DCHECK(DnsQueue::REDUNDANT_PUSH == result);
}

// Extract data from the Queue, and then send it off the the Browser process
// to be resolved.
void RenderDnsMaster::SubmitHostnames() {
  // Get all names out of the C_string_queue (into our map)
  ExtractBufferedNames();
  // TBD: IT could be that we should only extract about as many names as we are
  // going to send to the browser.  That would cause a "silly" page with a TON
  // of URLs to start to overrun the DnsQueue, which will cause the names to
  // be dropped (not stored in the queue).  By fetching ALL names, we are
  // taking on a lot of work, which may take a long time to process... perhaps
  // longer than the page may be visible!?!?!  If we implement a better
  // mechanism for doing domain_map.clear() (see end of this method), then
  // we'd automatically flush such pending work from a ridiculously link-filled
  // page.

  // Don't overload the browser DNS lookup facility, or take too long here,
  // by only sending off kMAX_SUBMISSION_PER_TASK names to the Browser.
  // This will help to avoid overloads when a page has a TON of links.
  DnsPrefetchNames(kMAX_SUBMISSION_PER_TASK);
  if (new_name_count_ > 0 || 0 < c_string_queue_.Size()) {
    render_dns_factory_.RevokeAll();
    RenderThread::current()->message_loop()->PostDelayedTask(FROM_HERE,
        render_dns_factory_.NewRunnableMethod(
            &RenderDnsMaster::SubmitHostnames), 10);
  } else {
    // TODO(JAR): Should we only clear the map when we navigate, or reload?
    domain_map_.clear();
  }
}

// Pull some hostnames from the queue, and add them to our map.
void RenderDnsMaster::ExtractBufferedNames(size_t size_goal) {
  size_t count(0);  // Number of entries to find (0 means find all).
  if (size_goal > 0) {
    if (size_goal <= domain_map_.size())
      return;  // Size goal was met.
    count = size_goal - domain_map_.size();
  }

  std::string name;
  while (c_string_queue_.Pop(&name)) {
    DCHECK(0 != name.size());
    // We don't put numeric IP names into buffer.
    DCHECK(!is_numeric_ip(name.c_str(), name.size()));
    DomainUseMap::iterator it;
    it = domain_map_.find(name);
    if (domain_map_.end() == it) {
      domain_map_[name] = kPending;
      new_name_count_++;
      if (0 == count) continue;  // Until buffer is empty.
      if (1 == count) break;  // We found size_goal.
      DCHECK(1 < count);
      count--;
    } else {
      DCHECK(kPending == it->second || kLookupRequested == it->second);
    }
  }
}

void RenderDnsMaster::DnsPrefetchNames(size_t max_count) {
  // We are on the renderer thread, and just need to send things to the browser.
  chrome_common_net::NameList names;
  for (DomainUseMap::iterator it = domain_map_.begin();
    it != domain_map_.end();
    it++) {
    if (0 == (it->second & kLookupRequested)) {
      it->second |= kLookupRequested;
      names.push_back(it->first);
      if (0 == max_count) continue;  // Get all, independent of count.
      if (1 == max_count) break;
      max_count--;
      DCHECK(1 <= max_count);
    }
  }
  new_name_count_ -= names.size();
  DCHECK(new_name_count_ >= 0);

  RenderThread::current()->Send(new ViewHostMsg_DnsPrefetch(names));
}

// is_numeric_ip() checks to see if all characters in name are either numeric,
// or dots.  Such a name will not actually be passed to DNS, as it is an IP
// address.
bool RenderDnsMaster::is_numeric_ip(const char* name, size_t length) {
  // Scan for a character outside our lookup list.
  while (length-- > 0) {
    if (!isdigit(*name) && '.' != *name)
      return false;
    name++;
  }
  return true;
}
