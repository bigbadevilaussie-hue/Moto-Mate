// Bike Mate MkII - case v4 (print-friendly)
// Axes (assembly): X = width (+X is the rider's right), Y = tube axis (FRONT = -Y, BACK = +Y), Z = height
//
// PRINTING
//   tube      : print_tube  -> stands on its end, open end up, brim recommended. No overhangs.
//   caps      : print_caps  -> flange down, flat on the bed
//   Material  : ASA preferred (sun under the windscreen); PETG softens ~70-80C
//   Screws    : M3 heat-set inserts (hole = pilot_d). For self-tapping set pilot_d = 2.6
//
// part = "assembly", "tube", "cap_front", "cap_back", "print_tube", "print_caps"

part = "assembly";
$fn = 48;

// ---------- BOARD (stripboard, 56 holes across X) ----------
board_l = 145;  board_w = 65;  board_t = 1;
fit = 0.3;

// ---------- TFT MODULE (1.8" ST7735, landscape, datasheet 56 x 35 x 3.45) ----------
mod_l = 56.6;  mod_h = 35.6;  mod_d = 4;
win_w = 35.6;  win_h = 28.6;  win_dx = 0;
mod_cx = -1.9;                 // module centre shifted so glass is centred on window
mount_dx = 52;  mount_dy = 31; // module corner holes (estimated - verify with calipers)
post_d = 6;  post_h = 2.2;  post_pilot = 2.1;   // M2.5 self-tap
stop_w = 2;

// ---------- CASE ----------
wall = 1.6;
H = mod_h + 2*wall + 0.6;
corner_r = 8;
under = 9;           // free space under board
front_room = 7;      // behind TFT module (header tails + wire bend)
back_room = 26;      // behind board: rocker (17 deep), buzzer (16), fan (13)
lip = 3;  flange = 2.4;

// ---------- RAILS / LEDGES (full length, board slides straight through) ----------
rail_overlap = 1.2;  // < 1.5mm green border, never covers holes
slot_clear = 0.2;    // above and below board
rail_h = 2.5;

// ---------- SCREW COLUMNS (4 corners, full length) ----------
pilot_d = 4.0;       // M3 heat-set insert hole
screw_d = 3.4;       // clearance hole in cap flange

// ---------- FRONT PANEL ----------
led_d = 5.2;  led_x = -40;  led_dz = 8;            // red on top, green below
btn_x = -58;  btn_d = 16;  btn_cb_d = 22;  btn_skin = 2.0;   // snap-in push button
sht_x = 40;  sht_z = 0;  sht_w = 16;  sht_h = 18;  sht_skin = 1.2;   // SHT40 louvre pocket
// 12V -> USB charger module (38 x 19), flat in the board plane, port pokes into front plate
usb_x = 61;  usb_mod_w = 19;  usb_pcb_t = 1.2;
usba_w = 14.6;  usba_h = 7.4;
usb_blk_len = 9;
usb_notch_len = 25.5;     // vero corner cut-out (front-right): ~21.5 wide x 25.5 deep

// ---------- BACK PANEL ----------
rock_x = 10;  rock_d = 20.2;  rock_cb_d = 26;  rock_skin = 2.0;     // round rocker switch
sae_x = -20;  sae_d = 10;  zip_dx = 9;                              // SAE cable + zip-tie slots
buz_x = -50;  buz_ear = 36;  buz_post_d = 7;  buz_post_h = 16.5;  buz_pilot = 2.6;  // SFM-27 buzzer
fan_x = 48;  fan_hole = 24;  fan_post_d = 6.5;  fan_post_h = 3;  fan_pilot = 2.6;   // 3010 fan, exhausts through plate
// fan intake: tall slots in the LEFT end wall (long axis along tube = vertical when printed)
fan_slot_z = [16.6, 21, 25.4];  fan_slot_w = 3.4;  fan_slot_len = 45;

// ---------- DERIVED ----------
Wi = board_l + 2*fit;   W = Wi + 2*wall;
Hi = H - 2*wall;
yb = lip + mod_d + front_room + 2;       // board front edge
Lt = yb + board_w + back_room + lip;     // tube length
zb  = wall + under;                      // board bottom
zs0 = zb - slot_clear;  zs1 = zb + board_t + slot_clear;
boss_r = (under - slot_clear)/2;         // column top meets ledge top exactly
usba_z = zb + usb_pcb_t + 3;
usb_blk_x0 = usb_x - usb_mod_w/2;
usb_blk_x1 = 63.6;                       // stays clear of the corner column

echo(str("Tube outer W x H x L: ", W, " x ", H, " x ", Lt));
echo(str("Overall length with caps: ", Lt + 2*flange));
echo(str("Board bottom height: ", zb, "   headroom above board: ", H - wall - zb - board_t));

