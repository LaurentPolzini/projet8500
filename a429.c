#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "a429.h"

unsigned int sizeMot = 32;
unsigned int SSM_SDI_size = 2;
unsigned int dataSize = 18;
unsigned int labelSize = 8;

float resol_taux_montee_angle_attak = 0.1;
unsigned int taux_montee_max = 800; // 800 m/min. Ex : 799.9. 5 bits de "spare" (a droite) en BCD.
unsigned int angle_max = 16; // ex : 15.9°. 9 bits de "spare" en BCD

unsigned int getSizeMot(void) {
    return sizeMot;
}

/*
    Découpage BCD (par blocs de 4 bits, on veut pouvoir ecrire 8): 
        29:26 chiffre le plus significatif ("8" de 800)
*/

/*
    Contraintes :
        - Altitude label 001. Mot binaire (BNR) encodé sur bits [13:28]. Résol à 1.
        - Label 001 détermine aussi état de l'avion : 0=AU_SOL, 1=CH.ALT, 2=VOL_CROISIERE bits [11:12]
        - taux montée mot BCD de 4 chiffres, label 002. Résol à 0.1. (SSM 00 si montée, 11 sinon)
        - Angle attaque mot BCD de 3 chiffres, label 003. Résol à 0.1° (SSM = 00 si positif, 11 sinon)

    AFDX utilisé pour envoyer la chaine de cara qui represente l'etat du systeme

    A429 : bits
    32 (31 30)  29 (29 .. 11) (10 9) (8 .. 1)
    P  (S S M) +/- (MSB LSB)  (SDI)  (LABEL)

    SSM => Sign/Status Matrix (FW / NCD / TEST/ NORMAL). BNR, BCD, DIS ?
    SDI => Source Destination Identifier (unused, usually)
    DATA => Arinc defines BNR, BCD, DIS
    P => parité (0 si impair, 1 sinon)
    LABEL => octal label

    Label 001 => 000 000 01 => "10 000 000"
          002 => 000 000 10 => "01 000 000"
          003 => 000 000 11 => "11 000 000"
*/

// nombre impair de "1" => 1. Sinon, 0.
// J'assume que 0 "1" est impair.
uint8_t calculParite(uint8_t *word, unsigned int size) {
    uint8_t parity = 1; // odd parity

    for (unsigned int i = 0; i < size; i++) {
        parity ^= word[i];
    }

    return parity;
}

int getNbSigBits(unsigned int range, float resol) {
    return log10(range / resol) / log10(2);
}

float getResol(unsigned int range, unsigned int nbSigBits) {
    return (range / pow(2, nbSigBits));
}

int getRange(unsigned int nbSigBits, float resol) {
    return round(resol * pow(2, nbSigBits));
}


/*
    Retourne value encrypté en BNR, dans un tableau de uint8
        pas besoin de resolution si on a sigbits
*/
uint8_t *BNR_encrypt(uint8_t sigBits, uint32_t range, float value) {
    uint8_t *out = calloc(sigBits, sizeof(uint8_t));
    if (!out) exit(EXIT_FAILURE);

    int negative = value < 0;
    if (negative) value = -value;

    float resol = getResol(range, sigBits);
    int scaled = (int) round(value / resol); // conversion en entier

    for (int i = 0; i < sigBits; i++) {
        out[sigBits - 1 - i] = (scaled >> i) & 1;
    }

    // complément à 2 si négatif
    if (negative) {
        for (int i = 0; i < sigBits; i++)
            out[i] = !out[i];

        for (int i = sigBits - 1; i >= 0; i--) {
            if (out[i] == 0) {
                out[i] = 1;
                break;
            } else {
                out[i] = 0;
            }
        }
    }

    return out;
}

/*
    Fonction inverse
    Sign = negative value
*/
float BNR_decrypt(uint8_t *bits, uint8_t sigBits, uint32_t range, uint8_t sign) {
    int value = 0;

    // reconstruire l'entier
    for (int i = 0; i < sigBits; i++) {
        value = (value << 1) | bits[i];
    }

    // gestion du complément à 2 (si négatif)
    if (sign) {
        value -= (1 << sigBits);
    }

    // conversion en valeur physique
    float resol = getResol(range, sigBits);

    return value * resol;
}


