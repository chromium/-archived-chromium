// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_HISTOGRAM_SYNCHRONIZER_H_
#define CHROME_COMMON_HISTOGRAM_SYNCHRONIZER_H_

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/condition_variable.h"
#include "base/lock.h"
#include "base/message_loop.h"
#include "base/process.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/time.h"

class MessageLoop;

class HistogramSynchronizer : public
    base::RefCountedThreadSafe<HistogramSynchronizer> {
 public:

  enum RendererHistogramRequester {
    ASYNC_HISTOGRAMS,
    SYNCHRONOUS_HISTOGRAMS
  };

  HistogramSynchronizer();

  ~HistogramSynchronizer();

  // Return pointer to the singleton instance, which is allocated and
  // deallocated on the main UI thread (during system startup and teardown).
  static HistogramSynchronizer* CurrentSynchronizer();

  // Contact all renderers, and get them to upload to the browser any/all
  // changes to histograms.  Return when all changes have been acquired, or when
  // the wait time expires (whichever is sooner). This method is called on the
  // main UI thread from about:histograms.
  void FetchRendererHistogramsSynchronously(base::TimeDelta wait_time);

  // Contact all renderers, and get them to upload to the browser any/all
  // changes to histograms.  When all changes have been acquired, or when the
  // wait time expires (whichever is sooner), post the callback_task to the UI
  // thread. Note the callback_task is posted exactly once. This method is
  // called on the IO thread from UMA via PostMessage.
  static void FetchRendererHistogramsAsynchronously(
      MessageLoop* callback_thread, Task* callback_task, int wait_time);

  // This method is called on the IO thread. Desrializes the histograms and
  // records that we have received histograms from a renderer process.
  static void DeserializeHistogramList(
      int sequence_number, const std::vector<std::string>& histograms);

 private:
  // Records that we have received the histograms from a renderer for the given
  // sequence number. If we have received a response from all histograms, either
  // signal the waiting process or call the callback function. Returns true when
  // we receive histograms from the last of N renderers that were contacted for
  // an update. This is called on IO Thread.
  bool RecordRendererHistogram(int sequence_number);

  void SetCallbackTaskToCallAfterGettingHistograms(
      MessageLoop* callback_thread, Task* callback_task);

  void ForceHistogramSynchronizationDoneCallback(int sequence_number);

  // Calls the callback task, if there is a callback_task.
  void CallCallbackTaskAndResetData();

  // Method to get a new sequence number to be sent to renderers from broswer
  // process.
  int GetNextAvaibleSequenceNumber(RendererHistogramRequester requster,
                                   size_t renderer_histograms_requested);

  // For use ONLY in a DCHECK. This method initializes io_message_loop_ in its
  // first call and then compares io_message_loop_ with MessageLoop::current()
  // in subsequent calls. This method guarantees we're consistently on the
  // singular IO thread and we don't need to worry about locks.
  bool IsOnIoThread();

  // This lock_ protects access to sequence number and
  // synchronous_renderers_pending_.
  Lock lock_;

  // This condition variable is used to block caller of the synchronous request
  // to update histograms, and to signal that thread when updates are completed.
  ConditionVariable received_all_renderer_historgrams_;

  // When a request is made to asynchronously update the histograms, we store
  // the task and thread we use to post a completion notification in
  // callback_task_ and callback_thread_.
  Task* callback_task_;
  MessageLoop* callback_thread_;

  // For use ONLY in a DCHECK and is used in IsOnIoThread(). io_message_loop_ is
  // initialized during the first call to IsOnIoThread(), and then compares
  // MessageLoop::current() against io_message_loop_ in subsequent calls for
  // consistency.
  MessageLoop* io_message_loop_;

  // We don't track the actual renderers that are contacted for an update, only
  // the count of the number of renderers, and we can sometimes time-out and
  // give up on a "slow to respond" renderer.  We use a sequence_number to be
  // sure a response from a renderer is associated with the current round of
  // requests (and not merely a VERY belated prior response).
  // next_available_sequence_number_ is the next available number (used to
  // avoid reuse for a long time).
  int next_available_sequence_number_;

  // The sequence number used by the most recent asynchronous update request to
  // contact all renderers.
  int async_sequence_number_;

  // The number of renderers that have not yet responded to requests (as part of
  // an asynchronous update).
  int async_renderers_pending_;

  // The time when we were told to start the fetch histograms asynchronously
  // from renderers.
  base::TimeTicks async_callback_start_time_;

  // The sequence number used by the most recent synchronous update request to
  // contact all renderers.
  int synchronous_sequence_number_;

  // The number of renderers that have not yet responded to requests (as part of
  // a synchronous update).
  int synchronous_renderers_pending_;

  // This singleton instance should be started during the single threaded
  // portion of main(). It initializes globals to provide support for all future
  // calls. This object is created on the UI thread, and it is destroyed after
  // all the other threads have gone away. As a result, it is ok to call it
  // from the UI thread (for UMA uploads), or for about:histograms.
  static HistogramSynchronizer* histogram_synchronizer_;

  DISALLOW_EVIL_CONSTRUCTORS(HistogramSynchronizer);
};

#endif  // CHROME_COMMON_HISTOGRAM_SYNCHRONIZER_H_
