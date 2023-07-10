use crate::{
    extract::read_rom_or_iwad,
    gfx, invalid_data,
    sound::SoundData,
    wad::{EntryMap, FlatEntry},
    Compression, EntryName, FlatWad, LumpType, Wad, WadEntry,
};
use std::{
    collections::BTreeMap,
    io,
    path::{Path, PathBuf},
};

#[derive(clap::Args)]
pub struct Args {
    /// Directories and ROMs to build into IWAD
    #[arg(required = true)]
    inputs: Vec<PathBuf>,
    /// IWAD file to output to [default: DOOM64.WAD]
    #[arg(short, long)]
    output: Option<PathBuf>,
    /// Glob patterns to exclude entry names
    #[arg(short, long)]
    exclude: Vec<String>,
    /// Do not recompress WAD data
    #[arg(long, default_value_t = false)]
    no_compress: bool,
    /// Do not generate WDD/WMD/WSD files
    #[arg(long, default_value_t = false)]
    no_sound: bool,
    /// Path to output WDD to [default: DOOM64.WDD]
    #[arg(long)]
    wdd: Option<PathBuf>,
    /// Path to output WMD to [default: DOOM64.WMD]
    #[arg(long)]
    wmd: Option<PathBuf>,
    /// Path to output WSD to [default: DOOM64.WSD]
    #[arg(long)]
    wsd: Option<PathBuf>,
}

