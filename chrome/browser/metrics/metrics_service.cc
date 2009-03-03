// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



//------------------------------------------------------------------------------
// Description of the life cycle of a instance of MetricsService.
//
//  OVERVIEW
//
// A MetricsService instance is typically created at application startup.  It
// is the central controller for the acquisition of log data, and the automatic
// transmission of that log data to an external server.  Its major job is to
// manage logs, grouping them for transmission, and transmitting them.  As part
// of its grouping, MS finalizes logs by including some just-in-time gathered
// memory statistics, snapshotting the current stats of numerous histograms,
// closing the logs, translating to XML text, and compressing the results for
// transmission.  Transmission includes submitting a compressed log as data in a
// URL-post, and retransmitting (or retaining at process termination) if the
// attempted transmission failed.  Retention across process terminations is done
// using the the PrefServices facilities.  The format for the retained
// logs (ones that never got transmitted) is always the uncompressed textual
// representation.
//
// Logs fall into one of two categories: "initial logs," and "ongoing logs."
// There is at most one initial log sent for each complete run of the chromium
// product (from startup, to browser shutdown).  An initial log is generally
// transmitted some short time (1 minute?) after startup, and includes stats
// such as recent crash info, the number and types of plugins, etc.  The
// external server's response to the initial log conceptually tells this MS if
// it should continue transmitting logs (during this session). The server
// response can actually be much more detailed, and always includes (at a
// minimum) how often additional ongoing logs should be sent.
//
// After the above initial log, a series of ongoing logs will be transmitted.
// The first ongoing log actually begins to accumulate information stating when
// the MS was first constructed.  Note that even though the initial log is
// commonly sent a full minute after startup, the initial log does not include
// much in the way of user stats.   The most common interlog period (delay)
// is 20 minutes. That time period starts when the first user action causes a
// logging event.  This means that if there is no user action, there may be long
// periods without any (ongoing) log transmissions.  Ongoing logs typically
// contain very detailed records of user activities (ex: opened tab, closed
// tab, fetched URL, maximized window, etc.)  In addition, just before an
// ongoing log is closed out, a call is made to gather memory statistics.  Those
// memory statistics are deposited into a histogram, and the log finalization
// code is then called.  In the finalization, a call to a Histogram server
// acquires a list of all local histograms that have been flagged for upload
// to the UMA server.  The finalization also acquires a the most recent number
// of page loads, along with any counts of renderer or plugin crashes.
//
// When the browser shuts down, there will typically be a fragment of an ongoing
// log that has not yet been transmitted.  At shutdown time, that fragment
// is closed (including snapshotting histograms), and converted to text.  Note
// that memory stats are not gathered during shutdown, as gathering *might* be
// too time consuming.  The textual representation of the fragment of the
// ongoing log is then stored persistently as a string in the PrefServices, for
// potential transmission during a future run of the product.
//
// There are two slightly abnormal shutdown conditions.  There is a
// "disconnected scenario," and a "really fast startup and shutdown" scenario.
// In the "never connected" situation, the user has (during the running of the
// process) never established an internet connection.  As a result, attempts to
// transmit the initial log have failed, and a lot(?) of data has accumulated in
// the ongoing log (which didn't yet get closed, because there was never even a
// contemplation of sending it).  There is also a kindred "lost connection"
// situation, where a loss of connection prevented an ongoing log from being
// transmitted, and a (still open) log was stuck accumulating a lot(?) of data,
// while the earlier log retried its transmission.  In both of these
// disconnected situations, two logs need to be, and are, persistently stored
// for future transmission.
//
// The other unusual shutdown condition, termed "really fast startup and
// shutdown," involves the deliberate user termination of the process before
// the initial log is even formed or transmitted. In that situation, no logging
// is done, but the historical crash statistics remain (unlogged) for inclusion
// in a future run's initial log.  (i.e., we don't lose crash stats).
//
// With the above overview, we can now describe the state machine's various
// stats, based on the State enum specified in the state_ member.  Those states
// are:
//
//    INITIALIZED,            // Constructor was called.
//    PLUGIN_LIST_REQUESTED,  // Waiting for plugin list to be loaded.
//    PLUGIN_LIST_ARRIVED,    // Waiting for timer to send initial log.
//    INITIAL_LOG_READY,      // Initial log generated, and waiting for reply.
//    SEND_OLD_INITIAL_LOGS,  // Sending unsent logs from previous session.
//    SENDING_OLD_LOGS,       // Sending unsent logs from previous session.
//    SENDING_CURRENT_LOGS,   // Sending standard current logs as they accrue.
//
// In more detail, we have:
//
//    INITIALIZED,            // Constructor was called.
// The MS has been constructed, but has taken no actions to compose the
// initial log.
//
//    PLUGIN_LIST_REQUESTED,  // Waiting for plugin list to be loaded.
// Typically about 30 seconds after startup, a task is sent to a second thread
// to get the list of plugins.  That task will (when complete) make an async
// callback (via a Task) to indicate the completion.
//
//    PLUGIN_LIST_ARRIVED,    // Waiting for timer to send initial log.
// The callback has arrived, and it is now possible for an initial log to be
// created.  This callback typically arrives back less than one second after
// the task is dispatched.
//
//    INITIAL_LOG_READY,      // Initial log generated, and waiting for reply.
// This state is entered only after an initial log has been composed, and
// prepared for transmission.  It is also the case that any previously unsent
// logs have been loaded into instance variables for possible transmission.
//
//    SEND_OLD_INITIAL_LOGS,  // Sending unsent logs from previous session.
// This state indicates that the initial log for this session has been
// successfully sent and it is now time to send any "initial logs" that were
// saved from previous sessions.  Most commonly, there are none, but all old
// logs that were "initial logs" must be sent before this state is exited.
//
//    SENDING_OLD_LOGS,       // Sending unsent logs from previous session.
// This state indicates that there are no more unsent initial logs, and now any
// ongoing logs from previous sessions should be transmitted.  All such logs
// will be transmitted before exiting this state, and proceeding with ongoing
// logs from the current session (see next state).
//
//    SENDING_CURRENT_LOGS,   // Sending standard current logs as they accrue.
// Current logs are being accumulated.  Typically every 20 minutes a log is
// closed and finalized for transmission, at the same time as a new log is
// started.
//
// The progression through the above states is simple, and sequential, in the
// most common use cases.  States proceed from INITIAL to SENDING_CURRENT_LOGS,
// and remain in the latter until shutdown.
//
// The one unusual case is when the user asks that we stop logging.  When that
// happens, any pending (transmission in progress) log is pushed into the list
// of old unsent logs (the appropriate list, depending on whether it is an
// initial log, or an ongoing log).  An addition, any log that is currently
// accumulating is also finalized, and pushed into the unsent log list.  With
// those pushes performed, we regress back to the SEND_OLD_INITIAL_LOGS state in
// case the user enables log recording again during this session.  This way
// anything we have "pushed back" will be sent automatically if/when we progress
// back to SENDING_CURRENT_LOG state.
//
// Also note that whenever the member variables containing unsent logs are
// modified (i.e., when we send an old log), we mirror the list of logs into
// the PrefServices.  This ensures that IF we crash, we won't start up and
// retransmit our old logs again.
//
// Due to race conditions, it is always possible that a log file could be sent
// twice.  For example, if a log file is sent, but not yet acknowledged by
// the external server, and the user shuts down, then a copy of the log may be
// saved for re-transmission.  These duplicates could be filtered out server
// side, but are not expected to be a significant problem.
//
//
//------------------------------------------------------------------------------

#include "chrome/browser/metrics/metrics_service.h"

#if defined(OS_WIN)
#include <windows.h>
#include <objbase.h>
#endif

#include "base/file_path.h"
#include "base/histogram.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/load_notification_details.h"
#include "chrome/browser/memory_details.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/child_process_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/libxml_utils.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/render_messages.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "third_party/bzip2/bzlib.h"

#if defined(OS_POSIX)
// TODO(port): Move these headers above as they are ported.
#include "chrome/common/temp_scaffolding_stubs.h"
#else
#include "chrome/installer/util/google_update_settings.h"
#endif

using base::Time;
using base::TimeDelta;

// Check to see that we're being called on only one thread.
static bool IsSingleThreaded();

static const char kMetricsURL[] =
    "https://clients4.google.com/firefox/metrics/collect";

static const char kMetricsType[] = "application/vnd.mozilla.metrics.bz2";

