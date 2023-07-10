fn main() {
    cc::Build::new()
        .file("src/compression.c")
        .emit_rerun_if_env_changed(false)
        .compile("compression");
}
