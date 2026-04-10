#ifndef __A429_H__
#define __A429_H__

#include <stdint.h>

/*
    Exemple d'utilisation : 
        // Test A429
        t_a429_word word = get_A429_word(LABEL_ALTITUDE, 18500, 1);

        afficheA429_word(word); // affichage des champs et de la valeur encodée
*/

typedef enum {
    LABEL_ALTITUDE = 1,
    LABEL_TAUX_MONTEE = 2,
    LABEL_ANGLE_ATTAK = 3
} Label;

typedef struct {
    uint8_t parite; // Odd (11 "1" => "0"), bit 32
    uint8_t ssm[2]; // Sign Status Matrix, bits 31:30
    uint8_t pn; // positif negatif, bit 29
    uint8_t data[18]; // champs de données, label determine si BNR, BCD. bits 28:11
    uint8_t sdi[2]; // not used. bits 10:9
    uint8_t label[8]; // octal label (reversed). Bits 8:1

    uint8_t total_word[32];
} t_a429_word;

// 0 si nombre de "1" impair dans word, 1 sinon.
// 11 "1" => 0. 
// 0 "1" => 1.
uint8_t calculParite(uint8_t *word) ;

// Trouve le 3eme paramètre selon les 2 connus.
int getNbSigBits(unsigned int range, float resol);
float getResol(unsigned int range, unsigned int nbSigBits);
int getRange(unsigned int nbSigBits, float resol);

/*
    Encryption et decryption d'un mot BNR
*/
uint8_t *BNR_encrypt(uint8_t sigBits, uint32_t range, float value);
float BNR_decrypt(uint8_t *bits, uint8_t sigBits, uint32_t range);

/*
    Encryption et decryption d'un mot BCD
*/
uint8_t *BCD_encrypt(float resol, uint8_t digits, float value);
float BCD_decrypt(uint8_t *bits, uint8_t digits, float resol, uint8_t *ssm);

/*
    Parité (32) calculée à la fin.
    SSM (31:30): si BNR, dépend de l'état. Si BCD : 00 = pos. 11 = neg.
        Dans les 2 cas, 01 = NoComputedData, 10 = Test
    +/- (29): S'agit-t-il d'une valeur negative ou positive ? (BCD : utilise SSM. Data étendue avec bit 29)
    Data (28:11)
    SDI (10:9)
    Label (8:1)
*/
t_a429_word get_A429_word(uint8_t label, float value, int etat);

void afficheA429_word(t_a429_word word);


#endif
