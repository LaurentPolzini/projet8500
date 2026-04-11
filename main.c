#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pannel.h"
#include "calculateur.h"
#include "a429.h"
#include "afdx.h"

/* Prototypes des fonctions de tests */
void tests_pannel(void);
void tests_calculateur(void);
void tests_a429(void);
void tests_afdx(void);

static void run_simulation(void)
{
    Panel panel = panel_init();
    if (!panel) {
        fprintf(stderr, "Erreur: panel_init a echoue\n");
        return;
    }

    /* Etat initial : 0 ft, AU_SOL. */
    PanelDisplay disp = panel_get_display(panel);
    disp.altitude_ft   = 0;
    disp.vitesse_mpm   = 0.0f;
    disp.puissance_pct = 0.0f;
    disp.mode          = MODE_AU_SOL;
    panel_set_display(panel, &disp);

    /* Commande utilisateur : monter à 10000 ft. */
    int altitude_target = 10000;
    if (!panel_set_altitude_desiree(panel, altitude_target)) {
        fprintf(stderr, "Erreur de saisie altitude: %s\n",
                panel_get_last_error(panel));
        panel_destroy(&panel);
        return;
    }

    printf("Simulation vers %d ft\n", altitude_target);

    for (int step = 0; step < 100; ++step) {
        PanelInputs in_vals  = panel_get_inputs(panel);
        PanelDisplay st_vals = panel_get_display(panel);

        CalcInput cin;
        cin.inputs   = in_vals;
        cin.state_in = st_vals;

        CalcOutput cout;
        calculateur_run(&cin, &cout);
        panel_set_display(panel, &cout.state_out);

        PanelDisplay outd = panel_get_display(panel);
        printf("t=%3d min : alt=%6d ft, taux=%7.1f m/min, mode=%d\n",
               step,
               outd.altitude_ft,
               outd.vitesse_mpm,
               outd.mode);

        if (outd.mode == MODE_VOL_CROISIERE ||
            outd.altitude_ft == 0 ||
            outd.altitude_ft == 40000) {
            break;
        }
    }

    panel_destroy(&panel);
}

int main(int argc, char *argv[])
{
    if (argc > 1 && strcmp(argv[1], "tests") == 0) {
        tests_pannel();
        tests_calculateur();
        tests_a429();
        tests_afdx();
        return 0;
    }

    run_simulation();
    return 0;
}
