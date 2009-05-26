function onSuccess()
{
  setTimeout(onFinished, 0, "OK");
}

function onFailure() {
  setTimeout(onFinished, 0, "FAIL");
}

function onFinished(result) {
  var statusPanel = document.getElementById("statusPanel");
  if (statusPanel) {
    statusPanel.innerHTML = result;
  }

  var cookie = "status=" + result + "; path=/";
  document.cookie = cookie;
}