// The delay, in seconds, after startup before sending the first log message.
static const int kInitialInterlogDuration = 60;  // one minute

// The default maximum number of events in a log uploaded to the UMA server.
static const int kInitialEventLimit = 2400;

// If an upload fails, and the transmission was over this byte count, then we
// will discard the log, and not try to retransmit it.  We also don't persist
// the log to the prefs for transmission during the next chrome session if this
// limit is exceeded.
static const int kUploadLogAvoidRetransmitSize = 50000;

// When we have logs from previous Chrome sessions to send, how long should we
// delay (in seconds) between each log transmission.
static const int kUnsentLogDelay = 15;  // 15 seconds

// Minimum time a log typically exists before sending, in seconds.
// This number is supplied by the server, but until we parse it out of a server
// response, we use this duration to specify how long we should wait before
// sending the next log.  If the channel is busy, such as when there is a
// failure during an attempt to transmit a previous log, then a log may wait
// (and continue to accrue now log entries) for a much greater period of time.
static const int kMinSecondsPerLog = 20 * 60;  // Twenty minutes.

// When we don't succeed at transmitting a log to a server, we progressively
// wait longer and longer before sending the next log.  This backoff process
// help reduce load on the server, and makes the amount of backoff vary between
// clients so that a collision (server overload?) on retransmit is less likely.
// The following is the constant we use to expand that inter-log duration.
static const double kBackoff = 1.1;
// We limit the maximum backoff to be no greater than some multiple of the
// default kMinSecondsPerLog.  The following is that maximum ratio.
static const int kMaxBackoff = 10;

// Interval, in seconds, between state saves.
static const int kSaveStateInterval = 5 * 60;  // five minutes

// The number of "initial" logs we're willing to save, and hope to send during
// a future Chrome session.  Initial logs contain crash stats, and are pretty
// small.
static const size_t kMaxInitialLogsPersisted = 20;

// The number of ongoing logs we're willing to save persistently, and hope to
// send during a this or future sessions.  Note that each log may be pretty
// large, as presumably the related "initial" log wasn't sent (probably nothing
// was, as the user was probably off-line).  As a result, the log probably kept
// accumulating while the "initial" log was stalled (pending_), and couldn't be
// sent.  As a result, we don't want to save too many of these mega-logs.
// A "standard shutdown" will create a small log, including just the data that
// was not yet been transmitted, and that is normal (to have exactly one
// ongoing_log_ at startup).
static const size_t kMaxOngoingLogsPersisted = 8;


// Handles asynchronous fetching of memory details.
// Will run the provided task after finished.
class MetricsMemoryDetails : public MemoryDetails {
 public:
  explicit MetricsMemoryDetails(Task* completion) : completion_(completion) {}

  virtual void OnDetailsAvailable() {
    MessageLoop::current()->PostTask(FROM_HERE, completion_);
  }

 private:
  Task* completion_;
  DISALLOW_EVIL_CONSTRUCTORS(MetricsMemoryDetails);
};

class MetricsService::GetPluginListTaskComplete : public Task {
  virtual void Run() {
    g_browser_process->metrics_service()->OnGetPluginListTaskComplete();
  }
};

class MetricsService::GetPluginListTask : public Task {
 public:
  explicit GetPluginListTask(MessageLoop* callback_loop)
      : callback_loop_(callback_loop) {}

  virtual void Run() {
    std::vector<WebPluginInfo> plugins;
    PluginService::GetInstance()->GetPlugins(false, &plugins);

    callback_loop_->PostTask(FROM_HERE, new GetPluginListTaskComplete());
  }

 private:
  MessageLoop* callback_loop_;
};

// static
void MetricsService::RegisterPrefs(PrefService* local_state) {
  DCHECK(IsSingleThreaded());
  local_state->RegisterStringPref(prefs::kMetricsClientID, L"");
  local_state->RegisterStringPref(prefs::kMetricsClientIDTimestamp, L"0");
  local_state->RegisterStringPref(prefs::kStabilityLaunchTimeSec, L"0");
  local_state->RegisterStringPref(prefs::kStabilityLastTimestampSec, L"0");
  local_state->RegisterStringPref(prefs::kStabilityUptimeSec, L"0");
  local_state->RegisterStringPref(prefs::kStabilityStatsVersion, L"");
  local_state->RegisterBooleanPref(prefs::kStabilityExitedCleanly, true);
  local_state->RegisterBooleanPref(prefs::kStabilitySessionEndCompleted, true);
  local_state->RegisterIntegerPref(prefs::kMetricsSessionID, -1);
  local_state->RegisterIntegerPref(prefs::kStabilityLaunchCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityCrashCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityIncompleteSessionEndCount,
                                   0);
  local_state->RegisterIntegerPref(prefs::kStabilityPageLoadCount, 0);
  local_state->RegisterIntegerPref(prefs::kSecurityRendererOnSboxDesktop, 0);
  local_state->RegisterIntegerPref(prefs::kSecurityRendererOnDefaultDesktop, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityRendererCrashCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityRendererHangCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityBreakpadRegistrationFail,
                                   0);
  local_state->RegisterIntegerPref(prefs::kStabilityBreakpadRegistrationSuccess,
                                   0);
  local_state->RegisterIntegerPref(prefs::kStabilityDebuggerPresent, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityDebuggerNotPresent, 0);

  local_state->RegisterDictionaryPref(prefs::kProfileMetrics);
  local_state->RegisterIntegerPref(prefs::kNumBookmarksOnBookmarkBar, 0);
  local_state->RegisterIntegerPref(prefs::kNumFoldersOnBookmarkBar, 0);
  local_state->RegisterIntegerPref(prefs::kNumBookmarksInOtherBookmarkFolder,
                                   0);
  local_state->RegisterIntegerPref(prefs::kNumFoldersInOtherBookmarkFolder, 0);
  local_state->RegisterIntegerPref(prefs::kNumKeywords, 0);
  local_state->RegisterListPref(prefs::kMetricsInitialLogs);
  local_state->RegisterListPref(prefs::kMetricsOngoingLogs);
}

// static
void MetricsService::DiscardOldStabilityStats(PrefService* local_state) {
  local_state->SetBoolean(prefs::kStabilityExitedCleanly, true);

  local_state->SetInteger(prefs::kStabilityIncompleteSessionEndCount, 0);
  local_state->SetInteger(prefs::kStabilityBreakpadRegistrationSuccess, 0);
  local_state->SetInteger(prefs::kStabilityBreakpadRegistrationFail, 0);
  local_state->SetInteger(prefs::kStabilityDebuggerPresent, 0);
  local_state->SetInteger(prefs::kStabilityDebuggerNotPresent, 0);

  local_state->SetInteger(prefs::kStabilityLaunchCount, 0);
  local_state->SetInteger(prefs::kStabilityCrashCount, 0);

  local_state->SetInteger(prefs::kStabilityPageLoadCount, 0);
  local_state->SetInteger(prefs::kStabilityRendererCrashCount, 0);
  local_state->SetInteger(prefs::kStabilityRendererHangCount, 0);

  local_state->SetInteger(prefs::kSecurityRendererOnSboxDesktop, 0);
  local_state->SetInteger(prefs::kSecurityRendererOnDefaultDesktop, 0);

  local_state->SetString(prefs::kStabilityUptimeSec, L"0");

  local_state->ClearPref(prefs::kStabilityPluginStats);
}