fn load_entries(
    wad: &mut Wad,
    snd: &mut SoundData,
    path: impl AsRef<Path>,
    excludes: &[String],
    meta: Option<std::fs::Metadata>,
    base_typ: LumpType,
    depth: usize,
) -> io::Result<()> {
    use LumpType::*;

    let path = path.as_ref();
    let meta = match meta {
        Some(meta) => meta,
        None => path.metadata()?,
    };
    if meta.is_file() {
        let name = match path.file_stem() {
            Some(n) => n,
            None => return Ok(()),
        };
        let name_str = name.to_string_lossy();
        if name_str.starts_with('.') || name_str.len() > 8 {
            return Ok(());
        }
        if excludes
            .iter()
            .any(|g| glob_match::glob_match(g, &name_str))
        {
            return Ok(());
        }
        let ext = path
            .extension()
            .and_then(|s| s.to_str())
            .map(|e| e.to_ascii_uppercase());
        let mut typ = match (base_typ, ext.as_deref()) {
            (Sprite, Some("LMP") | Some("PAL")) => Palette,
            (Sequence, Some("SF2") | Some("DLS")) => SoundFont,
            (Unknown, Some("PNG")) => Graphic,
            (Unknown, Some("WAD")) => Map,
            (Unknown, _) if name_str.starts_with("MAP") => Map,
            (Unknown, _) if name_str.starts_with("DEMO") => Demo,
            _ => base_typ,
        };
        if typ == Sky {
            if name_str == "FIRE" {
                typ = Fire;
            } else if name_str == "CLOUD" {
                typ = Cloud;
            }
        }
        let data = std::fs::read(path)?;
        let is_png = ext.as_deref() == Some("PNG");
        let data = match (typ, is_png) {
            (Palette, _) if ext.as_deref() == Some("PAL") => {
                if data.len() >= 256 * 3 {
                    let mut palette = vec![0; 8 + 256 * 2];
                    palette[2] = 1;
                    gfx::palette_rgb_to_16(&data, &mut palette[8..]);
                    palette
                } else {
                    panic!("Palette {name_str} does not have enough entries");
                }
            }
            (Graphic | Fire | Cloud, true) => gfx::Graphic::read_png(&data, false)
                .map_err(invalid_data)?
                .to_vec(typ),
            (Texture | Flat, true) => gfx::Texture::read_png(&data)
                .map_err(invalid_data)?
                .to_vec(),
            (Sprite | HudGraphic | Sky, true) => gfx::Sprite::read_png(&data, None)
                .map_err(invalid_data)
                .unwrap()
                .to_vec(),
            _ => data,
        };
        match typ {
            Sample => {
                let id = name_str
                    .strip_prefix("SFX_")
                    .and_then(|n| str::parse(n).ok())
                    .unwrap_or_else(|| {
                        snd.sequences
                            .last_key_value()
                            .map(|p| *p.0 + 1)
                            .unwrap_or_default()
                    });
                let (_, sample) = crate::sound::Sample::read_wav(&data).unwrap_or_else(|_| {
                    panic!(
                        "Failed to load WAV file `{}`. WAV must be uncompressed 16-bit mono or 8-bit mono.",
                        path.display(),
                    );
                });
                snd.sequences
                    .insert(id, crate::sound::Sequence::Effect(sample));
            }
            SoundFont => {
                let res = if ext.as_deref() == Some("DLS") {
                    snd.read_dls(&data)
                } else {
                    snd.read_sf2(&data)
                };
                res.unwrap_or_else(|_| {
                    panic!("Failed to load SoundFont `{}`.", path.display(),);
                });
            }
            Sequence => {
                let id = name_str
                    .strip_prefix("MUS_")
                    .and_then(|n| str::parse(n).ok())
                    .unwrap_or_else(|| {
                        snd.sequences
                            .last_key_value()
                            .map(|p| *p.0 + 1)
                            .unwrap_or_default()
                    });
                let seq = crate::music::MusicSequence::read_midi(&mut std::io::Cursor::new(data))
                    .unwrap();
                snd.sequences.insert(id, crate::sound::Sequence::Music(seq));
            }
            _ => {
                let mut upper = name_str.replace('^', "\\");
                upper.make_ascii_uppercase();
                let name = EntryName::new(&upper).unwrap();
                let entry = WadEntry::new(typ, data);
                wad.merge_one(name, entry);
            }
        }
    } else if meta.is_dir() {
        let name = match path.file_name() {
            Some(n) => n,
            None => return Ok(()),
        };
        let lower = name.to_ascii_uppercase();
        let base_typ = if depth < 2 {
            match lower.to_str() {
                Some("SPRITES") => Sprite,
                Some("PALETTES") => Palette,
                Some("TEXTURES") => Texture,
                Some("FLATS") => Flat,
                Some("GRAPHICS") => Graphic,
                Some("HUD") => HudGraphic,
                Some("SKIES") => Sky,
                Some("MAPS") => Map,
                Some("SOUNDS") => Sample,
                Some("MUSIC") => Sequence,
                Some("DEMOS") => Demo,
                _ => base_typ,
            }
        } else {
            base_typ
        };
        let dir = std::fs::read_dir(path)?;
        for entry in dir.flatten() {
            let meta = match entry.metadata() {
                Ok(meta) => meta,
                Err(_) => continue,
            };
            let _ = load_entries(
                wad,
                snd,
                entry.path(),
                excludes,
                Some(meta),
                base_typ,
                depth + 1,
            );
        }
    }
    Ok(())
}

#[inline]
fn is_map_wad(path: &impl AsRef<Path>) -> bool {
    if let Some(stem) = path.as_ref().file_stem() {
        stem.to_string_lossy()
            .to_ascii_uppercase()
            .starts_with("MAP")
    } else {
        false
    }
}

