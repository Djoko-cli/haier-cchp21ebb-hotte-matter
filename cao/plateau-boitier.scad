// Plateau du boitier de mesure (projet hotte Haier) : bac a rebord et clips, en PETG.
// Conception : docs/superpowers/specs/2026-11-04-plateau-boitier-design.md
//
// Reperes : origine au coin bas-gauche de la carte vue cote composants (X1 au bord gauche,
// USB-C du module vers le haut a droite). La carte occupe [0, W] x [0, H].
//
// Exports (sortie dans cao/sortie/, ignore par git) :
//   openscad -D 'piece="plateau"' -o cao/sortie/plateau-boitier.stl cao/plateau-boitier.scad
//   openscad -D 'piece="essai"'   -o cao/sortie/plateau-essai.stl   cao/plateau-boitier.scad

piece = "plateau";      // "plateau" ou "essai"

// --- carte (mesuree le 04/11, sauf l'epaisseur) ---
W = 69.5;               // largeur
H = 49.5;               // hauteur
e_carte = 1.6;          // epaisseur : SUPPOSEE, a verifier avec la piece d'essai
trou_d = 2.0;           // diametre des trous de coin
trou_m = 1.85;          // centre des trous a trou_m des deux bords
relief = 2.1;           // plus haut relief cote soudures

// --- plateau ---
jeu = 0.3;              // jeu entre la carte et le rebord
paroi = 2.0;            // epaisseur du rebord
fond = 2.0;             // epaisseur du fond
vide = 4.5;             // hauteur libre sous la carte (relief + jeu)
depasse = 0.8;          // le rebord depasse la face composants de tant
plot_d = 3.6;           // plots sous les trous de coin
teton_d = 1.7;          // tetons de centrage (moins que trou_d)
teton_h = 1.2;          // moins que e_carte : ils centrent, ils ne tiennent pas

// --- clips (milieu des deux grands cotes) ---
clip_l = 12;            // largeur de la languette
clip_ep = 1.2;          // epaisseur de la languette (plus fine que le rebord : elle plie)
fente = 0.8;            // fentes de part et d'autre
levre = 0.6;            // recouvrement du bord de la carte (0,6 : la languette plie de 0,9 mm, environ 3 % en PETG)
levre_jeu = 0.1;        // jeu vertical sous la levre

// --- pattes a collier de serrage ---
patte_l = 12;           // longueur le long du rebord
patte_p = 10;           // saillie vers l'exterieur
collier_l = 4.2;        // fente : largeur du collier + jeu
collier_w = 1.8;        // fente : epaisseur du collier + jeu
collier_ecart = 6;      // entre les deux fentes d'une patte
patte_xh_y = 27.3;      // en face de X1 (rangees 09 a 12)
patte_usb_x = 56;       // sous l'USB-C du module (colonnes R a X)

// --- patins adhesifs (desactives : pas de patins le 04/11) ---
patins = false;
patin_d = 10.5;
patin_p = 0.8;

$fn = 48;
eps = 0.01;

z_carte = fond + vide;                  // dessous de la carte
h_rebord = vide + e_carte + depasse;    // au-dessus du fond
z_haut = fond + h_rebord;
x0 = -jeu; y0 = -jeu; x1 = W + jeu; y1 = H + jeu;   // interieur du bac

trous = [[trou_m, trou_m], [W - trou_m, trou_m], [trou_m, H - trou_m], [W - trou_m, H - trou_m]];

