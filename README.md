# -

*[Version française](README.fr.md)*

Four small artificial life and artificial intelligence experiments for the PS Vita (hacked, HENkaku).

| Folder | App | In one sentence |
|---|---|---|
| [`monde/`](monde) | **-** | A world without intent: raw memory where programs appear, copy themselves and compete on their own. |
| [`jardin/`](jardin) | **Jardin** (garden) | Creatures with a real little neural brain that must eat, survive winters and reproduce. |
| [`ecrivain/`](ecrivain) | **Écrivain** (writer) | A tiny neural network that reads a Jules Verne novel over and over and learns to write French. |
| [`evasion/`](evasion) | **Évasion** (escape) | Jointed stick figures locked in a cell learn on their own to move, get out and clear obstacles. |

Every push to `main` triggers a build on GitHub: the four `.vpk` files are published under *Releases*. Copy them to the Vita and install them with VitaShell.

Shared code (screen drawing, a font with French accents) lives in [`common/`](common).

---

## - (monde)

There is no goal, no reward, no script. You seed a world, let it run, and watch what it becomes. Maybe nothing.

- A single 522,240-byte block of memory, split into 8,160 tapes of 64 bytes on a 120 × 68 grid. **Every byte is a pixel**: the screen *is* the memory.
- At first everything is noise, drawn from the console's hardware randomness. No run can ever be replayed.
- Each epoch, every tape meets a neighbour. Both are joined end to end and **executed as a program** (move a head, change a byte, copy, loop), then split apart. A few bytes also change at random.
- The meaning of each instruction lives in a **table that is part of the world**: the physics itself can be rewritten.
- A few tapes are wired to the outside, unprotected: the instruction table (tapes 0–3), the colour palette (4–15), sensor input (16: touch, buttons, sticks, motion, microphone, time, battery) and sound (last row, looped at 8,000 Hz).

**Controls**: START pauses and shows the log curves (entropy, diversity, instruction density, activity, sound). While paused, hold △ for 3 s to seed a new world.
**Files**: `ux0:data/-/monde.bin` (full state) and `journal.csv` (measurements every 10 s, no interpretation).

Inspiration: Tierra, Avida, and *Computational Life* (Agüera y Arcas et al., 2024), where self-copying programs appear spontaneously in a random soup.

---

## Jardin (garden)

Creatures in a world that pushes back. Nobody tells them what to do: what works is passed on, the rest disappears.

- **The ground**: food regrows depending on soil fertility and **the season** (plentiful in summer, scarce in winter). A year lasts about 3 min 20 s at ×1 speed.
- **Each creature's brain** is a small neural network with memory:
  - 9 inputs: food and neighbours to the left, ahead and to the right, its energy, a bit of noise, a constant;
  - 1 to 16 hidden neurons, which also listen to their own previous state;
  - 3 outputs: turn, move, attack.
- **Living costs energy**: moving, attacking, and even **thinking** (each neuron burns a little). A bigger brain must pay for itself.
- **Having offspring** happens above an energy threshold. The child's brain is a mutated copy, and may gain or lose a neuron.
- **Attacking** drains energy from the creature in front. Nothing forces hunting: if predators appear, evolution found them.
- At zero energy a creature dies and feeds the soil. If everyone dies, new random creatures arrive (counted as an "extinction").

**Controls**: touch a creature to watch **its brain think live** (inputs, neurons, outputs, connection strengths). L / R to slow down or speed up time (×1 to ×1000). START for pause and curves: population, average brain size, hunting, food. Hold △ for 3 s to seed a new garden.
**File**: `ux0:data/jardin/jardin.bin`, saved every 2 minutes and on every pause.

---

## Écrivain (writer)

The principle behind large language models, in miniature: **guess the next letter**.

- The network looks at the **last 8 letters**. Each becomes 12 numbers (its "embedding"), which go through **192 hidden neurons**, then an output layer gives a probability for each of the 96 possible letters.
- It reads *Around the World in Eighty Days* by Jules Verne, in French (Project Gutenberg, public domain), over and over. At each letter it compares its guess to the real letter and **nudges all its weights** (backpropagation).
- At first it writes gibberish, then syllables, then words, then fragments of sentences that look French without meaning anything.

**On screen**: what it writes, live; the 8 letters it looks at; its 192 neurons lighting up (green positive, red negative); its hesitations (top 4 letters); its error curve in bits per letter (6.6 for pure chance, about 2.7 after a few million letters on PC).
**Controls**: X for a new page, ↑↓ to set boldness (temperature), START to pause, hold △ for 3 s to forget everything.
**Files** (`ux0:data/ecrivain/`): `cerveau.bin` (its weights) and `carnet.txt`, a sample of its writing **every 10 minutes**, to read its progress in the morning.

---

## Évasion (escape)

Nobody teaches them to move: they have to work it out.

- **The body**: an 11-point stick figure (head, neck, hip, knees, ankles, heels, toes) with gravity, friction and fixed-length bones.
- **The muscles**: hips and knees. At every moment the brain picks the target angle of each joint. Muscles change the posture but **do not create momentum out of thin air**: to move forward, you must push against the ground. (A first version of the physics let them fly by kicking: evolution found the loophole within a few generations.)
- **The brain**: 15 inputs (joint angles, torso lean, height, speed, feet on the ground, a rhythm, distance, height and width of the next obstacle), 12 neurons, 4 outputs.
- **The course**: a cell with a half-open door, then steps and pits that get harder.
- **Evolution**: 40 figures try at once. The ones that get furthest parent the next generation (two brains mixed, plus mutations).
- **The rule**, switched with SELECT:
  - *free*: they may fall and keep going. In PC tests, evolution quickly invents **crawling on its back**, easier to discover than walking;
  - *strict*: the head touching the ground eliminates the figure. Evolution must find balance: much harder and much slower.

**Controls**: L / R for speed, □ to show the leader's brain, SELECT to switch the rule, START for pause and the progress curve, hold △ for 3 s to start over.
**File**: `ux0:data/evasion/evasion.bin`.

---

## Building by hand

With [VitaSDK](https://vitasdk.org):

```sh
# the writer's book
curl -L -o pg800.txt https://www.gutenberg.org/cache/epub/800/pg800.txt
python3 ecrivain/tools/prepare_livre.py pg800.txt ecrivain/livre.txt

for app in monde jardin ecrivain evasion; do cmake -S $app -B build/$app && cmake --build build/$app; done
```

Headless PC versions, to experiment:

```sh
gcc -O2 -o pc_sim     monde/tools/pc_sim.c monde/src/world.c -lm          # ./pc_sim 5000 images/
gcc -O2 -o pc_garden  jardin/tools/pc_garden.c jardin/src/garden.c -lm    # ./pc_garden 240000
gcc -O3 -o pc_train   ecrivain/tools/pc_train.c ecrivain/src/model.c -lm  # ./pc_train ecrivain/livre.txt 3000000
gcc -O2 -o pc_evasion evasion/tools/pc_evasion.c evasion/src/sim.c -lm    # ./pc_evasion 300
```
