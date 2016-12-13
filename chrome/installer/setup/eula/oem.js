function setInnerFrame() {
  var regex = new RegExp("\\?innerframe=\"?([^&#\"]*)\"?");
  var results = regex.exec(window.location.href);
  if(results && results[1])
    document.getElementById('ifr').src = results[1];
}

function checkAccept(f) {
  if (f.accept.checked) {
    window.returnValue = 6;
  } else {
    window.returnValue = 1;
  }
  window.close();
}

function resize() {
  var ifr = document.getElementById('ifr');
  var footer = document.getElementById('footer');
  
  ifr.height = footer.offsetTop - ifr.offsetTop;
  setInnerFrame();
}

window.onresize = resize;
window.onload = resize;