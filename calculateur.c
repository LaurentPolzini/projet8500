#include <stdio.h>

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
