// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Javascript that is being injected into the inspectable page
 * while debugging.
 */
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

  /**
   * This cache contains mapping from object it to an object instance for
   * all results of the evaluation / console logs.
   */
  this.cachedConsoleObjects_ = {};

  /**
   * Last id for the cache above.
   */
  this.lastCachedConsoleObjectId_ = 1;
};


/**
 * Returns object for given id. This can be either node wrapper for
 * integer ids or evaluation results that recide in cached console
 * objects cache for arbitrary keys.
 * @param {number|string} id Id to get object for.
 * @return {Object} resolved object.
 */
devtools.Injected.prototype.getObjectForId_ = function(id) {
  if (typeof id == 'number') {
    return DevToolsAgentHost.getNodeForId(id);
  }
  return this.cachedConsoleObjects_[id];
};


/**
 * Returns array of properties for a given node on a given path.
 * @param {number} nodeId Id of node to get prorotypes for.
 * @param {Array.<string>} path Path to the nested object.
 * @param {number} protoDepth Depth of the actual proto to inspect.
 * @return {Array.<Object>} Array where each property is represented
 *     by the four entries [{string} type, {string} name, {Object} value,
 *     {string} objectClassNameOrFunctionSignature].
 */
devtools.Injected.prototype.getProperties =
    function(nodeId, path, protoDepth) {
  var result = [];
  var obj = this.getObjectForId_(nodeId);
  if (!obj) {
    return [];
  }

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
    var value = obj[name];
    var type = typeof value;
    result.push(type);
    result.push(name);
    if (type == 'string') {
      var str = value;
      result.push(str.length > 99 ? str.substr(0, 99) + '...' : str);
      result.push(undefined);
    } else if (type == 'function') {
      result.push(undefined);
      var str = Function.prototype.toString.call(value);
      // Cut function signature (everything before first ')').
      var signatureLength = str.search(/\)/);
      str = str.substr(0, signatureLength + 1);
      // Collapse each group of consecutive whitespaces into one whitespaces
      // and add body brackets.
      str = str.replace(/\s+/g, ' ') + ' {}';
      result.push(str);
    } else if (type == 'object') {
      result.push(undefined);
      result.push(this.getClassName_(value));
    } else {
      result.push(value);
      result.push(undefined);
    }
  }
  return result;
};


/**
 * Returns array of prototypes for a given node.
 * @param {number} nodeId Id of node to get prorotypes for.
 * @return {Array<string>} Array of proto names.
 */
devtools.Injected.prototype.getPrototypes = function(nodeId) {
  var node = DevToolsAgentHost.getNodeForId(nodeId);
  if (!node) {
    return [];
  }

  var result = [];
  for (var prototype = node; prototype; prototype = prototype.__proto__) {
    result.push(this.getClassName_(prototype));
  }
  return result;
};


/**
 * @param {Object|Function|null} value An object whose class name to return.
 * @return {string} The value class name.
 */
devtools.Injected.prototype.getClassName_ = function(value) {
  return (value == null) ? 'null' : value.constructor.name;
};


/**
 * Returns style information that is used in devtools.js.
 * @param {number} nodeId Id of node to get prorotypes for.
 * @param {boolean} authorOnly Determines whether only author styles need to
 *     be added.
 * @return {string} Style collection descriptor.
 */
