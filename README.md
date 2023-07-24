# d64make

Build custom WADs for embedding in a Doom 64 ROM.

This program works by merging custom resources into the base game resources to
create a new N64-compatible IWAD. Thus, an original copy of the game ROM is
required to use it. All known commercial versions of the N64 ROM are supported.
Data files from the 2020 remaster by Nightdive can also be used.

## Building

Install [Rust](https://www.rust-lang.org/tools/install), check out the
repository to a folder, then run cargo in the folder:

```
cargo install --path .
```

## Running

```
d64make --help
# inspect data in a ROM or IWAD
d64make inspect ROM_OR_IWAD
# extract ROM or IWAD into editable PC formats
d64make extract ROM_OR_IWAD -o OUT_DIR
# build PC format data into N64 format WAD
d64make build WORK_DIR -o path/to/DOOM64.WAD
```

### Supported Base Files

| Source | SHA-256 Hash |
|-:|-|
| Doom 64 (U) (V1.0) [!].z64 | d3404a7e8ca9d20ba034651932e67aa90c6c475c5f4738f222cd1e3056df935f |
| Doom 64 (U) (V1.1) [!].z64 | c28eaac9a8a8cc1d30c1b50fbb04622c2ddeb9b14ddcecc6edbaad4a6d067f3f |
| Doom 64 (E) [!].z64        | e8460f2fa7e55172a296a1e30354cbb868be924a454ff883d1a6601c66b9610f |
| Doom 64 (J) [!].z64        | 19ad4130f8b259f24761d5c873e2ce468315cc5f7bce07e7f44db21241cef4a9 |
| DOOM64.WAD                 | 05ec0118cc130036d04bf6e6f7fe4792dfafc2d4bd98de349dd63e2022925365 |
| DOOMSND.DLS                | 88814285dea4cf3b91fd73c0195c55512725600531689c5314a2559777e71b17 |

It's recommended to use the data from the 2020 remaster due to its higher
quality sound files.

### Modding

To simplify workflows, d64make only supports reading/merging WADs that are
already in the N64 data format. It does not support WADs in the PC Doom format.
To merge in custom resources, d64make can load and extract individual assets
from a directory tree with a specific structure.

| Folder | Formats | Notes |
|-:|-|-|
| DEMOS    | LMP |                                     |
| FLATS    | PNG |                                     |
| GRAPHICS | PNG | Menu graphics and font              |
| HUD      | PNG | HUD graphics and font               |
| MAPS     | WAD |                                     |
| MUSIC    | MID |                                     |
| PALETTES | PAL | Sprite palettes (RGB8, 256 entries) |
| SKIES    | PNG | Sky textures                        |
| SOUNDS   | WAV |                                     |
| SPRITES  | PNG | 8-bit or 4-bit indexed color        |
| TEXTURES | PNG | 4-bit indexed color                 |

```sh
d64make extract /path/to/DOOM64.WAD -o ./mymod
```

### Details

d64make is able to extract resources from the original data files and
convert assets back into the N64 formats. It will output four data files that
can be used to build the Doom 64 sources:

| Filename | Description |
|-|-|
| DOOM64.WAD | Graphics, Maps, Demos  |
| DOOM64.WDD | Audio Samples          |
| DOOM64.WSD | Audio Sequences        |
| DOOM64.WMD | Instrument Definitions |

```sh
d64make build ./mymod -o /path/to/DOOM64-RE/data/
```

