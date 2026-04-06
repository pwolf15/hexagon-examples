# Release docker image

## Setup

Set these variables before running any release commands:

```bash
export GITHUB_USER="<your-github-username>"
export GITHUB_PAT="<your-github-pat>"
export ANDROID_VERSION="v0.4"           # version tag for arm64-android
export LINUX_VERSION="v0.1"            # version tag for arm64-linux
```

## Authenticate to GHCR

```bash
echo "$GITHUB_PAT" | docker login ghcr.io -u "$GITHUB_USER" --password-stdin
```

## Release Android only

```bash
cd docker/

./build.sh $ANDROID_VERSION arm64-android

docker push ghcr.io/snapdragon-toolchain/arm64-android:$ANDROID_VERSION
```

## Release IoT (arm64-linux) only

```bash
cd docker/

./build.sh $LINUX_VERSION arm64-linux

docker push ghcr.io/snapdragon-toolchain/arm64-linux:$LINUX_VERSION
```

## Release both Android and IoT

Note: Android and Linux use separate version tags, so they must be built in two separate calls.

```bash
cd docker/

./build.sh $ANDROID_VERSION arm64-android
docker push ghcr.io/snapdragon-toolchain/arm64-android:$ANDROID_VERSION

./build.sh $LINUX_VERSION arm64-linux
docker push ghcr.io/snapdragon-toolchain/arm64-linux:$LINUX_VERSION
```

Then create releases manually at `https://github.com/snapdragon-toolchain/docker/releases/new`