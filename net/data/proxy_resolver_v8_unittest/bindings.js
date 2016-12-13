// Try calling the browser-side bound functions with varying (invalid)
// inputs. There is no notion of "success" for this test, other than
// verifying the correct C++ bindings were reached with expected values.

function MyObject() {
  this.x = "3";
}

MyObject.prototype.toString = function() {
  throw "exception from calling toString()";
}

function FindProxyForURL(url, host) {
  // Call dnsResolve with some wonky arguments.
  dnsResolve();
  dnsResolve(null);
  dnsResolve(undefined);
  dnsResolve("");
  dnsResolve({foo: 'bar'});
  dnsResolve(fn);
  dnsResolve(['3']);
  dnsResolve("arg1", "arg2", "arg3", "arg4");

  // Call alert with some wonky arguments.
  alert();
  alert(null);
  alert(undefined);
  alert({foo:'bar'});

  // This should throw an exception when we toString() the argument
  // to alert in the bindings.
  try {
    alert(new MyObject());
  } catch (e) {
    alert(e);
  }

  // Call myIpAddress() with wonky arguments 
  myIpAddress(null);
  myIpAddress(null, null);

  return "DIRECT";
}

function fn() {}

