// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Javascript that is being injected into the inspectable page
 * while debugging.
 */
goog.require('goog.json');

/**
 * Returns JSON-serialized array of properties for a given node
 * on a given path.
 * @param {Node} node Node to get property value for.
 * @param {string} args JSON-serialized [{Array.<string>} Path to the
 *     nested object, {number} Depth to the actual proto to inspect].
 * @return {string} JSON-serialized array where each property is represented
 *     by the tree entryies [{string} type, {string} name, {Object} value].
 */
function devtools$$getProperties(node, args) {
  // Parse parameters.
  var parsedArgs = goog.json.parse(args);
  var path = parsedArgs[0];
  var protoDepth = parsedArgs[1];

  var result = [];
  var obj = node;

  // Follow the path.
  for (var i = 0; obj && i < path.length; ++i) {
    obj = obj[path[i]];
  }

  if (!obj) {
    return '[]';
  }

  // Get to the necessary proto layer.
  for (var i = 0; obj && i < protoDepth; ++i) {
    obj = obj.__proto__;
  }

  if (!obj) {
    return '[]';
  }

  // Go over properties, prepare results.
  for (var name in obj) {
    if (protoDepth != -1 && 'hasOwnProperty' in obj &&
       !obj.hasOwnProperty(name)) {
      continue;
    }
    var type = typeof obj[name];
    result.push(type);
    result.push(name);
    if (type == 'string') {
      var str = obj[name];
      result.push(str.length > 99 ? str.substr(0, 99) + '...' : str);
    } else if (type != 'object' && type != 'array' &&
         type != 'function') {
      result.push(obj[name]);
    } else {
      result.push(undefined);
    }
  }
  return goog.json.serialize(result);
}


/**
 * Returns JSON-serialized array of prototypes for a given node.
 * @param {Node} node Node to get prorotypes for.
 * @return {string} JSON-serialized array where each item is a proto name.
 */
function devtools$$getPrototypes(node, args) {
  var result = [];
  for (var prototype = node; prototype; prototype = prototype.__proto__) {
    var description = Object.prototype.toString.call(prototype);
    result.push(description.replace(/^\[object (.*)\]$/i, '$1'));
  }
  return goog.json.serialize(result);
}
