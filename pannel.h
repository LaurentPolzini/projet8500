#ifndef __PANNEL_H__

/* Modes de la boîte avionique */
typedef enum {
    MODE_AU_SOL = 0,
    MODE_CHANGEMENT_ALT = 1,
    MODE_VOL_CROISIERE = 2
} PanelMode;

/* Etat affiché sur le panneau (ce que voit le pilote) */
typedef struct {
    int    altitude_ft;        // altitude actuelle [0 .. 40000]
    double vitesse_mpm;        // ici = taux de montée (m/min)
    double puissance_pct;      // 0..100 % (à ajuster si tu veux autre chose)
    PanelMode mode;            // AU_SOL / CHANGEMENT_ALT / VOL_CROISIERE
} PanelDisplay;

/* Entrées utilisateur (ce que le pilote tape) */
typedef struct {
    int    altitude_desiree_ft;   // [0 .. 40000]
    double taux_monte_mpm;        // [-800 .. 800] par ex.
    double angle_deg;             // [-16 .. 16]
} PanelInputs;

/* Initialisation */
void panel_init(void);

/* Accès aux affichages */
PanelDisplay panel_get_display(void);
void panel_set_display(const PanelDisplay *display);

/* Accès aux entrées utilisateur (pour l’agrégateur / calculateur) */
PanelInputs panel_get_inputs(void);

/* Saisie utilisateur avec validation + message d'erreur */
int panel_set_altitude_desiree(int altitude_ft);
int panel_set_taux_monte(double taux_mpm);
int panel_set_angle(double angle_deg);

/* Gestion des erreurs (NULL si pas d’erreur) */
const char *panel_get_last_error(void);

#endif
