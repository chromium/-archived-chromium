// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Profiler processor is used to process log file produced
 * by V8 and produce an internal profile representation which is used
 * for building profile views in 'Profiles' tab.
 */
goog.provide('devtools.profiler.Processor');


/**
 * Ancestor of a profile object that leaves out only JS-related functions.
 * @constructor
 */
devtools.profiler.JsProfile = function() {
  devtools.profiler.Profile.call(this);
};
goog.inherits(devtools.profiler.JsProfile, devtools.profiler.Profile);


/**
 * RegExp that leaves only JS functions.
 * @type {RegExp}
 */
devtools.profiler.JsProfile.JS_FUNC_RE = /^(LazyCompile|Function|Script):/;

/**
 * RegExp that filters out native code (ending with "native src.js:xxx").
 * @type {RegExp}
 */
devtools.profiler.JsProfile.JS_NATIVE_FUNC_RE = /\ native\ \w+\.js:\d+$/;

/**
 * RegExp that filters out native scripts.
 * @type {RegExp}
 */
devtools.profiler.JsProfile.JS_NATIVE_SCRIPT_RE = /^Script:\ native/;


/**
 * @override
 */
devtools.profiler.JsProfile.prototype.skipThisFunction = function(name) {
  return !devtools.profiler.JsProfile.JS_FUNC_RE.test(name) ||
      // To profile V8's natives comment out two lines below and '||' above.
      devtools.profiler.JsProfile.JS_NATIVE_FUNC_RE.test(name) ||
      devtools.profiler.JsProfile.JS_NATIVE_SCRIPT_RE.test(name);
};


/**
 * Profiler processor. Consumes profiler log and builds profile views.
 *
 * @param {function(devtools.profiler.ProfileView)} newProfileCallback Callback
 *     that receives a new processed profile.
 * @constructor
 */
devtools.profiler.Processor = function(newProfileCallback) {
  /**
   * Callback that adds a new profile to view.
   * @type {function(devtools.profiler.ProfileView)}
   */
  this.newProfileCallback_ = newProfileCallback;

  /**
   * Profiles array.
   * @type {Array<devtools.profiler.JsProfile>}
   */
  this.profiles_ = [];

  /**
   * The current profile.
   * @type {devtools.profiler.JsProfile}
   */
  this.currentProfile_ = null;

  /**
   * Builder of profile views.
   * @type {devtools.profiler.ViewBuilder}
   */
  this.viewBuilder_ = new devtools.profiler.ViewBuilder(1);

  /**
   * Next profile id.
   * @type {number}
   */
  this.profileId_ = 1;
};


/**
 * A dispatch table for V8 profiler event log records.
 * @private
 */
devtools.profiler.Processor.RecordsDispatch_ = {
  'code-creation': { parsers: [null, parseInt, parseInt, null],
                     processor: 'processCodeCreation_', needsProfile: true },
  'code-move': { parsers: [parseInt, parseInt],
                 processor: 'processCodeMove_', needsProfile: true },
  'code-delete': { parsers: [parseInt],
                   processor: 'processCodeDelete_', needsProfile: true },
  'tick': { parsers: [parseInt, parseInt, parseInt, 'var-args'],
            processor: 'processTick_', needsProfile: true },
  'profiler': { parsers: [null], processor: 'processProfiler_',
                needsProfile: false },
  // Not used in DevTools Profiler.
  'shared-library': null,
  // Obsolete row types.
  'code-allocate': null,
  'begin-code-region': null,
  'end-code-region': null
};


/**
 * Sets new profile callback.
 * @param {function(devtools.profiler.ProfileView)} callback Callback function.
 */
devtools.profiler.Processor.prototype.setNewProfileCallback = function(
    callback) {
  this.newProfileCallback_ = callback;
};


/**
 * Processes a portion of V8 profiler event log.
 *
 * @param {string} chunk A portion of log.
 */
devtools.profiler.Processor.prototype.processLogChunk = function(chunk) {
  this.processLog_(chunk.split('\n'));
};


