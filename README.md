# Hexagon Kernel Playground

[![hexagon-sim](https://github.com/pwolf15/hexagon-examples/actions/workflows/hexagon-sim.yml/badge.svg)](https://github.com/pwolf15/hexagon-examples/actions/workflows/hexagon-sim.yml)

Repository demonstrating usage of Qualcomm SDK in both CI using Hexagon Sim and Radxa Dragon Q6A. The Q6A hosts the QCS6490 SoC, which contains cDSP v68 (renamed NPU). Built with Hexagon SDK 6.6. 

* Docker base image with toolchain can be found [here](https://github.com/snapdragon-toolchain/docker).
* The examples demonstrate usage of FastRPC, which coordinates invocation of DSP kernels from CPU host.

## Example Kernels

* `f32 sum`
* `u8 dot product`
* `qf32 rounding`: Qualcomm DSPs include custom Qualcomm floating point implementation which uses Von Neumann rounding for efficiency purposes.
* `fir filter`

## Supported Languages for Hexagon

* LLVM toolchain => (here)[https://github.com/qualcomm/toolchain_for_hexagon/tree/main]
* supports C++, Rust, Zig.

## Notes on HVX

* What features of the compute DSP make it energy efficient?
    * VLIW: very-long instruction word enables instruction level parallelism (ILP)
        * 4 32-bit command packets
        * compiler schedules packets to maximize utilization; programmer must be aware of dependencies to minimize 
    * co-processor model: sw threads mapped to 4-6 scalar hw threads (arch-specific); hw threads use contexts to obtain access to HVX/HMX
    * Hexagon Vectorized Extensions (HVX) have 1024-bit vectorized instructions
        * can operate on 128 bytes, 32 floats/ints etc. in parallel
        * use predicates instead of branching
        * compare against AVX-512 vector length
* Neural Processing Unit (NPU) is rebranding of cDSP after addition of Hexagon Matrix Extensions (HMX) coprocessor
* SoC uses unified memory model (across NPU/CPU/GPU)