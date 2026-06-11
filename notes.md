# Steps

``` bash
# setup env
export ANDROID_ROOT_DIR=/opt/android-ndk-r29 ANDROID_NDK=/opt/android-ndk-r29 ANDROID_NDK_ROOT=/opt/android-ndk-r29
```

1. build host

``` bash
bash /opt/hexagon/6.6.0.0/build/cmake/cmake_configure.bash /workspaces/hexagon-examples android BUILD=ReleaseG DOMAIN_FLAG=3 HLOS_ARCH=64
```

2. build dsp

``` bash
bash /opt/hexagon/6.6.0.0/build/cmake/cmake_configure.bash /workspaces/hexagon-examples hexagon BUILD=ReleaseG DSP_ARCH=v75 DOMAIN_FLAG=3 NO_QURT_INC=1
```
