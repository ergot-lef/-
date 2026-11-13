# -

*[English version](README.md)*

Un monde sans intention pour PS Vita.

Il n'y a ni but, ni récompense, ni scénario. On sème un monde, on le laisse tourner, et on regarde ce qu'il devient. Peut-être rien. Personne ne le sait à l'avance, pas même l'auteur du code.

## Ce qu'est le monde

- Un seul bloc de mémoire de 522 240 octets, découpé en 8 160 bandes de 64 octets posées sur une grille de 120 × 68.
- **Chaque octet est un pixel.** Une bande est un carré de 8 × 8 pixels, et la grille entière occupe l'écran (960 × 544). Rien n'est dessiné : l'écran *est* la mémoire.
- Au départ, tout est du bruit, tiré d'une **graine** issue du hasard matériel de la console mélangé au temps et au bruit des capteurs. Aucune partie ne peut être rejouée.

### La physique de départ

À chaque époque, chaque bande rencontre une voisine proche (distance ≤ 2). Les deux bandes sont collées bout à bout (128 octets) puis **exécutées comme un programme**, dans un petit langage à deux têtes de lecture : déplacer une tête, modifier un octet, copier d'une tête à l'autre, boucler. Puis elles sont séparées. Quelques octets changent aussi au hasard.

Le sens de chaque valeur d'octet (« quelle instruction est-ce ? ») est rangé dans une **table qui fait partie du monde**. Au départ, seules dix valeurs tirées au hasard signifient quelque chose. Mais la table vit, mute et peut être réécrite comme le reste : **la physique elle-même peut changer**.

### Les branchements

Quelques bandes sont simplement reliées à l'extérieur. Elles ne sont pas protégées et vivent comme les autres :

| Bandes | Rôle |
|---|---|
| 0–3 | la table d'instructions (la physique) |
| 4–15 | la palette : la couleur de chaque valeur d'octet |
| 16 | l'entrée : écrans tactiles, boutons, sticks, accéléromètre, gyroscope, micro, heure, batterie, réécrits à chaque image |
| dernière ligne | le son : 7 680 octets joués en boucle à 8 000 Hz |

## Sur la console

- **START** (hors du monde) : pause / reprise.
- En pause, l'image s'assombrit et les courbes du journal s'affichent, de haut en bas : entropie, diversité, densité d'instructions, activité, son.
- En pause, **maintenir TRIANGLE** (3 s) sème un nouveau monde. L'ancien est perdu.
- Le monde est sauvegardé toutes les 2 minutes et à chaque pause, et reprend au lancement.

Au départ, la zone son contient du bruit aléatoire : c'est normal. Baisse le volume si besoin.

### Fichiers (`ux0:data/-/`)

- `monde.bin` : l'état complet du monde.
- `journal.csv` : une ligne toutes les 10 secondes, uniquement des nombres (heure, époque, entropie, diversité, densité, activité, son). Aucune interprétation.

## Compiler

La compilation se fait automatiquement sur GitHub à chaque modification : le `.vpk` est publié dans les *Releases*.

À la main, avec [VitaSDK](https://vitasdk.org) :

```sh
cmake -S . -B build && cmake --build build
```

Version PC sans écran, pour expérimenter la physique :

```sh
gcc -O2 -o pc_sim tools/pc_sim.c src/world.c -lm
./pc_sim 5000 images/     # époques, dossier facultatif pour des captures PPM
```

## Inspiration

Ce monde s'inspire des expériences de vie artificielle où des programmes capables de se recopier apparaissent spontanément dans une soupe aléatoire (Tierra, Avida et *Computational Life*, Agüera y Arcas et al., 2024). Rien ne garantit qu'il se passe la même chose ici.
