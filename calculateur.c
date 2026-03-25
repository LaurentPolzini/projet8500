#include <stdio.h>
#include <float.h>
#include "pannel.h"

int TAUX_MONTEE_MAX = 800; // max 800m/min
int K = 10; // constante de calcul de vitesse.

/*
    montee = 1, descente => montee = 0
*/
int get_angle_attaque(int montee, int puissance_moteur) {
    return 0;
}

int get_vitesse(float angle, int altitude, int puissance_moteur) {
    return sin(angle) * altitude * puissance_moteur * K;
}

float get_taux_montee(int mode, int puissance_moteur, int altitude_desiree, int altitude_current) {
    if (mode == MODE_VOL_CROISIERE || mode == MODE_AU_SOL) {
        return 0;
    } else if (mode == MODE_CHANGEMENT_ALT) {
        float taux_montee = 100 * (puissance_moteur / 10); // 100m/min pour 10% puissance moteur
        // gestion transition lorsqu'approche de l'altitude desiree
        int diff = altitude_desiree - altitude_current;
        int distance_transition = 1000; // a moins de 1km de l'altitude desiree, on transitionne
        float facteur = (float)abs(diff) / distance_transition;

        if (facteur > 1) facteur = 1; // plus ou moins proche de la cible.
        // facteur entre 0 et 1.
        facteur = facteur * facteur; // transition plus smooth

        float taux_final = taux_montee * facteur;
        if (taux_final > TAUX_MONTEE_MAX) taux_final = TAUX_MONTEE_MAX;

        if (diff < 0) taux_final = -taux_final; // descente ou montée

        return taux_final;
    }
}

int get_puissance_moteur(void) {

}

#include "calculateur.h"

void calculateur_run(const CalcInput *in, CalcOutput *out)
{
    out->state_out = in->state_in;

    if (in->inputs.altitude_desiree_ft > in->state_in.altitude_ft) {
        out->state_out.altitude_ft += 100;
        out->state_out.mode = MODE_CHANGEMENT_ALT;
    } else if (in->inputs.altitude_desiree_ft < in->state_in.altitude_ft) {
        out->state_out.altitude_ft -= 100;  // descend
        out->state_out.mode = MODE_CHANGEMENT_ALT;
    } else {
        out->state_out.mode = MODE_VOL_CROISIERE;
        out->state_out.vitesse_mpm = 0.0;
    }
}
