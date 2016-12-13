// This file comes from Box2D-JS, Copyright (c) 2008 ANDO Yasushi.
// The original version is available at http://box2d-js.sourceforge.net/ under the
// zlib/libpng license (see License.txt).
// This version has been modified to make it work with O3D.

demos.top = {};
demos.top.createBall = function(world, x, y, rad, fixed) {
  var ballSd = new b2CircleDef();
  if (!fixed) ballSd.density = 1.0;
  ballSd.radius = rad || 10;
  ballSd.restitution = 0.2;
  var ballBd = new b2BodyDef();
  ballBd.AddShape(ballSd);
  ballBd.position.Set(x,y);
  // NOTE: Added the following line to create a 3d object to display.
  ballBd.userData = g.mgr.createCylinder(ballSd.radius);
  return world.CreateBody(ballBd);
};
demos.top.createPoly = function(world, x, y, points, fixed) {
  var polySd = new b2PolyDef();
  if (!fixed) polySd.density = 1.0;
  polySd.vertexCount = points.length;
  for (var i = 0; i < points.length; i++) {
    polySd.vertices[i].Set(points[i][0], points[i][1]);
  }
  var polyBd = new b2BodyDef();
  if (points.length == 3) {
    // NOTE: Added the following line to create a 3d object to display.
    polyBd.userData = g.mgr.createWedge(points);
  }
  polyBd.AddShape(polySd);
  polyBd.position.Set(x,y);
  return world.CreateBody(polyBd)
};
demos.top.initWorld = function(world) {
  demos.top.createBall(world, 350, 100, 50, true);
  demos.top.createPoly(world, 100, 100, [[0, 0], [10, 30], [-10, 30]], true);
  demos.top.createPoly(world, 150, 150, [[0, 0], [10, 30], [-10, 30]], true);
  var pendulum = createBox(world, 150, 100, 20, 20, false);
  var jointDef = new b2RevoluteJointDef();
  jointDef.body1 = pendulum;
  jointDef.body2 = world.GetGroundBody();
  jointDef.anchorPoint = pendulum.GetCenterPosition();
  world.CreateJoint(jointDef);

  var seesaw = demos.top.createPoly(world, 300, 200, [[0, 0], [100, 30], [-100, 30]]);
  jointDef.body1 = seesaw;
  jointDef.anchorPoint = seesaw.GetCenterPosition();
  world.CreateJoint(jointDef);
};
demos.InitWorlds.push(demos.top.initWorld);


