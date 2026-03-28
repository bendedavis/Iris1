# Iris 1

Iris 1 is an 8-stage gate sequencer for Eurorack.

Each stage has a 3-position switch:
- `Off`
- `Probability`
- `100%`

The module takes an incoming clock and decides which stages will pass that clock through to the output.

## Controls

### Step Switches
Each of the 8 stages can be set to:
- `Off` for no output
- `Probability` to fire based on the knob
- `100%` to always fire

### Probability Knob
Sets the chance that any stage in `Probability` mode will fire.

### Mute Button
Short press toggles mute.

Hold for 1 second to enter sequence length and division selection mode.

In this mode:
- LEDs 1-8 show the selected sequence length
- the knob selects one of 32 settings total
- the first 8 positions are `divide 1`, lengths `1-8`
- the next 8 are `divide 2`, lengths `1-8`
- then `divide 3`, lengths `1-8`
- then `divide 4`, lengths `1-8`

Once entered, the mode stays active until the knob is left untouched for 4 seconds.

Holding the mute button for 6 seconds performs a full reset to default settings.

### Clock In
Any signal above roughly 1V advances the sequence.

### Reset In
A high pulse queues the next clock pulse to return the sequence to step 1.

Iris 1 starts in auto reset mode on power-up. In this mode, if the clock stops for longer than about 1.5 clock periods, the module queues a reset so the next received pulse starts on step 1.

As soon as a valid reset pulse is received at the reset input, Iris 1 switches to manual reset mode. In manual reset mode, the module no longer auto-resets when the clock stops. It will only reset from the reset input. This stays active until the module is power cycled.

### Gate Out
If the current stage is active, the incoming clock is passed to the output.

If the current stage is inactive, the output is held low.

The output pulse width always matches the incoming clock pulse width.

## Chaining Modules
Iris 1 has two 3-pin connectors on the back for chaining clock and reset between modules.

Connect from the right connector of one module to the left connector of the next when viewing the module from the front.
