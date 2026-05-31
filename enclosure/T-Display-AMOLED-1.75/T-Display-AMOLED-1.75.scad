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

module H741_1() {
    translate([-125.3, -114.2, 0])
        rotate([0, 0, 0])
            import("assets/H741-1.stl");

    // reference center        
    *#cylinder(h = 50, d = 1, center = true);
}

module shell() {
    translate([0, 0, 0])
        rotate([-90, 0, 0])
            %import("assets/Transparent Shell.stl");

    translate([0, 0, -11.8]) {
        difference() {
            union() {
                translate([-14.85, 10.52, 0])
                    cylinder(d = 5, h = 10, center = true);
                translate([16.02, -8.57, 0])
                    cylinder(d = 5, h = 10, center = true);
            }

            translate([0, 0, 0]) {
                translate([-14.85, 10.52, 0])
                    cylinder(d = 2, h = 50, center = true);
                translate([16.02, -8.57, 0])
                    cylinder(d = 2, h = 50, center = true);
            }
        }
    }

    translate([0, 0, -5]) {
        difference() {
            union() {
                translate([0, 0, 0])
                    cylinder(d = 50, h = 10, center = true);
            }

            translate([0, 0, 0]) {
                cylinder(d = 47, h = 12, center = true);
            }

            translate([0, 0, 4.6]) {
                intersection() {
                    cube([16.5, 50, 3], center = true);

                    cylinder(d = 48.5, h = 3, center = true);
                }
            }

            // USB port
            translate([50 / 2, 0, -1.75])
                cuboid([30, 10, 4], rounding = 2);

            // micro SD
            translate([1.8, 50 / 2, 0.5])
                cuboid([13, 30, 3], rounding = 1);
        }
    }
    
    // reference center        
    *#cylinder(h = 50, d = 1, center = true);
}


// https://www.thingiverse.com/thing:6769037/files
module torus(d = 10, thickness = 4, res = 20) {
    rotate_extrude($fn = res) {
        translate([d, 0]) circle(thickness, $fn = res);
    }
}


// // // //
// MAIN
// // // //

translate([0, 0, 0])
    shell();
translate([70, 0, 0])
    shell();