MetricsService::MetricsService()
    : recording_active_(false),
      reporting_active_(false),
      user_permits_upload_(false),
      server_permits_upload_(true),
      state_(INITIALIZED),
      pending_log_(NULL),
      pending_log_text_(""),
      current_fetch_(NULL),
      current_log_(NULL),
      idle_since_last_transmission_(false),
      next_window_id_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(log_sender_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(state_saver_factory_(this)),
      logged_samples_(),
      interlog_duration_(TimeDelta::FromSeconds(kInitialInterlogDuration)),
      log_event_limit_(kInitialEventLimit),
      timer_pending_(false) {
  DCHECK(IsSingleThreaded());
  InitializeMetricsState();
}

MetricsService::~MetricsService() {
  SetRecording(false);
  if (pending_log_) {
    delete pending_log_;
    pending_log_ = NULL;
  }
  if (current_log_) {
    delete current_log_;
    current_log_ = NULL;
  }
}

void MetricsService::SetUserPermitsUpload(bool enabled) {
  HandleIdleSinceLastTransmission(false);
  user_permits_upload_ = enabled;
}

void MetricsService::Start() {
  SetRecording(true);
  SetReporting(true);
}

void MetricsService::StartRecordingOnly() {
  SetRecording(true);
  SetReporting(false);
}

void MetricsService::Stop() {
  SetReporting(false);
  SetRecording(false);
}

void MetricsService::SetRecording(bool enabled) {
  DCHECK(IsSingleThreaded());

  if (enabled == recording_active_)
    return;

  if (enabled) {
    StartRecording();
    ListenerRegistration(true);
  } else {
    // Turn off all observers.
    ListenerRegistration(false);
    PushPendingLogsToUnsentLists();
    DCHECK(!pending_log());
    if (state_ > INITIAL_LOG_READY && unsent_logs())
      state_ = SEND_OLD_INITIAL_LOGS;
  }
  recording_active_ = enabled;
}

bool MetricsService::recording_active() const {
  DCHECK(IsSingleThreaded());
  return recording_active_;
}

void MetricsService::SetReporting(bool enable) {
  if (reporting_active_ != enable) {
    reporting_active_ = enable;
    if (reporting_active_)
      StartLogTransmissionTimer();
  }
}

bool MetricsService::reporting_active() const {
  DCHECK(IsSingleThreaded());
  return reporting_active_;
}

void MetricsService::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  DCHECK(current_log_);
  DCHECK(IsSingleThreaded());

  if (!CanLogNotification(type, source, details))
    return;

  switch (type.value) {
    case NotificationType::USER_ACTION:
        current_log_->RecordUserAction(*Details<const wchar_t*>(details).ptr());
      break;

    case NotificationType::BROWSER_OPENED:
    case NotificationType::BROWSER_CLOSED:
      LogWindowChange(type, source, details);
      break;

    case NotificationType::TAB_PARENTED:
    case NotificationType::TAB_CLOSING:
      LogWindowChange(type, source, details);
      break;

    case NotificationType::LOAD_STOP:
      LogLoadComplete(type, source, details);
      break;

    case NotificationType::LOAD_START:
      LogLoadStarted();
      break;

    case NotificationType::RENDERER_PROCESS_TERMINATED:
      if (!*Details<bool>(details).ptr())
        LogRendererCrash();
      break;

    case NotificationType::RENDERER_PROCESS_HANG:
      LogRendererHang();
      break;

    case NotificationType::RENDERER_PROCESS_IN_SBOX:
      LogRendererInSandbox(*Details<bool>(details).ptr());
      break;

    case NotificationType::CHILD_PROCESS_HOST_CONNECTED:
    case NotificationType::CHILD_PROCESS_CRASHED:
    case NotificationType::CHILD_INSTANCE_CREATED:
      LogChildProcessChange(type, source, details);
      break;

    case NotificationType::TEMPLATE_URL_MODEL_LOADED:
      LogKeywords(Source<TemplateURLModel>(source).ptr());
      break;

    case NotificationType::OMNIBOX_OPENED_URL:
      current_log_->RecordOmniboxOpenedURL(
          *Details<AutocompleteLog>(details).ptr());
      break;

    case NotificationType::BOOKMARK_MODEL_LOADED:
      LogBookmarks(Source<Profile>(source)->GetBookmarkModel());
      break;

    default:
      NOTREACHED();
      break;
  }

  HandleIdleSinceLastTransmission(false);

  if (current_log_)
    DLOG(INFO) << "METRICS: NUMBER OF EVENTS = " << current_log_->num_events();
}

void MetricsService::HandleIdleSinceLastTransmission(bool in_idle) {
  // If there wasn't a lot of action, maybe the computer was asleep, in which
  // case, the log transmissions should have stopped.  Here we start them up
  // again.
  if (!in_idle && idle_since_last_transmission_)
    StartLogTransmissionTimer();
  idle_since_last_transmission_ = in_idle;
}

void MetricsService::RecordCleanShutdown() {
  RecordBooleanPrefValue(prefs::kStabilityExitedCleanly, true);
}

void MetricsService::RecordStartOfSessionEnd() {
  RecordBooleanPrefValue(prefs::kStabilitySessionEndCompleted, false);
}

void MetricsService::RecordCompletedSessionEnd() {
  RecordBooleanPrefValue(prefs::kStabilitySessionEndCompleted, true);
}

void MetricsService:: RecordBreakpadRegistration(bool success) {
  if (!success)
    IncrementPrefValue(prefs::kStabilityBreakpadRegistrationFail);
  else
    IncrementPrefValue(prefs::kStabilityBreakpadRegistrationSuccess);
}

void MetricsService::RecordBreakpadHasDebugger(bool has_debugger) {
  if (!has_debugger)
    IncrementPrefValue(prefs::kStabilityDebuggerNotPresent);
  else
    IncrementPrefValue(prefs::kStabilityDebuggerPresent);
}

//------------------------------------------------------------------------------
// private methods
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Initialization methods

void MetricsService::InitializeMetricsState() {
  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);

  if (WideToUTF8(pref->GetString(prefs::kStabilityStatsVersion)) !=
      MetricsLog::GetVersionString()) {
    // This is a new version, so we don't want to confuse the stats about the
    // old version with info that we upload.
    DiscardOldStabilityStats(pref);
    pref->SetString(prefs::kStabilityStatsVersion,
                    UTF8ToWide(MetricsLog::GetVersionString()));
  }

  client_id_ = WideToUTF8(pref->GetString(prefs::kMetricsClientID));
  if (client_id_.empty()) {
    client_id_ = GenerateClientID();
    pref->SetString(prefs::kMetricsClientID, UTF8ToWide(client_id_));

    // Might as well make a note of how long this ID has existed
    pref->SetString(prefs::kMetricsClientIDTimestamp,
                    Int64ToWString(Time::Now().ToTimeT()));
  }

  // Update session ID
  session_id_ = pref->GetInteger(prefs::kMetricsSessionID);
  ++session_id_;
  pref->SetInteger(prefs::kMetricsSessionID, session_id_);

  // Stability bookkeeping
  IncrementPrefValue(prefs::kStabilityLaunchCount);

  if (!pref->GetBoolean(prefs::kStabilityExitedCleanly)) {
    IncrementPrefValue(prefs::kStabilityCrashCount);
  }

  // This will be set to 'true' if we exit cleanly.
  pref->SetBoolean(prefs::kStabilityExitedCleanly, false);

  if (!pref->GetBoolean(prefs::kStabilitySessionEndCompleted)) {
    IncrementPrefValue(prefs::kStabilityIncompleteSessionEndCount);
  }
  // This is marked false when we get a WM_ENDSESSION.
  pref->SetBoolean(prefs::kStabilitySessionEndCompleted, true);

  int64 last_start_time = StringToInt64(
      WideToUTF16Hack(pref->GetString(prefs::kStabilityLaunchTimeSec)));
  int64 last_end_time = StringToInt64(
      WideToUTF16Hack(pref->GetString(prefs::kStabilityLastTimestampSec)));
  int64 uptime = StringToInt64(
      WideToUTF16Hack(pref->GetString(prefs::kStabilityUptimeSec)));

  if (last_start_time && last_end_time) {
    // TODO(JAR): Exclude sleep time.  ... which must be gathered in UI loop.
    uptime += last_end_time - last_start_time;
    pref->SetString(prefs::kStabilityUptimeSec, Int64ToWString(uptime));
  }
  pref->SetString(prefs::kStabilityLaunchTimeSec,
                  Int64ToWString(Time::Now().ToTimeT()));

  // Save profile metrics.
  PrefService* prefs = g_browser_process->local_state();
  if (prefs) {
    // Remove the current dictionary and store it for use when sending data to
    // server. By removing the value we prune potentially dead profiles
    // (and keys). All valid values are added back once services startup.
    const DictionaryValue* profile_dictionary =
        prefs->GetDictionary(prefs::kProfileMetrics);
    if (profile_dictionary) {
      // Do a deep copy of profile_dictionary since ClearPref will delete it.
      profile_dictionary_.reset(static_cast<DictionaryValue*>(
          profile_dictionary->DeepCopy()));
      prefs->ClearPref(prefs::kProfileMetrics);
    }
  }

  // Kick off the process of saving the state (so the uptime numbers keep
  // getting updated) every n minutes.
  ScheduleNextStateSave();
}

