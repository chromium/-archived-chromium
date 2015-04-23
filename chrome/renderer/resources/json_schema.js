// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// -----------------------------------------------------------------------------
// NOTE: If you change this file you need to touch renderer_resources.grd to
// have your change take effect.
// -----------------------------------------------------------------------------

//==============================================================================
// This file contains a class that implements a subset of JSON Schema.
// See: http://www.json.com/json-schema-proposal/ for more details.
//
// The following features of JSON Schema are not implemented:
// - requires
// - unique
// - disallow
// - union types (but replaced with 'choices')
//
// The following properties are not applicable to the interface exposed by
// this class:
// - options
// - readonly
// - title
// - description
// - format
// - default
// - transient
// - hidden
//
// There are also these departures from the JSON Schema proposal:
// - function and undefined types are supported
// - null counts as 'unspecified' for optional values
// - added the 'choices' property, to allow specifying a list of possible types
//   for a value
// - made additionalProperties default to false
//==============================================================================

var chrome = chrome || {};

/**
 * Validates an instance against a schema and accumulates errors. Usage:
 *
 * var validator = new chrome.JSONSchemaValidator();
 * validator.validate(inst, schema);
 * if (validator.errors.length == 0)
 *   console.log("Valid!");
 * else
 *   console.log(validator.errors);
 *
 * The errors property contains a list of objects. Each object has two
 * properties: "path" and "message". The "path" property contains the path to
 * the key that had the problem, and the "message" property contains a sentence
 * describing the error.
 */
chrome.JSONSchemaValidator = function() {
  this.errors = [];
};

chrome.JSONSchemaValidator.messages = {
  invalidEnum: "Value must be one of: [*].",
  propertyRequired: "Property is required.",
  unexpectedProperty: "Unexpected property.",
  arrayMinItems: "Array must have at least * items.",
  arrayMaxItems: "Array must not have more than * items.",
  itemRequired: "Item is required.",
  stringMinLength: "String must be at least * characters long.",
  stringMaxLength: "String must not be more than * characters long.",
  stringPattern: "String must match the pattern: *.",
  numberMinValue: "Value must not be less than *.",
  numberMaxValue: "Value must not be greater than *.",
  numberMaxDecimal: "Value must not have more than * decimal places.",
  invalidType: "Expected '*' but got '*'.",
  invalidChoice: "Value does not match any valid type choices.",
  invalidPropertyType: "Missing property type.",
  schemaRequired: "Schema value required.",
};

/**
 * Builds an error message. Key is the property in the |errors| object, and
 * |opt_replacements| is an array of values to replace "*" characters with.
 */
chrome.JSONSchemaValidator.formatError = function(key, opt_replacements) {
  var message = this.messages[key];
  if (opt_replacements) {
    for (var i = 0; i < opt_replacements.length; i++) {
      message = message.replace("*", opt_replacements[i]);
    }
  }
  return message;
};

/**
 * Classifies a value as one of the JSON schema primitive types. Note that we
 * don't explicitly disallow 'function', because we want to allow functions in
 * the input values.
 */
chrome.JSONSchemaValidator.getType = function(value) {
  var s = typeof value;

  if (s == "object") {
    if (value === null) {
      return "null";
    } else if (value instanceof Array ||
               Object.prototype.toString.call(value) == "[Object Array]") {
      return "array";
    }
  } else if (s == "number") {
    if (value % 1 == 0) {
      return "integer";
    }
  }

  return s;
};

/**
 * Validates an instance against a schema. The instance can be any JavaScript
 * value and will be validated recursively. When this method returns, the
 * |errors| property will contain a list of errors, if any.
 */
chrome.JSONSchemaValidator.prototype.validate = function(instance, schema,
                                                         opt_path) {
  var path = opt_path || "";

  if (!schema) {
    this.addError(path, "schemaRequired");
    return;
  }

  // If the schema has an extends property, the instance must validate against
  // that schema too.
  if (schema.extends)
    this.validate(instance, schema.extends, path);

  // If the schema has a choices property, the instance must validate against at
  // least one of the items in that array.
  if (schema.choices) {
    this.validateChoices(instance, schema, path);
    return;
  }

  // If the schema has an enum property, the instance must be one of those
  // values.
  if (schema.enum) {
    if (!this.validateEnum(instance, schema, path))
      return;
  }

  if (schema.type && schema.type != "any") {
    if (!this.validateType(instance, schema, path))
      return;

    // Type-specific validation.
    switch (schema.type) {
      case "object":
        this.validateObject(instance, schema, path);
        break;
      case "array":
        this.validateArray(instance, schema, path);
        break;
      case "string":
        this.validateString(instance, schema, path);
        break;
      case "number":
      case "integer":
        this.validateNumber(instance, schema, path);
        break;
    }
  }
};

/**
 * Validates an instance against a choices schema. The instance must match at
 * least one of the provided choices.
 */
chrome.JSONSchemaValidator.prototype.validateChoices = function(instance,
                                                                schema,
                                                                path) {
  var originalErrors = this.errors;

  for (var i = 0; i < schema.choices.length; i++) {
    this.errors = [];
    this.validate(instance, schema.choices[i], path);
    if (this.errors.length == 0) {
      this.errors = originalErrors;
      return;
    }
  }

  this.errors = originalErrors;
  this.addError(path, "invalidChoice");
};

/**
 * Validates an instance against a schema with an enum type. Populates the
 * |errors| property, and returns a boolean indicating whether the instance
 * validates.
 */
chrome.JSONSchemaValidator.prototype.validateEnum = function(instance, schema,
                                                             path) {
  for (var i = 0; i < schema.enum.length; i++) {
    if (instance === schema.enum[i])
      return true;
  }

  this.addError(path, "invalidEnum", [schema.enum.join(", ")]);
  return false;
};

