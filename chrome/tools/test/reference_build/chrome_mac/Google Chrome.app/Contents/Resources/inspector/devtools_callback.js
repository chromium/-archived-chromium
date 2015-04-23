// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Generic callback manager.
 */
goog.provide('devtools.Callback');


/**
 * Generic callback support as a singleton object.
 * @constructor
 */
devtools.Callback = function() {
  this.lastCallbackId_ = 1;
  this.callbacks_ = {};
};


/**
 * Assigns id to a callback.
 * @param {Function} callback Callback to assign id to.
 * @return {number} Callback id.
 */
devtools.Callback.prototype.wrap = function(callback) {
  var callbackId = this.lastCallbackId_++;
  this.callbacks_[callbackId] = callback || function() {};
  return callbackId;
};


/**
 * Executes callback with the given id.
 * @param {callbackId} callbackId Id of a callback to call.
 */
devtools.Callback.prototype.processCallback = function(callbackId,
    opt_vararg) {
  var args = Array.prototype.slice.call(arguments, 1);
  var callback = this.callbacks_[callbackId];
  callback.apply(null, args);
  delete this.callbacks_[callbackId];
};


/**
 * @type {devtools.Callback} Callback support singleton.
 * @private
 */
devtools.Callback.INSTANCE_ = new devtools.Callback();

devtools.Callback.wrap = goog.bind(
    devtools.Callback.INSTANCE_.wrap,
    devtools.Callback.INSTANCE_);
devtools.Callback.processCallback = goog.bind(
    devtools.Callback.INSTANCE_.processCallback,
    devtools.Callback.INSTANCE_);
