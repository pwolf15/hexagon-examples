# Release

## 1. Setup

```bash
export GITHUB_USER="<your-github-username>"
export GITHUB_PAT="<your-github-pat>"
export GITHUB_TAG="v0.x"
```

## 2. Authenticate to GHCR

Verify you see **Login Succeeded**:

```bash
echo "$GITHUB_PAT" | docker login ghcr.io -u "$GITHUB_USER" --password-stdin
```

## 3. Build and push images

### Android

```bash
export ANDROID_VERSION="v0.y"
./build.sh $ANDROID_VERSION arm64-android
docker push ghcr.io/snapdragon-toolchain/arm64-android:$ANDROID_VERSION
```

### Linux

```bash
export LINUX_VERSION="v0.z"
./build.sh $LINUX_VERSION arm64-linux
docker push ghcr.io/snapdragon-toolchain/arm64-linux:$LINUX_VERSION
```

## 4. Create GitHub release

```bash
git tag -a $GITHUB_TAG -m "Release $GITHUB_TAG"
git push origin $GITHUB_TAG
```

Then create the release at: https://github.com/snapdragon-toolchain/docker/releases/new