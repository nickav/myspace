clang --target=wasm32 -O2 --no-standard-libraries -mbulk-memory -Wl,--export-all -Wl,--no-entry -Wl,--import-memory -o hello.wasm hello.c
