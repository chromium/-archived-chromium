// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//==============================================================================
// This file contains a class that implements a subset of JSON Schema.
// See: http://www.json.com/json-schema-proposal/ for more details.
//
// The following features of JSON Schema are not implemented:
// - requires
// - unique
// - disallow
// - union types
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
//==============================================================================

var chromium = chromium || {};

/**
 * Validates an instance against a schema and accumulates errors. Usage:
 *
 * var validator = new chromium.JSONSchemaValidator();
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
chromium.JSONSchemaValidator = function() {
  this.errors = [];
};

chromium.JSONSchemaValidator.messages = {
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
  invalidType: "Expected '*' but got '*'."
};

/**
 * Builds an error message. Key is the property in the |errors| object, and
 * |opt_replacements| is an array of values to replace "*" characters with.
 */
chromium.JSONSchemaValidator.formatError = function(key, opt_replacements) {
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
chromium.JSONSchemaValidator.getType = function(value) {
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
chromium.JSONSchemaValidator.prototype.validate = function(instance, schema,
                                                           opt_path) {
  var path = opt_path || "";

  // If the schema has an extends property, the instance must validate against
  // that schema too.
  if (schema.extends)
    this.validate(instance, schema.extends, path);

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
 * Validates an instance against a schema with an enum type. Populates the
 * |errors| property, and returns a boolean indicating whether the instance
 * validates.
 */
chromium.JSONSchemaValidator.prototype.validateEnum = function(instance, schema,
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
chromium.JSONSchemaValidator.prototype.validateObject = function(instance,
                                                                 schema, path) {
  for (var prop in schema.properties) {
    var propPath = path ? path + "." + prop : prop;
    if (instance.hasOwnProperty(prop)) {
      this.validate(instance[prop], schema.properties[prop], propPath);
    } else if (!schema.properties[prop].optional) {
      this.addError(propPath, "propertyRequired");
    }
  }

  // The additionalProperties property can either be |false| or a schema
  // definition. If |false|, additional properties are not allowed. If a schema
  // defintion, all additional properties must validate against that schema.
  if (typeof schema.additionalProperties != "undefined") {
    for (var prop in instance) {
      if (instance.hasOwnProperty(prop)) {
        var propPath = path ? path + "." + prop : prop;
        if (!schema.properties.hasOwnProperty(prop)) {
          if (schema.additionalProperties === false)
            this.addError(propPath, "unexpectedProperty");
          else
            this.validate(instance[prop], schema.additionalProperties, propPath);
        }
      }
    }
  }
};

/**
 * Validates an instance against an array schema and populates the errors
 * property.
 */
chromium.JSONSchemaValidator.prototype.validateArray = function(instance,
                                                                schema, path) {
  var typeOfItems = chromium.JSONSchemaValidator.getType(schema.items);

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
      this.validate(instance[i], schema.items, path + "[" + i + "]");
    }
  } else if (typeOfItems == 'array') {
    // If the items property is an array of schemas, each item in the array must
    // validate against the corresponding schema.
    for (var i = 0; i < schema.items.length; i++) {
      var itemPath = path ? path + "[" + i + "]" : String(i);
      if (instance.hasOwnProperty(i)) {
        this.validate(instance[i], schema.items[i], itemPath);
      } else if (!schema.items[i].optional) {
        this.addError(itemPath, "itemRequired");
      }
    }

    if (schema.additionalProperties === false) {
      if (instance.length > schema.items.length) {
        this.addError(path, "arrayMaxItems", [schema.items.length]);
      }
    } else if (schema.additionalProperties) {
      for (var i = schema.items.length; i < instance.length; i++) {
        var itemPath = path ? path + "[" + i + "]" : String(i);
        this.validate(instance[i], schema.additionalProperties, itemPath);
      }
    }
  }
};

/**
 * Validates a string and populates the errors property.
 */
chromium.JSONSchemaValidator.prototype.validateString = function(instance,
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
chromium.JSONSchemaValidator.prototype.validateNumber = function(instance,
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
chromium.JSONSchemaValidator.prototype.validateType = function(instance, schema,
                                                               path) {
  var actualType = chromium.JSONSchemaValidator.getType(instance);
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
chromium.JSONSchemaValidator.prototype.addError = function(path, key,
                                                           replacements) {
  this.errors.push({
    path: path,
    message: chromium.JSONSchemaValidator.formatError(key, replacements)
  });
};

// Set up chromium.types with some commonly used types...
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

  chromium.types = types;
})();
