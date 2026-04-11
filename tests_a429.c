#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "a429.h"

void tests_a429(void)
{
    printf("==== TESTS A429 ====\n\n");

    uint8_t ssm[2] = {0,0};
    (void) ssm;

    //---- parité ----
    uint8_t word[3];
    for (int i = 0 ; i < 3 ; ++i) word[i] = 1;
    assert(calculParite(word, 3) == 0);
    word[0] = 0;
    assert(calculParite(word, 3) == 1);

    //---- deduction nombre bits significatifs ----
    assert(getNbSigBits(512, 0.25) == 11);
    assert(getNbSigBits(180, 0.04394) == 12);

    //---- deduction resolution ----
    assert(getResol(512, 11) == 0.25);
    assert(getResol(1024, 16) == 0.015625);

    //---- deduction range ----
    assert(getRange(11, 0.25) == 512);
    assert(getRange(12, 0.04394) == 180);

    //---- encryption BNR ----
    uint8_t cinq_un_un_sept_cinq[11];
    (void) cinq_un_un_sept_cinq;
    for (int i = 0 ; i < 11 ; ++i) cinq_un_un_sept_cinq[i] = 1;
    uint8_t *true_value = BNR_encrypt(11, 512, 511.75);
    for (int i = 0 ; i < 11 ; ++i) {
        assert(cinq_un_un_sept_cinq[i] == true_value[i]);
    }
    free(true_value);

    uint8_t deux_cinq_sept_cinq[11] = {0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1};
    (void) deux_cinq_sept_cinq;
    uint8_t *true_value_deux = BNR_encrypt(11, 512, 25.75);
    for (int i = 0 ; i < 11 ; ++i) {
        assert(deux_cinq_sept_cinq[i] == true_value_deux[i]);
    }
    free(true_value_deux);

    // decryption BNR
    assert(BNR_decrypt(cinq_un_un_sept_cinq, 11, 512, 0) == 511.75);

    assert(BNR_decrypt(deux_cinq_sept_cinq, 11, 512, 0) == 25.75);

    //---- encryption BCD ----
    uint8_t trois_quatre_six_sept_huit[20] = {0,0,1,1,0,1,0,0,0,1,1,0,0,1,1,1,1,0,0,0};
    (void) trois_quatre_six_sept_huit;
    uint8_t *true_value_trois = BCD_encrypt(1, 5, 34678);
    for (int i = 0 ; i < 20 ; ++i) {
        assert(trois_quatre_six_sept_huit[i] == true_value_trois[i]);
    }
    free(true_value_trois);

    uint8_t deux_cinq_nil_nil[20] = {0,0,0,0,0,0,1,0,0,1,0,1,0,0,0,0,0,0,0,0};
    (void) deux_cinq_nil_nil;
    uint8_t *true_value_quatre = BCD_encrypt(10, 4, 2500);
    for (int i = 0 ; i < 20 ; ++i) {
        assert(deux_cinq_nil_nil[i] == true_value_quatre[i]);
    }
    printf("\n");
    free(true_value_quatre);


    uint8_t deux_sept_neuf[20] = {0,0,1,0,0,1,1,1,1,0,0,1,0,0,0,0,0,0,0,0};
    (void) deux_sept_neuf;
    uint8_t *true_value_cinq = BCD_encrypt(1, 3, 279);
    for (int i = 0 ; i < 20 ; ++i) {
        assert(deux_sept_neuf[i] == true_value_cinq[i]);
    }
    free(true_value_cinq);

    // decryption BCD
    assert(BCD_decrypt(trois_quatre_six_sept_huit, 5, 1, ssm) == 34678);

    assert(BCD_decrypt(deux_cinq_nil_nil, 4, 10, ssm) == 2500);

    assert(BCD_decrypt(deux_sept_neuf, 3, 1, ssm) == 279);

    //---- obtention mot ARINC 429 ----
    t_a429_word w = get_A429_word(LABEL_ALTITUDE, 21503, 1);
    (void) w;
    assert(get_true_label(w) == LABEL_ALTITUDE);
    assert(get_value_from_a429(w) == 21503);

    t_a429_word w2 = get_A429_word(LABEL_ANGLE_ATTAK, 15.3, 1);
    assert(get_true_label(w2) == LABEL_ANGLE_ATTAK);
    printf("%.1f == 15.3 ?\n", get_value_from_a429(w2));

    t_a429_word w3 = get_A429_word(LABEL_ANGLE_ATTAK, -15.3, 1);
    assert(get_true_label(w3) == LABEL_ANGLE_ATTAK);
    printf("%.1f == -15.3 ?\n", get_value_from_a429(w3));

    t_a429_word w4 = get_A429_word(LABEL_TAUX_MONTEE, -759.5, 1);
    assert(get_true_label(w4) == LABEL_TAUX_MONTEE);
    printf("%.1f == -759.5 ?\n", get_value_from_a429(w4));

    t_a429_word w5 = get_A429_word(LABEL_TAUX_MONTEE, 324.2, 1);
    assert(get_true_label(w5) == LABEL_TAUX_MONTEE);
    printf("%.1f == 324.2 ?\n", get_value_from_a429(w5));

    

    printf("tests arinc 429 OK\n\n");
    return;
}
