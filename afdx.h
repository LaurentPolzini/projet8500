#ifndef __AFDX_H__
#define __AFDX_H__

#include <stdint.h>

/*
    Exemple utilisation : 

    t_afdx frame;

    init_afdx(&frame);

    // MAC exemple
    uint8_t dest[6] = {0x03,0x00,0x00,0x00,0x00,0x01}; // multicast VL
    uint8_t src[6]  = {0x02,0x00,0x00,0x00,0x00,0x01};

    // Build
    build_afdx_frame(&frame, "AFDX TEST", 9, src, dest);

    print_afdx_frame(&frame);
*/

extern uint8_t adr_mac_agreg[6];
extern uint8_t adr_mac_calc[6];

#define AFDX_MAX_PAYLOAD 1471
#define AFDX_FRAME_MAX   1538

typedef enum {
    ND = 0, // no data
    NO = 1, // normal operation
    FT = 2, // functionnal test
    NCD = 3 // no computed data
} FS;


typedef struct {

    uint8_t preamble[7]; // 1010 répété
    uint8_t sfd; // 10101011

    // @ agregateur = 1. @ calculateur = 2.
    uint8_t dest_mac[6]; // @ mac
    uint8_t src_mac[6];

    // IPv4
    uint16_t eth_type;

    uint8_t ip[20];
    uint8_t udp[8];

    uint8_t fs;
    uint8_t payload[AFDX_MAX_PAYLOAD];

    uint8_t seq_number;
    uint32_t fcs; // 0000 répété.

    uint8_t frame[AFDX_FRAME_MAX]; // frame totale
    uint16_t frame_size;

} t_afdx;

// Rempli les champs preamble, sfd, eth_type et seq_number.
void init_afdx(t_afdx *f);

// Concatenation des champs dans le tableau f.frame
void build_afdx_frame(t_afdx *f, const uint8_t *payload, uint16_t payload_len, uint8_t functional_status, uint8_t *src, uint8_t *dest);

// Cycle de 0 à 255
void update_seq(t_afdx *f);

void print_afdx_frame(t_afdx *f);
void affiche_functionnal_status(uint8_t fs);

#endif
