# Hexagon DSP kernels on QCS6490

[![hexagon-sim](https://github.com/pwolf15/hexagon-examples/actions/workflows/hexagon-sim.yml/badge.svg)](https://github.com/pwolf15/hexagon-examples/actions/workflows/hexagon-sim.yml)

HVX kernels in C++ and Rust, invoked over FastRPC from Linux, tested on `hexagon-sim` in CI and benchmarked on Radxa Dragon Q6A.

* Docker base image with toolchain can be found [here](https://github.com/snapdragon-toolchain/docker).
* The examples demonstrate usage of FastRPC, which coordinates invocation of DSP kernels from CPU host.

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

```
scripts/build 
BOARD=radxa@<ip> scripts/deploy
ssh radxa@<ip> 'cd ~/vecadd && ./vecadd_app`
```

## Results (Q6A, cDSP v68)

| kernel | N | scalar | HVX | Rust | correctness vs. double ref |
| --- | --- | --- | --- | --- | --- |
| f32 sum | 1024 | 17,692 | 2,533 | - | +1 ULP (all variants) |
| u8 dot product | 8192 | 112,187 | 6,162 | 7,080 | bit-exact (all variants) |

Cycles from `HAP_perf_get_pcycles` around the kernel call. At small `N`, overall numbers dominated by call overhead and final reduction. 

## Kernels

* `f32 sum`: scalar, HVX, interleaved
* `u8 dot product`: scalar, `vrmpyacc`, Rust
* `qf32 rounding`: Qualcomm DSPs include custom Qualcomm floating point implementation which uses Von Neumann rounding. qf32 skips normalization between adds
* `fir filter`

## Supported Languages for Hexagon

* LLVM toolchain => [here](https://github.com/qualcomm/toolchain_for_hexagon/tree/main)
* supports C++, Rust, Zig.

## Notes on HVX

* What features of the compute DSP make it energy efficient?
    * VLIW: very-long instruction word enables instruction level parallelism (ILP)
        * packets of up to four instructions, which are issued and commited togther
        * compiler schedules packets to maximize utilization; programmer must be aware of dependencies to minimize 
    * co-processor model: sw threads mapped to 4-6 scalar hw threads (arch-specific); hw threads use contexts to obtain access to HVX/HMX
    * Hexagon Vectorized Extensions (HVX) have 1024-bit vector registers
        * 128-byte mode; 32-float mode, etc.
        * use predicates instead of branching
        * compare against AVX-512 vector length
* Neural Processing Unit (NPU) is rebranding of cDSP after addition of Hexagon Matrix Extensions (HMX) coprocessor
* SoC uses unified memory model (across NPU/CPU/GPU)

### VLIW

* each packet is a transaction
* interleaving: 2 cycles/vector vs. 3.6
* every instruction executes simultaneously: 
    * all source operands read from register state as it was before packet
    * all destination wries commit togther at end of packet; no instruction in packet sees another packet instructions' result
* ILP (instruction level parallelism) increases occupancy of same pipeline
* `cycles/element = max (issue floor, latency / number of chains, memory bandwidth floor)`
