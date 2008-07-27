// sort object

var manual = 0; // single stepping
var interval_time = 0; // setTimeout interval
var inner_loop_enabled = 1; // should the stepper iterate a little during each step

// number of elements
var size = 300;
var query = window.location.search.substring(1);
var params = query.split('&');
for (var i = 0; i < params.length; i++) {
  var pos = params[i].indexOf('=');
  var key = params[i].substring(0, pos);
  var val = params[i].substring(pos+1);
  if (key == "size") {
    var sz = parseInt(val);
    size = Math.max(sz, 3);
    size = Math.min(1000, size);
  }
}

var log;
function log(msg) {
  if (window.console != undefined) {
    window.console.log(msg);
  }
}

function Sort(name, func) {
  this.name = name;
  this.func = func;
  this.size = size;
  this.compare_x = null;
  this.compare_y = null;
  this.compares = 0;
  this.swap_x = null;
  this.swap_y = null;
  this.swaps = 0;
  this.start_time = 0;
  this.stop_time = 0;
  this.work_queue = new Array();
  this.timer = 0;
  this.last_time = 0;
  this.num_iterations = 0;
  this.num_jobs = 0;
  this.overhead_total = 0;
  this.overhead_min = 1000000;
  this.overhead_max = 0;
  this.processing_total = 0;
  this.processing_min = 1000000;
  this.processing_max = 0;
  this.step_min = 1000000;
  this.step_max = 0;

  this.setup();
}

Sort.prototype.setup = function() {
  this.size = size;
  this.bars = new Array(this.size);
  this.numbers = new Array(this.size);
  for (i = 0; i < this.size; i++) {
    this.numbers[i] = i + 1;
  }
  for (i = 0; i < this.size; i++) {
    var r = Math.floor(Math.random() * this.numbers.length);
    if (i != r) {
      var tmp = this.numbers[i];
      this.numbers[i] = this.numbers[r];
      this.numbers[r] = tmp;
    }
  }
}

Sort.prototype.status = function(str) {
  var label = document.getElementById(this.name + "_label");
  label.innerHTML = "<b>" + this.name + " Sort</b><br />" + str;
}

Sort.prototype.stepper = function() {
  if (!manual) {
    var sort = this;
    this.timer = setTimeout(function(){sort.stepper();},interval_time);
  }
  var t = new Date();
  var overhead = t - this.last_time;
  this.overhead_total += overhead;
  this.overhead_min = Math.min(this.overhead_min, overhead);
  this.overhead_max = Math.max(this.overhead_max, overhead);
  this.last_time = t;

  var elapsed = t - this.start_time;
  var avg =
    Math.floor((elapsed - this.processing_total) / this.num_iterations);
  this.status("Overhead: " + avg + "ms");

  var ops = 0;
  for (;;) {
    var count = this.work_queue.length;
    if (count > 0) {
      var func = this.work_queue.pop();
      if (func.status != undefined) {
        //this.status(func.status);
      }
      ops++;
      this.num_jobs++;
      func();
    } else {
      break;
    }
    if (manual || inner_loop_enabled == 0) {
      break;
    }
    t = new Date();
    // If any measurable time has passed, we're good.
    // Since the Date has a resolution of 15ms on Windows
    // there's no way to differentiate accurately for anything
    // less than that.  We don't want to process for longer than
    // the timer interval anyway (which is about 10ms), so this
    // is fine.
    // NOTE: on non-windows platforms, this actually does matter since
    // their timer resolution is higher
    // TODO(erikkay): make this a parameter
    if (t - this.last_time > 10) {
      break;
    }
  }
  var processing = t - this.last_time;
  this.processing_min = Math.min(this.processing_min, processing);
  this.processing_max = Math.max(this.processing_max, processing);
  this.processing_total += processing;
  var step_time = processing + overhead;
  this.step_min = Math.min(this.step_min, step_time);
  this.step_max = Math.max(this.processing_max, step_time);
  this.num_iterations++;
  this.last_time = new Date();

  if (ops == 0) {
    this.finished();
  }
}

Sort.prototype.add_work = function(work, name) {
  if (name != undefined) {
    work.status = name;
  }
  this.work_queue.push(work);
}

Sort.prototype.init = function() {
  this.print();
  this.status("");
}

Sort.prototype.reset = function() {
  this.stop();
  this.start_time = 0;
  this.stop_time = 0;
  this.setup();
  this.print();
}

Sort.prototype.start = function() {
  if (this.start_time > 0) {
    if (this.stop_time > 0) {
      this.shuffle();
      this.start_time = 0;
      this.stop_time = 0;
      this.status("");
      return;
    } else if (manual) {
      this.stepper();
      return;
    } else {
      this.finished();
      return;
    }
  }
  if (!manual) {
    var t = this;
    this.timer = setTimeout(function(){t.stepper();},interval_time);
  }
  this.compares = 0;
  this.swaps = 0;
  this.start_time = (new Date()).getTime();
  this.last_time = this.start_time;
  this.num_jobs = 0;
  this.stop_time = 0;
  this.overhead_total = 0;
  this.overhead_min = 1000000;
  this.overhead_max = 0;
  this.processing_total = 0;
  this.processing_min = 1000000;
  this.processing_max = 0;
  this.num_iterations = 0;
  this.func(this);
}

