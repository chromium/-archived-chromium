// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "chrome/browser/jankometer.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/histogram.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/watchdog.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"

#if defined(OS_LINUX)
#include "chrome/common/gtk_util.h"
#endif

using base::TimeDelta;
using base::TimeTicks;

namespace {

// The maximum threshold of delay of the message  before considering it a delay.
// For a debug build, you may want to set IO delay around 500ms.
// For a release build, setting it around 350ms is sensible.
// Visit about:histograms to see what the distribution is on your system, with
// your build. Be sure to do some work to get interesting stats.
// The numbers below came from a warm start (you'll get about 5-10 alarms with
// a cold start), and running the page-cycler for 5 rounds.
#ifdef NDEBUG
const int kMaxUIMessageDelayMs = 350;
const int kMaxIOMessageDelayMs = 200;
#else
const int kMaxUIMessageDelayMs = 500;
const int kMaxIOMessageDelayMs = 400;
#endif

// Maximum processing time (excluding queueing delay) for a message before
// considering it delayed.
const int kMaxMessageProcessingMs = 100;

// TODO(brettw) Consider making this a pref.
const bool kPlaySounds = false;

//------------------------------------------------------------------------------
// Provide a special watchdog to make it easy to set the breakpoint on this
// class only.
class JankWatchdog : public Watchdog {
 public:
  JankWatchdog(const TimeDelta& duration,
               const std::string& thread_watched_name,
               bool enabled)
      : Watchdog(duration, thread_watched_name, enabled),
        thread_name_watched_(thread_watched_name),
        alarm_count_(0) {
  }

  virtual ~JankWatchdog() {}

  virtual void Alarm() {
    // Put break point here if you want to stop threads and look at what caused
    // the jankiness.
    alarm_count_++;
    Watchdog::Alarm();
  }

 private:
  std::string thread_name_watched_;
  int alarm_count_;

