# -

*[Version française](README.fr.md)*

A world without intent, for the PS Vita.

There is no goal, no reward, no script. You seed a world, let it run, and watch what it becomes. Maybe nothing. Nobody knows in advance, not even the author of the code.

## What the world is

- A single 522,240-byte block of memory, split into 8,160 tapes of 64 bytes laid on a 120 × 68 grid.
- **Every byte is a pixel.** A tape is an 8 × 8 pixel square, and the whole grid fills the screen (960 × 544). Nothing is drawn: the screen *is* the memory.
- At first everything is noise, drawn from a **seed** made of the console's hardware randomness mixed with time and sensor noise. No run can ever be replayed.

### Starting physics

Each epoch, every tape meets a nearby neighbour (distance ≤ 2). Both tapes are joined end to end (128 bytes) and **executed as a program** in a tiny two-head language: move a head, change a byte, copy from one head to the other, loop. Then they are split apart. A few bytes also change at random.

The meaning of each byte value ("which instruction is this?") lives in a **table that is part of the world**. At first only ten randomly chosen values mean anything. But the table lives, mutates and can be overwritten like everything else: **the physics itself can change**.

### Wiring

A few tapes are simply connected to the outside. They are not protected and live like the others:

| Tapes | Role |
|---|---|
| 0–3 | the instruction table (the physics) |
| 4–15 | the palette: the colour of each byte value |
| 16 | input: touch panels, buttons, sticks, accelerometer, gyroscope, microphone, time, battery, rewritten every frame |
| last row | sound: 7,680 bytes looped at 8,000 Hz |

## On the console

- **START** (outside the world): pause / resume.
- While paused, the picture dims and the log curves appear, top to bottom: entropy, diversity, instruction density, activity, sound.
- While paused, **hold TRIANGLE** (3 s) to seed a new world. The old one is lost.
- The world is saved every 2 minutes and on every pause, and resumes at launch.

At first the sound area holds random noise: that's expected. Lower the volume if needed.

### Files (`ux0:data/-/`)

- `monde.bin`: the full world state.
- `journal.csv`: one line every 10 seconds, numbers only (time, epoch, entropy, diversity, density, activity, sound). No interpretation.

## Building

The build runs automatically on GitHub on every change: the `.vpk` is published in *Releases*.

By hand, with [VitaSDK](https://vitasdk.org):

```sh
cmake -S . -B build && cmake --build build
```

Headless PC version, to experiment with the physics:

```sh
gcc -O2 -o pc_sim tools/pc_sim.c src/world.c -lm
./pc_sim 5000 images/     # epochs, optional folder for PPM snapshots
```

## Inspiration

This world draws on artificial-life experiments where self-copying programs appear spontaneously in a random soup (Tierra, Avida, and *Computational Life*, Agüera y Arcas et al., 2024). Nothing guarantees the same will happen here.
