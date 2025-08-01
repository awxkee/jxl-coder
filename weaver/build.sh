set -e
rustup default nightly

rustup +nightly target add x86_64-linux-android aarch64-linux-android armv7-linux-androideabi i686-linux-android

RUSTFLAGS="-C link-arg=-Wl,-z,max-page-size=16384 -C target-feature=+neon -C opt-level=3 -C strip=symbols" cargo +nightly build -Z build-std=std,panic_abort --target aarch64-linux-android --release --manifest-path Cargo.toml

RUSTFLAGS="-C link-arg=-Wl,-z,max-page-size=16384 -C opt-level=z -C strip=symbols" cargo +nightly build -Z build-std=std,panic_abort --target x86_64-linux-android --release --manifest-path Cargo.toml

RUSTFLAGS="-C link-arg=-Wl,-z,max-page-size=16384 -C opt-level=z -C strip=symbols" cargo +nightly build -Z build-std=std,panic_abort --target armv7-linux-androideabi --release --manifest-path Cargo.toml

RUSTFLAGS="-C link-arg=-Wl,-z,max-page-size=16384 -C opt-level=z -C strip=symbols" cargo +nightly build -Z build-std=std,panic_abort --target i686-linux-android --release --manifest-path Cargo.toml

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