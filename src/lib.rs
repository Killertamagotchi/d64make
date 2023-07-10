pub mod build;
mod compression;
pub mod extract;
mod gfx;
pub mod inspect;
mod music;
mod orders;
mod remaster;
mod sound;
mod soundfont;
mod wad;

pub use wad::*;

#[inline]
fn too_large(input: &[u8]) -> nom::Err<nom::error::Error<&[u8]>> {
    nom::Err::Error(nom::error::Error::new(
        input,
        nom::error::ErrorKind::TooLarge,
    ))
}

#[inline]
fn nom_fail(input: &[u8]) -> nom::Err<nom::error::Error<&[u8]>> {
    nom::Err::Error(nom::error::Error::new(input, nom::error::ErrorKind::Fail))
}

#[inline]
fn invalid_data(args: impl std::fmt::Display) -> std::io::Error {
    std::io::Error::new(std::io::ErrorKind::InvalidData, args.to_string())
}

#[inline]
fn hash(d: &impl std::hash::Hash) -> u64 {
    let mut hasher = std::collections::hash_map::DefaultHasher::new();
    std::hash::Hash::hash(d, &mut hasher);
    std::hash::Hasher::finish(&hasher)
}

#[inline]
fn is_log_level(lvl: log::LevelFilter) -> bool {
    lvl <= log::STATIC_MAX_LEVEL && lvl <= log::max_level()
}
