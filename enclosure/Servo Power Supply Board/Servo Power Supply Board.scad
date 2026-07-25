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


// // // //
// MAIN
// // // //

difference() {
    union() {
        difference() {
            union() {
                difference() {
                    translate([0, 0, 9 / 2 + 1.5])
                        cuboid([38.4 + 1 + 3, 22.4 + 1 + 3, 9], rounding = 3, except = [TOP,BOTTOM]);

                    // interior cut
                    translate([0, 0, 9 / 2])
                        cuboid([38.4 + 1, 22.4 + 1, 9], rounding = 3, except = [TOP,BOTTOM]);
                }

                // screw posts
                translate([-4, 0, 0]) {
                    translate([0, 0, 9 / 2 + 1.5]) {
                        translate([0, -16 / 2, 0])
                            cylinder(d = 7, h = 9, center = true);
                        translate([0, 16 / 2, 0])
                            cylinder(d = 7, h = 9, center = true);
                    }
                }
            }
        }

        union() {
            difference() {
                translate([0, 0, -6.5 / 2 + 1.5])
                    cuboid([38.4 + 1 + 3, 22.4 + 1 + 3, 6.5], rounding = 3, except = [TOP,BOTTOM]);

                // interior cut
                translate([0, 0, 0])
                    cuboid([38.4 + 1, 22.4 + 1, 5 + 3], rounding = 3, except = [TOP,BOTTOM]);

                // heat sink cut
                #translate([5.8, -5, -5.5 / 2])
                    cuboid([10, 10, 5.5], rounding = 0.75, except = [TOP,BOTTOM]);

            }

            // screw posts
            translate([-4, 0, 0]) {
                translate([0, 0, -6.5 / 2 + 1.5]) {
                    translate([0, -16 / 2, 0])
                        cylinder(d = 4.5, h = 6.5, center = true);
                    translate([0, 16 / 2, 0])
                        cylinder(d = 4.5, h = 6.5, center = true);
                }

                translate([0, 0, -5 / 2]) {
                    translate([0, -16 / 2, 0])
                        cylinder(d = 8, h = 5, center = true);
                    translate([0, 16 / 2, 0])
                        cylinder(d = 8, h = 5, center = true);
                }
                #translate([-13, 0, -5 / 2]) {
                    translate([0, -16 / 2, 0])
                        cylinder(d = 8, h = 5, center = true);
                    translate([0, 16 / 2, 0])
                        cylinder(d = 8, h = 5, center = true);
                }
                #translate([21, 0, -5 / 2]) {
                    translate([0, -16 / 2, 0])
                        cylinder(d = 8, h = 5, center = true);
                    *translate([0, 16 / 2, 0])
                        cylinder(d = 8, h = 5, center = true);
                }
            }
        }
    }

    // power LED cut
    #translate([-10.2, 9.8, 50 / 2])
        cylinder(d = 2, h = 50, center = true);

    // 2.54mm header cut
    #translate([9.8, 6.8, 50 / 2]) {
        translate([-1.5, 5 / 2, 0])
            cuboid([16 + 0.5, 5 + 0.5, 50], rounding = 0.75, except = [TOP,BOTTOM]);
        translate([0, -5 / 2, 0])
            cuboid([19 + 0.5, 5 + 0.5, 50], rounding = 0.75, except = [TOP,BOTTOM]);
        #translate([-8.75, 0, 0])
            cube([2, 5, 50], center = true);
    }

    // PH2.0 connector cut
    #translate([-19.2, 5.3, (6 + 0.25) / 2 + 0.75])
        cuboid([15, 7 + 0.5, 6 + 0.25], rounding = 0.75, except = [LEFT,RIGHT]);

    // 2.1mm barrel connector cut
    #translate([-15 / 2 - 19.7, -5.2, 5.5])
        rotate([0, 90, 0])
            cylinder(d = 7, h = 15, center = true);

    // screw hole cut
    #translate([-4, 0, 0]) {
        translate([0, -16 / 2, 0]) {
            cylinder(d = 3.6, h = 50, center = true);
            rotate([0, 0, 0])
                %screw("M3", head="socket", length = 18, anchor = "head_bot");
            translate([0, 0, -10 / 2 + 2.5])
                rotate([180, 0, 0])
                    nut_trap_inline(10, "M3", $slop = 0.1);
        }
        translate([0, 16 / 2, 0]) {
            cylinder(d = 3.6, h = 50, center = true);
            rotate([0, 0, 0])
                %screw("M3", head="socket", length = 18, anchor = "head_bot");
            translate([0, 0, -10 / 2 + 2.5])
                rotate([180, 0, 0])
                    nut_trap_inline(10, "M3", $slop = 0.1);
        }
    }
}
