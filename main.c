#include <stdio.h>
#include "panel.h"

int main(void)
{
    panel_init();

    // test altitude valide
    if (!panel_set_altitude_desiree(35000)) {
        printf("Erreur: %s\n", panel_get_last_error());
    }

    // test altitude invalide
    if (!panel_set_altitude_desiree(50000)) {
        printf("Erreur: %s\n", panel_get_last_error());
    }

    PanelInputs inputs = panel_get_inputs();
    printf("Altitude desiree = %d ft\n", inputs.altitude_desiree_ft);

    return 0;
}
