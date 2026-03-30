#include <stdio.h>
#include <stdlib.h>
#include "agregateur.h"
#include "calculateur.h"

Agregateur agregateur_init(Panel panel)
{
    Agregateur ag;
    ag.panel = panel;
    return ag;
}

void agregateur_step(Agregateur *ag)
{
    PanelInputs  inputs  = panel_get_inputs(ag->panel);
    PanelDisplay state   = panel_get_display(ag->panel);

    CalcInput cin;
    cin.inputs   = inputs;
    cin.state_in = state;

    CalcOutput cout;
    calculateur_run(&cin, &cout);

    panel_set_display(ag->panel, &cout.state_out);
}