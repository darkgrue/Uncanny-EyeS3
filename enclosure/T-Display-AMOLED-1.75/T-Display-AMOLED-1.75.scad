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

module Adafruit_4836() {
    translate([0, 0, 0])
        rotate([0, 0, 0])
            import("assets/4836-Wii-Nunchuck-Adapter.stl");

    // reference center        
    *#cylinder(h = 50, d = 1, center = true);
}

module B0C2YN8F7Z() {
    translate([0, 0, 0]) {
        rotate([0, 0, 90]) {
            width  = 21;
            length = 103;
            h_edge = 8.75;
            h_ctr  = 9.75;

            // Total thickness variance is 1mm, split evenly between top and bottom sides
            side_drop = (h_ctr - h_edge) / 2; // 0.5 mm drop per side

            // Calculate the radius of the crowning cylinders using the sagitta formula
            crown_radius = (pow(width, 2) / (8 * side_drop)) + (side_drop / 2);

            $fn = 64; // Smooth curve resolution

            intersection() {
                // 1. Base shape: Extruded rounded rectangle centered on the Z-axis
                linear_extrude(height = h_ctr, center = true) {
                    rect(size=[width, length], rounding=0);
                }
                
                // 2. Top Crowning Cylinder
                // Shifted down so its lower edge cuts the top curve
                translate([0, 0, (h_ctr/2) - crown_radius])
                    ycyl(r = crown_radius, l = length + 2);
                    
                // 3. Bottom Crowning Cylinder
                // Shifted up so its upper edge cuts the bottom curve
                translate([0, 0, -(h_ctr/2) + crown_radius])
                    ycyl(r = crown_radius, l = length + 2);
            }
        }

        translate([34.75 - 103 / 2, -(20.5 + 9) / 2, (15 - 9) / 2 + 2]) {
            *translate([17 * 0, 0, 0])
                cuboid([13, 9, 15], rounding = 1, except = [FRONT,BACK]);
            translate([17 * 1, 0, 0])
                cuboid([13, 9, 15], rounding = 1, except = [FRONT,BACK]);
            translate([17 * 2, 0, 0])
                cuboid([13, 9, 15], rounding = 1, except = [FRONT,BACK]);
            translate([17 * 3, 0, 0])
                cuboid([13, 9, 15], rounding = 1, except = [FRONT,BACK]);

            translate([103 / 2 + 17, 0, 0])
                cuboid([103, 9, 15], rounding = 1);
        }

        hull() {
            translate([-103 / 2 - 50 / 2 + 1, 0, 0])
                rotate([0, 90, 0])
                    cylinder(d = 5.75, h = 50, center = true);

            translate([-103 / 2 - 50 / 2 + 1, 0, -20])
                rotate([0, 90, 0])
                    cylinder(d = 5.75, h = 50, center = true);
        }
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

    *translate([0, 0, -11.8]) {
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
                translate([0, 0, -50 / 2])
                    cylinder(d = 50, h = 60, center = true);

                translate([0, 0, 2]) {
                    difference() {
                        translate([0, 0, -50 / 2])
                            cylinder(d = 52, h = 60, center = true);

                        translate([0, 0, -50 / 2])
                            cylinder(d = 50, h = 62, center = true);
                    }
                }
            }

            translate([0, 0, 0]) {
                cylinder(d = 47, h = 100, center = true);
            }

            translate([0, 0, 4.6]) {
                intersection() {
                    cube([16.5, 50, 3], center = true);

                    cylinder(d = 48.5, h = 3, center = true);
                }
            }

            // USB port
            translate([50 / 2, 0, -1.75])
                cuboid([30, 13, 9], rounding = 2);

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

difference() {
    union() {
        difference() {
            translate([3, 39, -3])
                cuboid([55, 28, 45], rounding = 1);

            translate([0, 49, 15])
                rotate([70, 0, 0])
                    %import("assets/Lux and Gesture.stl");

            translate([3, 37 - 27 / 2, 32])
                cuboid([60, 27, 50], rounding = 1);
        }

        translate([0, 0, 0]) {
            rotate([45, 0, 0]) {
                translate([-35, 0, 0])
                    shell();
                translate([35, 0, 0])
                    shell();
            }
        }

        translate([0, 65.7, -12]) {
            %Adafruit_4836();

            difference() {
                union() {
                    translate([0, -3.75, -12 / 2])
                        cuboid([25.5, 18, 12], rounding = 2, except = [TOP,BOTTOM,FRONT]);

                    translate([3, -12.25, -9.5])
                        cuboid([55, 35, 5], rounding = 2, except = [BOTTOM]);
                }

                translate([0, -0.5, 8])
                    cube([18, 25, 20], center = true);
                translate([0, 9, -3])
                    cube([18, 25, 20], center = true);
            }
        }
    }

    translate([10, 35.25, -19.5 + 2])
        B0C2YN8F7Z();

    translate([0, 49, 15]) {
        rotate([70, 0, 0]) {
            translate([3, 0, 6])
                cuboid([40 + 4 + 0.5, 44 + 4 + 0.5, 17 + 0.5], rounding = 3);
            translate([0, -20, 3.85])
                cuboid([35, 35, 10], rounding = 2, except = [FRONT,BACK]);
        }

        translate([0, 0, -20])
            cuboid([15, 35, 10], rounding = 2, except = [FRONT,BACK]);
        translate([40, 0, -20])
            rotate([90, 0, 0])
                cylinder(d = 8, h = 100, center = true);

        translate([0, 13, 0])
            grid_copies(spacing = [20.25, 12.75], n = [2, 2])
                cylinder(d = 2.5, h = 100, center = true);
    }
    
    translate([0, 0, -50 / 2 - 24])
        cube([200, 200, 50], center = true);
}
