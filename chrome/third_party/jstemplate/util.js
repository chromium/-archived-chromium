// Copyright 2005-2006 Google Inc. All Rights Reserved.
/**
 * @fileoverview This file contains javascript utility functions that
 * do not depend on anything defined elsewhere.
 *
 */

/**
 * Returns the value of the length property of the given object. Used
 * to reduce compiled code size.
 *
 * @param {Array | String} a  The string or array to interrogate.
 * @return {Number}  The value of the length property.
 */
function jsLength(a) {
  return a.length;
}

// Wrappers for Math functions
var min = Math.min;
var max = Math.max;
var ceil = Math.ceil;
var floor = Math.floor;
var round = Math.round;
var abs = Math.abs;

/**
 * Copies all properties from second object to the first.  Modifies to.
 *
 * @param {Object} to  The target object.
 * @param {Object} from  The source object.
 */
function copyProperties(to, from) {
  foreachin(from, function(p) {
    to[p] = from[p];
  });
}

/**
 * Iterates over the array, calling the given function for each
 * element.
 *
 * @param {Array} array
 * @param {Function} fn
 */
function foreach(array, fn) {
  var I = jsLength(array);
  for (var i = 0; i < I; ++i) {
    fn(array[i], i);
  }
}

/**
 * Safely iterates over all properties of the given object, calling
 * the given function for each property. If opt_all isn't true, uses
 * hasOwnProperty() to assure the property is on the object, not on
 * its prototype.
 *
 * @param {Object} object
 * @param {Function} fn
 * @param {Boolean} opt_all  If true, also iterates over inherited properties.
 */
function foreachin(object, fn, opt_all) {
  for (var i in object) {
    // NOTE: Safari/1.3 doesn't have hasOwnProperty(). In that
    // case, we iterate over all properties as a very lame workaround.
    if (opt_all || !object.hasOwnProperty || object.hasOwnProperty(i)) {
      fn(i, object[i]);
    }
  }
}

/**
 * Appends the second array to the first, copying its elements.
 * Optionally only a slice of the second array is copied.
 *
 * @param {Array} a1  Target array (modified).
 * @param {Array} a2  Source array.
 * @param {Number} opt_begin  Begin of slice of second array (optional).
 * @param {Number} opt_end  End (exclusive) of slice of second array (optional).
 */
function arrayAppend(a1, a2, opt_begin, opt_end) {
  var i0 = opt_begin || 0;
  var i1 = opt_end || jsLength(a2);
  for (var i = i0; i < i1; ++i) {
    a1.push(a2[i]);
  }
}

/**
 * Trim whitespace from begin and end of string.
 *
 * @see testStringTrim();
 *
 * @param {String} str  Input string.
 * @return {String}  Trimmed string.
 */
function stringTrim(str) {
  return stringTrimRight(stringTrimLeft(str));
}

/**
 * Trim whitespace from beginning of string.
 *
 * @see testStringTrimLeft();
 *
 * @param {String} str  Input string.
 * @return {String}  Trimmed string.
 */
function stringTrimLeft(str) {
  return str.replace(/^\s+/, "");
}

/**
 * Trim whitespace from end of string.
 *
 * @see testStringTrimRight();
 *
 * @param {String} str  Input string.
 * @return {String}  Trimmed string.
 */
function stringTrimRight(str) {
  return str.replace(/\s+$/, "");
}

/**
 * Jscompiler wrapper for parseInt() with base 10.
 *
 * @param {String} s String repersentation of a number.
 *
 * @return {Number} The integer contained in s, converted on base 10.
 */
function parseInt10(s) {
  return parseInt(s, 10);
}
