#include <stdio.h>
#include "pannel.h"

int main(void)
{
    Panel pan = panel_init();

    // test altitude valide
    if (!panel_set_altitude_desiree(pan, 35000)) {
        printf("Erreur (35000): %s\n", panel_get_last_error(pan));
    }

    // test altitude invalide
    if (!panel_set_altitude_desiree(pan, 50000)) {
        printf("Erreur (50000): %s\n", panel_get_last_error(pan));
    }

    PanelInputs inputs = panel_get_inputs(pan);
    printf("Altitude desiree = %d ft\n", inputs.altitude_desiree_ft);

    return 0;
}
