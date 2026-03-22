#include <stdio.h>
#include "panel.h"
#include <string.h>

static PanelDisplay g_display;
static PanelInputs  g_inputs;
static char         g_last_error[128];

static void panel_set_error(const char *msg)
{
    if (msg == NULL) {
        g_last_error[0] = '\0';
    } else {
        strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
        g_last_error[sizeof(g_last_error) - 1] = '\0';
    }
}

void panel_init(void)
{
    memset(&g_display, 0, sizeof(g_display));
    memset(&g_inputs, 0, sizeof(g_inputs));
    g_display.mode = MODE_AU_SOL;
    panel_set_error(NULL);
}

/* --- Etat affiché --- */

PanelDisplay panel_get_display(void)
{
    return g_display;  // retour par valeur, simple à utiliser
}

void panel_set_display(const PanelDisplay *display)
{
    if (!display) return;
    g_display = *display;
}

/* --- Entrées utilisateur --- */

PanelInputs panel_get_inputs(void)
{
    return g_inputs;
}

const char *panel_get_last_error(void)
{
    return g_last_error[0] ? g_last_error : NULL;
}

/* --- Fonctions de saisie avec validation --- */

bool panel_set_altitude_desiree(int altitude_ft)
{
    if (altitude_ft < 0 || altitude_ft > 40000) {
        panel_set_error("Altitude desiree hors [0, 40000] ft");
        return false;
    }
    g_inputs.altitude_desiree_ft = altitude_ft;
    panel_set_error(NULL);
    return true;
}

bool panel_set_taux_monte(double taux_mpm)
{
    if (taux_mpm < -800.0 || taux_mpm > 800.0) {
        panel_set_error("Taux de montee hors [-800, 800] m/min");
        return false;
    }
    g_inputs.taux_monte_mpm = taux_mpm;
    panel_set_error(NULL);
    return true;
}

bool panel_set_angle(double angle_deg)
{
    if (angle_deg < -16.0 || angle_deg > 16.0) {
        panel_set_error("Angle d'attaque hors [-16, 16] deg");
        return false;
    }
    g_inputs.angle_deg = angle_deg;
    panel_set_error(NULL);
    return true;
}
