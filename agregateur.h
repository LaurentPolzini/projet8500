#ifndef __AGREGATEUR_H__
#define __AGREGATEUR_H__

#include "pannel.h"

int agregateur_set_altitude_desiree(Panel panel, int altitude_ft);

int agregateur_set_taux_monte(Panel panel, double taux_mpm);

int agregateur_set_angle(Panel panel, double angle_deg);


#endif