  DISALLOW_COPY_AND_ASSIGN(JankWatchdog);
};

//------------------------------------------------------------------------------
class JankObserver : public base::RefCountedThreadSafe<JankObserver>,
                     public MessageLoopForUI::Observer {
 public:
  JankObserver(const char* thread_name,
               const TimeDelta& excessive_duration,
               bool watchdog_enable)
      : MaxMessageDelay_(excessive_duration),
        slow_processing_counter_(std::string("Chrome.SlowMsg") + thread_name),
        queueing_delay_counter_(std::string("Chrome.DelayMsg") + thread_name),
        process_times_((std::string("Chrome.ProcMsgL ") +
                        thread_name).c_str(), 1, 3600000, 50),
        total_times_((std::string("Chrome.TotalMsgL ") +
                      thread_name).c_str(), 1, 3600000, 50),
        total_time_watchdog_(excessive_duration, thread_name, watchdog_enable) {
    process_times_.SetFlags(kUmaTargetedHistogramFlag);
    total_times_.SetFlags(kUmaTargetedHistogramFlag);
  }

  // Attaches the observer to the current thread's message loop. You can only
  // attach to the current thread, so this function can be invoked on another
  // thread to attach it.
  void AttachToCurrentThread() {
    // TODO(darin): support monitoring jankiness on non-UI threads!
    if (MessageLoop::current()->type() == MessageLoop::TYPE_UI)
      MessageLoopForUI::current()->AddObserver(this);
  }

  // Detaches the observer to the current thread's message loop.
  void DetachFromCurrentThread() {
    if (MessageLoop::current()->type() == MessageLoop::TYPE_UI)
      MessageLoopForUI::current()->RemoveObserver(this);
  }

  // Called when a message has just begun processing, initializes
  // per-message variables and timers.
  void StartProcessingTimers() {
    // Simulate arming when the message entered the queue.
    total_time_watchdog_.ArmSomeTimeDeltaAgo(queueing_time_);
    if (queueing_time_ > MaxMessageDelay_) {
      // Message is too delayed.
      queueing_delay_counter_.Increment();
#if defined(OS_WIN)
      if (kPlaySounds)
        MessageBeep(MB_ICONASTERISK);
#endif
    }
  }

  // Called when a message has just finished processing, finalizes
  // per-message variables and timers.
  void EndProcessingTimers() {
    total_time_watchdog_.Disarm();
    TimeTicks now = TimeTicks::Now();
    if (begin_process_message_ != TimeTicks()) {
      TimeDelta processing_time = now - begin_process_message_;
      process_times_.AddTime(processing_time);
      total_times_.AddTime(queueing_time_ + processing_time);
    }
    if (now - begin_process_message_ >
        TimeDelta::FromMilliseconds(kMaxMessageProcessingMs)) {
      // Message took too long to process.
      slow_processing_counter_.Increment();
#if defined(OS_WIN)
      if (kPlaySounds)
        MessageBeep(MB_ICONHAND);
#endif
    }
  }

#if defined(OS_WIN)
  virtual void WillProcessMessage(const MSG& msg) {
    begin_process_message_ = TimeTicks::Now();

    // GetMessageTime returns a LONG (signed 32-bit) and GetTickCount returns
    // a DWORD (unsigned 32-bit). They both wrap around when the time is longer
    // than they can hold. I'm not sure if GetMessageTime wraps around to 0,
    // or if the original time comes from GetTickCount, it might wrap around
    // to -1.
    //
    // Therefore, I cast to DWORD so if it wraps to -1 we will correct it. If
    // it doesn't, then our time delta will be negative if a message happens
    // to straddle the wraparound point, it will still be OK.
    DWORD cur_message_issue_time = static_cast<DWORD>(msg.time);
    DWORD cur_time = GetTickCount();
    queueing_time_ =
        base::TimeDelta::FromMilliseconds(cur_time - cur_message_issue_time);

    StartProcessingTimers();
  }

  virtual void DidProcessMessage(const MSG& msg) {
    EndProcessingTimers();
  }
#elif defined(OS_LINUX)
  virtual void WillProcessEvent(GdkEvent* event) {
    begin_process_message_ = TimeTicks::Now();
    // TODO(evanm): we want to set queueing_time_ using
    // event_utils::GetGdkEventTime, but how do you convert that info
    // into a delta?
    // guint event_time = event_utils::GetGdkEventTime(event);
    queueing_time_ = base::TimeDelta::FromMilliseconds(0);
    StartProcessingTimers();
  }

  virtual void DidProcessEvent(GdkEvent* event) {
    EndProcessingTimers();
  }
#endif

 private:
  const TimeDelta MaxMessageDelay_;

  // Time at which the current message processing began.
  TimeTicks begin_process_message_;

  // Time the current message spent in the queue -- delta between message
  // construction time and message processing time.
  TimeDelta queueing_time_;

  // Counters for the two types of jank we measure.
  StatsCounter slow_processing_counter_;  // Messages with long processing time.
  StatsCounter queueing_delay_counter_;   // Messages with long queueing delay.
  Histogram process_times_;           // Time spent processing task.
  Histogram total_times_;             // Total of queueing plus processing time.
  JankWatchdog total_time_watchdog_;  // Watching for excessive total_time.

  DISALLOW_EVIL_CONSTRUCTORS(JankObserver);
};

// These objects are created by InstallJankometer and leaked.
JankObserver* ui_observer = NULL;
JankObserver* io_observer = NULL;

}  // namespace

void InstallJankometer(const CommandLine& parsed_command_line) {
  if (ui_observer || io_observer) {
    NOTREACHED() << "Initializing jank-o-meter twice";
    return;
  }

  bool ui_watchdog_enabled = false;
  bool io_watchdog_enabled = false;
  if (parsed_command_line.HasSwitch(switches::kEnableWatchdog)) {
    std::wstring list =
        parsed_command_line.GetSwitchValue(switches::kEnableWatchdog);
    if (list.npos != list.find(L"ui"))
      ui_watchdog_enabled = true;
    if (list.npos != list.find(L"io"))
      io_watchdog_enabled = true;
  }

  // Install on the UI thread.
  ui_observer = new JankObserver(
      "UI",
      TimeDelta::FromMilliseconds(kMaxUIMessageDelayMs),
      ui_watchdog_enabled);
  ui_observer->AddRef();
  ui_observer->AttachToCurrentThread();

  // Now install on the I/O thread. Hiccups on that thread will block
  // interaction with web pages. We must proxy to that thread before we can
  // add our observer.
  io_observer = new JankObserver(
      "IO",
      TimeDelta::FromMilliseconds(kMaxIOMessageDelayMs),
      io_watchdog_enabled);
  io_observer->AddRef();
  base::Thread* io_thread = g_browser_process->io_thread();
  if (io_thread) {
    io_thread->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(io_observer,
            &JankObserver::AttachToCurrentThread));
  }
}

void UninstallJankometer() {
  if (ui_observer) {
    ui_observer->DetachFromCurrentThread();
    ui_observer->Release();
    ui_observer = NULL;
  }
  if (io_observer) {
    // IO thread can't be running when we remove observers.
    DCHECK((!g_browser_process) || !(g_browser_process->io_thread()));
    io_observer->Release();
    io_observer = NULL;
  }
}
