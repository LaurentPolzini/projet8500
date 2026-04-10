#include <stdio.h>
#include <math.h>    // sinf, lroundf, etc.
#include <stdlib.h>  // abs
#include <float.h>
#include "pannel.h"
#include "calculateur.h"

int TAUX_MONTEE_MAX = 800; // max 800m/min
int K = 10; // constante de calcul de vitesse.

/*
    montee = 1, descente => montee = 0
*/
int get_angle_attaque(int montee, int puissance_moteur) {
    (void)montee;
    (void)puissance_moteur;

    return 10; 
}

float get_vitesse(float angle_deg, int altitude, int puissance_moteur)
{
    float angle_rad = angle_deg * 3.14159265f / 180.0f;
    return sinf(angle_rad) * (float)altitude * (float)puissance_moteur * (float)K;
}
float get_taux_montee(int mode, int puissance_moteur, int altitude_desiree, int altitude_current) {
    if (mode == MODE_VOL_CROISIERE || mode == MODE_AU_SOL) {
        return 0.0f;
    } else if (mode == MODE_CHANGEMENT_ALT) {
        float taux_montee = 100.0f * (puissance_moteur / 10.0f); // 100m/min pour 10% puissance moteur
        // gestion transition lorsqu'approche de l'altitude desiree
    
        int distance_transition = 1000; // a moins de 1km de l'altitude desiree, on transitionne
        int diff = altitude_desiree - altitude_current;
        float facteur = (float)abs(diff) / (float)distance_transition;

        if (facteur > 1.0f) facteur = 1.0f; // plus ou moins proche de la cible.
        // facteur entre 0 et 1.
        facteur = facteur * facteur; // transition plus smooth

        float taux_final = taux_montee * facteur;
        if (taux_final > TAUX_MONTEE_MAX) taux_final = TAUX_MONTEE_MAX;

        if (diff < 0) taux_final = -taux_final; // descente ou montée

        return taux_final;
    }
    return 0.0f;
}

int get_puissance_moteur(void) {
    return 50;
}

void calculateur_run(const CalcInput *in, CalcOutput *out)
{
    if (!in || !out) return;

    /* Point de départ : état courant */
    out->state_out = in->state_in;

    int current_alt = in->state_in.altitude_ft;
    int target_alt  = in->inputs.altitude_desiree_ft;

    /* Puissance moteur : si 0, on prend une valeur nominale (ex: 50 %) */
    int puissance = (int)in->state_in.puissance_pct;
    if (puissance <= 0) {
        puissance = get_puissance_moteur();  // par ex. retourner 50
    }
    out->state_out.puissance_pct = (float)puissance;

    /* Détermination du mode suivant la spec. [file:2] */
    PanelMode mode;
    if (current_alt == 0 && target_alt == 0) {
        mode = MODE_AU_SOL;
    } else if (current_alt == target_alt || current_alt >= 40000) {
        mode = MODE_VOL_CROISIERE;
    } else {
        mode = MODE_CHANGEMENT_ALT;
    }
    out->state_out.mode = mode;

    /* Taux de montée (m/min) en fonction du mode, puissance, etc. [file:36] */
    float taux_m = get_taux_montee(mode, puissance, target_alt, current_alt);
    out->state_out.vitesse_mpm = taux_m;

    /* Mise à jour de l'altitude : on suppose 1 minute par appel, 
       avec conversion approx m -> ft (1 m ≈ 3.28 ft). [file:2] */
    const float M_TO_FT = 3.28f;
    float dt_min = 10.0f / 60.0f;   // 10 secondes = 1/6 minute
    float delta_ft = taux_m * dt_min * M_TO_FT;
    int new_alt = current_alt + (int)lroundf(delta_ft);

    /* Clamps d'altitude suivant la spec : [0, 40000] ft. [file:2] */
    if (new_alt < 0)      new_alt = 0;
    if (new_alt > 40000)  new_alt = 40000;

    /* Si on est très proche de l'altitude désirée, on “snap” et on passe croisière. */
    if (abs(target_alt - new_alt) < 50) {
        new_alt = target_alt;
        out->state_out.mode = MODE_VOL_CROISIERE;
        out->state_out.vitesse_mpm = 0.0f;
    }

    out->state_out.altitude_ft = new_alt;
}