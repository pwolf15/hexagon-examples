# Hexagon DSP kernels on QCS6490

[![hexagon-sim](https://github.com/pwolf15/hexagon-examples/actions/workflows/hexagon-sim.yml/badge.svg)](https://github.com/pwolf15/hexagon-examples/actions/workflows/hexagon-sim.yml)

* Kernels run on the cDSP (v68) and are called from a Linux host app over FastRPC; the IDL in `inc/` defines the interface, `qaic` generates the stub and skel.
* Tests run on `hexagon-sim` in Github Actions; cycle counts come from the board.
* Development environment: `Dockerfile` + `.devcontainer/` (SDK 6.6, aarch64 cross tools, Rust nightly), based on [snapdragon-toolchain/docker](https://github.com/snapdragon-toolchain/docker); see `docs/toolchain.md`

## Build and run

Requires `HEXAGON_SDK_ROOT` (the devcontainer sets it).

### Simulator

``` shell
cmake -S sim -B build-sim -G Ninja
cmake --build build-sim
ctest --test-dir build-sim --output-on-failure
```

Runs `dsp/test_main.cpp` on `hexagon-sim` (v68)

### Board (Radxa Dragon Q6A)

``` shell
scripts/build 
BOARD=radxa@<ip> scripts/deploy
ssh radxa@<ip> 'cd ~/vecadd && ./vecadd_app'
```

`vecadd_app` loads `libvecadd_skel.so` from the current directory, so run it from the deploy directory; to run it from elsewhere, set `ADSP_LIBRARY_PATH` to that directory.
`vecadd_app_malloc` is the same program with `malloc` buffers, which FastRPC copies through an intermediate shared buffer, versus `rpcmem` buffers the DSP maps directly (zero-copy).

## Results (Q6A, cDSP v68)

| kernel | N | scalar | HVX | Rust | correctness vs. double ref |
| --- | --- | --- | --- | --- | --- |
| f32 sum | 1024 | 17,692 | 2,533 | - | +1 ULP (all variants) |
| u8 dot product | 8192 | 112,187 | 6,162 | 7,080 | bit-exact (all variants) |

Cycles from `HAP_perf_get_pcycles` around the kernel call. At small `N`, overall numbers dominated by call overhead and final reduction. 

## Kernels

* `f32 sum`: scalar, HVX, interleaved
* `u8 dot product`: scalar, `vrmpyacc`, Rust

## Supported Languages for Hexagon

* LLVM toolchain => [here](https://github.com/qualcomm/toolchain_for_hexagon/tree/main)
* open toolchain supports C++, Rust, Zig.

## Notes on HVX

* What features of the compute DSP make it energy efficient?
    * VLIW: very-long instruction word enables instruction level parallelism (ILP)
        * packets of up to four instructions, which are issued and committed together
        * compiler schedules packets to maximize utilization; programmer must be aware of dependencies 
    * co-processor model: sw threads mapped to 4-6 scalar hw threads (arch-specific); hw threads use contexts to obtain access to HVX/HMX
    * Hexagon Vectorized Extensions (HVX) have 1024-bit vector registers
        * 128-byte mode; 32-float mode, etc.
        * use predicates instead of branching
        * compare against AVX-512 vector length
* Neural Processing Unit (NPU) is rebranding of cDSP after addition of Hexagon Matrix Extensions (HMX) coprocessor
* SoC uses unified memory model (across NPU/CPU/GPU)

### VLIW

* each packet is a transaction
* every instruction executes simultaneously: 
    * all source operands read from register state as it was before packet
    * all destination writes commit together at end of packet; no instruction in packet sees another packet instructions' result
* ILP (instruction level parallelism) increases occupancy of same pipeline
* `cycles/element = max (issue floor, latency / number of chains, memory bandwidth floor)`
