# -

*[English version](README.md)*

Trois petites expériences d'intelligence et de vie artificielles pour PS Vita (hackée, HENkaku).

| Dossier | Application | En une phrase |
|---|---|---|
| [`monde/`](monde) | **-** | Un monde sans intention : de la mémoire brute où des programmes apparaissent, se recopient et s'affrontent tout seuls. |
| [`jardin/`](jardin) | **Jardin** | Des créatures avec un vrai petit cerveau de neurones, qui doivent manger, survivre aux hivers et se reproduire. |
| [`ecrivain/`](ecrivain) | **Écrivain** | Un petit réseau de neurones qui lit un roman de Jules Verne en boucle et apprend à écrire en français. |

Chaque modification poussée sur `main` déclenche la compilation sur GitHub : les trois `.vpk` sont publiés dans les *Releases*. Il suffit de les copier sur la Vita et de les installer avec VitaShell.

Le code commun (dessin à l'écran, police avec accents) est dans [`common/`](common).

---

## - (monde)

Il n'y a ni but, ni récompense, ni scénario. On sème un monde, on le laisse tourner, et on regarde ce qu'il devient. Peut-être rien.

- Un seul bloc de mémoire de 522 240 octets, découpé en 8 160 bandes de 64 octets posées sur une grille de 120 × 68. **Chaque octet est un pixel** : l'écran *est* la mémoire.
- Au départ, tout est du bruit, tiré du hasard matériel de la console. Aucune partie ne peut être rejouée.
- À chaque époque, chaque bande rencontre une voisine. Les deux sont collées bout à bout et **exécutées comme un programme** (déplacer une tête, modifier un octet, copier, boucler), puis séparées. Quelques octets changent aussi au hasard.
- Le sens des instructions est rangé dans une **table qui fait partie du monde** : la physique elle-même peut être réécrite.
- Quelques bandes sont reliées à l'extérieur, sans protection : la table d'instructions (bandes 0–3), la palette des couleurs (4–15), l'entrée des capteurs (16 : tactile, boutons, sticks, mouvement, micro, heure, batterie) et le son (dernière ligne, jouée en boucle à 8 000 Hz).

**Commandes** : START met en pause et affiche les courbes du journal (entropie, diversité, densité d'instructions, activité, son). En pause, maintenir △ pendant 3 s sème un nouveau monde.
**Fichiers** : `ux0:data/-/monde.bin` (l'état complet) et `journal.csv` (des mesures toutes les 10 s, sans interprétation).

Inspiration : Tierra, Avida et *Computational Life* (Agüera y Arcas et al., 2024), où des programmes capables de se recopier apparaissent spontanément dans une soupe aléatoire.

---

## Jardin

Des créatures dans un monde qui leur résiste. Personne ne leur dit quoi faire : ce qui marche se transmet, le reste disparaît.

- **Le sol** : la nourriture repousse selon la fertilité du terrain et **la saison** (abondante l'été, rare l'hiver). Une année dure environ 3 minutes 20 en vitesse ×1.
- **Le cerveau** de chaque créature est un petit réseau de neurones avec mémoire :
  - 9 entrées : nourriture et voisins à gauche, en face et à droite, son énergie, un peu de hasard, une constante ;
  - de 1 à 16 neurones cachés, qui écoutent aussi leur propre état au tour précédent ;
  - 3 sorties : tourner, avancer, attaquer.
- **Vivre coûte** : bouger, attaquer, et même **penser** (chaque neurone consomme un peu d'énergie). Un gros cerveau doit donc rapporter plus qu'il ne coûte.
- **Avoir un petit** : au-delà d'un seuil d'énergie. Le cerveau du petit est une copie avec des mutations, et il peut gagner ou perdre un neurone.
- **Attaquer** prend de l'énergie à la créature d'en face. Rien n'oblige à chasser : si des prédateurs apparaissent, c'est que l'évolution les a trouvés.
- À zéro d'énergie, la créature meurt et nourrit le sol. Si tout le monde meurt, de nouvelles créatures arrivent au hasard (compté comme « extinction »).

**Commandes** : toucher une créature pour voir **son cerveau penser en direct** (entrées, neurones, sorties, force des connexions). L / R pour ralentir ou accélérer le temps (×1 à ×1000). START pour la pause et les courbes : population, taille moyenne des cerveaux, chasse, nourriture. Maintenir △ pendant 3 s sème un nouveau jardin.
**Fichier** : `ux0:data/jardin/jardin.bin`, sauvegardé toutes les 2 minutes et à chaque pause.

---

## Écrivain

Le principe des grands modèles de langage, en minuscule : **deviner la lettre suivante**.

- Le réseau regarde les **8 dernières lettres**. Chacune devient 12 nombres (son « plongement »), qui passent dans **192 neurones cachés**, puis une couche de sortie donne une probabilité pour chacune des 96 lettres possibles.
- Il lit en boucle *Le Tour du monde en quatre-vingts jours* de Jules Verne (Project Gutenberg, domaine public). À chaque lettre, il compare sa prédiction à la vraie lettre et **corrige un peu tous ses poids** (rétropropagation).
- Au début, il écrit du charabia (« xkqz ptl »), puis des syllabes, puis des mots, puis des bouts de phrases qui ont l'air du français sans vouloir rien dire.

**À l'écran** : ce qu'il écrit en direct ; les 8 lettres qu'il regarde ; ses 192 neurones qui s'allument (vert positif, rouge négatif) ; ses hésitations (les 4 lettres les plus probables) ; sa courbe d'erreur en bits par lettre (6,6 au hasard pur, environ 2,7 après quelques millions de lettres sur PC).
**Commandes** : X pour une nouvelle page, ↑↓ pour régler l'audace (température : prudent ou créatif), START pour la pause, △ maintenu 3 s pour tout oublier.
**Fichiers** (`ux0:data/ecrivain/`) : `cerveau.bin` (ses poids) et `carnet.txt`, un extrait de ce qu'il sait écrire **toutes les 10 minutes**, pour relire ses progrès le matin.

---

## Compiler à la main

Avec [VitaSDK](https://vitasdk.org) :

```sh
# le livre de l'Écrivain
curl -L -o pg800.txt https://www.gutenberg.org/cache/epub/800/pg800.txt
python3 ecrivain/tools/prepare_livre.py pg800.txt ecrivain/livre.txt

for app in monde jardin ecrivain; do cmake -S $app -B build/$app && cmake --build build/$app; done
```

Versions PC sans écran, pour expérimenter :

```sh
gcc -O2 -o pc_sim     monde/tools/pc_sim.c monde/src/world.c -lm          # ./pc_sim 5000 images/
gcc -O2 -o pc_garden  jardin/tools/pc_garden.c jardin/src/garden.c -lm    # ./pc_garden 240000
gcc -O3 -o pc_train   ecrivain/tools/pc_train.c ecrivain/src/model.c -lm  # ./pc_train ecrivain/livre.txt 3000000
```