void MetricsService::OnGetPluginListTaskComplete() {
  DCHECK(state_ == PLUGIN_LIST_REQUESTED);
  if (state_ == PLUGIN_LIST_REQUESTED)
    state_ = PLUGIN_LIST_ARRIVED;
}

std::string MetricsService::GenerateClientID() {
#if defined(OS_WIN)
  const int kGUIDSize = 39;

  GUID guid;
  HRESULT guid_result = CoCreateGuid(&guid);
  DCHECK(SUCCEEDED(guid_result));

  std::wstring guid_string;
  int result = StringFromGUID2(guid,
                               WriteInto(&guid_string, kGUIDSize), kGUIDSize);
  DCHECK(result == kGUIDSize);

  return WideToUTF8(guid_string.substr(1, guid_string.length() - 2));
#else
  // TODO(port): Implement for Mac and linux.
  // Rather than actually implementing a random source, might this be a good
  // time to implement http://code.google.com/p/chromium/issues/detail?id=2278
  // ?  I think so!
  NOTIMPLEMENTED();
  return std::string();
#endif
}


//------------------------------------------------------------------------------
// State save methods

void MetricsService::ScheduleNextStateSave() {
  state_saver_factory_.RevokeAll();

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      state_saver_factory_.NewRunnableMethod(&MetricsService::SaveLocalState),
      kSaveStateInterval * 1000);
}

void MetricsService::SaveLocalState() {
  PrefService* pref = g_browser_process->local_state();
  if (!pref) {
    NOTREACHED();
    return;
  }

  RecordCurrentState(pref);
  pref->ScheduleSavePersistentPrefs(g_browser_process->file_thread());

  // TODO(jar): Does this run down the batteries????
  ScheduleNextStateSave();
}


//------------------------------------------------------------------------------
// Recording control methods

void MetricsService::StartRecording() {
  if (current_log_)
    return;

  current_log_ = new MetricsLog(client_id_, session_id_);
  if (state_ == INITIALIZED) {
    // We only need to schedule that run once.
    state_ = PLUGIN_LIST_REQUESTED;

    // Make sure the plugin list is loaded before the inital log is sent, so
    // that the main thread isn't blocked generating the list.
    g_browser_process->file_thread()->message_loop()->PostDelayedTask(FROM_HERE,
        new GetPluginListTask(MessageLoop::current()),
        kInitialInterlogDuration * 1000 / 2);
  }
}

void MetricsService::StopRecording(MetricsLog** log) {
  if (!current_log_)
    return;

  // TODO(jar): Integrate bounds on log recording more consistently, so that we
  // can stop recording logs that are too big much sooner.
  if (current_log_->num_events() > log_event_limit_) {
    UMA_HISTOGRAM_COUNTS("UMA.Discarded Log Events",
                         current_log_->num_events());
    current_log_->CloseLog();
    delete current_log_;
    current_log_ = NULL;
    StartRecording();  // Start trivial log to hold our histograms.
  }

  // Put incremental data (histogram deltas, and realtime stats deltas) at the
  // end of all log transmissions (initial log handles this separately).
  // Don't bother if we're going to discard current_log_.
  if (log) {
    current_log_->RecordIncrementalStabilityElements();
    RecordCurrentHistograms();
  }

  current_log_->CloseLog();
  if (log)
    *log = current_log_;
  else
    delete current_log_;
  current_log_ = NULL;
}

void MetricsService::ListenerRegistration(bool start_listening) {
  AddOrRemoveObserver(this, NotificationType::BROWSER_OPENED, start_listening);
  AddOrRemoveObserver(this, NotificationType::BROWSER_CLOSED, start_listening);
  AddOrRemoveObserver(this, NotificationType::USER_ACTION, start_listening);
  AddOrRemoveObserver(this, NotificationType::TAB_PARENTED, start_listening);
  AddOrRemoveObserver(this, NotificationType::TAB_CLOSING, start_listening);
  AddOrRemoveObserver(this, NotificationType::LOAD_START, start_listening);
  AddOrRemoveObserver(this, NotificationType::LOAD_STOP, start_listening);
  AddOrRemoveObserver(this, NotificationType::RENDERER_PROCESS_IN_SBOX,
                      start_listening);
  AddOrRemoveObserver(this, NotificationType::RENDERER_PROCESS_TERMINATED,
                      start_listening);
  AddOrRemoveObserver(this, NotificationType::RENDERER_PROCESS_HANG,
                      start_listening);
  AddOrRemoveObserver(this, NotificationType::CHILD_PROCESS_HOST_CONNECTED,
                      start_listening);
  AddOrRemoveObserver(this, NotificationType::CHILD_INSTANCE_CREATED,
                      start_listening);
  AddOrRemoveObserver(this, NotificationType::CHILD_PROCESS_CRASHED,
                      start_listening);
  AddOrRemoveObserver(this, NotificationType::TEMPLATE_URL_MODEL_LOADED,
                      start_listening);
  AddOrRemoveObserver(this, NotificationType::OMNIBOX_OPENED_URL,
                      start_listening);
  AddOrRemoveObserver(this, NotificationType::BOOKMARK_MODEL_LOADED,
                      start_listening);
}

// static
void MetricsService::AddOrRemoveObserver(NotificationObserver* observer,
                                         NotificationType type,
                                         bool is_add) {
  NotificationService* service = NotificationService::current();

  if (is_add)
    service->AddObserver(observer, type, NotificationService::AllSources());
  else
    service->RemoveObserver(observer, type, NotificationService::AllSources());
}

void MetricsService::PushPendingLogsToUnsentLists() {
  if (state_ < INITIAL_LOG_READY)
    return;  // We didn't and still don't have time to get plugin list etc.

  if (pending_log()) {
    PreparePendingLogText();
    if (state_ == INITIAL_LOG_READY) {
      // We may race here, and send second copy of initial log later.
      unsent_initial_logs_.push_back(pending_log_text_);
      state_ = SEND_OLD_INITIAL_LOGS;
    } else {
      // TODO(jar): Verify correctness in other states, including sending unsent
      // initial logs.
      PushPendingLogTextToUnsentOngoingLogs();
    }
    DiscardPendingLog();
  }
  DCHECK(!pending_log());
  StopRecording(&pending_log_);
  PreparePendingLogText();
  PushPendingLogTextToUnsentOngoingLogs();
  DiscardPendingLog();
  StoreUnsentLogs();
}

void MetricsService::PushPendingLogTextToUnsentOngoingLogs() {
  // If UMA response told us not to upload, there's no need to save the pending
  // log.  It wasn't supposed to be uploaded anyway.
  if (!server_permits_upload_)
    return;

  if (pending_log_text_.length() >
      static_cast<size_t>(kUploadLogAvoidRetransmitSize)) {
    UMA_HISTOGRAM_COUNTS("UMA.Large Accumulated Log Not Persisted",
                         static_cast<int>(pending_log_text_.length()));
    return;
  }
  unsent_ongoing_logs_.push_back(pending_log_text_);
}

//------------------------------------------------------------------------------
// Transmission of logs methods

void MetricsService::StartLogTransmissionTimer() {
  // If we're not reporting, there's no point in starting a log transmission
  // timer.
  if (!reporting_active())
    return;

  if (!current_log_)
    return;  // Recorder is shutdown.

  // If there is already a timer running, we leave it running.
  // If timer_pending is true because the fetch is waiting for a response,
  // we return for now and let the response handler start the timer.
  if (timer_pending_)
    return;

  // Before starting the timer, set timer_pending_ to true.
  timer_pending_ = true;

  // Right before the UMA transmission gets started, there's one more thing we'd
  // like to record: the histogram of memory usage, so we spawn a task to
  // collect the memory details and when that task is finished, we arrange for
  // TryToStartTransmission to take over.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      log_sender_factory_.
          NewRunnableMethod(&MetricsService::CollectMemoryDetails),
      static_cast<int>(interlog_duration_.InMilliseconds()));
}

