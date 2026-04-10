#ifndef __AGREGATEUR_H__
#define __AGREGATEUR_H__

#include "pannel.h"

typedef struct {
    Panel panel;
} Agregateur;

Agregateur agregateur_init(Panel panel);
void agregateur_step(Agregateur *ag);

#endif
