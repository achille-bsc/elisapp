*
# Norme de code Elisapp

Règles communes à tout le code C++ du projet. En cas de doute, faire comme le
code existant de `app/data/`.

## Nommage

- **snake_case** pour les variables, fonctions, fichiers et dossiers :
  `landform_get`, `weather_model.cpp`.
- **CONSTANTES EN MAJUSCULES** : `MARS_SOL_S`, `WEATHER_CACHE_SLOTS`.
- Types : `struct s_nom { ... } typedef t_nom;`, enums `enum e_nom { ... };`.
- Fonctions préfixées par leur module : `weather_get`, `grid_init`,
  `radiation_estimate`.
- Unité dans le nom quand une valeur en a une : `pressure_dpa` (dixièmes de
  pascal), `cell_mm`, `time_s`, `lon_udeg` (micro-degrés). Format virgule
  fixe dans le suffixe : `_q16`, `_q30`.

## Mise en forme

- Indentation de **2 espaces**, pas de tabulation.
- 80 colonnes visées.
- `return (valeur);` avec parenthèses.
- Sections d'en-tête commentées : `// Constants //`, `// Structures //`,
  `// Functions //`.
- Includes locaux relatifs : `#include "./coord.hpp"`, `"../core/coord.hpp"`.

## Nombres magiques

Tout ce qui peut porter un nom est nommé (constante `constexpr` en
majuscules). Un nombre peut rester littéral seulement si :

- sa signification est évidente (`0`, `1`, `/ 2` pour une moitié), ou
- un commentaire sur la ligne du dessus explique sa raison.

Les réglages modifiables du module data sont tous dans `app/data/config.hpp`.

## Vérifications à la compilation

- `static_assert` en **fin de fichier**, en grand nombre : tailles des
  structures, absence de padding, bornes des calculs (pas de débordement),
  valeurs de référence (CRC, cosinus...).
- Tout calcul intermédiaire qui pourrait dépasser 32 bits a son
  `static_assert` ou passe explicitement en 64 bits.

## Légèreté (module data)

- Le plus petit type entier qui suffit ; 64 bits seulement pour un produit
  intermédiaire qui le demande.
- Pas de tas, pas de stdio, pas de libm, pas d'exceptions, pas de flottant à
  l'exécution (tables calculées en `consteval`). Vérifié par
  `make check-embedded`.
- Cible : Linux (Snapdragon type Ingenuity). Build et tests sur Linux, voir
  `make test`.

## Git

- Commits : `THEMATIQUE | Message`, thématique en majuscules, message
  commençant par une majuscule. Exemples : `DATA | Add radiation model`,
  `TESTS | Cover the grid`, `BUILD | ...`, `TOOLS | ...`, `DOCS | ...`.
- Aucune attribution d'outil (pas de `Co-Authored-By`, pas de
  « Generated with »).
- Toute source externe (dataset, article, doc) va dans `SOURCES.txt`.