void MetricsService::TryToStartTransmission() {
  DCHECK(IsSingleThreaded());

  // This function should only be called via timer, so timer_pending_
  // should be true.
  DCHECK(timer_pending_);
  timer_pending_ = false;

  DCHECK(!current_fetch_.get());

  // If we're getting no notifications, then the log won't have much in it, and
  // it's possible the computer is about to go to sleep, so don't upload and
  // don't restart the transmission timer.
  if (idle_since_last_transmission_)
    return;

  // If somehow there is a fetch in progress, we return setting timer_pending_
  // to true and hope things work out.
  if (current_fetch_.get()) {
    timer_pending_ = true;
    return;
  }

  // If uploads are forbidden by UMA response, there's no point in keeping
  // the current_log_, and the more often we delete it, the less likely it is
  // to expand forever.
  if (!server_permits_upload_ && current_log_) {
    StopRecording(NULL);
    StartRecording();
  }

  if (!current_log_)
    return;  // Logging was disabled.
  if (!reporting_active())
    return;  // Don't do work if we're not going to send anything now.

  MakePendingLog();

  // MakePendingLog should have put something in the pending log, if it didn't,
  // we start the timer again, return and hope things work out.
  if (!pending_log()) {
    StartLogTransmissionTimer();
    return;
  }

  // If we're not supposed to upload any UMA data because the response or the
  // user said so, cancel the upload at this point, but start the timer.
  if (!TransmissionPermitted()) {
    DiscardPendingLog();
    StartLogTransmissionTimer();
    return;
  }

  PrepareFetchWithPendingLog();

  if (!current_fetch_.get()) {
    // Compression failed, and log discarded :-/.
    DiscardPendingLog();
    StartLogTransmissionTimer();  // Maybe we'll do better next time
    // TODO(jar): If compression failed, we should have created a tiny log and
    // compressed that, so that we can signal that we're losing logs.
    return;
  }

  DCHECK(!timer_pending_);

  // The URL fetch is a like timer in that after a while we get called back
  // so we set timer_pending_ true just as we start the url fetch.
  timer_pending_ = true;
  current_fetch_->Start();

  HandleIdleSinceLastTransmission(true);
}


void MetricsService::MakePendingLog() {
  if (pending_log())
    return;

  switch (state_) {
    case INITIALIZED:
    case PLUGIN_LIST_REQUESTED:  // We should be further along by now.
      DCHECK(false);
      return;

    case PLUGIN_LIST_ARRIVED:
      // We need to wait for the initial log to be ready before sending
      // anything, because the server will tell us whether it wants to hear
      // from us.
      PrepareInitialLog();
      DCHECK(state_ == PLUGIN_LIST_ARRIVED);
      RecallUnsentLogs();
      state_ = INITIAL_LOG_READY;
      break;

    case SEND_OLD_INITIAL_LOGS:
      if (!unsent_initial_logs_.empty()) {
        pending_log_text_ = unsent_initial_logs_.back();
        break;
      }
      state_ = SENDING_OLD_LOGS;
      // Fall through.

    case SENDING_OLD_LOGS:
      if (!unsent_ongoing_logs_.empty()) {
        pending_log_text_ = unsent_ongoing_logs_.back();
        break;
      }
      state_ = SENDING_CURRENT_LOGS;
      // Fall through.

    case SENDING_CURRENT_LOGS:
      StopRecording(&pending_log_);
      StartRecording();
      break;

    default:
      DCHECK(false);
      return;
  }

  DCHECK(pending_log());
}

bool MetricsService::TransmissionPermitted() const {
  // If the user forbids uploading that's they're business, and we don't upload
  // anything.  If the server forbids uploading, that's our business, so we take
  // that to mean it forbids current logs, but we still send up the inital logs
  // and any old logs.
  if (!user_permits_upload_)
    return false;
  if (server_permits_upload_)
    return true;

  switch (state_) {
    case INITIAL_LOG_READY:
    case SEND_OLD_INITIAL_LOGS:
    case SENDING_OLD_LOGS:
      return true;

    case SENDING_CURRENT_LOGS:
    default:
      return false;
  }
}

void MetricsService::CollectMemoryDetails() {
  Task* task = log_sender_factory_.
      NewRunnableMethod(&MetricsService::TryToStartTransmission);
  MetricsMemoryDetails* details = new MetricsMemoryDetails(task);
  details->StartFetch();

  // Collect WebCore cache information to put into a histogram.
  for (RenderProcessHost::iterator it = RenderProcessHost::begin();
       it != RenderProcessHost::end(); ++it) {
    it->second->Send(new ViewMsg_GetCacheResourceStats());
  }
}

void MetricsService::PrepareInitialLog() {
  DCHECK(state_ == PLUGIN_LIST_ARRIVED);
  std::vector<WebPluginInfo> plugins;
  PluginService::GetInstance()->GetPlugins(false, &plugins);

  MetricsLog* log = new MetricsLog(client_id_, session_id_);
  log->RecordEnvironment(plugins, profile_dictionary_.get());

  // Histograms only get written to current_log_, so setup for the write.
  MetricsLog* save_log = current_log_;
  current_log_ = log;
  RecordCurrentHistograms();  // Into current_log_... which is really log.
  current_log_ = save_log;

  log->CloseLog();
  DCHECK(!pending_log());
  pending_log_ = log;
}

void MetricsService::RecallUnsentLogs() {
  DCHECK(unsent_initial_logs_.empty());
  DCHECK(unsent_ongoing_logs_.empty());

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  ListValue* unsent_initial_logs = local_state->GetMutableList(
      prefs::kMetricsInitialLogs);
  for (ListValue::iterator it = unsent_initial_logs->begin();
      it != unsent_initial_logs->end(); ++it) {
    std::string log;
    (*it)->GetAsString(&log);
    unsent_initial_logs_.push_back(log);
  }

  ListValue* unsent_ongoing_logs = local_state->GetMutableList(
      prefs::kMetricsOngoingLogs);
  for (ListValue::iterator it = unsent_ongoing_logs->begin();
      it != unsent_ongoing_logs->end(); ++it) {
    std::string log;
    (*it)->GetAsString(&log);
    unsent_ongoing_logs_.push_back(log);
  }
}

void MetricsService::StoreUnsentLogs() {
  if (state_ < INITIAL_LOG_READY)
    return;  // We never Recalled the prior unsent logs.

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  ListValue* unsent_initial_logs = local_state->GetMutableList(
      prefs::kMetricsInitialLogs);
  unsent_initial_logs->Clear();
  size_t start = 0;
  if (unsent_initial_logs_.size() > kMaxInitialLogsPersisted)
    start = unsent_initial_logs_.size() - kMaxInitialLogsPersisted;
  for (size_t i = start; i < unsent_initial_logs_.size(); ++i)
    unsent_initial_logs->Append(
        Value::CreateStringValue(unsent_initial_logs_[i]));

  ListValue* unsent_ongoing_logs = local_state->GetMutableList(
      prefs::kMetricsOngoingLogs);
  unsent_ongoing_logs->Clear();
  start = 0;
  if (unsent_ongoing_logs_.size() > kMaxOngoingLogsPersisted)
    start = unsent_ongoing_logs_.size() - kMaxOngoingLogsPersisted;
  for (size_t i = start; i < unsent_ongoing_logs_.size(); ++i)
    unsent_ongoing_logs->Append(
        Value::CreateStringValue(unsent_ongoing_logs_[i]));
}

void MetricsService::PreparePendingLogText() {
  DCHECK(pending_log());
  if (!pending_log_text_.empty())
    return;
  int original_size = pending_log_->GetEncodedLogSize();
  pending_log_->GetEncodedLog(WriteInto(&pending_log_text_, original_size),
                              original_size);
}

void MetricsService::PrepareFetchWithPendingLog() {
  DCHECK(pending_log());
  DCHECK(!current_fetch_.get());
  PreparePendingLogText();
  DCHECK(!pending_log_text_.empty());

  // Allow security conscious users to see all metrics logs that we send.
  LOG(INFO) << "METRICS LOG: " << pending_log_text_;

  std::string compressed_log;
  if (!Bzip2Compress(pending_log_text_, &compressed_log)) {
    NOTREACHED() << "Failed to compress log for transmission.";
    DiscardPendingLog();
    StartLogTransmissionTimer();  // Maybe we'll do better on next log :-/.
    return;
  }

  current_fetch_.reset(new URLFetcher(GURL(kMetricsURL), URLFetcher::POST,
                                      this));
  current_fetch_->set_request_context(Profile::GetDefaultRequestContext());
  current_fetch_->set_upload_data(kMetricsType, compressed_log);
}

void MetricsService::DiscardPendingLog() {
  if (pending_log_) {  // Shutdown might have deleted it!
    delete pending_log_;
    pending_log_ = NULL;
  }
  pending_log_text_.clear();
}