// ---------- helpers ----------
module rr(w, h, r) { offset(r) square([w - 2*r, h - 2*r], center = true); }
// extrude 2D (x, z) along +Y from y=0
module ey(l) { mirror([0, 1, 0]) rotate([90, 0, 0]) linear_extrude(l) children(); }

function col_pts() = [for (sx = [-1, 1], zc = [wall + boss_r, H - wall - boss_r])
                        [sx*(Wi/2 - boss_r), zc]];

module col2d() {
  for (sx = [-1, 1], tz = [0, 1]) {
    cz = (tz == 0) ? wall + boss_r : H - wall - boss_r;
    wz = (tz == 0) ? wall : H - wall;
    hull() {
      translate([sx*(Wi/2 - boss_r), cz]) circle(r = boss_r);
      translate([sx*(Wi/2 + 0.25), wz + ((tz == 0) ? -0.25 : 0.25)]) square(0.5, center = true);
    }
  }
}

module rails2d() {
  for (m = [0, 1]) mirror([m, 0]) {
    translate([board_l/2 - rail_overlap, wall - 0.5])
      square([Wi/2 - board_l/2 + rail_overlap + 0.5, zs0 - wall + 0.5]);
    translate([board_l/2 - rail_overlap, zs1])
      square([Wi/2 - board_l/2 + rail_overlap + 0.5, rail_h]);
  }
}

module hole_y(x, z, d, y0, len) {
  translate([x, y0, z]) rotate([-90, 0, 0]) cylinder(d = d, h = len);
}

module ring_holes(cx, cz, r, n, d) {
  for (i = [0:n - 1])
    hole_y(cx + r*cos(360*i/n), cz + r*sin(360*i/n), d, -flange - 1, flange + lip + 2);
}

// ---------- TUBE ----------
module tube() {
  difference() {
    union() {
      difference() {
        ey(Lt) translate([0, H/2]) rr(W, H, corner_r);
        translate([0, -1, 0]) ey(Lt + 2) translate([0, H/2]) rr(Wi, Hi, corner_r - wall);
      }
      ey(Lt) { rails2d(); col2d(); }
    }
    // full-length screw holes
    for (p = col_pts()) hole_y(p[0], p[1], pilot_d, -1, Lt + 2);
    // fan intake slots, left end wall
    for (z = fan_slot_z)
      translate([-W/2 - 1, yb + 12, z - fan_slot_w/2]) cube([wall + 1.2, fan_slot_len, fan_slot_w]);
  }
}

// ---------- CAP (shared plate + plug) ----------
module cap_base() {
  translate([0, -flange, 0]) ey(flange) translate([0, H/2]) rr(W, H, corner_r);
  intersection() {
    ey(lip) translate([0, H/2]) rr(Wi - 0.3, Hi - 0.3, corner_r - wall - 0.15);
    difference() {
      translate([-W, -1, 0]) cube([2*W, lip + 2, H]);
      translate([0, -2, 0]) ey(lip + 4) offset(delta = 0.2) { col2d(); rails2d(); }
    }
  }
}