/*
    Encrypte selon BinaryCodedDecimal
*/
uint8_t *BCD_encrypt(float resol, uint8_t digits, float value) {
    uint8_t *out = calloc(20, sizeof(uint8_t));  // bits 10 à 29 => 20
    if (!out) exit(EXIT_FAILURE);

    // apres application resolution
    int scaled = (int) round(fabs(value) / resol);

    // encodage de chacun des chiffres du nombre
    for (int d = digits - 1; d >= 0; --d) {
        // on elimine l'unité puis on divise par 10
        // 234 => 23 et 4 encodé
        int digit = scaled % 10;
        scaled /= 10;

        for (int b = 0; b < 4; b++) {
            out[d * 4 + (3 - b)] = (digit >> b) & 1;
        }
    }

    return out;
}

/*
    Fonction inverse
*/
float BCD_decrypt(uint8_t *bits, uint8_t digits, float resol, uint8_t *ssm) {
    int value = 0;

    for (int d = 0; d < digits; d++) {
        int digit = 0;

        for (int b = 0; b < 4; b++) {
            digit = (digit << 1) | bits[d * 4 + b];
        }

        value = value * 10 + digit;
    }
    if (ssm[0] && ssm[1]) { // ssm = 11 => negatif
        value = -value;
    }
    return value * resol;
}

void init_word(t_a429_word *w) {
    w->parite = 0;
    for (unsigned int i = 0 ; i < sizeMot ; ++i) {
        if (i < 2) {
            w->ssm[i] = 0;
            w->sdi[i] = 0;
        }
        if (i < 8) {
            w->label[i] = 0;
        }
        if (i < 18) {
            w->data[i] = 0;
        }
        w->total_word[i] = 0;
    }
}

/// @brief compose un mot arinc 429
/// @param label 1, 2 ou 3. Altitude, taux montée, angle attaque
/// @param value la valeur a encoder
/// @param etatAvionnique au_sol (0), changement_alt (1), vol_croisiere (2). ignoré si label != 1
/// @return un mot de 32 bits.
t_a429_word get_A429_word(uint8_t label, float value, int etat) {

    t_a429_word w;
    init_word(&w);

    // Positif ou negatif
    w.pn = value < 0;
    
    // SDI. Unused
    w.sdi[0] = 0;
    w.sdi[1] = 0;
    w.total_word[9] = 0;
    w.total_word[8] = 0;

    uint8_t ssm[2] = {1, 0}; // NCD par défaut
    uint8_t *encoded = NULL;

    switch (label) {
        case LABEL_ALTITUDE:
            w.total_word[7] = 1;
            w.total_word[28] = w.pn;

            if (etat >= 0) {
                w.total_word[10] = etat & 1;
                w.total_word[11] = (etat >> 1) & 1;
            }

            encoded = BNR_encrypt(16, 40000, value);
            if (value == BNR_decrypt(encoded, 16, 40000, value < 0)) {
                ssm[1] = 1; // NORM
            } else {
                ssm[0] = 0; // FAIL
            }

            // bits 11 à 28 (bit 29 = signe value)
            for (int i = 0; i < 16; i++)
                // bits 28 à 12 (11 et 10 à 0)
                w.total_word[27 - i] = encoded[i];

            break;

        case LABEL_TAUX_MONTEE:
            w.total_word[6] = 1;

            ssm[0] = (value < 0);
            ssm[1] = (value < 0);

            encoded = BCD_encrypt(0.1, 4, value);

            for (int i = 0; i < 20; i++)
                w.total_word[28 - i] = encoded[i];

            break;

        case LABEL_ANGLE_ATTAK:
            w.total_word[6] = 1;
            w.total_word[7] = 1;

            ssm[0] = (value < 0);
            ssm[1] = (value < 0);

            encoded = BCD_encrypt(0.1, 3, value);

            for (int i = 0; i < 20; i++)
                w.total_word[28 - i] = encoded[i];

            break;
    }

    // SSM
    w.ssm[0] = ssm[0];
    w.ssm[1] = ssm[1];
    w.total_word[29] = ssm[0];
    w.total_word[30] = ssm[1];

    // Data
    memcpy(w.data, encoded, 16);

    // Label
    memcpy(w.label, w.total_word, 8);
 
    // Parité
    w.parite = calculParite(w.total_word, sizeMot);
    w.total_word[31] = w.parite;

    free(encoded);

    return w;
}