/**
 * Validates an instance against an object schema and populates the errors
 * property.
 */
chrome.JSONSchemaValidator.prototype.validateObject = function(instance,
                                                               schema, path) {
  for (var prop in schema.properties) {
    var propPath = path ? path + "." + prop : prop;
    if (schema.properties[prop] == undefined) {
      this.addError(propPath, "invalidPropertyType");
    } else if (prop in instance && instance[prop] !== null &&
        instance[prop] !== undefined) {
      this.validate(instance[prop], schema.properties[prop], propPath);
    } else if (!schema.properties[prop].optional) {
      this.addError(propPath, "propertyRequired");
    }
  }

  // By default, additional properties are not allowed on instance objects. This
  // can be overridden by setting the additionalProperties property to a schema
  // which any additional properties must validate against.
  for (var prop in instance) {
    if (prop in schema.properties)
      continue;

    var propPath = path ? path + "." + prop : prop;
    if (schema.additionalProperties)
      this.validate(instance[prop], schema.additionalProperties, propPath);
    else
      this.addError(propPath, "unexpectedProperty");
  }
};

/**
 * Validates an instance against an array schema and populates the errors
 * property.
 */
chrome.JSONSchemaValidator.prototype.validateArray = function(instance,
                                                              schema, path) {
  var typeOfItems = chrome.JSONSchemaValidator.getType(schema.items);

  if (typeOfItems == 'object') {
    if (schema.minItems && instance.length < schema.minItems) {
      this.addError(path, "arrayMinItems", [schema.minItems]);
    }

    if (typeof schema.maxItems != "undefined" &&
        instance.length > schema.maxItems) {
      this.addError(path, "arrayMaxItems", [schema.maxItems]);
    }

    // If the items property is a single schema, each item in the array must
    // have that schema.
    for (var i = 0; i < instance.length; i++) {
      this.validate(instance[i], schema.items, path + "." + i);
    }
  } else if (typeOfItems == 'array') {
    // If the items property is an array of schemas, each item in the array must
    // validate against the corresponding schema.
    for (var i = 0; i < schema.items.length; i++) {
      var itemPath = path ? path + "." + i : String(i);
      if (i in instance && instance[i] !== null && instance[i] !== undefined) {
        this.validate(instance[i], schema.items[i], itemPath);
      } else if (!schema.items[i].optional) {
        this.addError(itemPath, "itemRequired");
      }
    }

    if (schema.additionalProperties) {
      for (var i = schema.items.length; i < instance.length; i++) {
        var itemPath = path ? path + "." + i : String(i);
        this.validate(instance[i], schema.additionalProperties, itemPath);
      }
    } else {
      if (instance.length > schema.items.length) {
        this.addError(path, "arrayMaxItems", [schema.items.length]);
      }
    }
  }
};

/**
 * Validates a string and populates the errors property.
 */
chrome.JSONSchemaValidator.prototype.validateString = function(instance,
                                                               schema, path) {
  if (schema.minLength && instance.length < schema.minLength)
    this.addError(path, "stringMinLength", [schema.minLength]);

  if (schema.maxLength && instance.length > schema.maxLength)
    this.addError(path, "stringMaxLength", [schema.maxLength]);

  if (schema.pattern && !schema.pattern.test(instance))
    this.addError(path, "stringPattern", [schema.pattern]);
};

/**
 * Validates a number and populates the errors property. The instance is
 * assumed to be a number.
 */
chrome.JSONSchemaValidator.prototype.validateNumber = function(instance,
                                                               schema, path) {
  if (schema.minimum && instance < schema.minimum)
    this.addError(path, "numberMinValue", [schema.minimum]);

  if (schema.maximum && instance > schema.maximum)
    this.addError(path, "numberMaxValue", [schema.maximum]);

  if (schema.maxDecimal && instance * Math.pow(10, schema.maxDecimal) % 1)
    this.addError(path, "numberMaxDecimal", [schema.maxDecimal]);
};

/**
 * Validates the primitive type of an instance and populates the errors
 * property. Returns true if the instance validates, false otherwise.
 */
chrome.JSONSchemaValidator.prototype.validateType = function(instance, schema,
                                                             path) {
  var actualType = chrome.JSONSchemaValidator.getType(instance);
  if (schema.type != actualType && !(schema.type == "number" &&
      actualType == "integer")) {
    this.addError(path, "invalidType", [schema.type, actualType]);
    return false;
  }

  return true;
};

/**
 * Adds an error message. |key| is an index into the |messages| object.
 * |replacements| is an array of values to replace '*' characters in the
 * message.
 */
chrome.JSONSchemaValidator.prototype.addError = function(path, key,
                                                         replacements) {
  this.errors.push({
    path: path,
    message: chrome.JSONSchemaValidator.formatError(key, replacements)
  });
};

// Set up chrome.types with some commonly used types...
(function() {
  function extend(base, ext) {
    var result = {};
    for (var p in base)
      result[p] = base[p];
    for (var p in ext)
      result[p] = ext[p];
    return result;
  }

  var types = {};
  types.opt = {optional: true};
  types.bool = {type: "boolean"};
  types.int = {type: "integer"};
  types.str = {type: "string"};
  types.fun = {type: "function"};
  types.pInt = extend(types.int, {minimum: 0});
  types.optBool = extend(types.bool, types.opt);
  types.optInt = extend(types.int, types.opt);
  types.optStr = extend(types.str, types.opt);
  types.optFun = extend(types.fun, types.opt);
  types.optPInt = extend(types.pInt, types.opt);
  types.singleOrListOf = function(type) {
    return {
      choice: [
        type,
        { type: "array",  item: type,  minItems: 1 }
      ]
    };
  };

  chrome.types = types;
})();
