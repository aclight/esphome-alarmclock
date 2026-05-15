// =============================================================================
// Alarm Clock Enclosure for Elecrow CrowPanel Advance 4.3" (V1.1)
// Parametric OpenSCAD design - two-piece (front bezel + rear body)
// =============================================================================

// ---------------------------------------------------------------------------
// Parameters - Adjust these to fine-tune the fit
// ---------------------------------------------------------------------------

// Tolerances
tol = 0.3;              // General fit tolerance (per side)
printer_tol = 0.2;     // Extra tolerance for press-fit features

// Wall thickness
wall = 2.5;            // Main wall thickness
bezel_lip = 1.5;       // Bezel overlap onto display glass

// Tilt angle (degrees backward from vertical)
tilt_angle = 12;

// --- CrowPanel PCB ---
pcb_w = 118;           // Width (X)
pcb_h = 71.1;          // Height (Y)
pcb_thick = 2.0;       // PCB thickness (total 7.5 - display 5.5)
pcb_mount_dia = 3.0;   // Mounting hole diameter
pcb_mount_inset = 3.0; // Hole center distance from edge

// --- Display module (attached to PCB front) ---
disp_w = 106;          // Display width
disp_h = 67.2;         // Display height
disp_thick = 5.5;      // Display thickness
disp_offset_x = (pcb_w - disp_w) / 2;  // Centered horizontally
disp_offset_y = (pcb_h - disp_h) / 2;  // Centered vertically (adjust if needed)

// --- Viewable area (active display) ---
bezel_top = 4.0;
bezel_bottom = 8.5;
bezel_left = 4.75;
bezel_right = 4.75;
view_w = disp_w - bezel_left - bezel_right;  // ~96.5mm
view_h = disp_h - bezel_top - bezel_bottom;  // ~54.7mm

// --- Total board depth (front of display to tallest rear component) ---
total_depth = 15.5;

// --- Speaker ---
spk_w = 69.46;         // Length
spk_d = 30.75;         // Width
spk_h = 16.5;          // Depth/height
spk_mount_dia = 3.35;
spk_mount_inset = 3.14;

// --- BH1750 Light Sensor ---
ls_w = 14.11;          // Width
ls_l = 18.6;           // Length
ls_total_h = 19.0;     // Height with connectors
ls_mount_dia = 2.93;
ls_mount_inset = 2.93;

// --- USB-C port cutout (on back panel) ---
usbc_w = 12.5;         // Width of USB-C opening
usbc_h = 7.0;          // Height of USB-C opening
usbc_r = 1.5;          // Corner radius

// --- Screw bosses (assembly: screws from back into front posts) ---
screw_dia = 3.0;       // M3 screws
screw_boss_dia = 7.0;  // Boss outer diameter
screw_boss_depth = 8.0;// How deep the screw engages
screw_pilot_dia = 2.5; // Pilot hole for self-tapping into plastic

// ---------------------------------------------------------------------------
// Derived dimensions
// ---------------------------------------------------------------------------

// Internal cavity dimensions
cavity_w = pcb_w + 2 * tol;
cavity_h = pcb_h + 2 * tol;
// Depth behind PCB (from back of PCB to back wall interior)
rear_clearance = total_depth - disp_thick - pcb_thick + tol;
// Depth for speaker on top
spk_cavity_depth = spk_h + 1.0;

// Overall enclosure exterior dimensions
enc_w = cavity_w + 2 * wall;
enc_h = cavity_h + 2 * wall + spk_d + wall; // Extra height on top for speaker
enc_depth = wall + disp_thick + pcb_thick + rear_clearance + wall;

// Speaker position (centered on top section)
spk_x_offset = (enc_w - spk_w) / 2;
spk_y_offset = cavity_h + 2 * wall;  // Starts above the display area

// Light sensor position (on top, beside speaker or at the edge)
ls_x_offset = enc_w - wall - ls_w - 2;  // Right side of top area
ls_y_offset = spk_y_offset + (spk_d - ls_l) / 2;  // Centered in speaker band

// Front bezel split plane: at the front face of the display
split_z = wall + disp_thick;

// ---------------------------------------------------------------------------
// Modules
// ---------------------------------------------------------------------------

// Rounded rectangle (2D)
module rounded_rect(w, h, r) {
    offset(r) offset(-r) square([w, h]);
}

// Rounded box (3D)
module rounded_box(w, h, d, r) {
    linear_extrude(d)
        rounded_rect(w, h, r);
}

// Speaker grille pattern - slot array
module speaker_grille(w, l, slot_w, slot_l, spacing) {
    nx = floor((w - spacing) / (slot_w + spacing));
    ny = floor((l - spacing) / (slot_l + spacing));
    x_start = (w - (nx * (slot_w + spacing) - spacing)) / 2;
    y_start = (l - (ny * (slot_l + spacing) - spacing)) / 2;
    
