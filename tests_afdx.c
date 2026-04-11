#include <stdio.h>
#include <assert.h>
#include "afdx.h"

void tests_afdx(void)
{
    printf("==== TESTS AFDX ====\n\n");
    t_afdx frame_afdx;
    init_afdx(&frame_afdx);

    build_afdx_frame(&frame_afdx, "Premiere trame", 14, adr_mac_agreg, adr_mac_calc);

    print_afdx_frame(&frame_afdx);

    update_seq(&frame_afdx);
    build_afdx_frame(&frame_afdx, "Seconde trame", 13, adr_mac_calc, adr_mac_agreg);

    print_afdx_frame(&frame_afdx);

    printf("tests afdx OK\n");
    return;
}
