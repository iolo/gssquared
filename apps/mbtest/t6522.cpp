/*
 * 6522 Chip Emulation Test Harness
 */

#include "devices/mockingboard/W6522.hpp"

struct reg_record_t {
    uint64_t cycle;
    uint8_t reg;
    uint8_t value;
};

reg_record_t recs[] = {
    {1, MB_6522_IER, 0b11000000}, // enable T1 interrupt (7:1 = "set", 6:1 = "set")
    {5, MB_6522_T1L_L, 0x05},
    {8, MB_6522_T1C_H, 0x00},
    {20, MB_6522_T1C_H, 0x00}, // write T1C-H, should clear interrupt pending and set up for another 1-shot

    {29, MB_6522_ACR, 0x40},
    {30, MB_6522_T1C_H, 0x00},
    {40, MB_6522_IFR, 0x40}, // clear T1 interrupt
    {60, MB_6522_IER,0b10100000}, // enable T2 interrupt also
    {61, MB_6522_T2L_L, 0x05},
    {62, MB_6522_T2C_H, 0x00},
};

int main(int argc, char *argv[]) {
    W6522 chip("6522 #1 $80");

    int recindex = 0;
    uint64_t system_cycles = 0;

    while (1) {
        if (recindex < sizeof(recs) / sizeof(recs[0])) {
            if (system_cycles == recs[recindex].cycle) {
                chip.write(recs[recindex].reg, recs[recindex].value);
                printf("write: Reg %02X <= %02X\n", recs[recindex].reg, recs[recindex].value);
                recindex++;
            }
        }
        printf("%8llu: ",system_cycles); chip.debug_one();
        chip.incr_cycle();
        system_cycles++;
    }
    return 0;
}