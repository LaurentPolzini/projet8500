#include <stdio.h>
#include <assert.h>
#include "pannel.h"

void tests_pannel(void)
{
    Panel p = panel_init();
    assert(p != NULL);

    /* Altitude valide */
    assert(panel_set_altitude_desiree(p, 10000) == 1);
    assert(panel_get_altitude_desiree(p) == 10000);
    assert(panel_get_last_error(p) == NULL);

    /* Altitude invalide (> 40000) */
    assert(panel_set_altitude_desiree(p, 50000) == 0);
    assert(panel_get_last_error(p) != NULL);

    /* Taux de montée valide */
    assert(panel_set_taux_montee(p, 500.0f) == 1);
    assert(panel_get_last_error(p) == NULL);

    /* Taux de montée invalide */
    assert(panel_set_taux_montee(p, 1000.0f) == 0);
    assert(panel_get_last_error(p) != NULL);

    /* Angle valide */
    assert(panel_set_angle(p, 5.0f) == 1);
    assert(panel_get_last_error(p) == NULL);

    /* Angle invalide */
    assert(panel_set_angle(p, 30.0f) == 0);
    assert(panel_get_last_error(p) != NULL);

    printf("testsPannel OK\n");
    panel_destroy(&p);
    return;
}