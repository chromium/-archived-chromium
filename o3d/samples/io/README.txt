Steps to Create a level:

1. Install the SketchUp Level Creation tools
2. Start SU 6
3. Draw your "world path" using the line tool

- Using the line tool, draw a line from the drawing origin along the solid green axis line (that's your x axis) It can be as long or as short as you want.
- From there, you can draw lines continuously from the end of your first line segment. Each line is a distinct plane in the world path.
- Only draw horizontal lines along the Sketchup drawing's ground plane. No slopes or vertical changes in altitude.
- Your avatar will NOT walk on these lines. They are only used to define the path that the world will run along.

4. Draw your platforms
- Each horizontal line must be exactly parallel and exactly "above" or "below" one of your "world path" line segments.
- Good way to do this is to CTRL+MOVE your base lines up or down, and then chop them up from there.
- Each horizontal line is a platform that your avatar can walk on.
- Each vertical line is an "obstacle" that your avatar can't walk through.

5. Drag in your actors
- The installer created a directory under components > Prince IO (or it will, once we're done with it. -SEL)
- Drag these suckers in. Note that you're going to want to put them on line segments, typically.

6. Draw random geometry only in components and groups.

7. Test everything in SU.
- Press the "play" button in the Prince IO level designer. You can play the game.
- Note: Set your field of view to 45 degrees to have approximately the same camera view as in O3D.

8. Press the "export" button and save your myLevelName.js and myLevelName.kmz into your prince IO /levels directory.
(this will also automatically generate the autoincludes.js file.)
