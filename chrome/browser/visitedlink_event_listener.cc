// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/visitedlink_event_listener.h"

#include "base/shared_memory.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/common/render_messages.h"

using base::Time;
using base::TimeDelta;

// The amount of time we wait to accumulate visited link additions.
static const int kCommitIntervalMs = 100;

void VisitedLinkEventListener::NewTable(base::SharedMemory* table_memory) {
  if (!table_memory)
    return;

  // Send to all RenderProcessHosts.
  for (RenderProcessHost::iterator i = RenderProcessHost::begin();
       i != RenderProcessHost::end(); ++i) {
    if (!i->second->HasConnection())
      continue;

    base::SharedMemoryHandle new_table;
    base::ProcessHandle process = i->second->process().handle();
    if (!process) {
      // process can be null if it's started with the --single-process flag.
      process = base::Process::Current().handle();
    }

    table_memory->ShareToProcess(process, &new_table);
    i->second->Send(new ViewMsg_VisitedLink_NewTable(new_table));
  }
}

void VisitedLinkEventListener::Add(VisitedLinkMaster::Fingerprint fingerprint) {
  pending_visited_links_.push_back(fingerprint);

  if (!coalesce_timer_.IsRunning()) {
    coalesce_timer_.Start(
        TimeDelta::FromMilliseconds(kCommitIntervalMs), this,
        &VisitedLinkEventListener::CommitVisitedLinks);
  }
}

void VisitedLinkEventListener::Reset() {
  pending_visited_links_.clear();
  coalesce_timer_.Stop();

  // Send to all RenderProcessHosts.
  for (RenderProcessHost::iterator i = RenderProcessHost::begin();
       i != RenderProcessHost::end(); ++i) {
    i->second->ResetVisitedLinks();
  }
}

void VisitedLinkEventListener::CommitVisitedLinks() {
  // Send to all RenderProcessHosts.
  for (RenderProcessHost::iterator i = RenderProcessHost::begin();
       i != RenderProcessHost::end(); ++i) {
    i->second->AddVisitedLinks(pending_visited_links_);
  }

  pending_visited_links_.clear();
}
