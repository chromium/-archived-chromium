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
 * @param {string} args JSON-serialized {Array.<string>} Path to the
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


/**
 * Returns JSON-serialized style information that is used in devtools.js.
 * @param {Node} node Node to get prorotypes for.
 * @param {string} args JSON-serialized boolean authorOnly that determines
 *     whether only author styles need to be added.
 * @return {string} JSON-serialized style collection descriptor.
 */
function devtools$$getStyles(node, args) {
  var authorOnly = goog.json.parse(args);
  if (!node.nodeType == Node.ELEMENT_NODE) {
    return '{}';
  }
  var matchedRules = window.getMatchedCSSRules(node, '', authorOnly);
  var matchedCSSRulesObj = [];
  for (var i = 0; matchedRules && i < matchedRules.length; ++i) {
    var rule = matchedRules[i];   
    var style = devtools$$serializeStyle_(rule.style);
    var parentStyleSheetHref = (rule.parentStyleSheet ?
        rule.parentStyleSheet.href : undefined);
    var parentStyleSheetOwnerNodeName;
    if (rule.parentStyleSheet && rule.parentStyleSheet.ownerNode) {
      parentStyleSheetOwnerNodeName = rule.parentStyleSheet.ownerNode.name;
    }
    matchedCSSRulesObj.push({
      'selector' : rule.selectorText,
      'style' : style,
      'parentStyleSheetHref' : parentStyleSheetHref,
      'parentStyleSheetOwnerNodeName' : parentStyleSheetOwnerNodeName
    });
  }

  var attributeStyles = {};
  var attributes = node.attributes;
  for (var i = 0; attributes && i < attributes.length; ++i) {
    if (attributes[i].style) {
      attributeStyles[attributes[i].name] =
          devtools$$serializeStyle_(attributes[i].style);
    }
  }

  var result = {
    'inlineStyle' : devtools$$serializeStyle_(node.style),
    'computedStyle' : devtools$$serializeStyle_(
        window.getComputedStyle(node, '')),
    'matchedCSSRules' : matchedCSSRulesObj,
    'styleAttributes' : attributeStyles
  };
  return goog.json.serialize(result);
}


/**
 * Converts given style into serializable object.
 * @param {CSSStyleDeclaration} style Style to serialize.
 * @return {Array<Object>} Serializable object.
 * @private
 */
function devtools$$serializeStyle_(style) {
  var result = [];
  for (var i = 0; style && i < style.length; ++i) {
    var name = style[i];
    result.push([
      name,
      style.getPropertyPriority(name),
      style.isPropertyImplicit(name),
      style.getPropertyShorthand(name),
      style.getPropertyValue(name)
    ]);
  }
  return result;
}
