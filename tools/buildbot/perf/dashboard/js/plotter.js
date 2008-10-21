// plotting select reports against time

// vertical marker for columns
function Marker(color) {
  var m = document.createElement("DIV");
  m.setAttribute("class", "plot-cursor");
  m.style.backgroundColor = color;
  m.style.opacity = "0.3";
  m.style.position = "absolute";
  m.style.left = "-2px";
  m.style.top = "-2px";
  m.style.width = "0px";
  m.style.height = "0px";
  return m;
}

/**
 * Create a horizontal marker at the indicated mouse location.
 * @constructor
 *
 * @param canvasRect {Object} The canvas bounds (in client coords).
 * @param mousePoint {MousePoint} The mouse click location that spawned the
 *     marker.
 */
function HorizontalMarker(canvasRect, mousePoint) {
  // Add a horizontal line to the graph.
  var m = document.createElement("DIV");
  m.setAttribute("class", "plot-baseline");
  m.style.backgroundColor = HorizontalMarker.COLOR;
  m.style.opacity = "0.3";
  m.style.position = "absolute";
  m.style.left = canvasRect.x;
  var h = HorizontalMarker.HEIGHT;
  m.style.top = (mousePoint.clientPoint.y - h/2).toFixed(0) + "px";
  m.style.width = canvasRect.width + "px";
  m.style.height = h + "px";
  this.markerDiv_ = m;

  this.value = mousePoint.plotterPoint.y;
}

HorizontalMarker.HEIGHT = 5;
HorizontalMarker.COLOR = "rgb(0,100,100)";

// Remove the horizontal line from the graph.
HorizontalMarker.prototype.remove_ = function() {
  this.markerDiv_.parentNode.removeChild(this.markerDiv_);
}

function Plotter(xVals, yVals, yDevs, resultNode, zoomed) {
  this.xVals_ = xVals;
  this.yVals_ = yVals;
  this.yDevs_ = yDevs;
  this.resultNode_ = resultNode;
  this.zoomed_ = zoomed;
  this.extraData_ = [];
  this.units = "msec";
}

Plotter.prototype.getArrayMinMax_ = function(ary) {
  var aryMin = ary[0];
  var aryMax = ary[0];
  for (i = 1; i < ary.length; ++i) {
    if (ary[i] < aryMin) {
      aryMin = ary[i];
    } else if (ary[i] > aryMax) {
      aryMax = ary[i];
    }
  }
  return [aryMin, aryMax];
}

Plotter.prototype.calcGlobalExtents_ = function() {
  var e = {};

  var i;

  // yVals[i] + yDevs[i]
  var ymax = [];
  for (i = 0; i < this.yVals_.length; ++i) {
    ymax.push(this.yVals_[i] + this.yDevs_[i]);
  }
  var rmax = this.getArrayMinMax_(ymax);
  e.max = rmax[1];

  // yVals[i] - yDevs[i]
  var ymin = [];
  for (i = 0; i < this.yVals_.length; ++i) {
    ymin.push(this.yVals_[i] - this.yDevs_[i]);
  }
  var rmin = this.getArrayMinMax_(ymin);
  e.min = rmin[0];

  // check extra data for extents that exceed
  for (i in this.extraData_) {
    var r = this.getArrayMinMax_(this.extraData_[i].yvals);
    if (r[0] < e.min)
      e.min = r[0];
    if (r[1] > e.max)
      e.max = r[1];
  }

  return e;
}

Plotter.prototype.addAlternateData = function(yvals, color) {
  // add an extra trace to the plot; does not affect graph extents
  this.extraData_.push({"yvals": yvals, "color": color});
}