impl FlatWad {
    pub fn compress(&mut self) {
        for entry in &mut self.entries {
            if entry.entry.compression != Compression::None {
                continue;
            }
            let compression = entry.entry.typ.compression();
            match compression {
                Compression::Lzss(_) => {
                    if let Ok(data) = crate::compression::encode_jaguar(&entry.entry.data) {
                        let origsize = entry.entry.data.len();
                        if data.len() < origsize {
                            entry.entry.data = data;
                            entry.entry.compression = Compression::Lzss(origsize);
                        }
                    }
                }
                Compression::Huffman(_) => {
                    continue;
                    /*
                    // broken for now, very slow and crashes so just disable it
                    if let Ok(data) = crate::compression::encode_d64(&entry.entry.data) {
                        println!("compressing huff {} -> {}", entry.entry.data.len(), data.len());
                        let origsize = entry.entry.data.len();
                        if data.len() < origsize {
                            entry.entry.data = data;
                            entry.entry.compression = Compression::Huffman(origsize);
                        }
                    }
                    */
                }
                Compression::None => continue,
            }
        }
    }
    pub fn write(&self, out: &mut impl std::io::Write, verbose: bool) -> io::Result<()> {
        let count =
            u32::try_from(self.entries.len()).map_err(|_| invalid_data("too many entries"))?;
        let mut offset = 0xcu32;
        for entry in &self.entries {
            offset = entry
                .entry
                .padded_len()
                .and_then(|s| s.checked_add(offset))
                .ok_or_else(|| {
                    invalid_data(format_args!("entry {} too large", entry.name.display()))
                })?;
        }
        out.write_all(b"IWAD")?;
        out.write_all(&count.to_le_bytes())?;
        out.write_all(&offset.to_le_bytes())?;

        for entry in &self.entries {
            if verbose {
                let size = entry.entry.data.len();
                let name = entry.name.display();
                let hash = crate::hash(&entry.entry.data);
                log::debug!("  0x{size: <8x} {name: <8} 0x{hash:016x}");
            }
            const PAD_BYTES: [u8; 4] = [0; 4];
            out.write_all(&entry.entry.data)?;
            let len = entry.entry.data.len() as u32;
            let padded_len = entry.entry.padded_len().unwrap();
            let padding = (padded_len - len) as usize;
            if padding > 0 {
                out.write_all(&PAD_BYTES[..padding])?;
            }
        }
        let mut offset = 0xcu32;
        for entry in &self.entries {
            let size = entry.entry.uncompressed_len() as u32;
            let o = if size > 0 { offset } else { 0 };
            out.write_all(&o.to_le_bytes())?;
            out.write_all(&size.to_le_bytes())?;
            let mut name = entry.name.0.clone();
            while name.len() < name.capacity() {
                name.push(0);
            }
            let mut name = name.into_inner().unwrap();
            if entry.entry.compression != Compression::None {
                name[0] |= 0x80;
            }
            out.write_all(&name)?;
            offset += entry.entry.padded_len().unwrap();
        }
        Ok(())
    }
}

#[inline]
fn name_sort<T>(
    a: &EntryName,
    _: &WadEntry<T>,
    b: &EntryName,
    _: &WadEntry<T>,
) -> std::cmp::Ordering {
    a.cmp(b)
}

#[inline]
fn other_sort<T>(
    ak: &EntryName,
    a: &WadEntry<T>,
    bk: &EntryName,
    b: &WadEntry<T>,
) -> std::cmp::Ordering {
    match a.typ.cmp(&b.typ) {
        std::cmp::Ordering::Equal => ak.cmp(bk),
        o => o,
    }
}

fn take_entry<T>(
    map: &mut EntryMap<T>,
    mut pred: impl FnMut(&EntryName, &WadEntry<T>) -> bool,
) -> Option<(EntryName, WadEntry<T>)> {
    let index = map.iter().position(|(k, v)| pred(k, v))?;
    map.shift_remove_index(index)
}

fn order_fixed<T>(entries: &mut EntryMap<T>, order: &[&[u8]]) {
    let mut count = 0;
    for tex in order.iter().copied() {
        if let Some(index) = entries.get_index_of(tex) {
            if count != index {
                entries.move_index(index, count);
                count += 1;
            }
        }
    }
}

