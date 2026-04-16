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

## When does change to I inhibit bit take effect

https://gemini.google.com/app/fce7ab356521bfbd

Answer: on the second cycle of a CLI or SEI. so the IRQ check has already occurred during cycle 1. So we must make sure the change to the I flag on these instructions happens in the correct place - i.e., AFTER the phantom_read.

Hmm, BaseCPU::incr_cycles doesn't have access to the cpu struct. I either have to pass cpu to calls to incr_cycles, or, store it in the class parameters.

Instead of storing the 64-bit vector, cpu should just have a bool IRQ asserted input, like a real cpu.

I have a bunch of cross-dependency between BaseCPU and cpu_state. I should do something about that.

cpu_state->core is pretty much unused; deprecate


# Testing Approach

ok we've got a simple harness that is a sequence of 'instructions' that load a 6522 register at a certain cycle. then we just loop; display registers, tick cycle.

So now I guess we can take this, do a 2nd one, insert a cpu with an nclock with a special incr_cycle that ticks the 6522. Then I can run this  test code against it.

I implemented the 2-cycle lookback, So none of this so far has made any difference in the operation of irqtimetest. It still prints a single 0001 and then exits. and I lost 12eMHz. Need to run this test on real hardware, because it's not clear that he has done that.
