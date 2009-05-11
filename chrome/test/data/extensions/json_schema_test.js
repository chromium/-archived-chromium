// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function assert(truth) {
  if (!truth)
    throw new Error("Assertion failed.");
}

function assertValid(type, instance, schema) {
  var validator = new chrome.JSONSchemaValidator();
  validator["validate" + type](instance, schema, "");
  if (validator.errors.length != 0) {
    log("Got unexpected errors");
    log(validator.errors);
    assert(false);
  }
}

function assertNotValid(type, instance, schema, errors) {
  var validator = new chrome.JSONSchemaValidator();
  validator["validate" + type](instance, schema, "");
  assert(validator.errors.length === errors.length);
  for (var i = 0; i < errors.length; i++) {
    if (validator.errors[i].message == errors[i]) {
      log("Got expected error: " + validator.errors[i].message + " for path: " +
          validator.errors[i].path);
    } else {
      log("Missed expected error: " + errors[i] + ". Got: " +
          validator.errors[i].message + " instead.");
      assert(false);
    }
  }
}

function formatError(key, replacements) {
  return chrome.JSONSchemaValidator.formatError(key, replacements);
}

function testFormatError() {
  assert(formatError("propertyRequired") == "Property is required.");
  assert(formatError("invalidEnum", ["foo, bar"]) ==
         "Value must be one of: [foo, bar].");
  assert(formatError("invalidType", ["foo", "bar"]) ==
         "Expected 'foo' but got 'bar'.");
}

function testComplex() {
  var schema = {
    type: "array",
    items: [
      {
        type: "object",
        properties: {
          id: {
            type: "integer",
            minimum: 1
          },
          url: {
            type: "string",
            pattern: /^https?\:/,
            optional: true
          },
          index: {
            type: "integer",
            minimum: 0,
            optional: true
          },
          selected: {
            type: "boolean",
            optional: true
          }
        }
      },
      { type: "function", optional: true },
      { type: "function", optional: true }
    ]
  };

  var instance = [
    {
      id: 42,
      url: "http://www.google.com/",
      index: 2,
      selected: true
    },
    function(){},
    function(){}
  ];

  assertValid("", instance, schema);
  instance.length = 2;
  assertValid("", instance, schema);
  instance.length = 1;
  instance.push({});
  assertNotValid("", instance, schema,
                 [formatError("invalidType", ["function", "object"])]);
  instance[1] = function(){};

  instance[0].url = "foo";
  assertNotValid("", instance, schema,
                 [formatError("stringPattern",
                              [schema.items[0].properties.url.pattern])]);
  delete instance[0].url;
  assertValid("", instance, schema);

  instance[0].id = 0;
  assertNotValid("", instance, schema,
                 [formatError("numberMinValue",
                              [schema.items[0].properties.id.minimum])]);
}

function testEnum() {
  var schema = {
    enum: ["foo", 42, false]
  };

  assertValid("", "foo", schema);
  assertValid("", 42, schema);
  assertValid("", false, schema);
  assertNotValid("", "42", schema, [formatError("invalidEnum",
                                                [schema.enum.join(", ")])]);
  assertNotValid("", null, schema, [formatError("invalidEnum",
                                                [schema.enum.join(", ")])]);
}

function testChoices() {
  var schema = {
    choices: [
      { type: "null" },
      { type: "undefined" },
      { type: "integer", minimum:42, maximum:42 },
      { type: "object", properties: { foo: { type: "string" } } }
    ]
  }
    assertValid("", null, schema);
    assertValid("", undefined, schema);
    assertValid("", 42, schema);
    assertValid("", {foo: "bar"}, schema);

    assertNotValid("", "foo", schema, [formatError("invalidChoice", [])]);
    assertNotValid("", [], schema, [formatError("invalidChoice", [])]);
    assertNotValid("", {foo: 42}, schema, [formatError("invalidChoice", [])]);
}

function testExtends() {
  var parent = {
    type: "number"
  }
  var schema = {
    extends: parent
  };

  assertValid("", 42, schema);
  assertNotValid("", "42", schema,
                 [formatError("invalidType", ["number", "string"])]);

  // Make the derived schema more restrictive
  parent.minimum = 43;
  assertNotValid("", 42, schema, [formatError("numberMinValue", [43])]);
  assertValid("", 43, schema);
}

function testObject() {
  var schema = {
    properties: {
      "foo": {
        type: "string"
      },
      "bar": {
        type: "integer"
      }
    }
  };

  assertValid("Object", {foo:"foo",bar:42}, schema);
  assertNotValid("Object", {foo:"foo",bar:42,"extra":true}, schema,
                 [formatError("unexpectedProperty")]);
  assertNotValid("Object", {foo:"foo"}, schema,
                 [formatError("propertyRequired")]);
  assertNotValid("Object", {foo:"foo", bar:"42"}, schema,
                 [formatError("invalidType", ["integer", "string"])]);

  schema.additionalProperties = { type: "any" };
  assertValid("Object", {foo:"foo",bar:42,"extra":true}, schema);
  assertValid("Object", {foo:"foo",bar:42,"extra":"foo"}, schema);

  schema.additionalProperties = { type: "boolean" };
  assertValid("Object", {foo:"foo",bar:42,"extra":true}, schema);
  assertNotValid("Object", {foo:"foo",bar:42,"extra":"foo"}, schema,
                 [formatError("invalidType", ["boolean", "string"])]);

  schema.properties.bar.optional = true;
  assertValid("Object", {foo:"foo",bar:42}, schema);
  assertValid("Object", {foo:"foo"}, schema);
  assertValid("Object", {foo:"foo",bar:null}, schema);
  assertValid("Object", {foo:"foo",bar:undefined}, schema);
  assertNotValid("Object", {foo:"foo", bar:"42"}, schema,
                 [formatError("invalidType", ["integer", "string"])]);
}