// This implementation is based on the Firefox MetricsService implementation.
bool MetricsService::Bzip2Compress(const std::string& input,
                                   std::string* output) {
  bz_stream stream = {0};
  // As long as our input is smaller than the bzip2 block size, we should get
  // the best compression.  For example, if your input was 250k, using a block
  // size of 300k or 500k should result in the same compression ratio.  Since
  // our data should be under 100k, using the minimum block size of 100k should
  // allocate less temporary memory, but result in the same compression ratio.
  int result = BZ2_bzCompressInit(&stream,
                                  1,   // 100k (min) block size
                                  0,   // quiet
                                  0);  // default "work factor"
  if (result != BZ_OK) {  // out of memory?
    return false;
  }

  output->clear();

  stream.next_in = const_cast<char*>(input.data());
  stream.avail_in = static_cast<int>(input.size());
  // NOTE: we don't need a BZ_RUN phase since our input buffer contains
  //       the entire input
  do {
    output->resize(output->size() + 1024);
    stream.next_out = &((*output)[stream.total_out_lo32]);
    stream.avail_out = static_cast<int>(output->size()) - stream.total_out_lo32;
    result = BZ2_bzCompress(&stream, BZ_FINISH);
  } while (result == BZ_FINISH_OK);
  if (result != BZ_STREAM_END)  // unknown failure?
    return false;
  result = BZ2_bzCompressEnd(&stream);
  DCHECK(result == BZ_OK);

  output->resize(stream.total_out_lo32);

  return true;
}

static const char* StatusToString(const URLRequestStatus& status) {
  switch (status.status()) {
    case URLRequestStatus::SUCCESS:
      return "SUCCESS";

    case URLRequestStatus::IO_PENDING:
      return "IO_PENDING";

    case URLRequestStatus::HANDLED_EXTERNALLY:
      return "HANDLED_EXTERNALLY";

    case URLRequestStatus::CANCELED:
      return "CANCELED";

    case URLRequestStatus::FAILED:
      return "FAILED";

    default:
      NOTREACHED();
      return "Unknown";
  }
}

void MetricsService::OnURLFetchComplete(const URLFetcher* source,
                                        const GURL& url,
                                        const URLRequestStatus& status,
                                        int response_code,
                                        const ResponseCookies& cookies,
                                        const std::string& data) {
  DCHECK(timer_pending_);
  timer_pending_ = false;
  DCHECK(current_fetch_.get());
  current_fetch_.reset(NULL);  // We're not allowed to re-use it.

  // Confirm send so that we can move on.
  LOG(INFO) << "METRICS RESPONSE CODE: " << response_code << " status=" <<
      StatusToString(status);

  // Provide boolean for error recovery (allow us to ignore response_code).
  bool discard_log = false;

  if (response_code != 200 &&
      pending_log_text_.length() >
      static_cast<size_t>(kUploadLogAvoidRetransmitSize)) {
    UMA_HISTOGRAM_COUNTS("UMA.Large Rejected Log was Discarded",
                         static_cast<int>(pending_log_text_.length()));
    discard_log = true;
  } else if (response_code == 400) {
    // Bad syntax.  Retransmission won't work.
    UMA_HISTOGRAM_COUNTS("UMA.Unacceptable_Log_Discarded", state_);
    discard_log = true;
  }

  if (response_code != 200 && !discard_log) {
    LOG(INFO) << "METRICS: transmission attempt returned a failure code: "
      << response_code << ". Verify network connectivity";
    HandleBadResponseCode();
  } else {  // Successful receipt (or we are discarding log).
    LOG(INFO) << "METRICS RESPONSE DATA: " << data;
    switch (state_) {
      case INITIAL_LOG_READY:
        state_ = SEND_OLD_INITIAL_LOGS;
        break;

      case SEND_OLD_INITIAL_LOGS:
        DCHECK(!unsent_initial_logs_.empty());
        unsent_initial_logs_.pop_back();
        StoreUnsentLogs();
        break;

      case SENDING_OLD_LOGS:
        DCHECK(!unsent_ongoing_logs_.empty());
        unsent_ongoing_logs_.pop_back();
        StoreUnsentLogs();
        break;

      case SENDING_CURRENT_LOGS:
        break;

      default:
        DCHECK(false);
        break;
    }

    DiscardPendingLog();
    // Since we sent a log, make sure our in-memory state is recorded to disk.
    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
    if (local_state)
      local_state->ScheduleSavePersistentPrefs(
          g_browser_process->file_thread());

    // Provide a default (free of exponetial backoff, other varances) in case
    // the server does not specify a value.
    interlog_duration_ = TimeDelta::FromSeconds(kMinSecondsPerLog);

    GetSettingsFromResponseData(data);
    // Override server specified interlog delay if there are unsent logs to
    // transmit.
    if (unsent_logs()) {
      DCHECK(state_ < SENDING_CURRENT_LOGS);
      interlog_duration_ = TimeDelta::FromSeconds(kUnsentLogDelay);
    }
  }

  StartLogTransmissionTimer();
}

void MetricsService::HandleBadResponseCode() {
  LOG(INFO) << "Verify your metrics logs are formatted correctly.  "
      "Verify server is active at " << kMetricsURL;
  if (!pending_log()) {
    LOG(INFO) << "METRICS: Recorder shutdown during log transmission.";
  } else {
    // Send progressively less frequently.
    DCHECK(kBackoff > 1.0);
    interlog_duration_ = TimeDelta::FromMicroseconds(
        static_cast<int64>(kBackoff * interlog_duration_.InMicroseconds()));

    if (kMaxBackoff * TimeDelta::FromSeconds(kMinSecondsPerLog) <
        interlog_duration_) {
      interlog_duration_ = kMaxBackoff *
          TimeDelta::FromSeconds(kMinSecondsPerLog);
    }

    LOG(INFO) << "METRICS: transmission retry being scheduled in " <<
        interlog_duration_.InSeconds() << " seconds for " <<
        pending_log_text_;
  }
}

void MetricsService::GetSettingsFromResponseData(const std::string& data) {
  // We assume that the file is structured as a block opened by <response>
  // and that inside response, there is a block opened by tag <chrome_config>
  // other tags are ignored for now except the content of <chrome_config>.
  LOG(INFO) << "METRICS: getting settings from response data: " << data;

  int data_size = static_cast<int>(data.size());
  if (data_size < 0) {
    LOG(INFO) << "METRICS: server response data bad size: " << data_size <<
      "; aborting extraction of settings";
    return;
  }
  xmlDocPtr doc = xmlReadMemory(data.c_str(), data_size, "", NULL, 0);
  DCHECK(doc);
  // If the document is malformed, we just use the settings that were there.
  if (!doc) {
    LOG(INFO) << "METRICS: reading xml from server response data failed";
    return;
  }

  xmlNodePtr top_node = xmlDocGetRootElement(doc), chrome_config_node = NULL;
  // Here, we find the chrome_config node by name.
  for (xmlNodePtr p = top_node->children; p; p = p->next) {
    if (xmlStrEqual(p->name, BAD_CAST "chrome_config")) {
      chrome_config_node = p;
      break;
    }
  }
  // If the server data is formatted wrong and there is no
  // config node where we expect, we just drop out.
  if (chrome_config_node != NULL)
    GetSettingsFromChromeConfigNode(chrome_config_node);
  xmlFreeDoc(doc);
}

void MetricsService::GetSettingsFromChromeConfigNode(
    xmlNodePtr chrome_config_node) {
  // Iterate through all children of the config node.
  for (xmlNodePtr current_node = chrome_config_node->children;
       current_node;
       current_node = current_node->next) {
    // If we find the upload tag, we appeal to another function
    // GetSettingsFromUploadNode to read all the data in it.
    if (xmlStrEqual(current_node->name, BAD_CAST "upload")) {
      GetSettingsFromUploadNode(current_node);
      continue;
    }
  }
}

void MetricsService::InheritedProperties::OverwriteWhereNeeded(
    xmlNodePtr node) {
  xmlChar* salt_value = xmlGetProp(node, BAD_CAST "salt");
  if (salt_value)  // If the property isn't there, xmlGetProp returns NULL.
      salt = atoi(reinterpret_cast<char*>(salt_value));
  // If the property isn't there, we keep the value the property had before

  xmlChar* denominator_value = xmlGetProp(node, BAD_CAST "denominator");
  if (denominator_value)
     denominator = atoi(reinterpret_cast<char*>(denominator_value));
}

void MetricsService::GetSettingsFromUploadNode(xmlNodePtr upload_node) {
  InheritedProperties props;
  GetSettingsFromUploadNodeRecursive(upload_node, props, "", true);
}