/**
 * Processes a log lines.
 *
 * @param {Array<string>} lines Log lines.
 * @private
 */
devtools.profiler.Processor.prototype.processLog_ = function(lines) {
  var csvParser = new devtools.profiler.CsvParser();
  try {
    for (var i = 0, n = lines.length; i < n; ++i) {
      var line = lines[i];
      if (!line) {
        continue;
      }
      var fields = csvParser.parseLine(line);
      this.dispatchLogRow_(fields);
    }
  } catch (e) {
    debugPrint('line ' + (i + 1) + ': ' + (e.message || e));
    throw e;
  }
};


/**
 * Does a dispatch of a log record.
 *
 * @param {Array<string>} fields Log record.
 * @private
 */
devtools.profiler.Processor.prototype.dispatchLogRow_ = function(fields) {
  // Obtain the dispatch.
  var command = fields[0];
  if (!(command in devtools.profiler.Processor.RecordsDispatch_)) {
    throw new Error('unknown command: ' + command);
  }
  var dispatch = devtools.profiler.Processor.RecordsDispatch_[command];

  if (dispatch === null ||
      (dispatch.needsProfile && this.currentProfile_ == null)) {
    return;
  }

  // Parse fields.
  var parsedFields = [];
  for (var i = 0; i < dispatch.parsers.length; ++i) {
    var parser = dispatch.parsers[i];
    if (parser === null) {
      parsedFields.push(fields[1 + i]);
    } else if (typeof parser == 'function') {
      parsedFields.push(parser(fields[1 + i]));
    } else {
      // var-args
      parsedFields.push(fields.slice(1 + i));
      break;
    }
  }

  // Run the processor.
  this[dispatch.processor].apply(this, parsedFields);
};


devtools.profiler.Processor.prototype.processProfiler_ = function(state) {
  switch (state) {
    case "resume":
      this.currentProfile_ = new devtools.profiler.JsProfile();
      this.profiles_.push(this.currentProfile_);
      break;
    case "pause":
      if (this.currentProfile_ != null) {
        this.newProfileCallback_(this.createProfileForView());
        this.currentProfile_ = null;
      }
      break;
    // These events are valid but are not used.
    case "begin": break;
    case "end": break;
    default:
      throw new Error("unknown profiler state: " + state);
  }
};


devtools.profiler.Processor.prototype.processCodeCreation_ = function(
    type, start, size, name) {
  this.currentProfile_.addCode(type, name, start, size);
};


devtools.profiler.Processor.prototype.processCodeMove_ = function(from, to) {
  this.currentProfile_.moveCode(from, to);
};


devtools.profiler.Processor.prototype.processCodeDelete_ = function(start) {
  this.currentProfile_.deleteCode(start);
};


devtools.profiler.Processor.prototype.processTick_ = function(
    pc, sp, vmState, stack) {
  var fullStack = [pc];
  for (var i = 0, n = stack.length; i < n; ++i) {
    var frame = stack[i];
    // Leave only numbers starting with 0x. Filter possible 'overflow' string.
    if (frame.charAt(0) == '0') {
      fullStack.push(parseInt(frame, 16));
    }
  }
  this.currentProfile_.recordTick(fullStack);
};


/**
 * Creates a profile for further displaying in ProfileView.
 */
devtools.profiler.Processor.prototype.createProfileForView = function() {
  var profile = new devtools.profiler.ProfileView();
  profile.uid = this.profileId_++;
  profile.title = UserInitiatedProfileName + '.' + profile.uid;
  // A trick to cope with ProfileView.bottomUpProfileDataGridTree and
  // ProfileView.topDownProfileDataGridTree behavior.
  profile.head = profile;
  profile.heavyProfile = this.viewBuilder_.buildView(
      this.currentProfile_.getBottomUpProfile(), true);
  profile.treeProfile = this.viewBuilder_.buildView(
      this.currentProfile_.getTopDownProfile());
  return profile;
};
