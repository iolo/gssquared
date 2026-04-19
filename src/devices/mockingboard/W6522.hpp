/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <cstdint>

#include "debug.hpp"
#include "util/DebugFormatter.hpp"

#define MB_6522_DDRA 0x03
#define MB_6522_DDRB 0x02
#define MB_6522_ORA  0x01
#define MB_6522_ORB  0x00

#define MB_6522_T1C_L 0x04
#define MB_6522_T1C_H 0x05
#define MB_6522_T1L_L 0x06
#define MB_6522_T1L_H 0x07

#define MB_6522_T2L_L 0x08
#define MB_6522_T2C_L 0x08
#define MB_6522_T2C_H 0x09
#define MB_6522_SR 0x0A
#define MB_6522_ACR 0x0B
#define MB_6522_PCR 0x0C
#define MB_6522_IFR 0x0D
#define MB_6522_IER 0x0E
#define MB_6522_ORA_NH 0x0F

#define MB_6522_1 0x00
#define MB_6522_2 0x80


class W6522 {
private:
    union ifr_t {
        uint8_t value;
        struct {
            uint8_t ca2 : 1;
            uint8_t ca1 : 1;
            uint8_t shift_register : 1;
            uint8_t cb2 : 1;
            uint8_t cb1 : 1;
            uint8_t timer2 : 1;
            uint8_t timer1 : 1;
            uint8_t irq : 1;
        } bits;
    };

    union ier_t {
        uint8_t value;
        struct {
            uint8_t ca2 : 1;
            uint8_t ca1 : 1;
            uint8_t shift_register : 1;
            uint8_t cb2 : 1;
            uint8_t cb1 : 1;
            uint8_t timer2 : 1;
            uint8_t timer1 : 1;
        } bits;
    };

    uint8_t ora; /* 0x00 */
    uint8_t ira; /* 0x00 */
    uint8_t orb; /* 0x01 */
    uint8_t irb; /* 0x01 */
    uint8_t ddra; /* 0x02 */
    uint8_t ddrb; /* 0x03 */

    uint8_t sr; /* 0x0A */
    uint8_t acr; /* 0x0B */
    uint8_t pcr; /* 0x0C */
    ifr_t ifr; /* 0x0D */
    ier_t ier; /* 0x0E */

    uint16_t t1_latch;
    uint16_t t1_counter;  
    uint8_t t2_latch;
    uint16_t t2_counter;
    uint16_t t1_oneshot_pending = 0;
    uint16_t t2_oneshot_pending = 0;

    bool t1_rollover = false;
    bool t2_rollover = false;
    
    uint64_t system_cycles;
    uint64_t t1_next_event;
    uint64_t t2_next_event;

    const char *chip_id;

public:
    W6522(const char *chip_id) : chip_id(chip_id) {
        t1_counter = 0;
        t2_counter = 0;
        t1_latch = 0;
        t2_latch = 0;
        ddra = 0x00;
        ddrb = 0x00;
        ora = 0x00;
        orb = 0x00;
        ira = 0x00;
        irb = 0x00;
        ifr.value = 0;
        ier.value = 0;
        acr = 0;
        pcr = 0;
        sr = 0;
        system_cycles = 0;
        t1_next_event = 0;
        t2_next_event = 0;
        t1_rollover = 0;
        t2_rollover = 0;
    };
    //~W6522();
    
    void set_irq_callback(std::function<void(uint8_t irq)> irq_callback);
 
    void update_interrupt() {
        // for each chip, calculate the IFR bit 7.

        uint8_t irq = ((ifr.value & ier.value) & 0x7F) > 0;
        // set bit 7 of IFR to the result.
        ifr.bits.irq = irq;

        bool irq_to_slot = ifr.value & ier.value & 0x7F;
        
        printf("[%s]: assert irq: %d\n", chip_id, irq_to_slot);
        //irq_callback(irq_to_slot);
    }