void MetricsService::GetSettingsFromUploadNodeRecursive(
    xmlNodePtr node,
    InheritedProperties props,
    std::string path_prefix,
    bool uploadOn) {
  props.OverwriteWhereNeeded(node);

  // The bool uploadOn is set to true if the data represented by current
  // node should be uploaded. This gets inherited in the tree; the children
  // of a node that has already been rejected for upload get rejected for
  // upload.
  uploadOn = uploadOn && NodeProbabilityTest(node, props);

  // The path is a / separated list of the node names ancestral to the current
  // one. So, if you want to check if the current node has a certain name,
  // compare to name.  If you want to check if it is a certan tag at a certain
  // place in the tree, compare to the whole path.
  std::string name = std::string(reinterpret_cast<const char*>(node->name));
  std::string path = path_prefix + "/" + name;

  if (path == "/upload") {
    xmlChar* upload_interval_val = xmlGetProp(node, BAD_CAST "interval");
    if (upload_interval_val) {
      interlog_duration_ = TimeDelta::FromSeconds(
          atoi(reinterpret_cast<char*>(upload_interval_val)));
    }

    server_permits_upload_ = uploadOn;
  }
  if (path == "/upload/logs") {
    xmlChar* log_event_limit_val = xmlGetProp(node, BAD_CAST "event_limit");
    if (log_event_limit_val)
      log_event_limit_ = atoi(reinterpret_cast<char*>(log_event_limit_val));
  }
  if (name == "histogram") {
    xmlChar* type_value = xmlGetProp(node, BAD_CAST "type");
    if (type_value) {
      std::string type = (reinterpret_cast<char*>(type_value));
      if (uploadOn)
        histograms_to_upload_.insert(type);
      else
        histograms_to_omit_.insert(type);
    }
  }
  if (name == "log") {
    xmlChar* type_value = xmlGetProp(node, BAD_CAST "type");
    if (type_value) {
      std::string type = (reinterpret_cast<char*>(type_value));
      if (uploadOn)
        logs_to_upload_.insert(type);
      else
        logs_to_omit_.insert(type);
    }
  }

  // Recursive call.  If the node is a leaf i.e. if it ends in a "/>", then it
  // doesn't have children, so node->children is NULL, and this loop doesn't
  // call (that's how the recursion ends).
  for (xmlNodePtr child_node = node->children;
       child_node;
       child_node = child_node->next) {
    GetSettingsFromUploadNodeRecursive(child_node, props, path, uploadOn);
  }
}

bool MetricsService::NodeProbabilityTest(xmlNodePtr node,
                                         InheritedProperties props) const {
  // Default value of probability on any node is 1, but recall that
  // its parents can already have been rejected for upload.
  double probability = 1;

  // If a probability is specified in the node, we use it instead.
  xmlChar* probability_value = xmlGetProp(node, BAD_CAST "probability");
  if (probability_value)
    probability = atoi(reinterpret_cast<char*>(probability_value));

  return ProbabilityTest(probability, props.salt, props.denominator);
}

bool MetricsService::ProbabilityTest(double probability,
                                     int salt,
                                     int denominator) const {
  // Okay, first we figure out how many of the digits of the
  // client_id_ we need in order to make a nice pseudorandomish
  // number in the range [0,denominator).  Too many digits is
  // fine.

  // n is the length of the client_id_ string
  size_t n = client_id_.size();

  // idnumber is a positive integer generated from the client_id_.
  // It plus salt is going to give us our pseudorandom number.
  int idnumber = 0;
  const char* client_id_c_str = client_id_.c_str();

  // Here we hash the relevant digits of the client_id_
  // string somehow to get a big integer idnumber (could be negative
  // from wraparound)
  int big = 1;
  for (size_t j = n - 1; j >= 0; --j) {
    idnumber += static_cast<int>(client_id_c_str[j]) * big;
    big *= 10;
  }

  // Mod id number by denominator making sure to get a non-negative
  // answer.
  idnumber = ((idnumber % denominator) + denominator) % denominator;

  // ((idnumber + salt) % denominator) / denominator is in the range [0,1]
  // if it's less than probability we call that an affirmative coin
  // toss.
  return static_cast<double>((idnumber + salt) % denominator) <
      probability * denominator;
}

void MetricsService::LogWindowChange(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  int controller_id = -1;
  uintptr_t window_or_tab = source.map_key();
  MetricsLog::WindowEventType window_type;

  // Note: since we stop all logging when a single OTR session is active, it is
  // possible that we start getting notifications about a window that we don't
  // know about.
  if (window_map_.find(window_or_tab) == window_map_.end()) {
    controller_id = next_window_id_++;
    window_map_[window_or_tab] = controller_id;
  } else {
    controller_id = window_map_[window_or_tab];
  }
  DCHECK(controller_id != -1);

  switch (type.value) {
    case NotificationType::TAB_PARENTED:
    case NotificationType::BROWSER_OPENED:
      window_type = MetricsLog::WINDOW_CREATE;
      break;

    case NotificationType::TAB_CLOSING:
    case NotificationType::BROWSER_CLOSED:
      window_map_.erase(window_map_.find(window_or_tab));
      window_type = MetricsLog::WINDOW_DESTROY;
      break;

    default:
      NOTREACHED();
      return;
  }

  // TODO(brettw) we should have some kind of ID for the parent.
  current_log_->RecordWindowEvent(window_type, controller_id, 0);
}

void MetricsService::LogLoadComplete(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (details == NotificationService::NoDetails())
    return;

  // TODO(jar): There is a bug causing this to be called too many times, and
  // the log overflows.  For now, we won't record these events.
  UMA_HISTOGRAM_COUNTS("UMA.LogLoadComplete called", 1);
  return;

  const Details<LoadNotificationDetails> load_details(details);
  int controller_id = window_map_[details.map_key()];
  current_log_->RecordLoadEvent(controller_id,
                                load_details->url(),
                                load_details->origin(),
                                load_details->session_index(),
                                load_details->load_time());
}

void MetricsService::IncrementPrefValue(const wchar_t* path) {
  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);
  int value = pref->GetInteger(path);
  pref->SetInteger(path, value + 1);
}

void MetricsService::LogLoadStarted() {
  IncrementPrefValue(prefs::kStabilityPageLoadCount);
  // We need to save the prefs, as page load count is a critical stat, and it
  // might be lost due to a crash :-(.
}

void MetricsService::LogRendererInSandbox(bool on_sandbox_desktop) {
  PrefService* prefs = g_browser_process->local_state();
  DCHECK(prefs);
  if (on_sandbox_desktop)
    IncrementPrefValue(prefs::kSecurityRendererOnSboxDesktop);
  else
    IncrementPrefValue(prefs::kSecurityRendererOnDefaultDesktop);
}

void MetricsService::LogRendererCrash() {
  IncrementPrefValue(prefs::kStabilityRendererCrashCount);
}

void MetricsService::LogRendererHang() {
  IncrementPrefValue(prefs::kStabilityRendererHangCount);
}

void MetricsService::LogChildProcessChange(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  const std::wstring& child_name =
      Details<ChildProcessInfo>(details)->name();

  if (child_process_stats_buffer_.find(child_name) ==
      child_process_stats_buffer_.end()) {
    child_process_stats_buffer_[child_name] = ChildProcessStats();
  }

  ChildProcessStats& stats = child_process_stats_buffer_[child_name];
  switch (type.value) {
    case NotificationType::CHILD_PROCESS_HOST_CONNECTED:
      stats.process_launches++;
      break;

    case NotificationType::CHILD_INSTANCE_CREATED:
      stats.instances++;
      break;

    case NotificationType::CHILD_PROCESS_CRASHED:
      stats.process_crashes++;
      break;

    default:
      NOTREACHED() << "Unexpected notification type " << type.value;
      return;
  }
}

// Recursively counts the number of bookmarks and folders in node.
static void CountBookmarks(BookmarkNode* node, int* bookmarks, int* folders) {
  if (node->GetType() == history::StarredEntry::URL)
    (*bookmarks)++;
  else
    (*folders)++;
  for (int i = 0; i < node->GetChildCount(); ++i)
    CountBookmarks(node->GetChild(i), bookmarks, folders);
}

