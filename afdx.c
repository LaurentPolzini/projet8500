#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "afdx.h"

uint8_t adr_mac_agreg[6] = {0, 0, 0, 0, 0, 1}; // @ Mac = 1
uint8_t adr_mac_calc[6] = {0, 0, 0, 0, 0, 2}; // @ Mac = 2

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

    // SFD 
    f->sfd = 0xAB; // 1010 1011

    // EtherType IPv4 = 0x0800
    f->eth_type = 0x0800;

    f->seq_number = 1;
}

uint16_t ip_checksum(uint8_t *donnees, uint16_t taille) {
    uint32_t somme = 0;

    for (uint16_t i = 0; i < taille; i += 2) {
        uint16_t mot = donnees[i] << 8;

        if (i + 1 < taille) {
            mot |= donnees[i + 1];
        }

        somme += mot;
    }

    // rester sur 16 bits, on ajoute les dépassements (carries)
    while (somme > 0xFFFF) {
        somme = (somme & 0xFFFF) + (somme >> 16);
    }

    return ~somme; // complement a 1
}

void build_udp_header(t_afdx *f, uint16_t payload_len) {
    uint16_t src_port = 1234; // factice
    uint16_t dest_port = 4321;

    uint16_t length = 8 + payload_len + 1; // UDP + payload + seq number

    f->udp[0] = (src_port >> 8) & 0xFF; // séparation des octets
    f->udp[1] = src_port & 0xFF;

    f->udp[2] = (dest_port >> 8) & 0xFF;
    f->udp[3] = dest_port & 0xFF;

    f->udp[4] = (length >> 8) & 0xFF;
    f->udp[5] = length & 0xFF;

    f->udp[6] = 0; // checksum UDP, choix de ne pas le prendre en compte.
    // On se fiera à celui d'IP
    f->udp[7] = 0;
}

void build_ip_header(t_afdx *f, uint16_t payload_len, uint8_t *src_ip, uint8_t *dest_ip) {
    uint16_t total_length = 20 + 8 + payload_len + 1;

    f->ip[0] = 0x45;
    f->ip[1] = 0x00;

    f->ip[2] = (total_length >> 8) & 0xFF;
    f->ip[3] = total_length & 0xFF;

    for (int i = 4 ; i < 8 ; ++i) {
        f->ip[i] = 0x00;
    }

    f->ip[8] = 64; // TTL (64 sauts, factice)
    f->ip[9] = 17; // 17 = protocole UDP

    // checksum à 0 (pour l'instant)
    f->ip[10] = 0;
    f->ip[11] = 0;

    // IP source
    memcpy(&f->ip[12], src_ip, 4);

    // IP dest
    memcpy(&f->ip[16], dest_ip, 4);

    // checksum IP, important.
    uint16_t checksum = ip_checksum(f->ip, 20);

    f->ip[10] = (checksum >> 8) & 0xFF;
    f->ip[11] = checksum & 0xFF;
}

/*
    Concatene tous les champs de (t_afdx *f) dans "f.frame"
*/
void build_afdx_frame(t_afdx *f, const uint8_t *payload, uint16_t payload_len, uint8_t functional_status, uint8_t *src, uint8_t *dest) {
    uint16_t offset = 0;
    
    build_udp_header(f, payload_len);
    build_ip_header(f, payload_len, src, dest);

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

    // Functional Status
    f->fs = functional_status;
    f->frame[offset++] = f->fs;

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

/*
    Augmente le numéro de séquence de la trame
*/
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

/*
    Affichage d'une trame AFDX, division nette des champs
*/
void print_afdx_frame(t_afdx *f) {
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

    printf("Data Functional Status : ");
    fflush(stdout);
    affiche_functionnal_status(f->fs);

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

/*
    Affichage de "functionnal status"
*/
void affiche_functionnal_status(uint8_t fs) {
    switch (fs)
    {
    case ND:
        printf("No data\n");
        break;
    case NO:
        printf("Normal operation\n");
        break;
    case FT:
        printf("Functional test\n");
        break;
    case NCD:
        printf("No computed data\n");
        break;
    default:
        break;
    }
}