Plotter.prototype.plot = function() {
  var e = {};

  height = window.innerHeight - 16;
  width = window.innerWidth - 16;

  e.heightMax = Math.min(400, height - 80);
  e.widthMax = width;

  var canvas = document.createElement("CANVAS");
  canvas.setAttribute("class", "plot");
  canvas.setAttribute("width", e.widthMax);
  canvas.setAttribute("height", e.heightMax);

  // Display coordinates on the left, deltas from baseline on the right.
  var hoverStatsDiv = document.createElement("DIV");
  hoverStatsDiv.innerHTML =
      "<table border=0 width='100%'><tbody><tr>" +
      "<td class='plot-coordinates'><i>move mouse over graph</i></td>" +
      "<td align=right style='color: " + HorizontalMarker.COLOR +
      "'><i>Shift-click to place baseline</i></td>" +
      "</tr></tbody></table>";

  var tr = hoverStatsDiv.firstChild.firstChild.firstChild;
  this.coordinates_td_ = tr.childNodes[0];
  this.baseline_deltas_td_ = tr.childNodes[1];

  // horizontal ruler
  var ruler = document.createElement("DIV");
  ruler.setAttribute("class", "plot-ruler");
  ruler.style.borderBottom = "1px dotted black";
  ruler.style.position = "absolute";
  ruler.style.left = "-2px";
  ruler.style.top = "-2px";
  ruler.style.width = "0px";
  ruler.style.height = "0px";
  this.ruler_div_ = ruler;

  // marker for the result-point that the mouse is currently over
  this.cursor_div_ = new Marker("rgb(100,80,240)");

  // marker for the result-point for which details are shown
  this.marker_div_ = new Marker("rgb(100,100,100)");

  // find global extents

  e.xMin = -0.5;
  e.xMax = this.xVals_.length - 0.5;

  var yExt = this.calcGlobalExtents_();
  var yd = (yExt.max - yExt.min) / 10.0;
  //e.yMin = this.zoomed_ ? (yExt.min - yd) : 0;
  //e.yMax = yExt.max + yd;
  e.yMin = yExt.min - yd;
  e.yMax = yExt.max + yd;
  this.extents_ = e;

  var ctx = canvas.getContext("2d");

  var ss = {
    "yvals": "rgb(60,0,240)",
    "ydevs": "rgba(100,100,100,0.6)"
  };
  this.plot1_(ctx, this.yVals_, this.yDevs_, e, ss);

  for (var i = 0; i < this.extraData_.length; ++i) {
    ss = {
      "yvals": this.extraData_[i].color
    };
    this.plot1_(ctx, this.extraData_[i].yvals, null, e, ss);
  }

  //this.resultNode_.appendChild(legend);
  this.resultNode_.appendChild(canvas);
  this.resultNode_.appendChild(hoverStatsDiv);
  this.resultNode_.appendChild(ruler);
  this.resultNode_.appendChild(this.cursor_div_);
  this.resultNode_.appendChild(this.marker_div_);

  // hook up event listener
  var self = this;
  canvas.parentNode.addEventListener(
      "mousemove", function(evt) { self.onMouseMove_(evt); }, false);
  this.cursor_div_.addEventListener(
      "click", function(evt) { self.onMouseClick_(evt); }, false);

  this.canvas_ = canvas;
}

Plotter.prototype.getIndex_ = function(x, e) {
  // map x coord to xVals_ element

  var index;
  if (x < 0) {
    index = 0;
  } else if (x > this.xVals_.length - 1) {
    index = this.xVals_.length - 1;
  } else {
    index = x.toFixed(0);
  }
  return index;
}

function MousePoint(clientPoint, canvasPoint, plotterPoint) {
  this.clientPoint = clientPoint;
  this.canvasPoint = canvasPoint;
  this.plotterPoint = plotterPoint;
}

Plotter.prototype.getCanvasRect_ = function() {
  return {
    "x": this.canvas_.offsetLeft,
    "y": this.canvas_.offsetTop,
    "width": this.canvas_.offsetWidth,
    "height": this.canvas_.offsetHeight
  };
}

/**
 * Map the mouse position into various coordinate spaces.
 *
 * @param evt {MouseEvent} Either a mouse click or mouse move event.
 *
 * @return {MousePoint} |x| such that:
 *   |x.clientPoint| is the mouse point in the  client window coordinate space.
 *   |x.canvasPoint| is the mouse point in the canvas's coordinate space.
 *   |x.plotterPoint| is the mouse point in the plotter's coordinate space.
 */
Plotter.prototype.translateMouseEvent_ = function(evt) {
  var canvasRect = this.getCanvasRect_();

  var cx = evt.clientX - canvasRect.x;
  var cy = evt.clientY - canvasRect.y;

  var e = this.extents_;

  var x = cx / e.widthMax * (e.xMax - e.xMin) + e.xMin;
  var y = (e.heightMax - cy) / e.heightMax * (e.yMax - e.yMin + 1) + e.yMin;

  return new MousePoint(
      {x: evt.clientX, y: evt.clientY}, // clientPoint
      {x: cx, y:cy}, // canvasPoint
      {x: x, y: y}); // plotterPoint
}

