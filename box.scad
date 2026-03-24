// ============================================================
// Box enclosure — bottom + lid
// All dimensions in mm
// ============================================================

$fn = 48;

// --- Outer dimensions ---
box_w = 66;
box_d = 112;
box_h = 32;
wall  = 3;

// --- Corner posts (heat-set inserts) ---
post_od      = 8.2;  // post outer diameter
post_gap     = 0.5;  // gap between post outer surface and inner wall
insert_d     = 4.2;  // heat-set insert hole diameter
insert_depth = 6.0;  // depth of insert hole — adjust to match insert length

// --- Lid bolt holes ---
bolt_d      = 3.4;  // M3 bolt clearance hole diameter
csk_d       = 6.0;  // M3 countersink head diameter (90° ISO)
csk_h       = (csk_d - bolt_d) / 2;  // cone depth for 90° countersink = 1.3 mm

// --- USB-C breakout board cutout (front wall) ---
// PCB glued to box floor; port faces front wall
usbc_port_w    = 8.95;  // connector opening width
usbc_port_h    = 3.19;  // connector opening height
usbc_pcb_h     = 1.55;  // breakout PCB thickness
usbc_clearance = 0.4;   // clearance around port opening
usbc_chamfer   = 3.0;   // chamfer expansion on outer wall face for cable insertion
usbc_r         = 0.5;   // corner radius on cutout

// --- M5Dial mounting hole (lid) ---
m5dial_hole_d         = 44;   // diameter of the M5Dial bezel cutout
m5dial_hole_from_back = 11;   // distance from back edge of lid to nearest edge of hole
m5dial_recess_d       = m5dial_hole_d + 8.2;  // diameter of inside-face seating recess
m5dial_recess_depth   = 2;    // depth of seating recess from inside face

// --- Lid ---
lid_h     = 3;  // lid plate thickness
lip_wall  = 2;  // locating lip wall thickness
lip_depth = 3;  // how far the lip drops into the box opening

// --- GPS antenna cavity (inside face of lid) ---
pocket_w    = 25.5;  // GPS cutout width
pocket_d    = 25.5;  // GPS cutout depth
pocket_r    = 3;     // corner radius — adjust when needed
pocket_from_edge = 9;  // distance from front edge of lid to nearest edge of GPS cutout
pocket_depth = 1;    // depth of GPS cavity — adjust to match antenna thickness

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
// The intersection with the bounding cube forces a flat bottom by
// clipping the spheres that would otherwise extend below z=0.
// ============================================================
module rounded_prism(w, d, h, r) {
    intersection() {
        hull()
            for (x = [r, w-r])
                for (y = [r, d-r]) {
                    translate([x, y, h-r]) sphere(r=r, $fn=32);    // top corners + top edges
                    translate([x, y, 0])   cylinder(r=r, h=h-r, $fn=32); // vertical edges
                }
        cube([w, d, h]);  // clamps bottom flat at z=0
    }
}

// ============================================================
// Helper: rounded-rectangle disc in the XZ plane at a given Y offset.
// Used as hull() seed for the chamfered USB-C slot.
// ============================================================
module usbc_face(w, h, y_pos) {
    for (dx = [-(w/2 - usbc_r), (w/2 - usbc_r)])
        for (dz = [-(h/2 - usbc_r), (h/2 - usbc_r)])
            translate([dx, y_pos, dz])
                rotate([-90, 0, 0])
                    cylinder(r=usbc_r, h=0.01, $fn=16);
}

// ============================================================
// Helper: chamfered USB-C slot extruded in the +Y direction.
// Outer face (y=0) is chamfer-expanded; inner face (y=depth) is tight.
// Centered at caller's origin in X and Z.
// ============================================================
module usbc_slot(depth) {
    cw = usbc_port_w + 2*usbc_clearance;      // tight width
    ch = usbc_port_h + 2*usbc_clearance;      // tight height
    ow = cw + 2*usbc_chamfer;                 // flared width (outer face)
    oh = ch + 2*usbc_chamfer;                 // flared height (outer face)
    // Chamfer tapers through the wall only — tight at inner wall face (depth-2)
    hull() {
        usbc_face(ow, oh, 0);                 // outer wall face — large opening
        usbc_face(cw, ch, depth - 2);         // inner wall face — tight fit
    }
    // Straight bore past inner wall to ensure clean subtraction
    hull() {
        usbc_face(cw, ch, depth - 2);
        usbc_face(cw, ch, depth - 0.01);
    }
}

// ============================================================
// Bottom
// ============================================================
module box_bottom() {
    // Z-center of USB-C cutout:
    //   PCB sits on floor: PCB bottom = wall = 3 mm (outside Z)
    //   PCB top  = wall + usbc_pcb_h         = 3 + 1.55 = 4.55 mm
    //   port bottom = PCB top                = 4.55 mm
    //   port top    = port bottom + port_h   = 4.55 + 3.19 = 7.74 mm
    //   center                               = 6.145 mm
    usbc_z = wall + usbc_pcb_h + usbc_port_h/2;

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

        // M3 bolt clearance holes + countersink chamfer on outside face
        for (px = [post_cx, box_w - post_cx])
            for (py = [post_cy, box_d - post_cy]) {
                // Through hole
                translate([px, py, -1])
                    cylinder(d=bolt_d, h=lid_h + 2);
                // 90° countersink cone — sits on outside (top) face of lid
                translate([px, py, lid_h - csk_h])
                    cylinder(d1=bolt_d, d2=csk_d, h=csk_h + 0.01);
            }

        // M5Dial bezel cutout — centered side-to-side, m5dial_hole_from_back mm from back face
        translate([box_w/2, box_d - m5dial_hole_from_back - m5dial_hole_d/2, -1])
            cylinder(d=m5dial_hole_d, h=lid_h + 2);

        // M5Dial seating recess — inside face of lid, 2 mm deeper than hole, 2 mm larger diameter
        translate([box_w/2, box_d - m5dial_hole_from_back - m5dial_hole_d/2, -0.01])
            cylinder(d=m5dial_recess_d, h=m5dial_recess_depth + 0.01);

        // GPS antenna cavity — inside face of lid, centered side-to-side, 9 mm from front edge
        // Cuts 1 mm deep from the inside (z=0) face
        translate([box_w/2, pocket_from_edge + pocket_d/2, -0.01])
            hull()
                for (dx = [-(pocket_w/2 - pocket_r), (pocket_w/2 - pocket_r)])
                    for (dy = [-(pocket_d/2 - pocket_r), (pocket_d/2 - pocket_r)])
                        translate([dx, dy, 0])
                            cylinder(r=pocket_r, h=pocket_depth + 0.01);
    }
}

// ============================================================
// Render both parts side by side (bottom on left, lid on right)
// ============================================================
box_bottom();
translate([box_w + 10, box_d, lid_h])
    rotate([180, 0, 0])
        box_lid();