    for (ix = [0 : nx-1]) {
        for (iy = [0 : ny-1]) {
            translate([x_start + ix * (slot_w + spacing),
                       y_start + iy * (slot_l + spacing), 0])
                rounded_rect(slot_w, slot_l, 0.5);
        }
    }
}

// Light sensor window - small circular opening with diffuser ring
module sensor_window(dia) {
    circle(d = dia);
}

// PCB mounting boss (post rising from the back)
module mount_boss(od, id, height) {
    difference() {
        cylinder(d = od, h = height, $fn = 24);
        translate([0, 0, -0.1])
            cylinder(d = id, h = height + 0.2, $fn = 24);
    }
}

// USB-C port cutout shape (rounded rectangle)
module usbc_cutout() {
    offset(usbc_r) offset(-usbc_r)
        square([usbc_w - 2*usbc_r, usbc_h - 2*usbc_r], center = true);
}

// ---------------------------------------------------------------------------
// Assembly screw boss positions (4 corners of internal cavity)
// ---------------------------------------------------------------------------
assy_boss_inset = 5;  // Inset from enclosure interior corners
assy_boss_positions = [
    [wall + assy_boss_inset, wall + assy_boss_inset],
    [enc_w - wall - assy_boss_inset, wall + assy_boss_inset],
    [wall + assy_boss_inset, wall + cavity_h - assy_boss_inset],
    [enc_w - wall - assy_boss_inset, wall + cavity_h - assy_boss_inset],
];

// ---------------------------------------------------------------------------
// FRONT BEZEL
// ---------------------------------------------------------------------------
module front_bezel() {
    difference() {
        union() {
            // Main front plate (full enclosure face)
            cube([enc_w, enc_h, wall]);
            
            // Inner lip that overlaps onto display bezel (holds the display)
            translate([wall + tol + disp_offset_x + bezel_left - bezel_lip,
                       wall + tol + disp_offset_y + bezel_bottom - bezel_lip,
                       wall])
                cube([view_w + 2 * bezel_lip,
                      view_h + 2 * bezel_lip,
                      1.5]);  // Lip depth
            
            // Side walls extending back to the split plane
            // These form the "skirt" of the front piece that the back slots into
            difference() {
                cube([enc_w, enc_h, split_z]);
                translate([wall, wall, -0.1])
                    cube([cavity_w, cavity_h + spk_d + wall, split_z + 0.2]);
            }
            
            // Screw boss posts (receive screws from back)
            for (pos = assy_boss_positions) {
                translate([pos[0], pos[1], wall])
                    cylinder(d = screw_boss_dia, h = screw_boss_depth, $fn = 24);
            }
        }
        
        // Display viewable area cutout (through the front wall)
        translate([wall + tol + disp_offset_x + bezel_left,
                   wall + tol + disp_offset_y + bezel_bottom,
                   -0.1])
            cube([view_w, view_h, wall + 0.2]);
        
        // Speaker grille on top face
        translate([spk_x_offset, spk_y_offset, -0.1])
            linear_extrude(wall + 0.2)
                speaker_grille(spk_w, spk_d, 2.0, 15.0, 2.0);
        
        // Light sensor window on top face
        translate([ls_x_offset + ls_w/2, ls_y_offset + ls_l/2, -0.1])
            linear_extrude(wall + 0.2)
                circle(d = 6, $fn = 32);
        
        // Pilot holes in screw bosses
        for (pos = assy_boss_positions) {
            translate([pos[0], pos[1], -0.1])
                cylinder(d = screw_pilot_dia, h = wall + screw_boss_depth + 0.2, $fn = 24);
        }
    }
}

// ---------------------------------------------------------------------------
// REAR BODY
// ---------------------------------------------------------------------------
module rear_body() {
    rear_depth = enc_depth - split_z;
    
