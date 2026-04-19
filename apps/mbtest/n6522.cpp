/*
 * 6522 Chip Emulation Test Harness
 */

#include "devices/mockingboard/N6522.hpp"

uint64_t debug_level = 0;


enum rw {
    READ =0,
    WRITE = 1,
};

struct reg_record_t {
    rw action;
    uint64_t cycle;
    uint8_t reg;
    uint8_t value;
};

reg_record_t recs[] = {
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
};

class NClockMB : public NClock {
    protected:
        bool slow_mode = false;
    
    public:
        NClockMB(clock_set_t clock_set = CLOCK_SET_US, clock_mode_t clock_mode = CLOCK_1_024MHZ) : NClock(clock_set, clock_mode) {};

        inline virtual void slow_incr_cycles() {
            cycles++;
            video_cycles++;
        }
};

int main(int argc, char *argv[]) {
    NClockMB clock;
    InterruptController irq_controller;
    EventTimer event_timer;

    N6522 chip("6522 #1 $80", &clock, &irq_controller, &event_timer);

    int recindex = 0;

    while (1) {
        uint64_t system_cycles = clock.get_vid_cycles();
        // this section simulates the CPU doing things, changing the registers.
        if (recindex < sizeof(recs) / sizeof(recs[0])) {
            if (system_cycles >= recs[recindex].cycle) {
                if (recs[recindex].action == WRITE) {
                    chip.write(recs[recindex].reg, recs[recindex].value);
                    //printf("write: Reg %02X <= %02X\n", recs[recindex].reg, recs[recindex].value);
                } else {
                    uint8_t value = chip.read(recs[recindex].reg);
                    //printf("read: Reg %02X => %02X\n", recs[recindex].reg, value);
                }
                recindex++;
            }
        }
        // this section triggers our per-instruction timer callbacks.
        /*
        Note: in test harness, we are incrementing one clock at a time. In real emulator,
        we will add 2 to 7/8 cycles at a time. The potential issue here is the difference
        between CPU having noticed the interrupt in the penultimate cycle, which this code will
        NOT DO. This may be a distinction w/o a difference in real code, but, bear in mind that
        we may be required to use the single-cycle version.
        */
        if (event_timer.isEventPassed(clock.get_vid_cycles())) {
            event_timer.processEvents(clock.get_vid_cycles());
        }
        printf("%8llu: ",system_cycles); chip.debug_one();
        //chip.incr_cycle();
        clock.incr_cycles();

    }
    return 0;
}