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
 * Creates a Profile View builder object compatible with WebKit Profiler UI.
 *
 * @param {number} samplingRate Number of ms between profiler ticks.
 * @constructor
 */
devtools.profiler.WebKitViewBuilder = function(samplingRate) {
  devtools.profiler.ViewBuilder.call(this, samplingRate);
};
goog.inherits(devtools.profiler.WebKitViewBuilder,
    devtools.profiler.ViewBuilder);


/**
 * @override
 */
devtools.profiler.WebKitViewBuilder.prototype.createViewNode = function(
    funcName, totalTime, selfTime, head) {
  return new devtools.profiler.WebKitViewNode(
      funcName, totalTime, selfTime, head);
};


/**
 * Constructs a Profile View node object for displaying in WebKit Profiler UI.
 *
 * @param {string} internalFuncName A fully qualified function name.
 * @param {number} totalTime Amount of time that application spent in the
 *     corresponding function and its descendants (not that depending on
 *     profile they can be either callees or callers.)
 * @param {number} selfTime Amount of time that application spent in the
 *     corresponding function only.
 * @param {devtools.profiler.ProfileView.Node} head Profile view head.
 * @constructor
 */
devtools.profiler.WebKitViewNode = function(
    internalFuncName, totalTime, selfTime, head) {
  devtools.profiler.ProfileView.Node.call(this,
      internalFuncName, totalTime, selfTime, head);
  this.initFuncInfo_();
  this.callUID = internalFuncName;
};
goog.inherits(devtools.profiler.WebKitViewNode,
    devtools.profiler.ProfileView.Node);


/**
 * RegEx for stripping V8's prefixes of compiled functions.
 */
devtools.profiler.WebKitViewNode.FUNC_NAME_STRIP_RE =
    /^(?:LazyCompile|Function): (.*)$/;


/**
 * RegEx for extracting script source URL and line number.
 */
devtools.profiler.WebKitViewNode.FUNC_NAME_PARSE_RE = /^([^ ]+) (.*):(\d+)$/;


/**
 * Inits 'functionName', 'url', and 'lineNumber' fields using 'internalFuncName'
 * field.
 * @private
 */
devtools.profiler.WebKitViewNode.prototype.initFuncInfo_ = function() {
  var nodeAlias = devtools.profiler.WebKitViewNode;
  this.functionName = this.internalFuncName;

  var strippedName = nodeAlias.FUNC_NAME_STRIP_RE.exec(this.functionName);
  if (strippedName) {
    this.functionName = strippedName[1];
  }

  var parsedName = nodeAlias.FUNC_NAME_PARSE_RE.exec(this.functionName);
  if (parsedName) {
    this.functionName = parsedName[1];
    this.url = parsedName[2];
    this.lineNumber = parsedName[3];
  } else {
    this.url = '';
    this.lineNumber = 0;
  }
};


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
devtools.profiler.Processor = function() {
  /**
   * Callback that is called when a new profile is encountered in the log.
   * @type {function()}
   */
  this.startedProfileProcessing_ = null;

  /**
   * Callback that is called when a profile has been processed and is ready
   * to be shown.
   * @type {function(devtools.profiler.ProfileView)}
   */
  this.finishedProfileProcessing_ = null;

  /**
   * The current profile.
   * @type {devtools.profiler.JsProfile}
   */
  this.currentProfile_ = null;

  /**
   * Builder of profile views. Created during "profiler,begin" event processing.
   * @type {devtools.profiler.WebKitViewBuilder}
   */
  this.viewBuilder_ = null;

  /**
   * Next profile id.
   * @type {number}
   */
  this.profileId_ = 1;
};


/**
 * Sets profile processing callbacks.
 *
 * @param {function()} started Started processing callback.
 * @param {function(devtools.profiler.ProfileView)} finished Finished
 *     processing callback.
 */
devtools.profiler.Processor.prototype.setCallbacks = function(
    started, finished) {
  this.startedProfileProcessing_ = started;
  this.finishedProfileProcessing_ = finished;
};


/**
 * An address for the fake "(program)" entry. WebKit's visualisation
 * has assumptions on how the top of the call tree should look like,
 * and we need to add a fake entry as the topmost function. This
 * address is chosen because it's the end address of the first memory
 * page, which is never used for code or data, but only as a guard
 * page for catching AV errors.
 *
 * @type {number}
 */
devtools.profiler.Processor.PROGRAM_ENTRY = 0xffff;


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
  'profiler': { parsers: [null, 'var-args'], processor: 'processProfiler_',
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


devtools.profiler.Processor.prototype.processProfiler_ = function(
    state, params) {
  switch (state) {
    case 'resume':
      if (this.currentProfile_ == null) {
        this.currentProfile_ = new devtools.profiler.JsProfile();
        // see the comment for devtools.profiler.Processor.PROGRAM_ENTRY
        this.currentProfile_.addCode(
          'Function', '(program)',
          devtools.profiler.Processor.PROGRAM_ENTRY, 1);
        if (this.startedProfileProcessing_) {
          this.startedProfileProcessing_();
        }
      }
      break;
    case 'pause':
      if (this.currentProfile_ != null) {
        if (this.finishedProfileProcessing_) {
          this.finishedProfileProcessing_(this.createProfileForView());
        }
        this.currentProfile_ = null;
      }
      break;
    case 'begin':
      var samplingRate = NaN;
      if (params.length > 0) {
        samplingRate = parseInt(params[0]);
      }
      if (isNaN(samplingRate)) {
        samplingRate = 1;
      }
      this.viewBuilder_ = new devtools.profiler.WebKitViewBuilder(samplingRate);
      break;
    // This event is valid but isn't used.
    case 'end': break;
    default:
      throw new Error('unknown profiler state: ' + state);
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
  // see the comment for devtools.profiler.Processor.PROGRAM_ENTRY
  fullStack.push(devtools.profiler.Processor.PROGRAM_ENTRY);
  this.currentProfile_.recordTick(fullStack);
};


/**
 * Creates a profile for further displaying in ProfileView.
 */
devtools.profiler.Processor.prototype.createProfileForView = function() {
  var profile = this.viewBuilder_.buildView(
      this.currentProfile_.getTopDownProfile());
  profile.uid = this.profileId_++;
  profile.title = UserInitiatedProfileName + '.' + profile.uid;
  return profile;
};
