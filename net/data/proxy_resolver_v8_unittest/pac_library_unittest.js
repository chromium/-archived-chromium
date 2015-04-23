// This should output "PROXY success:80" if all the tests pass.
// Otherwise it will output "PROXY failure:<num-failures>".
//
// This aims to unit-test the PAC library functions, which are
// exposed in the PAC's execution environment. (Namely, dnsDomainLevels, 
// timeRange, etc.)

function FindProxyForURL(url, host) {
  var numTestsFailed = 0;

  // Run all the tests
  for (var test in Tests) {
    var t = new TestContext(test);

    // Run the test.
    Tests[test](t);

    if (t.failed()) {
      numTestsFailed++;
    }
  }

  if (numTestsFailed == 0) {
    return "PROXY success:80";
  }
  return "PROXY failure:" + numTestsFailed;
}

// --------------------------
// Tests
// --------------------------

var Tests = {};

Tests.testDnsDomainIs = function(t) {
  t.expectTrue(dnsDomainIs("google.com", ".com"));
  t.expectTrue(dnsDomainIs("google.co.uk", ".co.uk"));
  t.expectFalse(dnsDomainIs("google.com", ".co.uk"));
};

Tests.testDnsDomainLevels = function(t) {
  t.expectEquals(0, dnsDomainLevels("www"));
  t.expectEquals(2, dnsDomainLevels("www.google.com"));
  t.expectEquals(3, dnsDomainLevels("192.168.1.1"));
};

Tests.testIsInNet = function(t) {
  t.expectTrue(
      isInNet("192.89.132.25", "192.89.132.25", "255.255.255.255"));
  t.expectFalse(
      isInNet("193.89.132.25", "192.89.132.25", "255.255.255.255"));
 
  t.expectTrue(isInNet("192.89.132.25", "192.89.0.0", "255.255.0.0"));
  t.expectFalse(isInNet("193.89.132.25", "192.89.0.0", "255.255.0.0"));

  t.expectFalse(
      isInNet("192.89.132.a", "192.89.0.0", "255.255.0.0"));
};

Tests.testIsPlainHostName = function(t) {
  t.expectTrue(isPlainHostName("google"));
  t.expectFalse(isPlainHostName("google.com"));
};

Tests.testLocalHostOrDomainIs = function(t) {
  t.expectTrue(localHostOrDomainIs("www.google.com", "www.google.com"));
  t.expectTrue(localHostOrDomainIs("www", "www.google.com"));
  t.expectFalse(localHostOrDomainIs("maps.google.com", "www.google.com"));
};

Tests.testShExpMatch = function(t) {
  t.expectTrue(shExpMatch("foo.jpg", "*.jpg"));
  t.expectTrue(shExpMatch("foo5.jpg", "*o?.jpg"));
  t.expectFalse(shExpMatch("foo.jpg", ".jpg"));
  t.expectFalse(shExpMatch("foo.jpg", "foo"));
};

Tests.testWeekdayRange = function(t) {
  // Test with local time.
  MockDate.setCurrent("Tue Mar 03 2009");
  t.expectEquals(true, weekdayRange("MON", "FRI"));
  t.expectEquals(true, weekdayRange("TUE", "FRI"));
  t.expectEquals(true, weekdayRange("TUE", "TUE"));
  t.expectEquals(true, weekdayRange("TUE"));
  t.expectEquals(false, weekdayRange("WED", "FRI"));
  t.expectEquals(false, weekdayRange("SUN", "MON"));
  t.expectEquals(false, weekdayRange("SAT"));
  t.expectEquals(false, weekdayRange("FRI", "MON"));

  // Test with GMT time.
  MockDate.setCurrent("Tue Mar 03 2009 GMT");
  t.expectEquals(true, weekdayRange("MON", "FRI", "GMT"));
  t.expectEquals(true, weekdayRange("TUE", "FRI", "GMT"));
  t.expectEquals(true, weekdayRange("TUE", "TUE", "GMT"));
  t.expectEquals(true, weekdayRange("TUE", "GMT"));
  t.expectEquals(false, weekdayRange("WED", "FRI", "GMT"));
  t.expectEquals(false, weekdayRange("SUN", "MON", "GMT"));
  t.expectEquals(false, weekdayRange("SAT", "GMT"));
};

