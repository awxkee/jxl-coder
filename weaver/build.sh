set -e
rustup default nightly

rustup +nightly target add x86_64-linux-android aarch64-linux-android armv7-linux-androideabi i686-linux-android

# Keep panic unwinding while omitting panic source locations and std backtraces.
# AArch64 keeps speed optimizations; the remaining ABIs optimize for size.
RUSTFLAGS="-C link-arg=-Wl,-z,max-page-size=16384 -C target-feature=+neon -C opt-level=3 -C strip=symbols -Z location-detail=none" cargo +nightly build -Z build-std=std,panic_unwind -Z build-std-features=panic-unwind --target aarch64-linux-android --features rdm,sve --release --manifest-path Cargo.toml

RUSTFLAGS="-C link-arg=-Wl,-z,max-page-size=16384 -C opt-level=z -C strip=symbols -Z location-detail=none" cargo +nightly build -Z build-std=std,panic_unwind -Z build-std-features=panic-unwind --target x86_64-linux-android --release --manifest-path Cargo.toml

RUSTFLAGS="-C link-arg=-Wl,-z,max-page-size=16384 -C opt-level=z -C strip=symbols -Z location-detail=none" cargo +nightly build -Z build-std=std,panic_unwind -Z build-std-features=panic-unwind --target armv7-linux-androideabi --release --manifest-path Cargo.toml

RUSTFLAGS="-C link-arg=-Wl,-z,max-page-size=16384 -C opt-level=z -C strip=symbols -Z location-detail=none" cargo +nightly build -Z build-std=std,panic_unwind -Z build-std-features=panic-unwind --target i686-linux-android --release --manifest-path Cargo.toml

rm -rf target/android_build
mkdir -p target/android_build
mkdir -p target/android_build/x86_64
mkdir -p target/android_build/aarch64
mkdir -p target/android_build/armeabi-v7a
mkdir -p target/android_build/x86

cp -r target/aarch64-linux-android/release/libweaver.a ../jxlcoder/src/main/cpp/lib/arm64-v8a/libweaver.a
cp -r target/x86_64-linux-android/release/libweaver.a ../jxlcoder/src/main/cpp/lib/x86_64/libweaver.a
cp -r target/armv7-linux-androideabi/release/libweaver.a ../jxlcoder/src/main/cpp/lib/armeabi-v7a/libweaver.a
cp -r target/i686-linux-android/release/libweaver.a ../jxlcoder/src/main/cpp/lib/x86/libweaver.a
