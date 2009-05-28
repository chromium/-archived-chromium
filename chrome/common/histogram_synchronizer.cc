// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/histogram.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/common/child_thread.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/histogram_synchronizer.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

HistogramSynchronizer::HistogramSynchronizer()
  : lock_(),
    received_all_renderer_historgrams_(&lock_),
    callback_task_(NULL),
    callback_thread_(NULL),
    io_message_loop_(NULL),
    next_available_sequence_number_(0),
    async_sequence_number_(0),
    async_renderers_pending_(0),
    async_callback_start_time_(TimeTicks::Now()),
    synchronous_sequence_number_(0),
    synchronous_renderers_pending_(0) {
  DCHECK(histogram_synchronizer_ == NULL);
  histogram_synchronizer_ = this;
}

HistogramSynchronizer::~HistogramSynchronizer() {
  // Clean up.
  delete callback_task_;
  callback_task_ = NULL;
  callback_thread_ = NULL;
  histogram_synchronizer_ = NULL;
}

// static
HistogramSynchronizer* HistogramSynchronizer::CurrentSynchronizer() {
  DCHECK(histogram_synchronizer_ != NULL);
  return histogram_synchronizer_;
}

void HistogramSynchronizer::FetchRendererHistogramsSynchronously(
    TimeDelta wait_time) {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);

  int sequence_number = GetNextAvaibleSequenceNumber(
      SYNCHRONOUS_HISTOGRAMS, RenderProcessHost::size());
  for (RenderProcessHost::iterator it = RenderProcessHost::begin();
       it != RenderProcessHost::end(); ++it) {
    it->second->Send(new ViewMsg_GetRendererHistograms(sequence_number));
  }

  TimeTicks start = TimeTicks::Now();
  TimeTicks end_time = start + wait_time;
  int unresponsive_renderer_count;
  {
    AutoLock auto_lock(lock_);
    while (synchronous_renderers_pending_ > 0 &&
           TimeTicks::Now() < end_time) {
      wait_time = end_time - TimeTicks::Now();
      received_all_renderer_historgrams_.TimedWait(wait_time);
    }
    unresponsive_renderer_count = synchronous_renderers_pending_;
    synchronous_renderers_pending_ = 0;
    synchronous_sequence_number_ = 0;
  }
  UMA_HISTOGRAM_COUNTS("Histogram.RendersNotRespondingSynchronous",
                       unresponsive_renderer_count);
  if (!unresponsive_renderer_count)
    UMA_HISTOGRAM_TIMES("Histogram.FetchRendererHistogramsSynchronously",
                        TimeTicks::Now() - start);
}

// static
void HistogramSynchronizer::FetchRendererHistogramsAsynchronously(
    MessageLoop* callback_thread,
    Task* callback_task,
    int wait_time) {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  DCHECK(callback_thread != NULL);
  DCHECK(callback_task != NULL);

  HistogramSynchronizer* current_synchronizer =
      HistogramSynchronizer::CurrentSynchronizer();

  if (current_synchronizer == NULL) {
    // System teardown is happening.
    callback_thread->PostTask(FROM_HERE, callback_task);
    return;
  }

  // callback_task_ member can only be accessed on IO thread.
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(current_synchronizer,
          &HistogramSynchronizer::SetCallbackTaskToCallAfterGettingHistograms,
          callback_thread,
          callback_task));

  // Tell all renderer processes to send their histograms.
  int sequence_number =
      current_synchronizer->GetNextAvaibleSequenceNumber(
          ASYNC_HISTOGRAMS, RenderProcessHost::size());
  for (RenderProcessHost::iterator it = RenderProcessHost::begin();
       it != RenderProcessHost::end(); ++it) {
    it->second->Send(new ViewMsg_GetRendererHistograms(sequence_number));
  }

  // Post a task that would be called after waiting for wait_time.
  g_browser_process->io_thread()->message_loop()->PostDelayedTask(FROM_HERE,
      NewRunnableMethod(current_synchronizer,
          &HistogramSynchronizer::ForceHistogramSynchronizationDoneCallback,
          sequence_number),
      wait_time);
}

// static
void HistogramSynchronizer::DeserializeHistogramList(
    int sequence_number,
    const std::vector<std::string>& histograms) {
  HistogramSynchronizer* current_synchronizer =
      HistogramSynchronizer::CurrentSynchronizer();
  if (current_synchronizer == NULL)
    return;

  DCHECK(current_synchronizer->IsOnIoThread());

  for (std::vector<std::string>::const_iterator it = histograms.begin();
       it < histograms.end();
       ++it) {
    Histogram::DeserializeHistogramInfo(*it);
  }

  // Record that we have received a histogram from renderer process.
  current_synchronizer->RecordRendererHistogram(sequence_number);
}

