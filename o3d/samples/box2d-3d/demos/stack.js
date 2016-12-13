// This file comes from Box2D-JS, Copyright (c) 2008 ANDO Yasushi.
// The original version is available at http://box2d-js.sourceforge.net/ under the
// zlib/libpng license (see License.txt).
// This version has been modified to make it work with O3D.

demos.stack = {};
demos.stack.initWorld = function(world) {
  var sd = new b2BoxDef();
  var bd = new b2BodyDef();
  bd.AddShape(sd);
  sd.density = 1.0;
  sd.friction = 0.5;
  sd.extents.Set(10, 10);

  var i;
  for (i = 0; i < 8; i++) {
    bd.position.Set(500/2-Math.random()*2-1, (250-5-i*22));
    // NOTE: Added the following line to create a 3d object to display.
    bd.userData = g.mgr.createBox(10, 10);
    world.CreateBody(bd);
  }
  for (i = 0; i < 8; i++) {
    bd.position.Set(500/2-100-Math.random()*5+i, (250-5-i*22));
    // NOTE: Added the following line to create a 3d object to display.
    bd.userData = g.mgr.createBox(10, 10);
    world.CreateBody(bd);
  }
  for (i = 0; i < 8; i++) {
    bd.position.Set(500/2+100+Math.random()*5-i, (250-5-i*22));
    // NOTE: Added the following line to create a 3d object to display.
    bd.userData = g.mgr.createBox(10, 10);
    world.CreateBody(bd);
  }
}
demos.InitWorlds.push(demos.stack.initWorld);


