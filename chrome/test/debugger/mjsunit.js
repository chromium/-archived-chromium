/*
 * This file is included in all mini jsunit test cases.  The test
 * framework expects lines that signal failed tests to start with
 * the f-word and ignore all other lines.
 */

// Avoid writing the f-word, since some tests output parts of this code.
var the_f_word = "Fai" + "lure";

function fail(expected, found, name_opt) {
  var start;
  if (name_opt) {
    start = the_f_word + " (" + name_opt + "): ";
  } else {
    start = the_f_word + ":";
  }
  print(start + " expected <" + expected + "> found <" + found + ">");
}


function assertEquals(expected, found, name_opt) {
  if (expected != found) {
    fail(expected, found, name_opt);
  }
}


function assertArrayEquals(expected, found, name_opt) {
  var start = "";
  if (name_opt) {
    start = name_opt + " - ";
  }
  assertEquals(expected.length, found.length, start + "array length");
  if (expected.length == found.length) {
    for (var i = 0; i < expected.length; ++i) {
      assertEquals(expected[i], found[i], start + "array element at index " + i);
    }
  }
}


function assertTrue(value, name_opt) {
  assertEquals(true, value, name_opt);
}


function assertFalse(value, name_opt) {
  assertEquals(false, value, name_opt);
}


function assertNaN(value, name_opt) {
  if (!isNaN(value)) {
    fail("NaN", value, name_opt);
  }
}


function assertThrows(code) {
  try {
    eval(code);
    assertTrue(false, "did not throw exception");
  } catch (e) {
    // Do nothing.
  }
}


function assertInstanceof(obj, type) {
  if (!(obj instanceof type)) {
    assertTrue(false, "Object <" + obj + "> is not an instance of <" + type + ">");
  }
}


function assertDoesNotThrow(code) {
  try {
    eval(code);
  } catch (e) {
    assertTrue(false, "threw an exception");
  }
}


function assertUnreachable(name_opt) {
  var message = the_f_word + ": unreachable"
  if (name_opt) {
    message += " - " + name_opt;
  }
  print(message);
}

