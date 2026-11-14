# Module data

Fournit au front et au pathfinding tout ce qu'il faut savoir sur un point de
Mars : relief, météo locale, radiation, danger. Un seul include :
`#include "data/data.hpp"`.

## Organisation

| Dossier | Rôle |
|---|---|
| `config.hpp` | tous les réglages (seuils de danger, seuil critique radiation, modulation solaire, cache) |
| `core/` | coordonnées (`t_coord`, conversions E7), CRC16, little endian, maths en virgule fixe |
| `link/` | protocole radio du satellite météo (encodage, décodage, trames) |
| `landform/` | relief en couches (CTX ~20 m + MOLA 1,85 km), lecture seule : altitude et pente |
| `weather/` | cache satellite (`weather.*`) et estimation locale (`weather_model.*`) |
| `radiation/` | estimation de dose à partir de la pression locale |
| `grid/` | grille identique à celle du pathfinding (marspath `CoarseGrid`) |
| `hazard/` | zones de danger : évaluation d'un point, carte sur la grille, trajet (normes NASA) |
| `gathering.*` | point d'entrée : `data_gathering`, `data_is_hazard` |
| `sim/` | satellite simulé, PC et tests uniquement (hors check embarqué) |

## Démarrage

```cpp
landform_init(mola_blob, mola_size);       // couche MOLA de tools/fetch_landform.py
landform_add_layer(ctx_blob, ctx_size);    // couche CTX : prioritaire là où elle a des données
weather_set_link(weather_sat_sim);         // ou le vrai lien radio

const t_coord p = coord_from_e7(lat_e7, lon_e7);         // GeoPoint du pathfinding
const t_data_point d = data_gathering(p, time_s, DATA_ALL);
const bool danger = data_is_hazard(p, time_s);
```

`time_s` : secondes de mission depuis un minuit au méridien 0 (comme le temps
coordonné martien). Pour un long trajet, demander chaque point à son heure de
passage : le cycle diurne (pression, température, donc radiation) est
appliqué.

## Brancher le pathfinding (marspath)

```cpp
marspath::TerrainAdapter terrain([](marspath::GeoPoint p) {
  return static_cast<float>(landform_get(coord_from_e7(p.lat_e7, p.lon_e7)).elevation_m);
});
marspath::RiskMapAdapter risk([time_s](marspath::GeoPoint p) {
  return data_is_hazard(coord_from_e7(p.lat_e7, p.lon_e7), time_s);
});
```

La grille `grid_init(&grid, start, goal)` donne les mêmes cases que la grille
grossière de marspath : `grid_cell_coord`, `grid_index_of`, `grid_neighbor`
(mêmes 8 directions), `grid_distance_mm`, `grid_cell_data`,
`grid_cell_is_hazard`. Taille de case : `max(100 m, 1,2 × distance / 512)`.
Si les `GridParams` de marspath changent, mettre à jour les `GRID_*` de
`grid/grid.hpp`.

## Mélange CTX + MOLA

`landform_get` lit la couche la plus fine qui a une donnée au point (les
couches sont triées par pas, quel que soit l'ordre d'ajout) ; un trou CTX
(`LANDFORM_NODATA_M`) ou un point hors CTX retombe sur MOLA. Les deux sont
calés sur le géoïde MOLA. `t_landform.layer` dit d'où vient la valeur.
Préparer une couche CTX : voir l'aide de `tools/fetch_landform.py` (GDAL :
`gdalwarp` sur un pas entier de micro-degrés, puis `gdal_translate -of ENVI`).

## Cache météo : allocation et préchargement

```cpp
t_grid grid;
grid_init(&grid, start, goal);
std::vector<uint8_t> cache(weather_cache_bytes(grid_weather_chunk_count(&grid)));
weather_set_cache(cache.data(), static_cast<uint32_t>(cache.size())); // l'appli alloue
grid_weather_prefetch(&grid);                    // 0 = l'A* n'attendra jamais la radio
```

Éviction LRU. Sans `weather_set_cache`, cache interne de 16 chunks (7,5 Ko).

## Zones de danger

```cpp
const t_hazard h = hazard_assess(p, time_s);  // niveau + raisons (bits) + dose
hazard_map(&grid, time_s, levels, capacity);  // un niveau par case : la carte
const t_route_hazard r = hazard_route(points, n, depart_s, 0); // trajet à pied
hazard_set_spe_alert(true);                   // alerte éruption solaire : abri
```

| Niveau | Raisons |
|---|---|
| danger | terrain inconnu, pente > 20° (xEVA), vent/poussière, radiation critique, alerte SPE, budget de dose de carrière dépassé (trajet) |
| prudence | pente > 15°, pente lue sur MOLA seul, radiation > 80 % du seuil (ALARA), EVA > 8 h, trajet > 100 km |

Seuil critique radiation = `(600 mSv − dose déjà reçue) / durée du séjour`
(NASA-STD-3001 Vol. 1 Rev B [V1 4030]) ; régler `RAD_CREW_DOSE_RECEIVED_USV`
et `RAD_SURFACE_STAY_SOLS` dans `config.hpp`.

## Modèles

- **Pression locale** : le satellite donne la pression ramenée au géoïde
  (0 m) ; `P = P0 × exp(−z / 11,1 km)` avec l'altitude MOLA du point, puis
  marée thermique ±5 % selon l'heure solaire locale.
- **Température** : valeur satellite plus le cycle jour/nuit (amplitude et
  heure du pic approximatives, voir `config.hpp`).
- **Radiation** : `D = D01 + κ (P − 840 Pa) + β Φ` (Guo et al. 2015).
  Plus d'air au-dessus = moins de dose.
- Tout est en entiers : aucune libm, aucun flottant à l'exécution.

## Vérifier

```sh
make test                                   # check embarqué + tests (ASan, UBSan)
make test CROSS=arm-linux-gnueabihf- TEST_SAN= \
     TEST_RUN="qemu-arm -L /usr/arm-linux-gnueabihf"   # cible ARMv7
python3 tests/make_synthetic_mola.py /tmp/s.img        # outil Python de bout en bout
python3 tools/fetch_landform.py --src /tmp/s.img --out /tmp/zone.bin
python3 tests/make_synthetic_mola.py --ctx /tmp/ctx.raw
python3 tools/fetch_landform.py --grid /tmp/ctx.raw --width 400 --height 300 \
     --west 77.4 --north 18.5 --step 0.000337 --out /tmp/ctx.bin
make test ZONE=/tmp/zone.bin ZONE_FINE=/tmp/ctx.bin
```

Sources des constantes : `SOURCES.txt` à la racine. Norme : `NORME.md`.
