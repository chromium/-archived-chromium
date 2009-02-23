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
  // TODO(eroman):

  // t.expectTrue(
  //     isInNet("192.89.132.25", "192.89.132.25", "255.255.255.255"));
  // t.expectFalse(
  //     isInNet("193.89.132.25", "192.89.132.25", "255.255.255.255"));
  //
  // t.expectTrue(isInNet("192.89.132.25", "192.89.0.0", "255.255.0.0"));
  // t.expectFalse(isInNet("193.89.132.25", "192.89.0.0", "255.255.0.0"));
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
  // TODO(eroman):

  //t.expectTrue(shExpMatch("http://maps.google.com/blah/foo/moreblah.jpg",
  //   ".*/foo/.*jpg"));

  //t.expectFalse(shExpMatch("http://maps.google.com/blah/foo/moreblah.jpg",
  //    ".*/foo/.*.html"));
};

Tests.testWeekdayRange = function(t) {
  // TODO(eroman)
};

Tests.testDateRange = function(t) {
  // TODO(eroman)
};

Tests.testTimeRange = function(t) {
  // TODO(eroman)
};

// --------------------------
// Helpers
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

