// This file comes from Box2D-JS, Copyright (c) 2008 ANDO Yasushi.
// The original version is available at http://box2d-js.sourceforge.net/ under the
// zlib/libpng license (see License.txt).
// This version has been modified to make it work with O3D.

function createWorld() {
  var worldAABB = new b2AABB();
  worldAABB.minVertex.Set(-1000, -1000);
  worldAABB.maxVertex.Set(1000, 1000);
  var gravity = new b2Vec2(0, 300);
  var doSleep = true;
  var world = new b2World(worldAABB, gravity, doSleep);
  createGround(world);
  createBox(world, 0, 125, 10, 250);
  createBox(world, 500, 125, 10, 250);
  return world;
}

function createGround(world) {
  var groundSd = new b2BoxDef();
  groundSd.extents.Set(250, 50);
  groundSd.restitution = 0.2;
  var groundBd = new b2BodyDef();
  groundBd.AddShape(groundSd);
  groundBd.position.Set(250, 340);
  // NOTE: Added the following line to create a 3d object to display.
  groundBd.userData = g.mgr.createBox(250, 50);
  return world.CreateBody(groundBd)
}

function createBall(world, x, y) {
  var ballSd = new b2CircleDef();
  ballSd.density = 1.0;
  ballSd.radius = 20;
  ballSd.restitution = 1.0;
  ballSd.friction = 0;
  var ballBd = new b2BodyDef();
  ballBd.AddShape(ballSd);
  ballBd.position.Set(x,y);
  // NOTE: Added the following line to create a 3d object to display.
  ballBd.userData = g.mgr.createCylinder(ballSd.radius);
  return world.CreateBody(ballBd);
}

function createBox(world, x, y, width, height, fixed) {
  if (typeof(fixed) == 'undefined') fixed = true;
  var boxSd = new b2BoxDef();
  if (!fixed) boxSd.density = 1.0;
  boxSd.extents.Set(width, height);
  var boxBd = new b2BodyDef();
  // NOTE: Added the following line to create a 3d object to display.
  boxBd.userData = g.mgr.createBox(width, height);
  boxBd.AddShape(boxSd);
  boxBd.position.Set(x,y);
  return world.CreateBody(boxBd)
}

var demos = {};
demos.InitWorlds = [];
