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
        // ---- Agregateur vers Calculateur
        printf("Envoie des données AFDX de l'agregateur vers le calculateur\n");
        PanelInputs in_vals  = panel_get_inputs(panel);
        PanelDisplay st_vals = panel_get_display(panel);

        t_a429_word w_alt_desiree = get_A429_word(LABEL_ALTITUDE, in_vals.altitude_desiree_ft, st_vals.mode);
        t_a429_word w_taux_desire = get_A429_word(LABEL_TAUX_MONTEE, in_vals.taux_montee_mpm, 0); // etat ignoré car pas LABEL_ALT
        t_a429_word w_angle_desire = get_A429_word(LABEL_ANGLE_ATTAK, in_vals.angle_deg, 0);

        t_afdx w_afdx_agr_to_calc;
        init_afdx(&w_afdx_agr_to_calc);
        build_afdx_frame(&w_afdx_agr_to_calc, get_total_A429word(&w_alt_desiree), getSizeMot(), NO, adr_mac_agreg, adr_mac_calc);
        //afficheA429_word(word_from_a429_frame(w_afdx_agr_to_calc.payload));

        update_seq(&w_afdx_agr_to_calc);
        build_afdx_frame(&w_afdx_agr_to_calc, get_total_A429word(&w_taux_desire), getSizeMot(), NO, adr_mac_agreg, adr_mac_calc);
        //afficheA429_word(word_from_a429_frame(w_afdx_agr_to_calc.payload));

        update_seq(&w_afdx_agr_to_calc);
        build_afdx_frame(&w_afdx_agr_to_calc, get_total_A429word(&w_angle_desire), getSizeMot(), NO, adr_mac_agreg, adr_mac_calc);
        //afficheA429_word(word_from_a429_frame(w_afdx_agr_to_calc.payload));

        CalcInput cin;
        cin.inputs   = in_vals;
        cin.state_in = st_vals;

        CalcOutput cout;
        calculateur_run(&cin, &cout);
        PanelDisplay panelOut = get_calc_state_out(cout);

        // ---- Calculateur vers Agregateur
        // traduction en a429
        printf("Envoie des données AFDX du calculateur vers l'agregateur\n");
        t_a429_word w_alt = get_A429_word(LABEL_ALTITUDE, panelOut.altitude_ft, panelOut.mode);
        t_a429_word w_vz  = get_A429_word(LABEL_TAUX_MONTEE, panelOut.vitesse_mpm, 1);

        // a429 stockés en AFDX et affichage
        t_afdx w_afdx_calc_to_agreg;
        init_afdx(&w_afdx_calc_to_agreg);
        build_afdx_frame(&w_afdx_calc_to_agreg, get_total_A429word(&w_alt), getSizeMot(), NO, adr_mac_calc, adr_mac_agreg);
        afficheA429_word(word_from_a429_frame(w_afdx_calc_to_agreg.payload));

        update_seq(&w_afdx_calc_to_agreg);
        build_afdx_frame(&w_afdx_calc_to_agreg, get_total_A429word(&w_vz), getSizeMot(), NO, adr_mac_calc, adr_mac_agreg);
        afficheA429_word(word_from_a429_frame(w_afdx_calc_to_agreg.payload));

        update_seq(&w_afdx_calc_to_agreg);
        
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
