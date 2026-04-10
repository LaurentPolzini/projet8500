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
uint8_t calculParite(uint8_t *word) {
    uint8_t parity = 1; // odd parity

    for (int i = 0; i < 32; i++) {
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
    return (resol * pow(2, nbSigBits));
}


/*
    Retourne number_to_encrypt encrypté en BNR, dans un tableau de unsigned int
*/
uint8_t *BNR_encrypt(float resol, uint8_t sigBits, uint32_t range, float value) {
    uint8_t *out = calloc(sigBits, sizeof(uint8_t));
    if (!out) exit(EXIT_FAILURE);

    int negative = value < 0;
    if (negative) value = -value;

    float weight = range / 2.0;

    for (int i = 0; i < sigBits; i++) {
        if (value >= weight) {
            out[i] = 1;
            value -= weight;
        }
        weight /= 2.0;
    }

    if (negative) {
        // complément à 2
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
*/
float BNR_decrypt(uint8_t *bits, uint8_t sigBits, uint32_t range) {
    uint8_t temp[sigBits];

    // copie
    for (int i = 0; i < sigBits; i++)
        temp[i] = bits[i];

    int negative = temp[0]; // MSB

    if (negative) {
        // -1
        for (int i = sigBits - 1; i >= 0; i--) {
            if (temp[i] == 1) {
                temp[i] = 0;
                break;
            } else {
                temp[i] = 1;
            }
        }

        // inversion
        for (int i = 0; i < sigBits; i++)
            temp[i] = !temp[i];
    }

    float value = 0.0;
    float weight = range / 2.0;

    for (int i = 0; i < sigBits; i++) {
        if (temp[i])
            value += weight;
        weight /= 2.0;
    }

    return negative ? -value : value;
}


/*
    Encrypte selon BinaryCodedDecimal
*/
uint8_t *BCD_encrypt(float resol, uint8_t digits, float value) {
    uint8_t *out = calloc(20, sizeof(uint8_t));  // bits 10 à 29 => 20
    if (!out) exit(EXIT_FAILURE);

    // apres application resolution
    int scaled = (int)(fabs(value) / resol + 0.5);

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

/// @brief compose un mot arinc 429
/// @param label 1, 2 ou 3. Altitude, taux montée, angle attaque
/// @param value la valeur a encoder
/// @param etatAvionnique au_sol (0), changement_alt (1), vol_croisiere (2). ignoré si label != 1
/// @return un mot de 32 bits.
t_a429_word get_A429_word(uint8_t label, float value, int etat) {

    t_a429_word w;
    uint8_t *mot = calloc(32, sizeof(uint8_t));

    uint8_t ssm[2] = {1, 0}; // NCD par défaut
    uint8_t *encoded = NULL;

    switch (label) {

        case LABEL_ALTITUDE:
            mot[7] = 1;

            if (etat >= 0) {
                mot[10] = etat & 1;
                mot[11] = (etat >> 1) & 1;
            }

            encoded = BNR_encrypt(1, 16, 40000, value);

            for (int i = 0; i < 16; i++)
                mot[12 + i] = encoded[i];

            break;

        case LABEL_TAUX_MONTEE:
            mot[6] = 1;

            ssm[0] = (value < 0);
            ssm[1] = (value < 0);

            encoded = BCD_encrypt(0.1, 4, value);

            for (int i = 0; i < 16; i++)
                mot[12 + i] = encoded[i];

            break;

        case LABEL_ANGLE_ATTAK:
            mot[6] = 1;
            mot[7] = 1;

            ssm[0] = (value < 0);
            ssm[1] = (value < 0);

            encoded = BCD_encrypt(0.1, 3, value);

            for (int i = 0; i < 12; i++)
                mot[12 + i] = encoded[i];

            break;
    }

    // SSM
    mot[29] = ssm[0];
    mot[30] = ssm[1];

    // Parité
    mot[31] = calculParite(mot);

    // Remplissage struct
    w.parite = mot[31];
    memcpy(w.ssm, &mot[29], 2);
    w.pn = mot[28];
    memcpy(w.data, &mot[12], 18);
    memcpy(w.sdi, &mot[9], 2);
    memcpy(w.label, &mot[0], 8);

    memcpy(w.total_word, mot, 32);

    free(encoded);
    free(mot);

    return w;
}

int get_true_label(t_a429_word word) {
    int result = 0;
    for (int i = 0 ; i < 8 ; ++i) {
        result = result * 2 + word.label[i]; // transformation binaire.
    }
    return result;
}

float get_value_from_a429(t_a429_word w)
{
    switch (get_true_label(w)) {

        case LABEL_ALTITUDE:
            return (int) round(BNR_decrypt(w.data, 16, 40000));

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

    for (int i = 0; i < s; i++) {
        printf("%d", word[i]);

        // espace visuel toutes les 4 bits
        if ((i + 1) % 4 == 0)
            printf(" ");
        
        fflush(stdout);
    }
    printf("\n");
}

void afficheA429_word(t_a429_word word) {
    printf("=== A429 WORD ===\n");

    printf("Parité : %d\n", word.parite);

    printf("SSM : ");
    afficheMot(word.ssm, 2);

    printf("PN : %d\n", word.pn);

    printf("DATA : ");
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
        printf("Altitude : %d pieds\n", (int) val);
        break;
    case LABEL_TAUX_MONTEE:
        printf("Taux de montée : %.1f m/min\n", val);
        break;
    case LABEL_ANGLE_ATTAK:
        printf("Angle d'attaque : %.1f°\n", val);
        break;
    }
}