impl Wad {
    pub fn sort(&mut self) {
        self.maps.sort_by(name_sort);
        self.palettes.sort_by(name_sort);
        self.sprites.sort_by(name_sort);
        order_fixed(&mut self.sprites, crate::orders::SPRITE_ORDER);
        self.textures.sort_by(name_sort);
        order_fixed(&mut self.textures, crate::orders::TEXTURE_ORDER);
        self.flats.sort_by(name_sort);
        order_fixed(&mut self.flats, crate::orders::FLAT_ORDER);
        self.graphics.sort_by(name_sort);
        self.hud_graphics.sort_by(name_sort);
        self.skies.sort_by(name_sort);
        self.other.sort_by(other_sort);
    }
    pub fn flatten(mut self) -> FlatWad {
        let mut flat = FlatWad::default();
        let mut sprite_prefixes = BTreeMap::new();

        flat.entries.push(FlatEntry::marker("S_START"));
        for (name, mut sprite) in self.sprites {
            let name = name.0;
            let mut palindex = None;
            if name.len() >= 4 && !name.starts_with(b"PAL") {
                use std::collections::btree_map::Entry;

                let prefix = <[u8; 4]>::try_from(&name[..4]).unwrap();
                match sprite_prefixes.entry(prefix) {
                    Entry::Vacant(entry) => {
                        let index = flat.entries.len();
                        let pal_prefix =
                            [b'P', b'A', b'L', prefix[0], prefix[1], prefix[2], prefix[3]];
                        let mut has_palette = false;
                        while let Some((name, palette)) =
                            take_entry(&mut self.palettes, |k, _| k.0.starts_with(&pal_prefix))
                        {
                            has_palette = true;
                            let mut data = vec![0; palette.data.len() * 2 + 8];
                            data[2] = 1;
                            gfx::palette_rgba_to_16(&palette.data, &mut data[8..]);
                            flat.entries.push(FlatEntry::new_entry(
                                name,
                                WadEntry::new(LumpType::Palette, data),
                            ));
                        }
                        // only makes sense to remove palette from image if the palettes all match
                        /*
                        if !has_palette {
                            if let gfx::SpritePalette::Rgb8(palette) = &sprite.data.palette {
                                has_palette = true;

                                let mut data = vec![0; palette.len() * 2 + 8];
                                data[2] = 1;
                                gfx::palette_rgba_to_16(palette.as_slice(), &mut data[8..]);

                                let mut pal_name = ArrayVec::<u8, 8>::new();
                                pal_name.try_extend_from_slice(&pal_prefix).unwrap();
                                pal_name.push(b'0');

                                flat.entries.push(FlatEntry::new_entry(
                                    EntryName(pal_name),
                                    WadEntry::new(LumpType::Palette, data),
                                ));
                            }
                        }
                        */
                        if has_palette {
                            entry.insert(index);
                            palindex = Some(index);
                        }
                    }
                    Entry::Occupied(entry) => palindex = Some(*entry.get()),
                }
            }
            if let Some(index) = palindex {
                let index = u16::try_from(flat.entries.len() - index).expect("too many sprites");
                sprite.data.palette = gfx::SpritePalette::Offset(index);
            }
            flat.entries
                .push(FlatEntry::new_entry(EntryName(name), sprite));
        }
        flat.entries.push(FlatEntry::marker("S_END"));

        flat.entries.push(FlatEntry::marker("T_START"));
        flat.entries
            .reserve(self.textures.len() + self.flats.len() + 2);
        for (name, entry) in self.textures {
            flat.entries.push(FlatEntry::new_entry(name, entry));
        }
        for (name, entry) in self.flats {
            flat.entries.push(FlatEntry::new_entry(name, entry));
        }
        flat.entries.push(FlatEntry::marker("T_END"));

        flat.append(self.hud_graphics);
        flat.entries.reserve(self.graphics.len());
        for (name, entry) in self.graphics {
            flat.entries.push(FlatEntry {
                name,
                entry: WadEntry::new(entry.typ, entry.data.to_vec(entry.typ)),
            });
        }
        flat.append(self.skies);
        flat.entries.reserve(self.maps.len());
        for (name, entry) in self.maps {
            let mut data = Vec::new();
            entry.data.write(&mut data, false).unwrap();
            flat.entries.push(FlatEntry {
                name,
                entry: WadEntry::new(entry.typ, data),
            });
        }
        flat.append(self.other);

        flat.entries.push(FlatEntry::marker("ENDOFWAD"));

        flat
    }
}

