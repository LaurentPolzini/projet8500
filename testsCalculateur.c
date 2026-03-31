#include <stdio.h>
#include <assert.h>
#include "pannel.h"
#include "calculateur.h"

static PanelDisplay make_display(int altitude_ft, PanelMode mode)
{
    PanelDisplay d;
    d.altitude_ft   = altitude_ft;
    d.vitesse_mpm   = 0.0f;
    d.puissance_pct = 0.0f;
    d.mode          = mode;
    return d;
}

void tests_calculateur(void)
{
    /* Cas 1 : AU_SOL, altitude désirée > 0 => doit monter. */
    PanelDisplay st1 = make_display(0, MODE_AU_SOL);
    PanelInputs  in1 = { .altitude_desiree_ft = 5000,
                         .taux_montee_mpm = 0.0f,
                         .angle_deg = 0.0f };

    CalcInput c1 = { .inputs = in1, .state_in = st1 };
    CalcOutput o1;
    calculateur_run(&c1, &o1);

    assert(o1.state_out.altitude_ft >= 0);
    assert(o1.state_out.mode == MODE_CHANGEMENT_ALT ||
           o1.state_out.mode == MODE_VOL_CROISIERE);

    /* Cas 2 : déjà à l'altitude désirée => VOL_CROISIERE, taux nul. */
    PanelDisplay st2 = make_display(10000, MODE_CHANGEMENT_ALT);
    PanelInputs  in2 = { .altitude_desiree_ft = 10000,
                         .taux_montee_mpm = 0.0f,
                         .angle_deg = 0.0f };

    CalcInput c2 = { .inputs = in2, .state_in = st2 };
    CalcOutput o2;
    calculateur_run(&c2, &o2);

    assert(o2.state_out.mode == MODE_VOL_CROISIERE);
    assert(fabsf(o2.state_out.vitesse_mpm) < 0.01f);

    /* Cas 3 : clamp sur altitude max 40000. */
    PanelDisplay st3 = make_display(39500, MODE_CHANGEMENT_ALT);
    PanelInputs  in3 = { .altitude_desiree_ft = 45000,
                         .taux_montee_mpm = 0.0f,
                         .angle_deg = 0.0f };

    CalcInput c3 = { .inputs = in3, .state_in = st3 };
    CalcOutput o3;
    calculateur_run(&c3, &o3);

    assert(o3.state_out.altitude_ft <= 40000);

    printf("testsCalculateur OK\n");
    return;
}