module bac() {
    difference() {
        translate([x0 - paroi, y0 - paroi, 0])
            cube([x1 - x0 + 2 * paroi, y1 - y0 + 2 * paroi, z_haut]);
        translate([x0, y0, fond]) cube([x1 - x0, y1 - y0, h_rebord + eps]);
        // fentes des clips, jusqu'au fond
        for (cote = [0, 1]) for (s = [-1, 1]) {
            yy = cote == 0 ? y0 - paroi - eps : y1 - eps;
            translate([W / 2 + s * (clip_l / 2 + fente / 2) - fente / 2, yy, fond])
                cube([fente, paroi + 2 * eps, h_rebord + eps]);
        }
        // languettes amincies : on retire l'exterieur du rebord entre les fentes
        for (cote = [0, 1]) {
            yy = cote == 0 ? y0 - paroi - eps : y1 + clip_ep;
            translate([W / 2 - clip_l / 2, yy, fond])
                cube([clip_l, paroi - clip_ep + eps, h_rebord + eps]);
        }
        if (patins) for (p = trous)
            translate([p[0], p[1], -eps]) cylinder(d = patin_d, h = patin_p + eps);
    }
}

// levre d'un clip, cote interieur ; cote = 0 en bas (y0), 1 en haut (y1)
module levre_clip(cote) {
    z0 = z_carte + e_carte + levre_jeu;
    avance = jeu + levre;
    profil = [[0, z0], [avance, z0], [avance, z0 + 0.3], [0, z0 + 0.3 + avance]];
    h_lang = z0 + 0.3 + avance;
    // la languette monte jusqu'au haut de la levre
    translate([W / 2 - clip_l / 2, cote == 0 ? y0 - clip_ep : y1, fond])
        cube([clip_l, clip_ep, h_lang - fond]);
    translate([W / 2 - clip_l / 2, 0, 0])
        if (cote == 0)
            translate([0, y0, 0]) rotate([90, 0, 90])
                linear_extrude(clip_l) polygon(profil);
        else
            translate([0, y1, 0]) mirror([0, 1, 0]) rotate([90, 0, 90])
                linear_extrude(clip_l) polygon(profil);
}

module plots() {
    for (p = trous) translate([p[0], p[1], fond - eps]) {
        cylinder(d = plot_d, h = vide + eps);
        translate([0, 0, vide]) cylinder(d = teton_d, h = teton_h + eps);
        translate([0, 0, vide + teton_h]) cylinder(d1 = teton_d, d2 = teton_d - 0.6, h = 0.3);
    }
}

// patte plate prolongeant le fond, percee de deux fentes pour un collier
module patte(centre, dir) {   // dir : [0,1] vers le haut, [-1,0] vers la gauche
    vertical = dir[0] == 0;
    difference() {
        if (vertical)
            translate([centre - patte_l / 2, y1 + paroi - eps, 0]) cube([patte_l, patte_p, fond]);
        else
            translate([x0 - paroi - patte_p + eps, centre - patte_l / 2, 0]) cube([patte_p, patte_l, fond]);
        for (s = [-1, 1])
            if (vertical)
                translate([centre + s * collier_ecart / 2 - collier_w / 2,
                           y1 + paroi + patte_p / 2 - collier_l / 2, -eps])
                    cube([collier_w, collier_l, fond + 2 * eps]);
            else
                translate([x0 - paroi - patte_p / 2 - collier_l / 2,
                           centre + s * collier_ecart / 2 - collier_w / 2, -eps])
                    cube([collier_l, collier_w, fond + 2 * eps]);
    }
}

module plateau() {
    bac();
    plots();
    levre_clip(0);
    levre_clip(1);
    patte(patte_xh_y, [-1, 0]);
    patte(patte_usb_x, [0, 1]);
}

// piece d'essai : le coin bas-gauche (plot, teton, rebord) et le clip du bas
module essai() {
    intersection() {
        plateau();
        translate([x0 - paroi - eps, y0 - paroi - eps, -eps]) cube([14, 14, 20]);
    }
    translate([-W / 2 + 25, 0, 0]) intersection() {
        plateau();
        translate([W / 2 - clip_l / 2 - 6, y0 - paroi - eps, -eps]) cube([clip_l + 12, 12, 20]);
    }
}

if (piece == "essai") essai(); else plateau();
