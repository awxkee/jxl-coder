#!/bin/bash
echo "Running pre-sync script..."

SPARKYUV_RELEASE_VERSION="0.8.25"

ABI_LIST="armeabi-v7a arm64-v8a x86 x86_64"

for abi in ${ABI_LIST}; do
  # Specify the destination file path where you want to save the downloaded file
  destination_file="src/main/cpp/lib/${abi}/libsparkyuv.a"

  file_url="https://github.com/awxkee/sparkyuv/releases/download/${SPARKYUV_RELEASE_VERSION}/libsparkyuv.android-${abi}-static.tar.gz"

  if [ ! -f "$destination_file" ]; then
      echo "File does not exist. Downloading..."
      echo "Downloading ${file_url}"

      store_file="libsparkyuv.android-${abi}-static.tar.gz"

      rm -rf "$store_file"

      curl -L -f -o "$store_file" "$file_url"

      rm -rf libsparkyuv.a
      tar -xf "$store_file"

      cp libsparkyuv.a "$destination_file"
      mkdir -p "src/main/cpp/sparkyuv"
      cp -r include/* "./src/main/cpp/sparkyuv"

      rm -rf "$store_file"
      rm -rf libsparkyuv.a
      rm -rf include

      echo "Download complete."
  else
      echo "File already exists. Skipping download."
  fi
done