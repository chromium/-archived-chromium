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

// A RenderDnsMaster instance is maintained for each RenderThread.
// Hostnames are typically added to the embedded queue during rendering.
// The first addition to the queue (transitioning from empty to having
// some names) causes a processing task to be added to the Renderer Thread.
// The processing task gathers all buffered names, and send them via IPC
// to the browser, so that DNS lookups can be performed before the user attempts
// to traverse a link.
// This class removed some duplicates, and discards numeric IP addresss
// (which wouldn't looked up in DNS anyway).
// To limit the time during the processing task (and avoid stalling the Render
// thread), several limits are placed on how much of the queue to process.
// If the processing task is not able to completely empty the queue, it
// schedules a future continuation of the task, and keeps the map of already
// sent names.  If the entire queue is processed, then the list of "sent names"
// is cleared so that future gatherings may again pass along the same names.

#ifndef CHROME_RENDERER_RENDER_NET_DNS_MASTER_H__
#define CHROME_RENDERER_RENDER_NET_DNS_MASTER_H__

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/task.h"
#include "chrome/renderer/net/render_dns_queue.h"

// Global API consists to do Prefetching in renderer.  This uses IPC to reach
// the Browser's global functions.
void DnsPrefetchCString(const char* hostname, size_t length);

class RenderDnsMaster {
 public:
  RenderDnsMaster();

  ~RenderDnsMaster() {}

  // Push a name into the queue to be resolved.
  void Resolve(const char* name, size_t length);

  // SubmitHosts processes the buffered names, and submits them for DNS
  // prefetching.
  // Note that browser process may decide which names should be looked up (to
  // pre-warm the cache) based on what has been (or not been) looked up
  // recently.
  // If sending for DNS lookup is incomplete (queue is not empty, or not all
  // names in map are sent, or ...) then a task to continue processing is
  // sent to our thread loop.
  void SubmitHostnames();

  // The following is private, but exposed for testing purposes only.
  static bool RenderDnsMaster::is_numeric_ip(const char* name, size_t length);

 private:
  // ExtractBufferedNames pulls names from queue into the map, reducing or
  // eliminating a waiting queue.
  // The size_goal argument can be used to reduce the amount of
  // processing done in this method, and can leave some data
  // in the buffer under some circumstances.
  // If size_goal is zero, then extraction proceeds until
  // the queue is empty.  If size goal is positive, then
  // extraction continues until the domain_map_ contains
  // at least the specified number of names, or the buffer is empty.
  void ExtractBufferedNames(size_t size_goal = 0);

  // DnsPrefetchNames does not check the buffer, and just sends names
  // that are already collected in the domain_map_ for DNS lookup.
  // If max_count is zero, then all available names are sent; and
  // if positive, then at most max_count names will be sent.
  void DnsPrefetchNames(size_t max_count = 0);

  // Reset() restores initial state provided after construction.
  // This discards ALL queue entries, and map entries.
  void Reset();

  // We use c_string_queue_ to hold lists of names supplied typically) by the
  // renderer.  It queues the names, at minimal cost to the renderer's thread,
  // and allows this class to process them when time permits (in a later task).
  DnsQueue c_string_queue_;


  // domain_map_ contains (for each domain) one of the next two constants,
  // depending on whether we have asked the browser process to do the actual
  // DNS lookup.
  static const int kLookupRequested = 0x1;
  static const int kPending = 0x0;
  typedef std::map<std::string, int> DomainUseMap;
  DomainUseMap domain_map_;

  // Cache a tally of the count of names that haven't yet been sent
  // for DNS pre-fetching.  Note that we *could* recalculate this
  // count by iterating over domain_map_, looking for even values.
  size_t new_name_count_;

  // We have some metrics to examine performance.  We might use
  // these metrics to modify buffer counts etc. some day.
  int buffer_full_discard_count_;
  int numeric_ip_discard_count_;

  ScopedRunnableMethodFactory<RenderDnsMaster> render_dns_factory_;

  DISALLOW_EVIL_CONSTRUCTORS(RenderDnsMaster);
};  // class RenderDnsMaster

#endif  // CHROME_RENDERER_RENDER_NET_DNS_MASTER_H__