bool HistogramSynchronizer::RecordRendererHistogram(int sequence_number) {
  DCHECK(IsOnIoThread());

  if (sequence_number == async_sequence_number_) {
    if ((async_renderers_pending_ == 0) ||
        (--async_renderers_pending_ > 0))
      return false;
    DCHECK(callback_task_ != NULL);
    CallCallbackTaskAndResetData();
    return true;
  }

  {
    AutoLock auto_lock(lock_);
    if (sequence_number != synchronous_sequence_number_) {
      // No need to do anything if the sequence_number does not match current
      // synchronous_sequence_number_ or async_sequence_number_.
      return true;
    }
    if ((synchronous_renderers_pending_ == 0) ||
        (--synchronous_renderers_pending_ > 0))
      return false;
    DCHECK_EQ(synchronous_renderers_pending_, 0);
  }

  // We could call Signal() without holding the lock.
  received_all_renderer_historgrams_.Signal();
  return true;
}

// This method is called on the IO thread.
void HistogramSynchronizer::SetCallbackTaskToCallAfterGettingHistograms(
    MessageLoop* callback_thread,
    Task* callback_task) {
  DCHECK(IsOnIoThread());

  // Test for the existence of a previous task, and post call to post it if it
  // exists. We promised to post it after some timeout... and at this point, we
  // should just force the posting.
  if (callback_task_ != NULL) {
    CallCallbackTaskAndResetData();
  }

  // Assert there was no callback_task_ already.
  DCHECK(callback_task_ == NULL);

  // Save the thread and the callback_task.
  DCHECK(callback_thread != NULL);
  DCHECK(callback_task != NULL);
  callback_task_ = callback_task;
  callback_thread_ = callback_thread;
  async_callback_start_time_ = TimeTicks::Now();
}

void HistogramSynchronizer::ForceHistogramSynchronizationDoneCallback(
    int sequence_number) {
  DCHECK(IsOnIoThread());

  if (sequence_number == async_sequence_number_) {
     CallCallbackTaskAndResetData();
  }
}

// If wait time has elapsed or if we have received all the histograms from all
// the renderers, call the callback_task if a callback_task exists. This is
// called on IO Thread.
void HistogramSynchronizer::CallCallbackTaskAndResetData() {
  DCHECK(IsOnIoThread());

  // callback_task_ would be set to NULL, if we have heard from all renderers
  // and we would have called the callback_task already.
  if (callback_task_ == NULL) {
    return;
  }

  UMA_HISTOGRAM_COUNTS("Histogram.RendersNotRespondingAsynchronous",
                       async_renderers_pending_);
  if (!async_renderers_pending_)
    UMA_HISTOGRAM_TIMES("Histogram.FetchRendererHistogramsAsynchronously",
                        TimeTicks::Now() - async_callback_start_time_);

  DCHECK(callback_thread_ != NULL);
  DCHECK(callback_task_ != NULL);
  callback_thread_->PostTask(FROM_HERE, callback_task_);
  async_renderers_pending_ = 0;
  async_callback_start_time_ = TimeTicks::Now();
  callback_task_ = NULL;
  callback_thread_ = NULL;
}

int HistogramSynchronizer::GetNextAvaibleSequenceNumber(
    RendererHistogramRequester requester,
    size_t renderer_histograms_requested) {
  AutoLock auto_lock(lock_);
  ++next_available_sequence_number_;
  if (requester == ASYNC_HISTOGRAMS) {
    async_sequence_number_ = next_available_sequence_number_;
    async_renderers_pending_ = renderer_histograms_requested;
  } else if (requester == SYNCHRONOUS_HISTOGRAMS) {
    synchronous_sequence_number_ = next_available_sequence_number_;
    synchronous_renderers_pending_ = renderer_histograms_requested;
  }
  return next_available_sequence_number_;
}

bool HistogramSynchronizer::IsOnIoThread() {
  if (io_message_loop_ == NULL) {
    io_message_loop_ = MessageLoop::current();
  }
  return (MessageLoop::current() == io_message_loop_);
}

// static
HistogramSynchronizer* HistogramSynchronizer::histogram_synchronizer_ = NULL;