module cap(front = true) {
  difference() {
    union() {
      cap_base();
      if (front) {
        // TFT end stops
        for (s = [-1, 1])
          translate([mod_cx + s*(mod_l/2 + stop_w/2) - stop_w/2, lip - 0.01, H/2 - mod_h/2])
            cube([stop_w, mod_d, mod_h]);
        // TFT screw posts
        for (sx = [-1, 1], sz = [-1, 1])
          translate([mod_cx + sx*mount_dx/2, lip - 0.01, H/2 + sz*mount_dy/2])
            rotate([-90, 0, 0]) cylinder(d = post_d, h = post_h + 0.01);
        // USB charger support block (module PCB rests on it)
        translate([usb_blk_x0, lip - 0.01, wall]) cube([usb_blk_x1 - usb_blk_x0, usb_blk_len, zb - wall]);
        // board stop peg (left slot only; right side is the USB notch)
        translate([-Wi/2 + 0.2, lip - 0.01, zs0 + 0.15])
          cube([1.2, yb - lip - 0.3, zs1 - zs0 - 0.3]);
      } else {
        // buzzer posts (ears assumed at buzzer rear, buzzer faces plate)
        for (s = [-1, 1])
          translate([buz_x + s*buz_ear/2, lip - 0.01, H/2]) rotate([-90, 0, 0])
            cylinder(d = buz_post_d, h = buz_post_h);
        // fan posts
        for (sx = [-1, 1], sz = [-1, 1])
          translate([fan_x + sx*fan_hole/2, lip - 0.01, H/2 + sz*fan_hole/2]) rotate([-90, 0, 0])
            cylinder(d = fan_post_d, h = fan_post_h);
      }
    }

    // screws through flange into the 4 columns
    for (p = col_pts()) hole_y(p[0], p[1], screw_d, -flange - 1, flange + 2);

    if (front) {
      // TFT window
      translate([win_dx - win_w/2, -flange - 1, H/2 - win_h/2]) cube([win_w, flange + lip + 2, win_h]);
      // TFT post pilots
      for (sx = [-1, 1], sz = [-1, 1])
        hole_y(mod_cx + sx*mount_dx/2, H/2 + sz*mount_dy/2, post_pilot, lip - 1, post_h + 3);
      // LEDs
      hole_y(led_x,  led_dz, led_d, -flange - 1, flange + lip + 2);
      hole_y(led_x, -led_dz, led_d, -flange - 1, flange + lip + 2);
      // wake button + rear counterbore
      hole_y(btn_x, H/2, btn_d, -flange - 1, flange + lip + 2);
      hole_y(btn_x, H/2, btn_cb_d, -flange + btn_skin, flange + lip - btn_skin + 1);
      // USB-A port hole
      translate([usb_x - usba_w/2, -flange - 1, usba_z - usba_h/2])
        cube([usba_w, flange + lip + 2, usba_h]);
      // SHT40 pocket (open to rear) + angled louvre slots through the front skin
      translate([sht_x - sht_w/2, -flange + sht_skin, H/2 + sht_z - sht_h/2])
        cube([sht_w, flange + lip - sht_skin + 1, sht_h]);
      for (i = [0:3])
        translate([sht_x, -flange + sht_skin/2, H/2 + sht_z - 3.9 + i*2.6])
          rotate([-45, 0, 0]) cube([sht_w - 4, 1.0, 6], center = true);
    } else {
      // rocker switch + rear counterbore
      hole_y(rock_x, H/2, rock_d, -flange - 1, flange + lip + 2);
      hole_y(rock_x, H/2, rock_cb_d, -flange + rock_skin, flange + lip - rock_skin + 1);
      // SAE cable hole + two zip-tie slots
      hole_y(sae_x, H/2, sae_d, -flange - 1, flange + lip + 2);
      for (s = [-1, 1])
        translate([sae_x + s*zip_dx - 0.9, -flange - 1, H/2 - 1.8]) cube([1.8, flange + lip + 2, 3.6]);
      // buzzer grill (ring pattern)
      hole_y(buz_x, H/2, 2.6, -flange - 1, flange + lip + 2);
      ring_holes(buz_x, H/2, 5.5, 6, 2.6);
      ring_holes(buz_x, H/2, 10, 12, 2.6);
      // buzzer post pilots (blind, from free end)
      for (s = [-1, 1])
        hole_y(buz_x + s*buz_ear/2, H/2, buz_pilot, lip + buz_post_h - 8, 9);
      // fan exhaust grill + post pilots
      hole_y(fan_x, H/2, 3.5, -flange - 1, flange + lip + 2);
      ring_holes(fan_x, H/2, 5.5, 6, 3.5);
      ring_holes(fan_x, H/2, 10.5, 12, 3.5);
      for (sx = [-1, 1], sz = [-1, 1])
        hole_y(fan_x + sx*fan_hole/2, H/2 + sz*fan_hole/2, fan_pilot, lip + 0.5, fan_post_h + 1);
    }
  }
}

// ---------- ghosts (assembly view only) ----------
module ghosts() {
  color("green", 0.5) translate([-board_l/2, yb, zb]) difference() {
    cube([board_l, board_w, board_t]);
    translate([usb_x - usb_mod_w/2 - 0.5 + board_l/2, -1, -1]) cube([board_l, usb_notch_len + 1, board_t + 2]);
  }
  color("black", 0.6) translate([mod_cx - mod_l/2 + 0.3, lip, (H - mod_h)/2 + 0.3])
    cube([mod_l - 0.6, mod_d, mod_h - 0.6]);
  color("royalblue", 0.7) translate([usb_x - usb_mod_w/2, lip, zb]) cube([usb_mod_w, 38, usb_pcb_t]);
}

// ---------- output ----------
if (part == "tube") tube();
else if (part == "cap_front") cap(true);
else if (part == "cap_back") cap(false);
else if (part == "print_tube") translate([0, H, 0]) rotate([90, 0, 0]) tube();
else if (part == "print_caps") {
  translate([0, H + 2, flange]) rotate([90, 0, 0]) cap(true);
  translate([0, 2*H + 10, flange]) rotate([90, 0, 0]) cap(false);
} else {
  tube();
  color("orange") cap(true);
  color("orange") translate([0, Lt, 0]) mirror([0, 1, 0]) cap(false);
  ghosts();
}
