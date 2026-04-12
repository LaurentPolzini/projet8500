#ifndef __CALCULATEUR_H__
#define __CALCULATEUR_H__

#include "pannel.h"

/* Entrée du calculateur : ce que l'agrégateur/l'appli lui fournit */
typedef struct {
    PanelInputs  inputs;    // altitude désirée, taux cmd, angle cmd
    PanelDisplay state_in;  // état courant affiché
} CalcInput;

/* Sortie du calculateur : nouvel état du système à afficher */
typedef struct {
    PanelDisplay state_out;
} CalcOutput;

/* Algorithme principal */
void calculateur_run(const CalcInput *in, CalcOutput *out);

PanelInputs get_calc_inputs(CalcInput ci);

PanelDisplay get_calc_state_in(CalcInput ci);

PanelDisplay get_calc_state_out(CalcOutput co);

#endif
