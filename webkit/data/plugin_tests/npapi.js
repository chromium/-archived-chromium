//
// This script provides some mechanics for testing NPAPI
//

/* public */ function onSuccess(name, id)
{
  setTimeout(onFinished, 0, name, id, "OK");
}

/* public */ function onFailure(name, id, status)
{
  setTimeout(onFinished, 0, name, id, status);
}



// Finish running a test by setting the status 
// and the cookie.
/* private */ function onFinished(name, id, result)
{
  var statusPanel = document.getElementById("statusPanel");
  if (statusPanel) {
    statusPanel.innerHTML = result;
  }

  var cookie = name + "." + id + ".status=" + result + "; path=/";
  document.cookie = cookie;
}
