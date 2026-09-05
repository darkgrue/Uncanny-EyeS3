// // // //
// INCLUDES
// // // //

// https://github.com/BelfrySCAD/BOSL2
include <BOSL2/std.scad>
include <BOSL2/screws.scad>
include <BOSL2/threading.scad>


// // // //
// PARAMETERS
// // // //

// Minimum angle for a fragment. The default value is 12 (i.e. 30 fragments
// for a full circle). The minimum allowed value is 0.01.
$fa = $preview ? 5 : 1;
// Minimum size of a fragment. The default value is 2 so very small circles
// have a smaller number of fragments than specified using $fa. The minimum
// allowed value is 0.01.
$fs = $preview ? 1 : 0.1;
// Number of fragments in a full circle. When this variable has a value
// greater than zero, the other two variables are ignored, and a full circle
// is rendered using this number of fragments.
$fn = 0;


// // // //
// FUNCTIONS
// // // //

// Two formulas have been identified for modifying the dimensions of
// hole diameters. To use these formulas, the x value is your desired
// diameter (for example 4 mm) and the y value is the adjusted diameter
// for your CAD model (in this case 4.34 mm). Use the vertical hole
// formula if the axis of the hole is parallel with the Z axis of the
// build plate and the horizontal hole formula if the axis of the hole
// is parallel with the X or Y axes. Both of these formulas are in
// millimeters.
//   y = 1.0155x + 0.2795 vertical
//   y = 0.9927x + 0.3602 horizontal
function dah(diameter, vertical = true) = vertical == true ? (1.0155 * diameter) + 0.2795 : (0.9927 * diameter) + 0.3602;


// // // //
// MODULES
// // // //

module fillet(r, h) {
    translate([r / 2, r / 2, 0]) {
        difference() {
            cube([r + 0.01, r + 0.01, h], center = true);

            translate([r / 2, r / 2, 0])
                cylinder(r = r, h = h + 1, center = true);

        }
    }
}

module hsi(d, h) {
    union() {
        translate([0, 0, -(1.8 * h + 1) / 2 + 1])
            cylinder(d = dah(d), h = 1.8 * h + 1, center = true);

        cylinder(d1 = dah(d), d2 = dah(d) + 2, h = 2, center = true);
    }
}

// https://www.thingiverse.com/thing:6769037/files
module torus(d = 10, thickness = 4, res = 20) {
    rotate_extrude($fn = res) {
        translate([d, 0]) circle(thickness, $fn = res);
    }
}

module Adafruit_4836() {
    translate([0, 0, 0])
        rotate([0, 0, 0])
            import("assets/4836-Wii-Nunchuck-Adapter.stl");

    // reference center        
    *#cylinder(h = 50, d = 1, center = true);
}


// // // //
// MAIN
// // // //

difference() {
    %translate([0, 0, 0])
        Adafruit_4836();

    union() {
        difference() {
            union() {
                difference() {
                    translate([0, -3.5, 5 / 2 + 2])
                        cuboid([25 + 4, 22, 7], rounding = 3, except = [TOP,BOTTOM]);

                    // interior cut
                    translate([0, -3.65, 7 / 2 - 0.25])
                        cuboid([25.5 + 1, 18 + 1, 7], rounding = 3, except = [TOP,BOTTOM]);
                }

                // screw posts
                translate([0, -3.685, 0]) {
                    translate([0, 0, 6 / 2 + 2])
                        grid_copies(spacing = [20.32, 12.7], n = [2, 2])
                            cylinder(d = 4, h = 6, center = true);
                }
            }

            // top screw hole cut
            translate([0, -3.685, 0])
                grid_copies(spacing = [20.32, 12.7], n = [2, 2  ])
                    cylinder(d = 2.9, h = 100, center = true);
        }

        union() {
            difference() {
                translate([0, -3.5, -5 / 2 - 0.5])
                    cuboid([25 + 4, 22, 5 + 3], rounding = 3, except = [TOP,BOTTOM]);

                // interior cut
                translate([0, -3.65, -1])
                    cuboid([25.5 + 1, 18 + 1, 5 + 3], rounding = 3, except = [TOP,BOTTOM]);
            }

            // screw posts
            translate([0, -3.685, 0]) {
                translate([0, 0, -6 / 2])
                    grid_copies(spacing = [20.32, 12.7], n = [2, 2])
                        cylinder(d = 4, h = 6, center = true);
            }
        }
    }

    // 2.54mm header cut
    #translate([0, -10, 50 / 2])
        cuboid([14, 3.5, 50], rounding = 0.75, except = [TOP,BOTTOM]);

    // Wii Nunchuck connector cut
    #translate([0, -3.685 + 17, 2 / 2])
        cuboid([17.5, 18, 11.5], rounding = 0.75, except = [FRONT,BACK]);

    // QWIIC connector cut
    #translate([0, -3.685, 3 / 2 + 2.25])
        cuboid([50, 7 + 0.2, 4 + 0.2], rounding = 0.75, except = [LEFT,RIGHT]);

    // screw hole cut
    #translate([0, -3.685, 0])
        grid_copies(spacing = [20.32, 12.7], n = [2, 2])
            cylinder(d = 2.5, h = 100, center = true);
}
