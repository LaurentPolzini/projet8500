#ifndef __PANNEL_H__
#define __PANNEL_H__

/* Modes de la boîte avionique */
typedef enum {
    MODE_AU_SOL = 0,
    MODE_CHANGEMENT_ALT = 1,
    MODE_VOL_CROISIERE = 2
} PanelMode;

/* Etat affiché sur le panneau (ce que voit le pilote) */
typedef struct {
    int    altitude_ft;        // altitude actuelle [0 .. 40000]
    float vitesse_mpm;        // ici = taux de montée (m/min). Résolution à 0.1
    float puissance_pct;      // 0..100 % (à ajuster si tu veux autre chose). Résol : 0.1
    PanelMode mode;            // AU_SOL / CHANGEMENT_ALT / VOL_CROISIERE
} PanelDisplay;

/* Entrées utilisateur (ce que le pilote tape) */
typedef struct {
    int   altitude_desiree_ft;   // [0 .. 40000]. Prévaut sur les autres valeurs car elle les détermine.
    float taux_montee_mpm;        // [-800 .. 800] par ex.
    float angle_deg;             // [-16 .. 16]
} PanelInputs;

typedef struct s_Panel *Panel;

/* Initialisation */
Panel panel_init(void);

// Destruction
void panel_destroy(Panel *pan);

// Reset des valeurs
void resetPanel(Panel panel);

/* Accès aux affichages */
PanelDisplay panel_get_display(Panel pan);
void panel_set_display(Panel pan, const PanelDisplay *display);

/* Accès aux entrées utilisateur (pour l’agrégateur / calculateur) */
PanelInputs panel_get_inputs(Panel pan);

/* Saisie utilisateur avec validation + message d'erreur */
// des que valeur entrée, elle est passée a l'agregateur.
int panel_set_altitude_desiree(Panel pan, int altitude_ft);
int panel_set_taux_montee(Panel pan, float taux_mpm);
int panel_set_angle(Panel pan, float angle_deg);

int panel_get_altitude_desiree(Panel pan);
float panel_get_taux_montee(Panel pan);
float panel_get_angle(Panel pan);

/* Gestion des erreurs (NULL si pas d’erreur) */
const char *panel_get_last_error(Panel pan);

#endif
