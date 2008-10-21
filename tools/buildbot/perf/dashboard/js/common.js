function Fetch(url, callback) {
  var r = new XMLHttpRequest();
  r.open("GET", url);
  r.setRequestHeader("pragma", "no-cache");
  r.setRequestHeader("cache-control", "no-cache");
  r.send(null);

  r.onload = function() {
    callback(r.responseText);
  }
}

// returns an Object with properties given by the parameters specified in the
// URL's query string.
function ParseParams() {
  var result = new Object();
  var s = window.location.search.substring(1).split('&');
  for (i = 0; i < s.length; ++i) {
    var v = s[i].split('=');
    result[v[0]] = v[1];
  }
  return result;
}

// create the URL constructed from the current pathname and the given params.
function MakeURL(params) {
  var url = window.location.pathname;
  var sep = '?';
  for (p in params) {
    if (!p)
      continue;
    url = url + sep + p + '=' + params[p];
    sep = '&';
  }
  return url;
}
