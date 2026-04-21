#pragma once

#include <cstdint>

#include "regs.hpp"

enum rw {
    READ =0,
    WRITE = 1,
    END = 2,
};

struct reg_record_t {
    rw action;
    uint64_t cycle;
    uint8_t reg;
    uint8_t value;
};

reg_record_t recs1[] = {
    // we're in 1-shot mode.
    {WRITE, 1, MB_6522_IER, 0b11000000}, // enable T1 interrupt (7:1 = "set", 6:1 = "set")
    {WRITE,5, MB_6522_T1L_L, 0x05},
    {WRITE, 8, MB_6522_T1C_H, 0x00},
    {WRITE, 20, MB_6522_T1C_H, 0x00}, // write T1C-H, should clear interrupt pending and set up for another 1-shot

    {WRITE, 29, MB_6522_ACR, 0x40},
    {WRITE, 30, MB_6522_T1C_H, 0x00},
    {WRITE, 40, MB_6522_IFR, 0x40}, // clear T1 interrupt
    {WRITE, 60, MB_6522_IER,0b10100000}, // enable T2 interrupt also
    {WRITE, 61, MB_6522_T2L_L, 0x05},
    {WRITE, 62, MB_6522_T2C_H, 0x00},

    {WRITE, 79, MB_6522_IER, 0x40}, // disable T1 interrupt
    {WRITE, 80, MB_6522_T1L_L, 10},
    {WRITE, 81, MB_6522_T1C_H, 0x00},
    {WRITE, 85, MB_6522_T1L_L, 10},
    {WRITE, 86, MB_6522_T1C_H, 0x00},
    {WRITE, 90, MB_6522_T1L_L, 10},
    {WRITE, 91, MB_6522_T1C_H, 0x00},
    

    // should have a concept here to READ the register.
    {READ, 100, MB_6522_T2L_L, 0x00},
    {END, 105, 0, 0},
};

// Test:
// Continuous mode, T1 Latch set to 0, should cycle 1.5 times before IRQ.
reg_record_t recs2[] = {
    {WRITE, 1, MB_6522_IER, 0b11000000}, // enable T1 interrupt (7:1 = "set", 6:1 = "set")
    {WRITE, 2, MB_6522_ACR, 0x40},
    {WRITE,5, MB_6522_T1L_L, 0x00},
    {WRITE, 8, MB_6522_T1C_H, 0x00},
    {END, 30, 0, 0},
};

// Test:
// Continuous mode, T1 Latch set to 5
reg_record_t recs3[] = {    
    {WRITE, 1, MB_6522_IER, 0b11000000}, // enable T1 interrupt (7:1 = "set", 6:1 = "set")
    {WRITE, 2, MB_6522_ACR, 0x40},
    {WRITE,5, MB_6522_T1L_L, 0x05},
    {WRITE, 8, MB_6522_T1C_H, 0x00},
    {END, 30, 0, 0},
};

// Test:
// Continuous mode, T1 Latch set to FFFF
reg_record_t recs4[] = {    
    {WRITE, 1, MB_6522_IER, 0b11000000}, // enable T1 interrupt (7:1 = "set", 6:1 = "set")
    {WRITE, 2, MB_6522_ACR, 0x40},
    {WRITE,5, MB_6522_T1L_L, 0xFF},
    {WRITE, 8, MB_6522_T1C_H, 0xFF},
    {END, 65550, 0, 0},
};

// Test:
// Continuous mode, T1 Latch set to 5, then go back to one-shot mode.
reg_record_t recs5[] = {    
    {WRITE, 1, MB_6522_IER, 0b11000000}, // enable T1 interrupt (7:1 = "set", 6:1 = "set")
    {WRITE, 2, MB_6522_ACR, 0x40},
    {WRITE,5, MB_6522_T1L_L, 0x05},
    {WRITE, 8, MB_6522_T1C_H, 0x00},
    {WRITE, 18, MB_6522_ACR, 0x00}, // turn off continuous mode
    {END, 50, 0, 0},
};

reg_record_t *recs[] = {
    recs1,
    recs2,
    recs3,
    recs4,
    recs5,
};