pub fn build(args: Args) -> io::Result<()> {
    let Args {
        inputs,
        output,
        exclude,
        no_compress,
        no_sound,
        wdd,
        wmd,
        wsd,
    } = args;
    let output = output.unwrap_or_else(|| PathBuf::from("DOOM64.WAD"));
    let mut iwad = Wad::default();
    let mut pwad = Wad::default();
    let mut snd = SoundData::default();
    for input in inputs {
        let ext = input
            .extension()
            .and_then(|e| e.to_str())
            .map(|p| p.to_ascii_lowercase());
        let ext = ext.as_deref();
        if ext == Some("z64") || (ext == Some("wad") && !is_map_wad(&input)) {
            let mut flags = crate::extract::ReadFlags::IWAD;
            if !no_sound {
                flags |= crate::extract::ReadFlags::SOUND;
            }
            let (flat, isnd) =
                read_rom_or_iwad(input, flags, &crate::extract::ExtFiles::default())?;
            let mut flat = flat.unwrap();
            if !exclude.is_empty() {
                flat.entries.retain(|entry| {
                    let name = entry.name.display();
                    !exclude.iter().any(|g| glob_match::glob_match(g, &name))
                });
            }
            iwad.merge_flat(flat);
            if let Some(isnd) = isnd {
                snd = isnd;
            }
        } else {
            log::info!("Reading `{}`", input.display());
            load_entries(
                &mut pwad,
                &mut snd,
                input,
                &exclude,
                None,
                LumpType::Unknown,
                0,
            )?;
            pwad.sort();
            iwad.merge(std::mem::take(&mut pwad));
        }
    }
    let mut flat = iwad.flatten();
    log::info!(
        "Writing `{}` with {} entries",
        output.display(),
        flat.entries.len()
    );
    if !no_compress {
        flat.compress();
    }
    log::debug!("  SIZE       NAME     HASH");
    {
        let out = std::fs::File::create(&output)?;
        let mut out = std::io::BufWriter::new(out);
        flat.write(&mut out, crate::is_log_level(log::LevelFilter::Debug))?;
    }

    if !no_sound {
        snd.compress();
        let mut sample_count = 0u32;
        snd.foreach_sample(|_| {
            sample_count += 1;
            Ok(())
        })
        .unwrap();
        {
            let filename = wdd.unwrap_or_else(|| output.with_extension("WDD"));
            log::info!(
                "Writing `{}` with {sample_count} samples",
                filename.display(),
            );
            let out = std::fs::File::create(filename)?;
            let mut out = std::io::BufWriter::new(out);
            snd.write_wdd(&mut out)?;
            drop(out);
        }

        {
            let filename = wmd.unwrap_or_else(|| output.with_extension("WMD"));
            log::info!(
                "Writing `{}` with {} instruments",
                filename.display(),
                snd.instruments.len(),
            );
            let out = std::fs::File::create(filename)?;
            let mut out = std::io::BufWriter::new(out);
            snd.write_wmd(&mut out)?;
        }

        {
            let filename = wsd.unwrap_or_else(|| output.with_extension("WSD"));
            log::info!(
                "Writing `{}` with {} sequences",
                filename.display(),
                snd.sequences.len()
            );
            let out = std::fs::File::create(filename)?;
            let mut out = std::io::BufWriter::new(out);
            snd.write_wsd(&mut out)?;
        }
    }

    Ok(())
}
