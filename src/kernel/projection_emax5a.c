#include <stddef.h>
#include "emax7.h"
void dense_tile(int LANE, Ull input[1], Ull weight0[1], Ull initial0, Ull target0[1], Ull weight1[1], Ull initial1, Ull target1[1], Ull words, Ull pairs) {
    Ull CHIP, INIT1, LOOP1, INIT0, LOOP0, pair_offsets;
    Ull AR[64][4];
    Ull BR[64][4][4];
    Ull acc0 = initial0;
    Ull acc1 = initial1;
    Ull pair_initial = (0xfffffff8LL << 32) | 0xfffffffcLL;
    Ull pair_step = (8LL << 32) | 4LL;
    Ull zero_offset = 0;
    Ull scalar_words = 2;
    Ull Force = 1;
    Ull input_words = words >> 1;
//EMAX5A begin dense_tile mapdist=0
    for (CHIP = 0; CHIP < 1; CHIP++) {
      for (INIT1 = 1, LOOP1 = 1, pair_offsets = pair_initial; LOOP1--; INIT1 = 0) {
        for (INIT0 = 1, LOOP0 = pairs, pair_offsets = pair_initial; LOOP0--; INIT0 = 0) {
          exe(OP_ADD, &pair_offsets, INIT0 ? pair_offsets : pair_offsets, EXP_H3210, pair_step, EXP_H3210, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL);
          mop(OP_LDWR, 1, &BR[2][0][1], input[CHIP], pair_offsets, MSK_W0, input[CHIP], input_words, 0, Force, 0LL, input_words);
          mop(OP_LDR, 3, &BR[4][0][1], weight0[CHIP], pair_offsets, MSK_W1, weight0[CHIP], words, 0, Force, 0LL, words);
          exe(OP_FML, &AR[6][0], BR[4][0][1], EXP_H3210, BR[2][0][1], EXP_H1010, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL);
          exe(OP_FAD, &acc0, INIT0 ? acc0 : acc0, EXP_H3210, AR[6][0], EXP_H3210, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL);
          mop(OP_STR, 3, &acc0, target0[CHIP], zero_offset, MSK_W0, target0[CHIP], scalar_words, 0, Force, 0LL, scalar_words);
          mop(OP_LDR, 3, &BR[8][0][1], weight1[CHIP], pair_offsets, MSK_W1, weight1[CHIP], words, 0, Force, 0LL, words);
          exe(OP_FML, &AR[10][0], BR[8][0][1], EXP_H3210, BR[2][0][1], EXP_H1010, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL);
          exe(OP_FAD, &acc1, INIT0 ? acc1 : acc1, EXP_H3210, AR[10][0], EXP_H3210, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL);
          mop(OP_STR, 3, &acc1, target1[CHIP], zero_offset, MSK_W0, target1[CHIP], scalar_words, 0, Force, 0LL, scalar_words);
        }
      }
    }
//EMAX5A end
    Force = 0;
//EMAX5A drain_dirty_lmm
}
