#include <stdio.h>
#include <stdlib.h>
#include "afdx.h"

/*
    Rempli les champs "de base" de la trame
        =>   Preamble
             SFD
             Type (IPv4)
             Initialisation seq_num
*/
void init_afdx(t_afdx *f) {
    // Preamble = 0xAA répété
    for (int i = 0; i < 7; i++)
        f->preamble[i] = 0xAA;

    // SFD = 0xAB
    f->sfd = 0xAB; // 1010 1011

    // EtherType IPv4 = 0x0800
    f->eth_type = 0x0800;

    // Sequence number start
    f->seq_number = 1;
}

/*
    Concatene tous les champs de (t_afdx *f) dans "f.frame"
*/
void build_afdx_frame(t_afdx *f, const char *payload, uint16_t payload_len, uint8_t *src, uint8_t *dest) {
    uint16_t offset = 0;

    // Preamble
    memcpy(f->frame + offset, f->preamble, 7);
    offset += 7;

    // SFD
    f->frame[offset++] = f->sfd;

    // MAC dest
    memcpy(f->dest_mac, dest, 6);
    memcpy(f->frame + offset, f->dest_mac, 6);
    offset += 6;

    // MAC src
    memcpy(f->src_mac, src, 6);
    memcpy(f->frame + offset, f->src_mac, 6);
    offset += 6;

    // EtherType (big endian)
    f->frame[offset++] = (f->eth_type >> 8) & 0xFF;
    f->frame[offset++] = f->eth_type & 0xFF;

    // IP
    memcpy(f->frame + offset, f->ip, 20);
    offset += 20;

    // UDP
    memcpy(f->frame + offset, f->udp, 8);
    offset += 8;

    // Payload
    memcpy(f->payload, payload, payload_len);
    memcpy(f->frame + offset, f->payload, payload_len);
    offset += payload_len;

    // Sequence Number (AFDX spécifique)
    f->frame[offset++] = f->seq_number;

    // FCS (ici fake = 0)
    f->fcs = 0;
    memcpy(f->frame + offset, &f->fcs, 4);
    offset += 4;

    f->frame_size = offset;
}

void update_seq(t_afdx *f) {
    if (f->seq_number == 255)
        f->seq_number = 1;
    else
        f->seq_number++;
}

void print_frame_hex(uint8_t *frame, uint16_t size) {
    for (int i = 0; i < size; i++) {
        printf("%02X ", frame[i]);

        // retour à la ligne toutes les 16 bytes
        if ((i + 1) % 16 == 0)
            printf("\n");
    }

    printf("\n");
}

void print_afdx_frame(t_afdx *f)
{
    int offset = 0;

    printf("=== AFDX FRAME ===\n");

    printf("Preamble : ");
    print_frame_hex(&(f->frame[offset]), 7);
    offset += 7;

    printf("SFD : %02X\n", f->frame[offset++]);

    printf("Dest MAC : ");
    print_frame_hex(&(f->frame[offset]), 6);
    offset += 6;

    printf("Src MAC : ");
    print_frame_hex(&(f->frame[offset]), 6);
    offset += 6;

    printf("EtherType : %02X %02X\n",
           f->frame[offset], f->frame[offset + 1]);
    offset += 2;

    printf("IP Header : ");
    print_frame_hex(&(f->frame[offset]), 20);
    offset += 20;

    printf("UDP Header : ");
    print_frame_hex(&(f->frame[offset]), 8);
    offset += 8;

    printf("Payload : ");
    int payload_len = f->frame_size - offset - 5; // SN + FCS
    print_frame_hex(&(f->frame[offset]), payload_len);

    printf("\tTraduction ASCII : ");
    fflush(stdout);
    for (int i = 0 ; i < payload_len ; ++i) {
        printf("%c", (f->frame[offset + i]));
        fflush(stdout);
    }
    printf("\n");
    offset += payload_len;

    printf("Sequence Number : %02X\n", f->frame[offset++]);

    printf("FCS : ");
    print_frame_hex(&(f->frame[offset]), 4);

    printf("===================\n");
}