Tests.testDateRange = function(t) {
  // dateRange(day)
  MockDate.setCurrent("Mar 03 2009");
  t.expectEquals(true, dateRange(3));
  t.expectEquals(false, dateRange(1));

  // dateRange(day, "GMT")
  MockDate.setCurrent("Mar 03 2009 GMT");
  t.expectEquals(true, dateRange(3, "GMT"));
  t.expectEquals(false, dateRange(1, "GMT"));

  // dateRange(day1, day2)
  MockDate.setCurrent("Mar 03 2009");
  t.expectEquals(true, dateRange(1, 4));
  t.expectEquals(false, dateRange(4, 20));

  // dateRange(day, month)
  MockDate.setCurrent("Mar 03 2009");
  t.expectEquals(true, dateRange(3, "MAR"));
  MockDate.setCurrent("Mar 03 2014");
  t.expectEquals(true, dateRange(3, "MAR"));
  // TODO(eroman):
  //t.expectEquals(false, dateRange(2, "MAR"));
  //t.expectEquals(false, dateRange(3, "JAN"));

  // dateRange(day, month, year)
  MockDate.setCurrent("Mar 03 2009");
  t.expectEquals(true, dateRange(3, "MAR", 2009));
  t.expectEquals(false, dateRange(4, "MAR", 2009));
  t.expectEquals(false, dateRange(3, "FEB", 2009));
  MockDate.setCurrent("Mar 03 2014");
  t.expectEquals(false, dateRange(3, "MAR", 2009));

  // dateRange(month1, month2)
  MockDate.setCurrent("Mar 03 2009");
  t.expectEquals(true, dateRange("JAN", "MAR"));
  t.expectEquals(true, dateRange("MAR", "APR"));
  t.expectEquals(false, dateRange("MAY", "SEP"));

  // dateRange(day1, month1, day2, month2)
  MockDate.setCurrent("Mar 03 2009");
  t.expectEquals(true, dateRange(1, "JAN", 3, "MAR"));
  t.expectEquals(true, dateRange(3, "MAR", 4, "SEP"));
  t.expectEquals(false, dateRange(4, "MAR", 4, "SEP"));

  // dateRange(month1, year1, month2, year2)
  MockDate.setCurrent("Mar 03 2009");
  t.expectEquals(true, dateRange("FEB", 2009, "MAR", 2009));
  MockDate.setCurrent("Apr 03 2009");
  t.expectEquals(true, dateRange("FEB", 2009, "MAR", 2010));
  t.expectEquals(false, dateRange("FEB", 2009, "MAR", 2009));

  // dateRange(day1, month1, year1, day2, month2, year2)
  MockDate.setCurrent("Mar 03 2009");
  t.expectEquals(true, dateRange(1, "JAN", 2009, 3, "MAR", 2009));
  t.expectEquals(true, dateRange(3, "MAR", 2009, 4, "SEP", 2009));
  t.expectEquals(true, dateRange(3, "JAN", 2009, 4, "FEB", 2010));
  t.expectEquals(false, dateRange(4, "MAR", 2009, 4, "SEP", 2009));
};