Plotter.prototype.onMouseMove_ = function(evt) {
  var canvasRect = this.getCanvasRect_();

  var mousePoint = this.translateMouseEvent_(evt);
  var x = mousePoint.plotterPoint.x;
  var y = mousePoint.plotterPoint.y;

  var index = this.getIndex_(x);
  this.current_index_ = index;

  this.coordinates_td_.innerHTML =
      "CL #" + this.xVals_[index] + ": " +
               this.yVals_[index].toFixed(2) + " \u00B1 " +
               this.yDevs_[index].toFixed(2) + " " + this.units + ": " +
               y.toFixed(2) + " " + this.units;

  // If there is a horizontal marker, display deltas relative to it.
  if (this.horizontalMarker_) {
    var baseline = this.horizontalMarker_.value;
    var delta = y - baseline
    var fraction = delta / baseline; // allow division by 0

    var deltaStr = (delta >= 0 ? "+" : "") + delta.toFixed(0) + " " + this.units;
    var percentStr = (fraction >= 0 ? "+" : "") +
        (fraction * 100).toFixed(3) + "%";

    this.baseline_deltas_td_.innerHTML = deltaStr + ": " + percentStr;
  }

  // update ruler
  var r = this.ruler_div_;
  r.style.left = canvasRect.x + "px";
  r.style.top = canvasRect.y + "px";
  r.style.width = canvasRect.width + "px";
  var h = evt.clientY - canvasRect.y;
  if (h > canvasRect.height)
    h = canvasRect.height;
  r.style.height = h + "px";

  // update cursor
  var c = this.cursor_div_;
  c.style.top = canvasRect.y + "px";
  c.style.height = canvasRect.height + "px";
  var w = canvasRect.width / this.xVals_.length;
  var x = (canvasRect.x + w * index).toFixed(0);
  c.style.left = x + "px";
  c.style.width = w + "px";

  if ("onmouseover" in this)
    this.onmouseover(this.xVals_[index], this.yVals_[index], this.yDevs_[index], e);
}

/**
 * Left-click in the plotter selects the column (changelist).
 * Shift-left-click in the plotter lays down a horizontal marker.
 * This horizontal marker is used as the baseline for ruler metrics.
 *
 * @param evt {DOMEvent} The click event in the plotter.
 */
Plotter.prototype.onMouseClick_ = function(evt) {

  if (evt.shiftKey) {
    if (this.horizontalMarker_) {
      this.horizontalMarker_.remove_();
    }

    this.horizontalMarker_ = new HorizontalMarker(
        this.getCanvasRect_(), this.translateMouseEvent_(evt));

    // Insert before cursor node, otherwise it catches clicks.
    this.cursor_div_.parentNode.insertBefore(
        this.horizontalMarker_.markerDiv_, this.cursor_div_);
  } else {
    var index = this.current_index_;

    var m = this.marker_div_;
    var c = this.cursor_div_;
    m.style.top = c.style.top;
    m.style.left = c.style.left;
    m.style.width = c.style.width;
    m.style.height = c.style.height;

    if ("onclick" in this) {
      var this_x = this.xVals_[index];
      var prev_x = index > 0 ? (this.xVals_[index-1] + 1) : this_x;
      this.onclick(prev_x, this_x, this.yVals_[index], this.yDevs_[index],
                   this.extents_);
    }
  }
}

Plotter.prototype.plot1_ =
function(ctx, yVals, yDevs, extents, strokeStyles) {
  var e = extents;

  function mapY(y) {
    var r = e.heightMax - e.heightMax * (y - e.yMin) / (e.yMax - e.yMin);
    //document.getElementById('log').appendChild(document.createTextNode(r + '\n'));
    return r;
  }

  ctx.strokeStyle = strokeStyles.yvals;
  ctx.lineWidth = 2;
  ctx.beginPath();

  // draw main line
  var initial = true;
  for (i = 0; i < yVals.length; ++i) {
    if (yVals[i] == 0.0)
      continue;
    var x = e.widthMax * (i - e.xMin) / (e.xMax - e.xMin);
    var y = mapY(yVals[i]);
    if (initial) {
      initial = false;
    } else {
      ctx.lineTo(x, y);
    }
    ctx.moveTo(x, y);
  }

  ctx.closePath();
  ctx.stroke();

  // deviation bars
  if (yDevs) {
    ctx.strokeStyle = strokeStyles.ydevs;
    ctx.lineWidth = 1.0;
    ctx.beginPath();
      
    for (i = 0; i < yVals.length; ++i) {
      var x = e.widthMax * (i - e.xMin) / (e.xMax - e.xMin);
      var y = mapY(yVals[i]);

      var y2 = mapY(yVals[i] + yDevs[i]);
      ctx.moveTo(x, y);
      ctx.lineTo(x, y2);

      var m = 2;
      ctx.moveTo(x, y2);
      ctx.lineTo(x - m, y2);
      ctx.moveTo(x, y2);
      ctx.lineTo(x + m, y2);

      var y2 = mapY(yVals[i] - yDevs[i]);
      ctx.moveTo(x, y);
      ctx.lineTo(x, y2);

      ctx.moveTo(x, y2);
      ctx.lineTo(x - m, y2);
      ctx.moveTo(x, y2);
      ctx.lineTo(x + m, y2);

      // draw a marker 
      var d = 3;
      ctx.moveTo(x, y);
      ctx.lineTo(x + d, y + d);
      ctx.moveTo(x, y);
      ctx.lineTo(x - d, y + d);
      ctx.moveTo(x, y);
      ctx.lineTo(x - d, y - d);
      ctx.moveTo(x, y);
      ctx.lineTo(x + d, y - d);

      ctx.moveTo(x, y);
    }

    ctx.closePath();
    ctx.stroke();
  }
}
