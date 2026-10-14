# Heading arrow

`arrow.png` is a 96 × 96 white arrow on a transparent background, pointing up
(+Y on the map). It was drawn for this project with Pillow, so no third-party
license applies.

CMake copies the PNG beside the executable. The map loads it once and tints it.
It is rotated to show the robot's heading and the heading of a pending pose
reset.