uint8_t *get_total_A429word(t_a429_word *w) {
    return w->total_word;
}

int get_true_label(t_a429_word word) {
    int result = 0;
    for (int i = 0 ; i < 8 ; ++i) {
        result = result * 2 + word.label[i]; // transformation binaire.
    }
    return result;
}

float get_value_from_a429(t_a429_word w) {
    switch (get_true_label(w)) {
        case LABEL_ALTITUDE:
            return (int) round(BNR_decrypt(w.data, 16, 40000, w.pn));

        case LABEL_TAUX_MONTEE:
            return ((int) (BCD_decrypt(w.data, 4, 0.1, w.ssm) * 10)) / 10.0; // on garde le dizieme

        case LABEL_ANGLE_ATTAK:
            return ((int) (BCD_decrypt(w.data, 3, 0.1, w.ssm) * 10)) / 10.0;

        default:
            return 0;
    }
}

// Affiche un mot de size bits.
// si size = 0, alors 32 bits.
void afficheMot(uint8_t *word, int size)
{
    int s = (size == 0) ? 32 : size;

    for (int i = s - 1 ; i >= 0; --i) {
        printf("%d", word[i]);

        // espace visuel toutes les 4 bits
        if (i % 4 == 0)
            printf(" ");
        
        fflush(stdout);
    }
    printf("\n");
}

void afficheA429_word(t_a429_word word) {
    printf("\n=== A429 WORD ===\n");

    printf("Parité : %d\n", word.parite);

    printf("SSM : ");
    afficheMot(word.ssm, 2);

    printf("Negatif : %d\n", word.pn);

    printf("DATA (%s) : ", get_true_label(word) == LABEL_ALTITUDE ? "BNR" : "BCD");
    afficheMot(word.data, 18);

    printf("SDI : ");
    afficheMot(word.sdi, 2);

    printf("LABEL : ");
    afficheMot(word.label, 8);

    printf("FULL WORD : ");
    afficheMot(word.total_word, 0);

    printf("=================\n");

    float val = get_value_from_a429(word);
    switch (get_true_label(word))
    {
    case LABEL_ALTITUDE:
        printf("\t => Altitude : %d pieds\n", (int) val);
        break;
    case LABEL_TAUX_MONTEE:
        printf("\t => Taux de montée : %.1f m/min\n", val);
        break;
    case LABEL_ANGLE_ATTAK:
        printf("\t => Angle d'attaque : %.1f°\n", val);
        break;
    }
    printf("\n");
}

t_a429_word word_from_a429_frame(uint8_t *frame) {
    t_a429_word w;
    init_word(&w);

    // 1. Copier le mot complet
    for (int i = 0; i < 32; i++) {
        w.total_word[i] = frame[i];
    }

    // 2. Parité (bit 32)
    w.parite = frame[31];

    // 3. SSM (bits 31:30)
    w.ssm[0] = frame[29];
    w.ssm[1] = frame[30];

    // 4. Signe (bit 29)
    w.pn = frame[28];

    // 5. DATA (bits 28 à 11)
    for (int i = 0; i < 16; i++) {
        w.data[i] = frame[27 - i];
    }

    // 6. SDI (bits 10:9)
    w.sdi[0] = frame[8];
    w.sdi[1] = frame[9];

    // 7. LABEL (bits 8:1)
    for (int i = 0; i < 8; i++) {
        w.label[i] = frame[i];
    }

    return w;
}

