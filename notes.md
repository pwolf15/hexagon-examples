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

3. push and run

``` bash
adb push android_ReleaseG_aarch64/ship/vecadd_app /data/local/tmp/ && adb push android_ReleaseG_aarch64/ship/libvecadd.so /data/local/tmp/ && adb push android_ReleaseG_aarch64/ship/libc++_shared.so /data/local/tmp/ && adb push hexagon_ReleaseG_toolv19_v75/ship/libvecadd_skel.so /data/local/tmp/ && adb push hexagon_ReleaseG_toolv19_v75/ship/libc++.so.1 /data/local/tmp/ && adb push hexagon_ReleaseG_toolv19_v75/ship/libc++abi.so.1 /data/local/tmp/ && adb shell 'cd /data/local/tmp && \
LD_LIBRARY_PATH=/data/local/tmp \
DSP_LIBRARY_PATH=/data/local/tmp \
ADSP_LIBRARY_PATH=/data/local/tmp \
./vecadd_app; echo EXIT:$?'
```

# VLIW notes

* `vror`: vector-rotate-right
* interleaving: 2 cycles/vector vs. 3.6
* packet is a transaction: every instruction executes "simulatenously"; all source operands read from register state as it was before packet; all destination writes commit together at end of packet; no instruction in packet sees another same packet instructions' result
* .new suffix: ISA forwards same packet value
* `cycles/element = max (issue floor, latency / number of chains, memory bandwidth floor)`
* ILP (instruction level parallelism) buys occupancy of the same pipeline