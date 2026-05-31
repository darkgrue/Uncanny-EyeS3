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
$fn = 50;


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

/**
 * @brief Generate a cylinder with a 1° slope.
 *
 * @param d Diameter of cylinder.
 * @param h Height of cylinder.
**/
function d1deg(d, h) = d + (2 * h * tan(1));


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

module Adafruit_6498() {
    translate([8.89, -12.7, 0])
        rotate([0, 0, 90])
            import("assets/6498 MAX44009 Light Sensor.3mf");

    // reference center        
    *#cylinder(h = 50, d = 1, center = true);
}

module SEN0626() {
    translate([-375, -330, -15.30106])
        rotate([0, 0, 0])
            import("assets/SEN0626_3D_STP.stl");

    // reference center        
    *#cylinder(h = 50, d = 1, center = true);
}


// // // //
// MAIN
// // // //

$top = 1;
$bottom = 1;

$internal_x = 40;
$internal_y = 44;
$internal_z = 12.63;
$wall_thk = 2;
$exterior_rounding_r = 3;

adafruit_thick = 1.70; 
sen0626_thick  = 1.05;

difference() {
    union() {
        if($top)
        difference() {
            union() {
                translate([3, 0, ($internal_z / 2 + $wall_thk) / 2 + $internal_z / 2]) {
                    difference() {
                            cuboid([$internal_x + $wall_thk * 2, $internal_y + $wall_thk * 2, $internal_z / 2 + $wall_thk], rounding = $exterior_rounding_r, except = [BOTTOM]);

                            // interior cut
                            translate([0, 0, -$wall_thk])
                                cuboid([$internal_x, $internal_y, $internal_z / 2 + $wall_thk], rounding = $exterior_rounding_r, except = [BOTTOM]);
                    }
                }

                translate([13.25, 0, 7.6]) {
                    %Adafruit_6498();

                    translate([6.35, 0, adafruit_thick]) {
                        difference() {
                            translate([0, 0, 4 / 2])
                                ycopies(spacing = 20.32, n = 2)
                                    cylinder(d = 5, h = 4, center = true);
                            translate([0, 0, -3 / 2 + 2])
                                ycopies(spacing = 20.32, n = 2)
                                cylinder(d = 3, h = 3, center = true);
                        }
                    }
                }

                translate([0, 0, 6]) {
                    %SEN0626();

                    difference() {
                        translate([0, 0, sen0626_thick]) {
                            translate([0, 0, 6.2 / 2])
                                grid_copies(spacing = [25, 35], n = [2, 2])
                                    cylinder(d = 7, h = 6.2, center = true);
                        }

                        translate([0, 0, sen0626_thick]) {
                            translate([-25 / 2, -35 / 2, 0])
                                cylinder(d = 4, h = 6.2, center = true);
                            translate([25 / 2, 35 / 2, 0])
                                cylinder(d = 8, h = 11.2, center = true);
                        }
                    }
                }
            }
        }

        if($bottom)
        difference() {
            union() {
                translate([3, 0, ($internal_z / 2 + $wall_thk) / 2 - $wall_thk]) {
                    difference() {
                            cuboid([$internal_x + $wall_thk * 2, $internal_y + $wall_thk * 2, $internal_z / 2 + $wall_thk], rounding = $exterior_rounding_r, except = [TOP]);

                            // interior cut
                            translate([0, 0, $wall_thk])
                                cuboid([$internal_x, $internal_y, $internal_z / 2 + $wall_thk], rounding = $exterior_rounding_r, except = [TOP]);
                    }
                }

                translate([13.25, 0, 7.6]) {
                    %Adafruit_6498();

                    translate([6.35, 0, 0]) {
                        translate([0, 0, -7.6 / 2])
                            ycopies(spacing = 20.32, n = 2)
                                cylinder(d = 5, h = 7.6, center = true);
                        translate([0, 0, -10.6 / 2 + 3])
                            ycopies(spacing = 20.32, n = 2)
                            cylinder(d = 2.5, h = 10.6, center = true);
                    }
                }

                translate([0, 0, 6]) {
                    %SEN0626();
                    
                    translate([0, 0, 0]) {
                        translate([0, 0, -6 / 2])
                            grid_copies(spacing = [25, 35], n = [2, 2])
                                cylinder(d = 7, h = 6, center = true);
                        translate([0, 0, -8 / 2 + 2])
                            grid_copies(spacing = [25, 35], n = [2, 2])
                                cylinder(d = 3, h = 8, center = true);
                    }
                }
            }
        }
    }

    // case screws
    translate([0, 0, 6 + sen0626_thick + $wall_thk]) {
        translate([25 / 2, -35 / 2, 0]) {
            translate([0, 0, -14 / 2 + 3])
                cylinder(d1 = 3.1 * 2 - d1deg(3.1, 11), d2 = 3.2, h = 14, center = true);
            translate([0, 0, 50 / 2])
                cylinder(d1 = 6, d2 = d1deg(6, 50), h = 50, center = true);
            if ($top)
            %screw("M3x1", head = "pan", drive = "phillips", length = 10, anchor = "head_bot");
        }
        translate([-25 / 2, 35 / 2, 0]) {
            translate([0, 0, -14 / 2 + 3])
                cylinder(d1 = 3.1 * 2 - d1deg(3.1, 11), d2 = 3.2, h = 14, center = true);
            translate([0, 0, 50 / 2])
                cylinder(d1 = 6, d2 = d1deg(6, 50), h = 50, center = true);
            if ($top)
            %screw("M3x1", head = "pan", drive = "phillips", length = 10, anchor = "head_bot");
        }
    }

    //Adafruit_6498
    translate([13.25, -0, 24.15]) {
        cylinder(d = 3.25, h = 30, center = true);
        translate([0, 0, -9])
            cylinder(d1 = 3.25, d2 = 9, h = 5, center = true);
    }
    translate([9.56, -9.15, 24.15])
        cylinder(d = 2, h = 30, center = true);

    // SEN0626
    translate([0, -$internal_y / 2, 3.85])
        cuboid([12, 20, 6], rounding = 2, except = [FRONT,BACK]);
    translate([0, 0, 24.5]) {
        cylinder(d = 7, h = 30, center = true);
        translate([0, 0, -9])
            cylinder(d1 = 7, d2 = 14, h = 5, center = true);
    }
    translate([0, 11, 24.5])
        cylinder(d = 3, h = 30, center = true);
    translate([0, -11, 24.5])
        cylinder(d = 3, h = 30, center = true);

    // temp cuts
    *translate([100 / 2 + 13, 0, 0]) {
        cube([100, 100, 50], center = true);
    }
}