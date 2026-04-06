# Release docker image

## Setup

Set these variables before running any release commands:

```bash
export GITHUB_USER="<your-github-username>"
export GITHUB_PAT="<your-github-pat>"
export ANDROID_VERSION="v0.4"           # version tag for arm64-android
export LINUX_VERSION="v0.1"            # version tag for arm64-linux
export GITHUB_TAG="v0.4-beta"            # version tag for GitHub release
```

## Authenticate to GHCR
Make sure you see **Login Succeeded** after running this command:
```bash
echo "$GITHUB_PAT" | docker login ghcr.io -u "$GITHUB_USER" --password-stdin
```

## Release docker image
### Android only

```bash
cd docker/

./build.sh $ANDROID_VERSION arm64-android

docker push ghcr.io/snapdragon-toolchain/arm64-android:$ANDROID_VERSION
```

### IoT (arm64-linux) only

```bash
cd docker/

./build.sh $LINUX_VERSION arm64-linux

docker push ghcr.io/snapdragon-toolchain/arm64-linux:$LINUX_VERSION
```

### Both Android and IoT

Note: Android and Linux use separate version tags, so they must be built in two separate calls.

```bash
cd docker/

./build.sh $ANDROID_VERSION arm64-android
docker push ghcr.io/snapdragon-toolchain/arm64-android:$ANDROID_VERSION

./build.sh $LINUX_VERSION arm64-linux
docker push ghcr.io/snapdragon-toolchain/arm64-linux:$LINUX_VERSION
```

## Create GitHub release

```bash
git tag -a $GITHUB_TAG -m "Release $GITHUB_TAG"
git push origin $GITHUB_TAG
```
Then create releases manually at `https://github.com/snapdragon-toolchain/docker/releases/new`

## Remove a previous tag

```bash
# Delete local tag
git tag -d <tag-name>

# Delete remote tag
git push origin --delete <tag-name>
```