void MetricsService::LogBookmarks(BookmarkNode* node,
                                  const wchar_t* num_bookmarks_key,
                                  const wchar_t* num_folders_key) {
  DCHECK(node);
  int num_bookmarks = 0;
  int num_folders = 0;
  CountBookmarks(node, &num_bookmarks, &num_folders);
  num_folders--;  // Don't include the root folder in the count.

  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);
  pref->SetInteger(num_bookmarks_key, num_bookmarks);
  pref->SetInteger(num_folders_key, num_folders);
}

void MetricsService::LogBookmarks(BookmarkModel* model) {
  DCHECK(model);
  LogBookmarks(model->GetBookmarkBarNode(),
               prefs::kNumBookmarksOnBookmarkBar,
               prefs::kNumFoldersOnBookmarkBar);
  LogBookmarks(model->other_node(),
               prefs::kNumBookmarksInOtherBookmarkFolder,
               prefs::kNumFoldersInOtherBookmarkFolder);
  ScheduleNextStateSave();
}

void MetricsService::LogKeywords(const TemplateURLModel* url_model) {
  DCHECK(url_model);

  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);
  pref->SetInteger(prefs::kNumKeywords,
                   static_cast<int>(url_model->GetTemplateURLs().size()));
  ScheduleNextStateSave();
}

void MetricsService::RecordPluginChanges(PrefService* pref) {
  ListValue* plugins = pref->GetMutableList(prefs::kStabilityPluginStats);
  DCHECK(plugins);

  for (ListValue::iterator value_iter = plugins->begin();
       value_iter != plugins->end(); ++value_iter) {
    if (!(*value_iter)->IsType(Value::TYPE_DICTIONARY)) {
      NOTREACHED();
      continue;
    }

    DictionaryValue* plugin_dict = static_cast<DictionaryValue*>(*value_iter);
    string16 plugin_name16;
    plugin_dict->GetString(WideToUTF16Hack(prefs::kStabilityPluginName),
                           &plugin_name16);
    if (plugin_name16.empty()) {
      NOTREACHED();
      continue;
    }

    std::wstring plugin_name = UTF16ToWideHack(plugin_name16);
    if (child_process_stats_buffer_.find(plugin_name) ==
        child_process_stats_buffer_.end())
      continue;

    ChildProcessStats stats = child_process_stats_buffer_[plugin_name];
    if (stats.process_launches) {
      int launches = 0;
      plugin_dict->GetInteger(WideToUTF16Hack(prefs::kStabilityPluginLaunches),
                              &launches);
      launches += stats.process_launches;
      plugin_dict->SetInteger(WideToUTF16Hack(prefs::kStabilityPluginLaunches),
                              launches);
    }
    if (stats.process_crashes) {
      int crashes = 0;
      plugin_dict->GetInteger(WideToUTF16Hack(prefs::kStabilityPluginCrashes),
                              &crashes);
      crashes += stats.process_crashes;
      plugin_dict->SetInteger(WideToUTF16Hack(prefs::kStabilityPluginCrashes),
                              crashes);
    }
    if (stats.instances) {
      int instances = 0;
      plugin_dict->GetInteger(WideToUTF16Hack(prefs::kStabilityPluginInstances),
                              &instances);
      instances += stats.instances;
      plugin_dict->SetInteger(WideToUTF16Hack(prefs::kStabilityPluginInstances),
                              instances);
    }

    child_process_stats_buffer_.erase(plugin_name);
  }

  // Now go through and add dictionaries for plugins that didn't already have
  // reports in Local State.
  for (std::map<std::wstring, ChildProcessStats>::iterator cache_iter =
           child_process_stats_buffer_.begin();
       cache_iter != child_process_stats_buffer_.end(); ++cache_iter) {
    std::wstring plugin_name = cache_iter->first;
    ChildProcessStats stats = cache_iter->second;
    DictionaryValue* plugin_dict = new DictionaryValue;

    plugin_dict->SetString(WideToUTF16Hack(prefs::kStabilityPluginName),
                           WideToUTF16Hack(plugin_name));
    plugin_dict->SetInteger(WideToUTF16Hack(prefs::kStabilityPluginLaunches),
                            stats.process_launches);
    plugin_dict->SetInteger(WideToUTF16Hack(prefs::kStabilityPluginCrashes),
                            stats.process_crashes);
    plugin_dict->SetInteger(WideToUTF16Hack(prefs::kStabilityPluginInstances),
                            stats.instances);
    plugins->Append(plugin_dict);
  }
  child_process_stats_buffer_.clear();
}

bool MetricsService::CanLogNotification(NotificationType type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  // We simply don't log anything to UMA if there is a single off the record
  // session visible. The problem is that we always notify using the orginal
  // profile in order to simplify notification processing.
  return !BrowserList::IsOffTheRecordSessionActive();
}

void MetricsService::RecordBooleanPrefValue(const wchar_t* path, bool value) {
  DCHECK(IsSingleThreaded());

  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);

  pref->SetBoolean(path, value);
  RecordCurrentState(pref);
}

void MetricsService::RecordCurrentState(PrefService* pref) {
  pref->SetString(prefs::kStabilityLastTimestampSec,
                  Int64ToWString(Time::Now().ToTimeT()));

  RecordPluginChanges(pref);
}

void MetricsService::CollectRendererHistograms() {
  for (RenderProcessHost::iterator it = RenderProcessHost::begin();
       it != RenderProcessHost::end(); ++it) {
    it->second->Send(new ViewMsg_GetRendererHistograms());
  }
}

void MetricsService::RecordCurrentHistograms() {
  DCHECK(current_log_);

  CollectRendererHistograms();

  // TODO(raman): Delay the metrics collection activities until we get all the
  // updates from the renderers, or we time out (1 second?  3 seconds?).

  StatisticsRecorder::Histograms histograms;
  StatisticsRecorder::GetHistograms(&histograms);
  for (StatisticsRecorder::Histograms::iterator it = histograms.begin();
       histograms.end() != it;
       ++it) {
    if ((*it)->flags() & kUmaTargetedHistogramFlag)
      // TODO(petersont): Only record historgrams if they are not precluded by
      // the UMA response data.
      // Bug http://code.google.com/p/chromium/issues/detail?id=2739.
      RecordHistogram(**it);
  }
}

void MetricsService::RecordHistogram(const Histogram& histogram) {
  // Get up-to-date snapshot of sample stats.
  Histogram::SampleSet snapshot;
  histogram.SnapshotSample(&snapshot);

  const std::string& histogram_name = histogram.histogram_name();

  // Find the already sent stats, or create an empty set.
  LoggedSampleMap::iterator it = logged_samples_.find(histogram_name);
  Histogram::SampleSet* already_logged;
  if (logged_samples_.end() == it) {
    // Add new entry
    already_logged = &logged_samples_[histogram.histogram_name()];
    already_logged->Resize(histogram);  // Complete initialization.
  } else {
    already_logged = &(it->second);
    // Deduct any stats we've already logged from our snapshot.
    snapshot.Subtract(*already_logged);
  }

  // snapshot now contains only a delta to what we've already_logged.

  if (snapshot.TotalCount() > 0) {
    current_log_->RecordHistogramDelta(histogram, snapshot);
    // Add new data into our running total.
    already_logged->Add(snapshot);
  }
}

void MetricsService::AddProfileMetric(Profile* profile,
                                      const std::wstring& key,
                                      int value) {
  // Restriction of types is needed for writing values. See
  // MetricsLog::WriteProfileMetrics.
  DCHECK(profile && !key.empty());
  PrefService* prefs = g_browser_process->local_state();
  DCHECK(prefs);

  // Key is stored in prefs, which interpret '.'s as paths. As such, key
  // shouldn't have any '.'s in it.
  DCHECK(key.find(L'.') == std::wstring::npos);
  // The id is most likely an email address. We shouldn't send it to the server.
  const std::wstring id_hash =
      UTF8ToWide(MetricsLog::CreateBase64Hash(WideToUTF8(profile->GetID())));
  DCHECK(id_hash.find('.') == std::string::npos);

  DictionaryValue* prof_prefs = prefs->GetMutableDictionary(
      prefs::kProfileMetrics);
  DCHECK(prof_prefs);
  const std::wstring pref_key = std::wstring(prefs::kProfilePrefix) + id_hash +
      L"." + key;
  prof_prefs->SetInteger(WideToUTF16Hack(pref_key), value);
}

static bool IsSingleThreaded() {
  static PlatformThreadId thread_id = 0;
  if (!thread_id)
    thread_id = PlatformThread::CurrentId();
  return PlatformThread::CurrentId() == thread_id;
}
