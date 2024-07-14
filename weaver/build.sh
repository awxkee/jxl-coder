set -e
rustup default nightly

rustup +nightly target add x86_64-linux-android aarch64-linux-android armv7-linux-androideabi i686-linux-android

RUSTFLAGS="-C target-feature=+neon -C opt-level=3 -C strip=symbols" cargo build --target aarch64-linux-android --release --manifest-path Cargo.toml

RUSTFLAGS="-C target-feature=+sse4.2 -C opt-level=3 -C strip=symbols" cargo +nightly build --target x86_64-linux-android --release --manifest-path Cargo.toml

RUSTFLAGS="-C opt-level=3 -C strip=symbols" cargo build --target armv7-linux-androideabi --release --manifest-path Cargo.toml

RUSTFLAGS="-C opt-level=3 -C strip=symbols" cargo build --target i686-linux-android --release --manifest-path Cargo.toml

rm -rf target/android_build
mkdir -p target/android_build
mkdir -p target/android_build/x86_64
mkdir -p target/android_build/aarch64
mkdir -p target/android_build/armeabi-v7a
mkdir -p target/android_build/x86

cp -r target/aarch64-linux-android/release/libweaver.so target/android_build/aarch64/libweaver.so
cp -r target/x86_64-linux-android/release/libweaver.so target/android_build/x86_64/libweaver.so
cp -r target/armv7-linux-androideabi/release/libweaver.so target/android_build/armeabi-v7a/libweaver.so
cp -r target/i686-linux-android/release/libweaver.so target/android_build/x86/libweaver.so

cp -r target/aarch64-linux-android/release/libweaver.so ../jxlcoder/src/main/cpp/lib/arm64-v8a/libweaver.so
cp -r target/x86_64-linux-android/release/libweaver.so ../jxlcoder/src/main/cpp/lib/x86_64/libweaver.so
cp -r target/armv7-linux-androideabi/release/libweaver.so ../jxlcoder/src/main/cpp/lib/armeabi-v7a/libweaver.so
cp -r target/i686-linux-android/release/libweaver.so ../jxlcoder/src/main/cpp/lib/x86/libweaver.so

rm -rf built.zip

zip -r built.zip target/android_build/*