devtools.Injected.prototype.getStyles = function(nodeId, authorOnly) {
  var node = DevToolsAgentHost.getNodeForId(nodeId);
  if (!node) {
    return {};
  }

  if (node.nodeType != Node.ELEMENT_NODE) {
    return {};
  }
  var matchedRules = window.getMatchedCSSRules(node, '', false);
  var matchedCSSRulesObj = [];
  for (var i = 0; matchedRules && i < matchedRules.length; ++i) {
    var rule = matchedRules[i];
    var parentStyleSheet = rule.parentStyleSheet;
    var isUserAgent = parentStyleSheet && !parentStyleSheet.ownerNode &&
        !parentStyleSheet.href;
    var isUser = parentStyleSheet && parentStyleSheet.ownerNode &&
        parentStyleSheet.ownerNode.nodeName == '#document';

    var style = this.serializeStyle_(rule.style, !isUserAgent && !isUser);
    var ruleValue = {
      'selector' : rule.selectorText,
      'style' : style
    };
    if (parentStyleSheet) {
      ruleValue['parentStyleSheet'] = {
        'href' : parentStyleSheet.href,
        'ownerNodeName' : parentStyleSheet.ownerNode ?
            parentStyleSheet.ownerNode.name : null
      };
    }
    matchedCSSRulesObj.push(ruleValue);
  }

  var attributeStyles = {};
  var attributes = node.attributes;
  for (var i = 0; attributes && i < attributes.length; ++i) {
    if (attributes[i].style) {
      attributeStyles[attributes[i].name] =
          this.serializeStyle_(attributes[i].style, true);
    }
  }
  var result = {
    'inlineStyle' : this.serializeStyle_(node.style, true),
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
 * @param {boolean} opt_bind Determins whether this style should be bound.
 * @return {Array<Object>} Serializable object.
 * @private
 */
devtools.Injected.prototype.serializeStyle_ = function(style, opt_bind) {
  if (!style) {
    return [];
  }
  var id = style.__id;
  if (opt_bind && !id) {
    id = style.__id = this.lastStyleId_++;
    this.styles_.push(style);
  }
  var result = [
    id,
    style.width,
    style.height,
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
 * @param {number} nodeId Id of node to get prorotypes for.
 * @param {number} styleId Id of style to toggle.
 * @param {boolean} enabled Determines value to toggle to,
 * @param {string} name Name of the property.
 */
devtools.Injected.prototype.toggleNodeStyle = function(nodeId, styleId,
    enabled, name) {
  var node = DevToolsAgentHost.getNodeForId(nodeId);
  if (!node) {
    return false;
  }

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
 * Applies given text to a style.
 * @param {number} nodeId Id of node to get prorotypes for.
 * @param {number} styleId Id of style to toggle.
 * @param {string} name Style element name.
 * @param {string} styleText New style text.
 * @return {boolean} True iff style has been edited successfully.
 */
devtools.Injected.prototype.applyStyleText = function(nodeId, styleId,
    name, styleText) {
  var node = DevToolsAgentHost.getNodeForId(nodeId);
  if (!node) {
    return false;
  }

  var style = this.getStyleForId_(node, styleId);
  if (!style) {
    return false;
  }

  var styleTextLength = this.trimWhitespace_(styleText).length;

  // Create a new element to parse the user input CSS.
  var parseElement = document.createElement("span");
  parseElement.setAttribute("style", styleText);

  var tempStyle = parseElement.style;
  if (tempStyle.length || !styleTextLength) {
    // The input was parsable or the user deleted everything, so remove the
    // original property from the real style declaration. If this represents
    // a shorthand remove all the longhand properties.
    if (style.getPropertyShorthand(name)) {
      var longhandProperties = this.getLonghandProperties_(style, name);
      for (var i = 0; i < longhandProperties.length; ++i) {
        style.removeProperty(longhandProperties[i]);
      }
    } else {
      style.removeProperty(name);
    }
  }
  if (!tempStyle.length) {
    // The user typed something, but it didn't parse. Just abort and restore
    // the original title for this property.
    return false;
  }

  // Iterate of the properties on the test element's style declaration and
  // add them to the real style declaration. We take care to move shorthands.
  var foundShorthands = {};
  var uniqueProperties = this.getUniqueStyleProperties_(tempStyle);
  for (var i = 0; i < uniqueProperties.length; ++i) {
    var name = uniqueProperties[i];
    var shorthand = tempStyle.getPropertyShorthand(name);

    if (shorthand && shorthand in foundShorthands) {
      continue;
    }

    if (shorthand) {
      var value = this.getShorthandValue_(tempStyle, shorthand);
      var priority = this.getShorthandPriority_(tempStyle, shorthand);
      foundShorthands[shorthand] = true;
    } else {
      var value = tempStyle.getPropertyValue(name);
      var priority = tempStyle.getPropertyPriority(name);
    }
    // Set the property on the real style declaration.
    style.setProperty((shorthand || name), value, priority);
  }
  return true;
};


/**
 * Sets style property with given name to a value.
 * @param {number} nodeId Id of node to get prorotypes for.
 * @param {string} name Style element name.
 * @param {string} value Value.
 * @return {boolean} True iff style has been edited successfully.
 */
devtools.Injected.prototype.setStyleProperty = function(nodeId,
    name, value) {
  var node = DevToolsAgentHost.getNodeForId(nodeId);
  if (!node) {
    return false;
  }

  node.style.setProperty(name, value, "");
  return true;
};


/**
 * Taken from utilities.js as is for injected evaluation.
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


/**
 * Taken from utilities.js as is for injected evaluation.
 */
devtools.Injected.prototype.getShorthandValue_ = function(style,
    shorthandProperty) {
  var value = style.getPropertyValue(shorthandProperty);
  if (!value) {
    // Some shorthands (like border) return a null value, so compute a
    // shorthand value.
    // FIXME: remove this when http://bugs.webkit.org/show_bug.cgi?id=15823
    // is fixed.

    var foundProperties = {};
    for (var i = 0; i < style.length; ++i) {
      var individualProperty = style[i];
      if (individualProperty in foundProperties ||
          style.getPropertyShorthand(individualProperty) !==
              shorthandProperty) {
        continue;
      }

      var individualValue = style.getPropertyValue(individualProperty);
      if (style.isPropertyImplicit(individualProperty) ||
          individualValue === "initial") {
          continue;
      }

      foundProperties[individualProperty] = true;

      if (!value) {
        value = "";
      } else if (value.length) {
        value += " ";
      }
      value += individualValue;
    }
  }
  return value;
};


/**
 * Taken from utilities.js as is for injected evaluation.
 */
devtools.Injected.prototype.getShorthandPriority_ = function(style,
    shorthandProperty) {
  var priority = style.getPropertyPriority(shorthandProperty);
  if (!priority) {
    for (var i = 0; i < style.length; ++i) {
      var individualProperty = style[i];
      if (style.getPropertyShorthand(individualProperty) !==
              shorthandProperty) {
        continue;
      }
      priority = style.getPropertyPriority(individualProperty);
      break;
    }
  }
  return priority;
};


/**
 * Taken from utilities.js as is for injected evaluation.
 */
devtools.Injected.prototype.trimWhitespace_ = function(str) {
  return str.replace(/^[\s\xA0]+|[\s\xA0]+$/g, '');
};


/**
 * Taken from utilities.js as is for injected evaluation.
 */
devtools.Injected.prototype.getUniqueStyleProperties_ = function(style) {
  var properties = [];
  var foundProperties = {};

  for (var i = 0; i < style.length; ++i) {
    var property = style[i];
    if (property in foundProperties) {
      continue;
    }
    foundProperties[property] = true;
    properties.push(property);
  }
  return properties;
};


/**
 * Caches console object for subsequent calls to getConsoleObjectProperties.
 * @param {Object} obj Object to cache.
 * @return {Object} console object wrapper.
 */
devtools.Injected.prototype.wrapConsoleObject = function(obj) {
  var type = typeof obj;
  if (type == 'object' || type == 'function') {
    var objId = '#consoleobj#' + this.lastCachedConsoleObjectId_++;
    this.cachedConsoleObjects_[objId] = obj;
    var result = { ___devtools_id : objId };
    // Loop below fills dummy object with properties for completion.
    for (var name in obj) {
      result[name] = '';
    }
    return result;
  } else {
    return obj;
  }
};


/**
 * Caches console object for subsequent calls to getConsoleObjectProperties.
 * @param {Object} obj Object to cache.
 * @return {string} console object id.
 */
devtools.Injected.prototype.evaluate = function(expression) {
  try {
    // Evaluate the expression in the global context of the inspected window.
    return [ this.wrapConsoleObject(contentWindow.eval(expression)), false ];
  } catch (e) {
    return [ e.toString(), true ];
  }
};