Tests.testTimeRange = function(t) {
  // timeRange(hour)
  MockDate.setCurrent("Mar 03, 2009 03:34:01");
  t.expectEquals(true, timeRange(3));
  t.expectEquals(false, timeRange(2));

  // timeRange(hour1, hour2)
  MockDate.setCurrent("Mar 03, 2009 03:34:01");
  t.expectEquals(true, timeRange(2, 3));
  t.expectEquals(true, timeRange(2, 4));
  t.expectEquals(true, timeRange(3, 5));
  t.expectEquals(false, timeRange(1, 2));
  t.expectEquals(false, timeRange(11, 12));

  // timeRange(hour1, min1, hour2, min2)
  MockDate.setCurrent("Mar 03, 2009 03:34:01");
  t.expectEquals(true, timeRange(1, 0, 3, 34));
  t.expectEquals(true, timeRange(1, 0, 3, 35));
  t.expectEquals(true, timeRange(3, 34, 5, 0));
  t.expectEquals(false, timeRange(1, 0, 3, 0));
  t.expectEquals(false, timeRange(11, 0, 16, 0));

  // timeRange(hour1, min1, sec1, hour2, min2, sec2)
  MockDate.setCurrent("Mar 03, 2009 03:34:14");
  t.expectEquals(true, timeRange(1, 0, 0, 3, 34, 14));
  t.expectEquals(false, timeRange(1, 0, 0, 3, 34, 0));
  t.expectEquals(true, timeRange(1, 0, 0, 3, 35, 0));
  t.expectEquals(true, timeRange(3, 34, 0, 5, 0, 0));
  t.expectEquals(false, timeRange(1, 0, 0, 3, 0, 0));
  t.expectEquals(false, timeRange(11, 0, 0, 16, 0, 0));
};

// --------------------------
// TestContext
// --------------------------

// |name| is the name of the test being executed, it will be used when logging
// errors.
function TestContext(name) {
  this.numFailures_ = 0;
  this.name_ = name;
};

TestContext.prototype.failed = function() {
  return this.numFailures_ != 0;
};

TestContext.prototype.expectEquals = function(expectation, actual) {
  if (!(expectation === actual)) {
    this.numFailures_++;
    this.log("FAIL: expected: " + expectation + ", actual: " + actual);
  }
};

TestContext.prototype.expectTrue = function(x) {
  this.expectEquals(true, x);
};

TestContext.prototype.expectFalse = function(x) {
  this.expectEquals(false, x);
};

TestContext.prototype.log = function(x) {
  // Prefix with the test name that generated the log.
  try {
    alert(this.name_ + ": " + x);
  } catch(e) {
    // In case alert() is not defined.
  }
};

// --------------------------
// MockDate
// --------------------------

function MockDate() {
  this.wrappedDate_ = new MockDate.super_(MockDate.currentDateString_);
};

// Setup the MockDate so it forwards methods to "this.wrappedDate_" (which is a
// real Date object).  We can't simply chain the prototypes since Date() doesn't
// allow it.
MockDate.init = function() {
  MockDate.super_ = Date;

  function createProxyMethod(methodName) {
    return function() {
      return this.wrappedDate_[methodName]
          .apply(this.wrappedDate_, arguments);
    }
  };

  for (i in MockDate.methodNames_) {
    var methodName = MockDate.methodNames_[i];
    // Don't define the closure directly in the loop body, since Javascript's
    // crazy scoping rules mean |methodName| actually bleeds out of the loop!
    MockDate.prototype[methodName] = createProxyMethod(methodName);
  }

  // Replace the native Date() with our mock.
  Date = MockDate;
};

// Unfortunately Date()'s methods are non-enumerable, therefore list manually.
MockDate.methodNames_ = [
  "toString", "toDateString", "toTimeString", "toLocaleString",
  "toLocaleDateString", "toLocaleTimeString", "valueOf", "getTime",
  "getFullYear", "getUTCFullYear", "getMonth", "getUTCMonth",
  "getDate", "getUTCDate", "getDay", "getUTCDay", "getHours", "getUTCHours",
  "getMinutes", "getUTCMinutes", "getSeconds", "getUTCSeconds",
  "getMilliseconds", "getUTCMilliseconds", "getTimezoneOffset", "setTime",
  "setMilliseconds", "setUTCMilliseconds", "setSeconds", "setUTCSeconds",
  "setMinutes", "setUTCMinutes", "setHours", "setUTCHours", "setDate",
  "setUTCDate", "setMonth", "setUTCMonth", "setFullYear", "setUTCFullYear",
  "toGMTString", "toUTCString", "getYear", "setYear"
];

MockDate.setCurrent = function(currentDateString) {
  MockDate.currentDateString_ = currentDateString;
}

// Bind the methods to proxy requests to the wrapped Date().
MockDate.init();