Sort.prototype.cleanup = function() {
  if (this.compare_x) {
    this.compare_x.style.borderColor = "black";
    this.compare_y.style.borderColor = "black";
  }
  if (this.swap_x) {
    this.swap_x.style.backgroundColor = "green";
    this.swap_y.style.backgroundColor = "green";
  }
  this.work_queue = new Array();
}

Sort.prototype.stop = function() {
  if (this.timer != 0) {
    clearTimeout(this.timer);
    this.timer = 0;
  }
  this.cleanup();
}

Sort.prototype.finished = function(err) {
  this.stop();

  this.stop_time = (new Date()).getTime();

  var total = (this.stop_time - this.start_time);
  if (err == null) {
    var step_avg = Math.floor(total / this.num_iterations);
    var overhead = total - this.processing_total;
    var overhead_avg = Math.floor(overhead / this.num_iterations);
    var processing_avg = Math.floor(this.processing_total / this.num_iterations);
    var table = "<table><tr><td>Times(ms)</td><td>Total</td><td>Avg</td><td>Max</td></tr>"
      + "<tr><td>Total</td><td>" + total + "</td><td>" + step_avg + "</td><td>" + this.step_max + "</tr>"
      + "<tr><td>Work</td><td>" + this.processing_total + "</td><td>" + processing_avg + "</td><td>" +  this.processing_max + "</tr>"
      + "<tr><td>Overhead</td><td>" + overhead + "</td><td>" + overhead_avg + "</td><td>" + this.overhead_max + "</tr>"
      + "</table>";
    this.status(table);
  } else {
    this.status(err);
    log("error: " + err);
  }
  log("finished in: " + total);
}

Sort.prototype.shuffle = function() {
  for (i = 0; i < this.size; i++) {
    var r = Math.floor(Math.random() * this.size);
    if (i != r) {
      this.swap(i,r);
    }
  }
  this.cleanup();
}

Sort.prototype.print = function() {
  var graph = document.getElementById(this.name);
  if (graph == undefined) {
    alert("can't find " + this.name);
  }
  var text = "<div id='" + this.name + "_label' class='label'>" + this.name + " Sort</div>";
  var len = this.numbers.length;
  var height_multiple = (graph.clientHeight-20) / len;
  var width = Math.max(1,Math.floor((graph.clientWidth-10) / len));
  if (width < 3) {
    border = 0;
  } else {
    border = 1;
  }
  var left_offset = Math.round((graph.clientWidth - (width*len))/2);
  for (i = 0; i < len; i++) {
    var val = this.numbers[i];
    var height = Math.max(1, Math.floor(val * height_multiple));
    var left = left_offset + i * width;
    text += "<li class='bar' style='border: " + border + "px solid black; height:" + height + "px; left:" + left + "; width:" + width + "' id='" + this.name + val + "' value='" + val + "'></li>";
  }
  graph.innerHTML = text;
  var nodes = document.getElementsByTagName("li");
  var j = 0;
  for (i = 0; i < nodes.length; i++) {
    var name = nodes[i].id;
    if (name.indexOf(this.name) == 0) {
      this.bars[j] = nodes[i];
      j++;
    }
  }
}

Sort.prototype.compare = function(x, y) {
  var bx = this.bars[x];
  var by = this.bars[y];
  //log("compare " + x + "(" + bx.value + ")," + y + "(" + by.value + ")");
  if (this.compare_x != bx) {
    if (this.compare_x) {
      this.compare_x.style.borderColor="black";
    }
    bx.style.borderColor="yellow";
    this.compare_x = bx;
  }
  if (this.compare_y != by) {
    if (this.compare_y) {
      this.compare_y.style.borderColor="black";
    }
    by.style.borderColor="white";
    this.compare_y = by;
  }
  this.compares++;
  return bx.value - by.value;
}

Sort.prototype.swap = function(x, y) {
  var bx = this.bars[x];
  var by = this.bars[y];
  //log("swap " + x + "(" + bx.value + ")," + y + "(" + by.value + ")");
  if (this.swap_x != x) {
    if (this.swap_x) {
      this.swap_x.style.backgroundColor="green";
    }
    bx.style.backgroundColor="blue";
    this.swap_x = bx;
  }
  if (this.swap_y != y) {
    if (this.swap_y) {
      this.swap_y.style.backgroundColor="green";
    }
    by.style.backgroundColor="red";
    this.swap_y = by;
  }
  var tmp = bx.style.left;
  bx.style.left = by.style.left;
  by.style.left = tmp;
  this.bars[x] = by;
  this.bars[y] = bx;
  this.swaps++;
}

Sort.prototype.insert = function(from, to) {
  var bf = this.bars[from];
  if (from > to) {
    for (i = from; i > to; i--) {
      var b1 = this.bars[i];
      var b2 = this.bars[i-1];
      b2.style.left = b1.style.left;
      this.bars[i] = b2;
    }
    bf.style.left = this.bars[to].style.left;
    this.bars[to] = bf;
  } else {
    // TODO
  } 
}