    difference() {
        union() {
            // Main rear shell
            difference() {
                // Outer box
                cube([enc_w, enc_h, rear_depth]);
                
                // Inner cavity (main board area)
                translate([wall, wall, -0.1])
                    cube([cavity_w, cavity_h, rear_depth - wall + 0.1]);
                
                // Speaker cavity (upper section)
                translate([wall, wall + cavity_h, -0.1])
                    cube([cavity_w, spk_d, rear_depth - wall + 0.1]);
            }
            
            // PCB mounting bosses (rise from the back wall)
            pcb_boss_h = rear_depth - wall - pcb_thick - rear_clearance + pcb_thick;
            for (corner = [[pcb_mount_inset, pcb_mount_inset],
                           [pcb_w - pcb_mount_inset, pcb_mount_inset],
                           [pcb_mount_inset, pcb_h - pcb_mount_inset],
                           [pcb_w - pcb_mount_inset, pcb_h - pcb_mount_inset]]) {
                translate([wall + tol + corner[0], wall + tol + corner[1], wall])
                    mount_boss(6, pcb_mount_dia + 0.2, rear_clearance - 1);
            }
            
            // Speaker mounting posts
            spk_int_x = spk_x_offset - wall;  // Offset into cavity
            spk_int_y = cavity_h;
            for (corner = [[spk_mount_inset, spk_mount_inset],
                           [spk_w - spk_mount_inset, spk_mount_inset],
                           [spk_mount_inset, spk_d - spk_mount_inset],
                           [spk_w - spk_mount_inset, spk_d - spk_mount_inset]]) {
                translate([spk_x_offset + corner[0],
                           spk_y_offset - wall + corner[1],
                           wall])
                    mount_boss(6, spk_mount_dia + 0.2, rear_depth - wall - spk_h - 1);
            }
            
            // Light sensor mounting posts
            // Sensor has 2 holes on the side opposite connector
            // Mount vertically so sensor IC faces toward the top grille
            for (i = [0, 1]) {
                translate([ls_x_offset + ls_mount_inset + i * (ls_w - 2*ls_mount_inset),
                           ls_y_offset + ls_l - ls_mount_inset,
                           wall])
                    mount_boss(5.5, ls_mount_dia + 0.2, rear_depth - wall - 5);
            }
        }
        
        // USB-C port cutout (bottom center of back wall)
        translate([enc_w / 2, 0, rear_depth / 2])
            rotate([90, 0, 0])
                linear_extrude(wall + 0.2)
                    usbc_cutout();
        
        // Screw through-holes (for assembly screws coming from the back)
        for (pos = assy_boss_positions) {
            translate([pos[0], pos[1], -0.1])
                cylinder(d = screw_dia + 0.3, h = wall + 0.2, $fn = 24);
            // Counterbore for screw head
            translate([pos[0], pos[1], -0.1])
                cylinder(d = 6.5, h = 2.0, $fn = 24);
        }
        
        // Ventilation slots on back wall (for heat dissipation)
        vent_count = 5;
        vent_w = 30;
        vent_h = 2;
        vent_spacing = 5;
        vent_start_y = (enc_h - (vent_count * vent_h + (vent_count-1) * vent_spacing)) / 2;
        for (i = [0 : vent_count - 1]) {
            translate([(enc_w - vent_w) / 2,
                       vent_start_y + i * (vent_h + vent_spacing),
                       rear_depth - wall - 0.1])
                cube([vent_w, vent_h, wall + 0.2]);
        }
    }
}

// ---------------------------------------------------------------------------
// STAND (integrated wedge for tilt angle)
// ---------------------------------------------------------------------------
module stand() {
    // Creates a wedge that attaches to the bottom of the rear body
    // to tilt the enclosure backward by tilt_angle degrees
    stand_depth = enc_depth;
    stand_w = enc_w;
    // Height of the wedge at the front (tallest point)
    stand_front_h = stand_depth * tan(tilt_angle);
    // Foot depth (flat bottom contact area)
    foot_depth = 8;
    
    difference() {
        union() {
            // Main wedge
            hull() {
                // Front bottom edge (tall side)
                translate([0, 0, 0])
                    cube([stand_w, 0.1, stand_front_h]);
                // Rear bottom edge (short side - just the foot)
                translate([0, stand_depth - foot_depth, 0])
                    cube([stand_w, foot_depth, 2]);
            }
            
            // Anti-slip foot pads
            foot_w = 15;
            foot_h = 1.5;
            for (x = [10, stand_w - 10 - foot_w]) {
                // Front feet
                translate([x, 0, 0])
                    cube([foot_w, foot_depth, foot_h]);
                // Rear feet
                translate([x, stand_depth - foot_depth, 0])
                    cube([foot_w, foot_depth, foot_h]);
            }
        }
        
        // Hollow out the interior to save material
        translate([wall, wall, wall])
            hull() {
                translate([0, 0, 0])
                    cube([stand_w - 2*wall, 0.1, stand_front_h - wall]);
                translate([0, stand_depth - foot_depth - wall - 0.1, 0])
                    cube([stand_w - 2*wall, 0.1, 0.1]);
            }
    }
}

// ---------------------------------------------------------------------------
// COMPLETE ASSEMBLY (for visualization)
// ---------------------------------------------------------------------------
module assembly() {
    // Rear body
    color("DarkSlateGray", 0.8)
        translate([0, 0, split_z])
            rear_body();
    
    // Front bezel
    color("DimGray", 0.9)
        front_bezel();
    
    // Stand (attached below rear body, rotated to show tilt)
    color("DarkSlateGray", 0.6)
        translate([0, -5, enc_depth])
            rotate([180, 0, 0])
                translate([0, -enc_h, 0])
                    stand();
}

// ---------------------------------------------------------------------------
// PRINT PLATES (for export)
// ---------------------------------------------------------------------------

// Uncomment ONE of these for STL export:

// Front bezel - print face-down
//front_bezel();

// Rear body - print open-side-up
//translate([0, 0, 0]) rear_body();

// Stand - print flat-side-down
//stand();

// Assembly view (default)
assembly();
