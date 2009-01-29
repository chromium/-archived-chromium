function checkAccept(f) {
  if (f.accept.checked) {
    window.returnValue = 1;
  } else {
    window.returnValue = 6;
  }
  window.close();
}

function resize() {
  var ifr = document.getElementById('ifr');
  var footer = document.getElementById('footer');
  
  ifr.height = footer.offsetTop - ifr.offsetTop;
}

window.onresize = resize;
window.onload = resize;