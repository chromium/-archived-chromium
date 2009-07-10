function claim() {
  var views = chrome.self.getViews();
  for (var i = 0; i < views.length; i++) {
    views[i].respond(document.getElementById("me").innerHTML);
  }
}

function respond(val) {
  var me = document.getElementById("me");
  if (val == me.innerHTML) {
    me.className = "strong";
  } else { 
    me.className = "weak";
  }
}