# MB Dev Notes

So the AppleWin 6522 code looks a lot like what I was doing. Calculating event timers to fire in the future, calculating what the counter's value is expected to be at a certain point, this is complex. Proceed with trying it out cycle-by-cycle.

At least, use the cycle-by-cycle to validate operation, develop tests, then we can use the tests against the more complex version.

## One-shot mode

in the one-shot mode, when the counter is set (to N), it counts down to 0, then rolls to FFFF.
Then continues to FFFE, etc. because the counter does not reset, and an IRQ is generated only once.

Both data sheets (R and WDC) imply it resets to N in one-shot mode in their figures, but that is not actually the case.

## Free-run mode

in free-run mode (T1 only) the counter is set (to N), it counts to 0, then rolls to FFFF.
it is then reset to N and repeats.

The figures that claim a T1 reset on one-shot must actually refer to free-run.

## T2 Latch - only 8 bits

the T2 latch appears to be only 8 bits wide.
because T2 is one-shot only, a typical sequence like this:
write T2L-L, write T2C-H causes T2L-L to be put into T2C-L.

$08 read: T2C-L
$08 write: T2L-L

Previous code erroneously pretends there is a hi byte latch. It did not matter in practice as we ignored the "latch hi byte" when setting the counter. But modified it to be clearer.

## Counter op: either decrement, or load

In any given cycle, a counter may either be decremented, or may be loaded. 
I.e., if we are in free-run and we have hit FFFF, we reload to the latch value, but DO NOT decrement this cycle.
If we are in one shot, and we have hit FFFF, we -do- decrement.

## When does 6502 check for IRQ status

https://gemini.google.com/app/fce7ab356521bfbd

So all the documentation I've seen says the cpu checks IRQ level at the second-to-last cycle of an instruction; or, basically, the level at the start of the last cycle. This would be easy to track by maintaining a 2-cycle (or 1?) sliding window in cpux->incr_cycle of the IRQ status and then using that as the "IRQ asserted" test instead of the current value.

IMPORTANT: The sampling is of the IRQ line AND the inhibit flag. So that's what needs to be stored into the sliding window.

in cpux->incr_cycle, we do:
    irq_sample1 = irq_sample0
    irq_sample0 = !I & IRQ

at the top of an instruction, we do:
    if irq_sample1 .. do IRQ

This is AppleWin discussing the same thing:
https://github.com/AppleWin/AppleWin/issues/718

## When does change to I inhibit bit take effect

https://gemini.google.com/app/fce7ab356521bfbd

Answer: on the second cycle of a CLI or SEI. so the IRQ check has already occurred during cycle 1. So we must make sure the change to the I flag on these instructions happens in the correct place - i.e., AFTER the phantom_read.

Hmm, BaseCPU::incr_cycles doesn't have access to the cpu struct. I either have to pass cpu to calls to incr_cycles, or, store it in the class parameters.

Instead of storing the 64-bit vector, cpu should just have a bool IRQ asserted input, like a real cpu.

I have a bunch of cross-dependency between BaseCPU and cpu_state. I should do something about that.

cpu_state->core is pretty much unused; deprecate

# Comparing Production 6522 to Test Harness 6522

W6522 - cycle-at-a-time
N6522 - callback-based

Next step is to harness the existing 6522 as-is to see how it compares to the cycle-by-cycle one.

I did that - I took the code and rationalized it as a class with the same interface as W6522, it's called N6522.

It was a full-throated hairy mess! Lots of things were wrong - counter values on reset were wrong, interrupts triggered at wrong time, no accounting for rollover to FFFF.

I got everything fixed up, though, and it seems to now be producing exactly the same results as W6522. And the code was drastically cleaned up and simplified, it even starts to make sense. LOLZ.



# Testing Approach

ok we've got a simple harness that is a sequence of 'instructions' that load a 6522 register at a certain cycle. then we just loop; display registers, tick cycle.

So now I guess we can take this, do a 2nd one, insert a cpu with an nclock with a special incr_cycle that ticks the 6522. Then I can run this  test code against it.

I implemented the 2-cycle lookback, So none of this so far has made any difference in the operation of irqtimetest. It still prints a single 0001 and then exits. and I lost 12eMHz. Need to run this test on real hardware, because it's not clear that he has done that.

## arrekusu - iigs irq test now largely failing.

main fails the QTR and 1 sec IRQ tests.
mbpercycle additionally fails the SCB and VBL tests.
I have changed behavior likely for RTI and PLP - check those routines to see if they are being modified in the correct cycles ("these should operate immediately").

## issue 94

The op insists if an IRQ is pending before CLI, and you CLI then SEI that the SEI should be interrupted. i.e. should go from CLI to IRQ.
his test program does indeed count from 1 to 100 in applewin.
is it 100 hex, not 100 decimal, so 256 interrupts.
likely what is expected here is that immediately after RTI it should trigger again.
yah yah. So it's turning the interrupt off after executing the IRQ routine x100 times.
Which means it's expecting:
interrupt immediately after CLI
and then again immediately after RTI

OK I can produce his desired results if I:
sense IRQ in 2nd to last cycle
gate with inhibit at top of instruction
this also makes arrekusu SCB and VBL tests pass again.
(the QTR and SEC are still borked so that is something else).
And I'm at 258MHz peak, so I have only lost a few MHz at this point.

The question remains: has his test program been run on actual hardware and has the behavior above been demonstrated to be correct?
(this does not resolve the Deater demo chiptunes player issues, which are likely more related to 6522 timing issues).

## mb-audit

Correctly identifies the mockingboard with two 6522 chips.
fails Test 11:04:00, Expected F1 Actual F0
this is a counter that's off by one.

Tom C isn't confident he knows the correct answers either! We need to do some science.

# New Tests

## Behavior of CLI/SEI

### IIe + Mockingboard

on a //e with Mockingboard, we can use the test program I was sent.

1. SEI
1. set up mockingboard to generate an interrupt
1. CLI and immediate SEI.

If the SEI executes, the IRQ will fire once. If the SEI does not execute, the IRQ will fire until we turn it off in the interrupt handler.

### IIgs

1. Wait until VBL clears
1. SEI
1. set up most direct IRQ vector possible.
1. enable VBL interrupt
1. spin until VBL sets
1. the VBL interrupt should now be active
1. CLI / SEI

Same test results as above.

## Behavior of RTI

Variations of the above, where we want to see if a) at least one instruction executes after an RTI before going back into IRQ handler, or b) the IRQ immediately fires after the RTI meaning unless you clear the source you will loop forever.


* Variation T1: only IRQ is sampled at penultimate cycle.

This passes both GS-IRQ test and the MB-IRQ test.

* Variation T2: both IRQ and I are sampled at penultimate cycle.

This fails both GS-IRQ test and MB-IRQ test.
However, this appears to be what perfect6502 is doing.



# Test Log

## GStest

real IIgs:
    system speed set to 1MHz
    get: 0001

## mbtest

IIe Enh/Mockingboard/65C02

IIe/Mockingboard/N6502

Xot emulator: 0001-0100

perfect6502: the SEI is executed, IRQ after that
