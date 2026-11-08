#!/usr/bin/env bash
# package_apk.sh — 用 aapt2 + zip + apksigner 手工打包签名 APK（不经 Gradle）
# 环境变量：BT（build-tools 目录，必填）、PLAT（platforms/android-XX 目录，必填）
#           JDK_BIN（keytool 所在 bin 目录，默认取 JAVA_HOME/bin）、OUT（apk 名）
set -e
cd "$(dirname "$0")/.."
: "${BT:?需要设置 BT 指向 build-tools 目录}"
: "${PLAT:?需要设置 PLAT 指向 platforms/android-XX 目录}"
JDK_BIN="${JDK_BIN:-${JAVA_HOME}/bin}"
OUT="${OUT:-MidnightWard-v2.1-cpp.apk}"
ABIS="${ABIS:-arm64-v8a}"
STAGE=build-android/pkg

rm -rf "$STAGE"
mkdir -p "$STAGE/lib"
for ABI in $ABIS; do
    if [ -f "build-android/$ABI/libmidnight_ward.so" ]; then
        mkdir -p "$STAGE/lib/$ABI"
        cp "build-android/$ABI/libmidnight_ward.so" "$STAGE/lib/$ABI/"
    else
        echo "跳过缺失的 ABI: $ABI"
    fi
done

echo "== aapt2 编译资源 =="
rm -f build-android/res.zip
"$BT/aapt2" compile --dir res -o build-android/res.zip

echo "== aapt2 链接 =="
"$BT/aapt2" link \
    -o "$STAGE/base.apk" \
    -I "$PLAT/android.jar" \
    --manifest AndroidManifest.xml \
    -A assets \
    build-android/res.zip \
    --auto-add-overlay

echo "== 打入原生库 =="
(cd "$STAGE" && zip -q -r base.apk lib)

echo "== zipalign =="
"$BT/zipalign" -f -p 4 "$STAGE/base.apk" "$STAGE/aligned.apk"

echo "== 生成调试密钥并签名 =="
KS="$STAGE/debug.keystore"
if [ ! -f "$HOME/.android/mw-debug.keystore" ]; then
    mkdir -p "$HOME/.android"
    "$JDK_BIN/keytool" -genkeypair -keystore "$HOME/.android/mw-debug.keystore" \
        -alias mwdebug -storepass midnight -keypass midnight -keyalg RSA \
        -keysize 2048 -validity 10000 \
        -dname "CN=Midnight Ward Debug,O=MidnightWard,C=CN" >/dev/null 2>&1
fi
"$BT/apksigner" sign \
    --ks "$HOME/.android/mw-debug.keystore" \
    --ks-pass pass:midnight --ks-key-alias mwdebug \
    --key-pass pass:midnight \
    --out "$OUT" "$STAGE/aligned.apk"

echo "== 校验 =="
"$BT/apksigner" verify "$OUT" && echo "签名校验通过"
ls -lh "$OUT"
echo "OK: $OUT"
