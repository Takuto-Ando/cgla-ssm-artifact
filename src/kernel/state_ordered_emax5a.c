#include <stddef.h>
#include <stdint.h>
#include "emax7.h"

void nj_ordered_scan(int LANE, Ull s_in_base, Ull a_base, Ull bx_base, Ull c_base, Ull y_in_base, Ull s_out_base, Ull y_out_base, int pairs)
{
    Ull CHIP, LOOP1, LOOP0, INIT1, INIT0;
    Ull AR[64][4], BR[64][4][4];
    Ull offset, step = 8, trips = (Ull)pairs, words = (Ull)pairs * 2;
    Ull offset_start = 0x00000000fffffff8ULL, Force = 1;
    if (pairs <= 0 || pairs > 512) return;
//EMAX5A begin nj_ordered_scan mapdist=0
    /*3*/ for (CHIP = 0; CHIP < 1; CHIP++) {
      /*2*/ for (INIT1 = 1, LOOP1 = 1, offset = 0LL; LOOP1--; INIT1 = 0) {
        /*1*/ for (INIT0 = 1, LOOP0 = trips, offset = offset_start; LOOP0--; INIT0 = 0) {
            exe(OP_ADD, &offset, offset, EXP_H3210, step, EXP_H3210, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL);
            mop(OP_LDR, 3, &BR[2][0][1], s_in_base, offset, MSK_D0, s_in_base, words, 0, Force, (Ull)NULL, words);
            mop(OP_LDR, 3, &BR[2][1][1], a_base, offset, MSK_D0, a_base, words, 0, Force, (Ull)NULL, words);
            mop(OP_LDR, 3, &BR[2][2][1], bx_base, offset, MSK_D0, bx_base, words, 0, Force, (Ull)NULL, words);
            exe(OP_FML, &AR[3][0], BR[2][0][1], EXP_H3210, BR[2][1][1], EXP_H3210, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL);
            exe(OP_FAD, &AR[4][0], AR[3][0], EXP_H3210, BR[2][2][1], EXP_H3210, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL);
            mop(OP_STR, 3, &AR[4][0], s_out_base, offset, MSK_D0, s_out_base, words, 0, Force, (Ull)NULL, words);
            mop(OP_LDR, 3, &BR[5][0][1], y_in_base, offset, MSK_D0, y_in_base, words, 0, Force, (Ull)NULL, words);
            mop(OP_LDR, 3, &BR[5][1][1], c_base, offset, MSK_D0, c_base, words, 0, Force, (Ull)NULL, words);
            exe(OP_FML, &AR[6][0], AR[4][0], EXP_H3210, BR[5][1][1], EXP_H3210, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL);
            exe(OP_FAD, &AR[7][0], BR[5][0][1], EXP_H3210, AR[6][0], EXP_H3210, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL);
            mop(OP_STR, 3, &AR[7][0], y_out_base, offset, MSK_D0, y_out_base, words, 0, Force, (Ull)NULL, words);
        }
      }
    }
//EMAX5A end
    if (Force) Force = 0;
//EMAX5A drain_dirty_lmm
}
