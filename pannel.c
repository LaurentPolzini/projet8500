#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "float.h"
#include "pannel.h"

struct s_Panel {
    PanelDisplay g_display;
    PanelInputs  g_inputs;

    char         g_last_error[128];
};


static void panel_set_error(Panel panel, const char *msg) {
    if (msg == NULL) {
        (panel->g_last_error)[0] = '\0';
    } else {
        strncpy(panel->g_last_error, msg, sizeof(panel->g_last_error) - 1);
        panel->g_last_error[sizeof(panel->g_last_error) - 1] = '\0';
    }
}

/*
    Reset values
*/
void resetDisplay(Panel panel) {
    panel->g_display.altitude_ft = 0;
    panel->g_display.puissance_pct = 0;
    panel->g_display.vitesse_mpm = 0;
    panel->g_display.mode = MODE_AU_SOL;
}

void resetInputs(Panel panel) {
    panel->g_inputs.altitude_desiree_ft = 0;
    panel->g_inputs.angle_deg = 0;
    panel->g_inputs.taux_montee_mpm = 0;
}

void resetPanel(Panel panel) {
    resetDisplay(panel);
    resetInputs(panel);
    (panel->g_last_error)[0] = '\0';
}

Panel panel_init(void) {
    Panel pan = malloc(sizeof(struct s_Panel));

    pan->g_display.mode = MODE_AU_SOL;

    resetPanel(pan); // set values to 0

    panel_set_error(pan, NULL);

    return pan;
}

void panel_destroy(Panel *pan) {
    resetPanel(*pan); // au cas ou ce soit toujours accessible
    free(*pan);
    pan = NULL; // safety belt
}

/* --- Etat affiché --- */

PanelDisplay panel_get_display(Panel pan)
{
    return pan->g_display;
}

void panel_set_display(Panel pan, const PanelDisplay *display)
{
    if (!display) return;
    pan->g_display = *display;
}

/* --- Entrées utilisateur --- */

PanelInputs panel_get_inputs(Panel pan)
{
    return pan->g_inputs;
}

const char *panel_get_last_error(Panel pan)
{
    return (pan->g_last_error)[0] ? pan->g_last_error : NULL;
}


/* --- Fonctions de saisie avec validation --- */

int panel_set_altitude_desiree(Panel pan, int altitude_ft) 
{
    if (altitude_ft < 0 || altitude_ft > 40000) {
        panel_set_error(pan, "Altitude desiree hors [0, 40000] ft");
        return 0;
    }
    (pan->g_inputs).altitude_desiree_ft = altitude_ft;
    pan->g_display.mode = MODE_CHANGEMENT_ALT; // mode changement d'altitude
    panel_set_error(pan, NULL);
    return 1;
}

int panel_set_taux_montee(Panel pan, float taux_mpm) 
{
    if (taux_mpm < -800.0 || taux_mpm > 800.0) {
        panel_set_error(pan, "Taux de montee hors [-800, 800] m/min");
        return 0;
    }
    (pan->g_inputs).taux_montee_mpm = taux_mpm;
    panel_set_error(pan, NULL);
    return 1;
}

int panel_set_angle(Panel pan, float angle_deg)
{
    if (angle_deg < -16.0 || angle_deg > 16.0) {
        panel_set_error(pan, "Angle d'attaque hors [-16, 16] deg");
        return 0;
    }
    (pan->g_inputs).angle_deg = angle_deg;
    panel_set_error(pan, NULL);
    return 1;
}

int panel_get_altitude_desiree(Panel pan) {
    return (pan->g_inputs).altitude_desiree_ft;
}

float panel_get_taux_monte(Panel pan) {
    return (pan->g_inputs).taux_montee_mpm;
}

float panel_get_angle(Panel pan) {
    return (pan->g_inputs).angle_deg;
}

