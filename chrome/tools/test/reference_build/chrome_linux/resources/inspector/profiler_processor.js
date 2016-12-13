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
 * @type {RegExp}
 */
devtools.profiler.JsProfile.JS_FUNC_RE = /^(LazyCompile|Function|Script):/;


/**
 * @override
 */
devtools.profiler.JsProfile.prototype.skipThisFunction = function(name) {
  return !devtools.profiler.JsProfile.JS_FUNC_RE.test(name);
};


/**
 * Profiler processor. Consumes profiler log and builds profile views.
 * @constructor
 */
devtools.profiler.Processor = function() {
  /**
   * Current profile.
   * @type {devtools.profiler.JsProfile}
   */
  this.profile_ = new devtools.profiler.JsProfile();

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
                     processor: 'processCodeCreation_' },
  'code-move': { parsers: [parseInt, parseInt],
                 processor: 'processCodeMove_' },
  'code-delete': { parsers: [parseInt], processor: 'processCodeDelete_' },
  'tick': { parsers: [parseInt, parseInt, parseInt, 'var-args'],
            processor: 'processTick_' },
  // Not used in DevTools Profiler.
  'profiler': null,
  'shared-library': null,
  // Obsolete row types.
  'code-allocate': null,
  'begin-code-region': null,
  'end-code-region': null
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

  if (dispatch === null) {
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


devtools.profiler.Processor.prototype.processCodeCreation_ = function(
    type, start, size, name) {
  this.profile_.addCode(type, name, start, size);
};


devtools.profiler.Processor.prototype.processCodeMove_ = function(from, to) {
  this.profile_.moveCode(from, to);
};


devtools.profiler.Processor.prototype.processCodeDelete_ = function(start) {
  this.profile_.deleteCode(start);
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
  this.profile_.recordTick(fullStack);
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
      this.profile_.getBottomUpProfile(), true);
  profile.treeProfile = this.viewBuilder_.buildView(
      this.profile_.getTopDownProfile());
  return profile;
};