    void incr_cycle() {
        system_cycles++;

        // T1 Logic
        if (t1_counter == 0) { // triggers rollover behavior at -next- cycle, not this one
            t1_rollover = 1;
        }

        if (t1_rollover && (t1_counter == 0xFFFF)) {
            if (acr & 0x40) { // continuous mode
                ifr.bits.timer1 = 1; // "Set by 'time out of T1'"
                update_interrupt();

                t1_counter = t1_latch;
            } else {         // one-shot mode
                // if a T1 oneshot was pending, set interrupt status.
                if (t1_oneshot_pending) {
                    ifr.bits.timer1 = 1; // "Set by 'time out of T1'"
                    update_interrupt();
                }
                // We don't schedule a next interrupt - that is only done when writing to T1C-H.
                // We don't reset the counter to the latch, we continue decrementing from 0.
                t1_oneshot_pending = 0;
                t1_counter--;
            }
            t1_rollover = 0; // reset
        } else {
            t1_counter--;
        }
        
        // T2 Logic
        if (t2_counter == 0) {
            t2_rollover = 1;
        }
        if (t2_rollover && (t2_counter == 0xFFFF)) {
            if (t2_oneshot_pending) {
                ifr.bits.timer2 = 1; // "Set by 'time out of T2'"
                update_interrupt();
            }
            // We don't schedule a next interrupt - that is only done when writing to T1C-H.
            // We don't reset the counter to the latch, we continue decrementing from 0.
            t2_oneshot_pending = 0;
            t2_rollover = 0; // reset
        }/*  else { */
            t2_counter--;
        /* } */
    }

#if 0
    void t2_timer_callback(uint64_t instanceID, void *user_data) {

        if (DEBUG(DEBUG_MOCKINGBOARD)) std::cout << "MB 6522 Timer callback " << chip_id << std::endl;

        // TODO: "after timing out, the counter will continue to decrement."
        // so do NOT reset the counter to the latch.
        // processor must rewrite T2C-H to enable setting of the interrupt flag.
        if (t2_oneshot_pending) {
            ifr.bits.timer2 = 1; // "Set by 'time out of T2'"
            update_interrupt();
        }
        t2_oneshot_pending = 0;
    }
#endif

    void write(uint16_t reg, uint8_t data) {

        switch (reg) {

            case MB_6522_DDRA:
                ddra = data;
                break;
            case MB_6522_DDRB:
                ddrb = data;
                break;
            case MB_6522_ORA: case MB_6522_ORA_NH: // TODO: what the heck is NH here??
                // TODO: need to mask with DDRA
                ora = data;
                break;
            case MB_6522_ORB:
                // TODO: instead of having this here, we could put this logic into the AY/Mockingboard code.
                // TODO: write to the connected chip callback - pass ORA and ORB 
                orb = data;
                // TODO: why is there a read below?
                /* 
                } else if ((data & 0b111) == 5) { // read from the specified register
                    // to read, the command is run through ORB, but the data goes back and forth on ORA/IRA.
                    //printf("attempt to read with cmd 05 from PSG, unimplemented\n");
                    ira = mb_d->mockingboard->read_register(chip, reg_num);
                    printf("read register: %02X -> %02X\n", reg_num, ira);
                } */
                break;
            case MB_6522_T1L_L: 
                /* 8 bits loaded into T1 low-order latches. This operation is no different than a write into REG 4 (2-42) */
                t1_latch = (t1_latch & 0xFF00) | data;
                break;
            case MB_6522_T1L_H:
                /* 6522 doc doesn't say it, but AppleWin and UltimaV clear timer1 interrupt flag when T1L_H is written. */
                ifr.bits.timer1 = 0;
                update_interrupt();       
            
                /* 8 bits loaded into T1 high-order latches. Unlike REG 4 OPERATION, no latch-to-counter transfers take place (2-42) */
                t1_latch = (t1_latch & 0x00FF) | (data << 8);
                break;
            case MB_6522_T1C_L:
                /* 8 bits loaded into T1 low-order latches. Transferred into low-order counter at the time the high-order counter is loaded (reg 5) */
                t1_latch = (t1_latch & 0xFF00) | data;
                break;
            case MB_6522_T1C_H:
                /* 8 bits loaded into T1 high-order latch. Also both high-and-low order latches transferred into T1 Counter. T1 Interrupt flag is also reset (2-42) */
                // write of t1 counter high clears the interrupt.
                
                ifr.bits.timer1 = 0;
                update_interrupt();
                t1_latch = (t1_latch & 0x00FF) | (data << 8);
                t1_counter = t1_latch;
                t1_oneshot_pending = 1;
                break;

            // TODO: there is indication that the T2 latch is only for the LO portion (8 bit, not 16-bit)
            case MB_6522_T2L_L:
                t2_latch = data;
                break;
            case MB_6522_T2C_H:
                t2_counter = t2_latch | (data << 8);
                ifr.bits.timer2 = 0;
                update_interrupt();
                t2_oneshot_pending = 1;
                break;

            case MB_6522_PCR:
                pcr = data;
                break;
            case MB_6522_SR:
                sr = data;
                break;
            case MB_6522_ACR:
                acr = data;
                break;
            
            case MB_6522_IFR:
                // for any bit set in data[6:0], clear the corresponding bit in the IFR.
                // Pg 2-49 6522 Data Sheet
                ifr.value &= ~(data & 0x7F);
                update_interrupt();
                break;

            case MB_6522_IER:
                // if bit 7 is a 0, then each 1 in bits 0-6 clears the corresponding bit in the IER
                // if bit 7 is a 1, then each 1 in bits 0-6 enables the corresponding interrupt.
                // Pg 2-49 6522 Data Sheet
                if (data & 0x80) {
                    ier.value |= data & 0x7F;
                } else {
                    ier.value &= ~data;
                }
                update_interrupt();             

                break;
        }
    }