function testArrayTuple() {
  var schema = {
    items: [
      {
        type: "string"
      },
      {
        type: "integer"
      }
    ]
  };

  assertValid("Array", ["42", 42], schema);
  assertNotValid("Array", ["42", 42, "anything"], schema,
                 [formatError("arrayMaxItems", [schema.items.length])]);
  assertNotValid("Array", ["42"], schema, [formatError("itemRequired")]);
  assertNotValid("Array", [42, 42], schema,
                 [formatError("invalidType", ["string", "integer"])]);

  schema.additionalProperties = { type: "any" };
  assertValid("Array", ["42", 42, "anything"], schema);
  assertValid("Array", ["42", 42, []], schema);

  schema.additionalProperties = { type: "boolean" };
  assertNotValid("Array", ["42", 42, "anything"], schema,
                 [formatError("invalidType", ["boolean", "string"])]);
  assertValid("Array", ["42", 42, false], schema);

  schema.items[0].optional = true;
  assertValid("Array", ["42", 42], schema);
  assertValid("Array", [, 42], schema);
  assertValid("Array", [null, 42], schema);
  assertValid("Array", [undefined, 42], schema);
  assertNotValid("Array", [42, 42], schema,
                 [formatError("invalidType", ["string", "integer"])]);
}

function testArrayNonTuple() {
  var schema = {
    items: {
      type: "string"
    },
    minItems: 2,
    maxItems: 3
  };

  assertValid("Array", ["x", "x"], schema);
  assertValid("Array", ["x", "x", "x"], schema);

  assertNotValid("Array", ["x"], schema,
                 [formatError("arrayMinItems", [schema.minItems])]);
  assertNotValid("Array", ["x", "x", "x", "x"], schema,
                 [formatError("arrayMaxItems", [schema.maxItems])]);
  assertNotValid("Array", [42], schema,
                 [formatError("arrayMinItems", [schema.minItems]),
                  formatError("invalidType", ["string", "integer"])]);
}

function testString() {
  var schema = {
    minLength: 1,
    maxLength: 10,
    pattern: /^x/
  };

  assertValid("String", "x", schema);
  assertValid("String", "xxxxxxxxxx", schema);

  assertNotValid("String", "y", schema,
                 [formatError("stringPattern", [schema.pattern])]);
  assertNotValid("String", "xxxxxxxxxxx", schema,
                 [formatError("stringMaxLength", [schema.maxLength])]);
  assertNotValid("String", "", schema,
                 [formatError("stringMinLength", [schema.minLength]),
                  formatError("stringPattern", [schema.pattern])]);
}

function testNumber() {
  var schema = {
    minimum: 1,
    maximum: 100,
    maxDecimal: 2
  };

  assertValid("Number", 1, schema);
  assertValid("Number", 50, schema);
  assertValid("Number", 100, schema);
  assertValid("Number", 88.88, schema);

  assertNotValid("Number", 0.5, schema,
                 [formatError("numberMinValue", [schema.minimum])]);
  assertNotValid("Number", 100.1, schema,
                 [formatError("numberMaxValue", [schema.maximum])]);
  assertNotValid("Number", 100.111, schema,
                 [formatError("numberMaxValue", [schema.maximum]),
                  formatError("numberMaxDecimal", [schema.maxDecimal])]);
}

function testType() {
  // valid
  assertValid("Type", {}, {type:"object"});
  assertValid("Type", [], {type:"array"});
  assertValid("Type", function(){}, {type:"function"});
  assertValid("Type", "foobar", {type:"string"});
  assertValid("Type", "", {type:"string"});
  assertValid("Type", 88.8, {type:"number"});
  assertValid("Type", 42, {type:"number"});
  assertValid("Type", 0, {type:"number"});
  assertValid("Type", 42, {type:"integer"});
  assertValid("Type", 0, {type:"integer"});
  assertValid("Type", true, {type:"boolean"});
  assertValid("Type", false, {type:"boolean"});
  assertValid("Type", null, {type:"null"});
  assertValid("Type", undefined, {type:"undefined"});

  // not valid
  assertNotValid("Type", [], {type: "object"},
                 [formatError("invalidType", ["object", "array"])]);
  assertNotValid("Type", null, {type: "object"},
                 [formatError("invalidType", ["object", "null"])]);
  assertNotValid("Type", function(){}, {type: "object"},
                 [formatError("invalidType", ["object", "function"])]);
  assertNotValid("Type", 42, {type: "array"},
                 [formatError("invalidType", ["array", "integer"])]);
  assertNotValid("Type", 42, {type: "string"},
                 [formatError("invalidType", ["string", "integer"])]);
  assertNotValid("Type", "42", {type: "number"},
                 [formatError("invalidType", ["number", "string"])]);
  assertNotValid("Type", 88.8, {type: "integer"},
                 [formatError("invalidType", ["integer", "number"])]);
  assertNotValid("Type", 1, {type: "boolean"},
                 [formatError("invalidType", ["boolean", "integer"])]);
  assertNotValid("Type", false, {type: "null"},
                 [formatError("invalidType", ["null", "boolean"])]);
  assertNotValid("Type", undefined, {type: "null"},
                 [formatError("invalidType", ["null", "undefined"])]);
  assertNotValid("Type", {}, {type: "function"},
                 [formatError("invalidType", ["function", "object"])]);
}
