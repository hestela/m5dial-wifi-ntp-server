// ============================================================
// Box enclosure — bottom + lid
// All dimensions in mm
// ============================================================

$fn = 48;

// --- Outer dimensions ---
box_w = 60;
box_d = 110;
box_h = 19;
wall  = 3;

// --- Corner posts (heat-set inserts) ---
post_od      = 8.2;  // post outer diameter
post_gap     = 0.5;  // gap between post outer surface and inner wall
insert_d     = 4.2;  // heat-set insert hole diameter
insert_depth = 6.0;  // depth of insert hole — adjust to match insert length

// --- Lid bolt holes ---
bolt_d = 3.4;  // M3 bolt clearance hole diameter

// --- USB-C panel mount cutout (front wall) ---
usbc_w        = 25;  // cutout width
usbc_h        = 8;   // cutout height
usbc_r        = 3;   // corner radius — adjust when part arrives
usbc_from_top = 7;   // distance from top of box to top edge of cutout

// --- Lid ---
lid_h     = 3;  // lid plate thickness
lip_wall  = 2;  // locating lip wall thickness
lip_depth = 3;  // how far the lip drops into the box opening

// --- Lid pocket (inside face, back side) ---
pocket_w    = 25.5;  // pocket width
pocket_d    = 25.5;  // pocket depth
pocket_r    = 3;     // corner radius — adjust when needed
pocket_from_edge = 9;  // distance from back edge of lid to nearest pocket edge
pocket_depth = 1;    // how deep the pocket cuts into the lid

// --- Fillets ---
fillet_r = 2;  // outside edge/corner fillet radius

// --- Derived (do not edit) ---
inner_w = box_w - 2*wall;            // 54
inner_d = box_d - 2*wall;            // 104
inner_h = box_h - wall;              // 16 (floor to open top)
post_r  = post_od / 2;               // 4.1
post_cx = wall + post_gap + post_r;  // 7.6 — from each outer edge
post_cy = wall + post_gap + post_r;  // 7.6

// ============================================================
// Helper: box with filleted vertical edges and bottom corners, flat top.
// ============================================================
module rounded_box(w, d, h, r) {
    hull()
        for (x = [r, w-r])
            for (y = [r, d-r]) {
                translate([x, y, r])   sphere(r=r, $fn=32);   // bottom corners
                translate([x, y, r])   cylinder(r=r, h=h-r, $fn=32); // vertical edges to flat top
            }
}

// ============================================================
// Helper: rounded lid plate — top edges and vertical edges filleted,
// bottom face flat (seats on box). Works even when h < 2*r.
// ============================================================
module rounded_prism(w, d, h, r) {
    hull()
        for (x = [r, w-r])
            for (y = [r, d-r]) {
                translate([x, y, h-r]) sphere(r=r, $fn=32);    // top corners + top edges
                translate([x, y, 0])   cylinder(r=r, h=h-r, $fn=32); // vertical edges to flat bottom
            }
}

// ============================================================
// Helper: rounded-rectangle slot extruded in the +Y direction
// Centered at caller's origin in X and Z.
// ============================================================
module usbc_slot(depth) {
    ox = usbc_w/2 - usbc_r;
    oz = usbc_h/2 - usbc_r;
    hull()
        for (dx = [-ox, ox])
            for (dz = [-oz, oz])
                translate([dx, 0, dz])
                    rotate([-90, 0, 0])
                        cylinder(r=usbc_r, h=depth);
}

// ============================================================
// Bottom
// ============================================================
module box_bottom() {
    // Z-center of USB-C cutout:
    //   top of cutout  = box_h - usbc_from_top         = 19 - 9 = 10
    //   bottom of cutout = top - usbc_h                = 10 - 8 =  2
    //   center                                          = 6
    usbc_z = box_h - usbc_from_top - usbc_h/2;

    difference() {
        union() {
            // Hollow shell — interior carved out first so posts aren't subtracted
            difference() {
                rounded_box(box_w, box_d, box_h, fillet_r);
                // Interior cavity (keeps floor and walls, opens the top)
                translate([wall, wall, wall])
                    cube([inner_w, inner_d, inner_h + 1]);
            }

            // Corner posts — unioned after hollowing so they survive the cavity cut
            // Each post: 8.2 mm OD (solid), 0.5 mm clear of inner walls
            for (px = [post_cx, box_w - post_cx])
                for (py = [post_cy, box_d - post_cy])
                    translate([px, py, wall])
                        cylinder(d=post_od, h=6);
        }

        // Heat-set insert holes (4.2 mm Ø) — drilled from top of each post down
        // Post top = wall + 6 = 9; hole bottom = 9 - insert_depth = 3 (floor level)
        for (px = [post_cx, box_w - post_cx])
            for (py = [post_cy, box_d - post_cy])
                translate([px, py, wall + 6 - insert_depth])
                    cylinder(d=insert_d, h=insert_depth + 1);

        // USB-C cutout — centered on front wall (y=0), at correct height
        translate([box_w/2, -1, usbc_z])
            usbc_slot(wall + 2);
    }
}

// ============================================================
// Lid
// ============================================================
module box_lid() {
    difference() {
        union() {
            // Flat top plate with filleted vertical edges
            rounded_prism(box_w, box_d, lid_h, fillet_r);

            // Locating lip on underside — outer dims match inner box (54×104),
            // lip_wall thick, drops lip_depth below the plate.
            translate([wall, wall, -lip_depth])
                difference() {
                    cube([inner_w, inner_d, lip_depth]);
                    translate([lip_wall, lip_wall, -1])
                        cube([inner_w - 2*lip_wall, inner_d - 2*lip_wall, lip_depth + 2]);
                }
        }

        // M3 bolt clearance holes — aligned with bottom post centers
        for (px = [post_cx, box_w - post_cx])
            for (py = [post_cy, box_d - post_cy])
                translate([px, py, -1])
                    cylinder(d=bolt_d, h=lid_h + 2);

        // 44 mm circular hole — centered side-to-side, 10 mm from front face
        translate([box_w/2, box_d - 10 - 44/2, -1])
            cylinder(d=44, h=lid_h + 2);

        // Rounded square pocket — inside face, centered side-to-side, 9 mm from back edge
        // Cuts 1 mm deep from the inside (z=0) face
        translate([box_w/2, pocket_from_edge + pocket_d/2, 0])
            hull()
                for (dx = [-(pocket_w/2 - pocket_r), (pocket_w/2 - pocket_r)])
                    for (dy = [-(pocket_d/2 - pocket_r), (pocket_d/2 - pocket_r)])
                        translate([dx, dy, 0])
                            cylinder(r=pocket_r, h=pocket_depth);
    }
}

// ============================================================
// Render both parts side by side (bottom on left, lid on right)
// ============================================================
box_bottom();
translate([box_w + 10, box_d, lid_h])
    rotate([180, 0, 0])
        box_lid();