    uint8_t read(uint16_t reg) {
        if (DEBUG(DEBUG_MOCKINGBOARD)) printf("read: %02x => ", reg);
        uint8_t retval = 0xFF;

        switch (reg) {
            case MB_6522_DDRA:
                retval = ddra;
                break;
            case MB_6522_DDRB:
                retval = ddrb;
                break;
            case MB_6522_ORA: case MB_6522_ORA_NH: { 
                // TODO: what the heck is NH here??
                uint8_t a_in = ira & (~ddra);
                uint8_t a_out = ora & ddra;
                retval = a_out | a_in;
                } 

                break;
            case MB_6522_ORB: {
                uint8_t b_in = orb & (~ddrb);
                uint8_t b_out = orb & ddrb;
                retval = b_out | b_in;
                } 

                break;
            case MB_6522_T1L_L:
                /* 8 bits from T1 low order latch transferred to mpu. unlike read T1 low counter, does not cause reset of T1 IFR6. */
                retval = t1_latch & 0xFF;
                break;
            case MB_6522_T1L_H:     
                /* 8 bits from t1 high order latch transferred to mpu */
                retval = (t1_latch >> 8) & 0xFF;
                break;
            case MB_6522_T1C_L:  {  
                // IFR Timer 1 flag cleared by read T1 counter low. pg 2-42
                ifr.bits.timer1 = 0;
                update_interrupt();
                retval = t1_counter & 0xFF;
                break;
            }
            case MB_6522_T1C_H:    {  
                // IFR Timer 1 flag cleared by read T1 counter high. pg 2-42
                // read of t1 counter high DOES NOT clear interrupt; write does.

                retval = (t1_counter >> 8) & 0xFF;
                break;
            }
            case MB_6522_T2C_L: { 
                /* 8 bits from T2 low order counter transferred to mpu - t2 interrupt flag is reset. */
                ifr.bits.timer2 = 0;
                update_interrupt();

                retval = t2_counter & 0xFF;
                break;
            }
            case MB_6522_T2C_H: { 
                /* 8 bits from T2 high order counter transferred to mpu */
                retval = (t2_counter >> 8) & 0xFF;
                break;
            }
            case MB_6522_PCR:
                retval = pcr;
                break;
            case MB_6522_SR:
                retval = sr;
                break;
            case MB_6522_ACR:
                retval = acr;
                break;
            case MB_6522_IFR:
                retval = ifr.value;
                break;
            case MB_6522_IER:
                // if a read of this register is done, bit 7 will be "1" and all other bits will reflect their enable/disable state.
                retval = ier.value | 0x80;
                break;
        }
        if (DEBUG(DEBUG_MOCKINGBOARD)) printf("%02x\n", retval);
        return retval;
    }

    void reset() {
        acr = 0;
        ifr.value = 0;
        ier.value = 0;

        update_interrupt(); // this reads the slot number and does the right IRQ thing.    
    }

    DebugFormatter *debug() {
        DebugFormatter *df = new DebugFormatter();

        df->addLine("--------- 6522 --------");
        df->addLine("DDRA: %02X    DDRB: %02X", ddra, ddrb);
        df->addLine("ORA : %02X    ORB : %02X", ora, orb);
        df->addLine("IRA : %02X    IRB : %02X", ira, irb);
        
        df->addLine("T1L : %04X  T1C: %04X      ", t1_latch, t1_counter);
        df->addLine("T2L : %04X  T2C: %04X      ", t2_latch, t2_counter);
        df->addLine("SR  : %02X", sr);
        df->addLine("ACR : %02X", acr);
        df->addLine("PCR : %02X", pcr);
        df->addLine("IFR : %02X    IER: %02X", ifr.value, ier.value|0x80);
        //df->addLine("IER: %02X                   | IER: %02X", mb_d->d_6522[0].ier.value, mb_d->d_6522[1].ier.value);

        return df;
    }
    void debug_one() {
        printf("DDRA %02X DDRB %02X ORA %02X ORB %02X IRA %02X IRB %02X T1[L %04X C: %04X] T2[L %02X C %04X] SR %02X ACR %02X PCR %02X IFR %02X IER %02X\n",
            ddra, ddrb, ora, orb, ira, irb, t1_latch, t1_counter, t2_latch, t2_counter, sr, acr, pcr, ifr.value, ier.value);
    }

};