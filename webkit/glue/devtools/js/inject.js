// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Javascript that is being injected into the inspectable page
 * while debugging.
 */
goog.require('goog.json');
goog.provide('devtools.Injected');


/**
 * Main injected object.
 * @constructor.
 */
devtools.Injected = function() {
  /**
   * Unique style id generator.
   * @type {number}
   * @private
   */
  this.lastStyleId_ = 1;

  /**
   * This array is not unused as it may seem. It stores references to the
   * styles so that they could be found for future editing.
   * @type {Array<CSSStyleDeclaration>}
   * @private
   */
  this.styles_ = [];
};


/**
 * Returns array of properties for a given node on a given path.
 * @param {Node} node Node to get property value for.
 * @param {Array.<string>} path Path to the nested object.
 * @param {number} protoDepth Depth of the actual proto to inspect.
 * @return {Array.<Object>} Array where each property is represented
 *     by the tree entries [{string} type, {string} name, {Object} value].
 */
devtools.Injected.prototype.getProperties = function(node, path, protoDepth) {
  var result = [];
  var obj = node;

  // Follow the path.
  for (var i = 0; obj && i < path.length; ++i) {
    obj = obj[path[i]];
  }

  if (!obj) {
    return [];
  }

  // Get to the necessary proto layer.
  for (var i = 0; obj && i < protoDepth; ++i) {
    obj = obj.__proto__;
  }

  if (!obj) {
    return [];
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
  return result;
};


/**
 * Returns array of prototypes for a given node.
 * @param {Node} node Node to get prorotypes for.
 * @return {Array<string>} Array of proto names.
 */
devtools.Injected.prototype.getPrototypes = function(node) {
  var result = [];
  for (var prototype = node; prototype; prototype = prototype.__proto__) {
    var description = Object.prototype.toString.call(prototype);
    result.push(description.replace(/^\[object (.*)\]$/i, '$1'));
  }
  return result;
};


/**
 * Returns style information that is used in devtools.js.
 * @param {Node} node Node to get prorotypes for.
 * @param {boolean} authorOnly Determines whether only author styles need to
 *     be added.
 * @return {string} Style collection descriptor.
 */
devtools.Injected.prototype.getStyles = function(node, authorOnly) {
  if (!node.nodeType == Node.ELEMENT_NODE) {
    return {};
  }
  var matchedRules = window.getMatchedCSSRules(node, '', authorOnly);
  var matchedCSSRulesObj = [];
  for (var i = 0; matchedRules && i < matchedRules.length; ++i) {
    var rule = matchedRules[i];   
    var style = this.serializeStyle_(rule.style);
    var ruleValue = {
      'selector' : rule.selectorText,
      'style' : style
    };
    if (rule.parentStyleSheet) {
      ruleValue['parentStyleSheet'] = {
        'href' : rule.parentStyleSheet.href,
        'ownerNodeName' : rule.parentStyleSheet.ownerNode ?
            rule.parentStyleSheet.ownerNode.name : null
      };
    }
    var parentStyleSheetHref = (rule.parentStyleSheet ?
        rule.parentStyleSheet.href : undefined);
    var parentStyleSheetOwnerNodeName;
    if (rule.parentStyleSheet && rule.parentStyleSheet.ownerNode) {
      parentStyleSheetOwnerNodeName = rule.parentStyleSheet.ownerNode.name;
    }
    matchedCSSRulesObj.push(ruleValue);
  }

  var attributeStyles = {};
  var attributes = node.attributes;
  for (var i = 0; attributes && i < attributes.length; ++i) {
    if (attributes[i].style) {
      attributeStyles[attributes[i].name] =
          this.serializeStyle_(attributes[i].style);
    }
  }

  var result = {
    'inlineStyle' : this.serializeStyle_(node.style),
    'computedStyle' : this.serializeStyle_(
        window.getComputedStyle(node, '')),
    'matchedCSSRules' : matchedCSSRulesObj,
    'styleAttributes' : attributeStyles
  };
  return result;
};


/**
 * Returns style decoration object for given id.
 * @param {Node} node Node to get prorotypes for.
 * @param {number} id Style id.
 * @return {Object} Style object.
 * @private
 */
devtools.Injected.prototype.getStyleForId_ = function(node, id) {
  var matchedRules = window.getMatchedCSSRules(node, '', false);
  for (var i = 0; matchedRules && i < matchedRules.length; ++i) {
    var rule = matchedRules[i];
    if (rule.style.__id == id) {
      return rule.style;
    }
  }
  var attributes = node.attributes;
  for (var i = 0; attributes && i < attributes.length; ++i) {
    if (attributes[i].style && attributes[i].style.__id == id) {
      return attributes[i].style;
    }
  }
  if (node.style.__id == id) {
    return node.style;
  }
  return null;
};




/**
 * Converts given style into serializable object.
 * @param {CSSStyleDeclaration} style Style to serialize.
 * @return {Array<Object>} Serializable object.
 * @private
 */
devtools.Injected.prototype.serializeStyle_ = function(style) {
  if (!style) {
    return [];
  }
  if (!style.__id) {
    style.__id = this.lastStyleId_++;
    this.styles_.push(style);
  }
  var result = [
    style.__id,
    style.__disabledProperties,
    style.__disabledPropertyValues,
    style.__disabledPropertyPriorities
  ];
  for (var i = 0; i < style.length; ++i) {
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
};


/**
 * Toggles style with given id on/off.
 * @param {Node} node Node to get prorotypes for.
 * @param {number} styleId Id of style to toggle.
 * @param {boolean} enabled Determines value to toggle to,
 * @param {string} name Name of the property.
 */
devtools.Injected.prototype.toggleNodeStyle = function(node, styleId, enabled,
    name) {
  var style = this.getStyleForId_(node, styleId);
  if (!style) {
    return false;
  }

  if (!enabled) {
    if (!style.__disabledPropertyValues ||
        !style.__disabledPropertyPriorities) {
      style.__disabledProperties = {};
      style.__disabledPropertyValues = {};
      style.__disabledPropertyPriorities = {};
    }

    style.__disabledPropertyValues[name] = style.getPropertyValue(name);
    style.__disabledPropertyPriorities[name] = style.getPropertyPriority(name);

    if (style.getPropertyShorthand(name)) {
      var longhandProperties = this.getLonghandProperties_(style, name);
      for (var i = 0; i < longhandProperties.length; ++i) {
        style.__disabledProperties[longhandProperties[i]] = true;
        style.removeProperty(longhandProperties[i]);
      }
    } else {
      style.__disabledProperties[name] = true;
      style.removeProperty(name);
    }
  } else if (style.__disabledProperties &&
      style.__disabledProperties[name]) {
    var value = style.__disabledPropertyValues[name];
    var priority = style.__disabledPropertyPriorities[name];
    style.setProperty(name, value, priority);

    delete style.__disabledProperties[name];
    delete style.__disabledPropertyValues[name];
    delete style.__disabledPropertyPriorities[name];
  }
  return true;
};


/**
 * Returns longhand proeprties for a given shorthand one.
 * @param {CSSStyleDeclaration} style Style declaration to use for lookup.
 * @param {string} shorthandProperty Shorthand property to get longhands for.
 * @return {Array.<string>} Array with longhand properties.
 * @private
 */
devtools.Injected.prototype.getLonghandProperties_ = function(style,
    shorthandProperty) {
  var properties = [];
  var foundProperties = {};

  for (var i = 0; i < style.length; ++i) {
    var individualProperty = style[i];
    if (individualProperty in foundProperties ||
        style.getPropertyShorthand(individualProperty) != shorthandProperty) {
      continue;
    }
    foundProperties[individualProperty] = true;
    properties.push(individualProperty);
  }
  return properties;
};
