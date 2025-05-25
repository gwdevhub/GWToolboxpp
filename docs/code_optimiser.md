---
layout: default
---

# Code Optimiser Module

The Code Optimiser Module in GWToolbox++ enhances Guild Wars' performance by replacing certain inefficient game functions with more optimized implementations.

## Features

### CRC32 Computation Optimization

The module replaces Guild Wars' original CRC32 (Cyclic Redundancy Check) computation function with a significantly faster implementation that uses a tabular method.

#### Performance Improvements:
- For large files (100MB): Reduces computation time from approximately 240ms to 45ms
- For small files (1MB): Reduces computation time from less than 3ms to less than 1ms

## Benefits

- **Faster Loading Times**: CRC32 is used for file verification during loading, so this optimization can reduce loading times.
- **Smoother Gameplay**: More efficient code execution means less CPU usage, potentially resulting in smoother gameplay.
- **Reduced Resource Usage**: The optimized implementation uses system resources more efficiently.

## Technical Details

The optimization uses a tabular method for CRC32 computation, which pre-computes CRC values for all possible byte values and uses table lookups instead of bit-by-bit computation. This approach is significantly faster, especially for larger data sets.

The implementation processes data in chunks of 16 bytes when possible, further improving performance through better utilization of modern CPU capabilities.

## Usage

This module is enabled automatically when GWToolbox++ starts and requires no user configuration. It works silently in the background to improve game performance.

## Notes

- This optimization is completely safe and does not modify any game data
- The module only replaces the computation method, not the CRC32 algorithm itself
- The resulting CRC32 values are identical to those produced by the original function

This module is an example of how GWToolbox++ can improve the Guild Wars experience through technical optimizations that enhance performance without changing gameplay.

[back](./)
