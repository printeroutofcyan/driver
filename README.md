# SwissKnife Driver Toolkit

This repository contains a conceptual Windows 11 kernel-mode driver and a user-mode
console client that together form the **SwissKnife** diagnostics and forensics toolkit.
The goal is to provide a single driver that can surface a wide collection of
system-level capabilities via IOCTLs while a rich user-mode companion exposes the
functionality interactively.

## Layout

- `include/SwissKnifeCommon.h` – shared IOCTL codes, data structures, and helpers used
  by both the kernel driver and the user-mode console application.
- `src/kernel/SwissKnifeDriver.c` – WDM-style kernel driver that creates the
  `\\Device\\SwissKnife` device and implements a multi-function IOCTL interface for
  diagnostics, enumeration, file access, registry inspection, and random data
  generation.
- `src/user/SwissKnifeClient.cpp` – Windows console application that opens the driver
  and provides a menu-driven “Swiss army knife” UI to exercise every IOCTL.

## Driver feature set

The driver exposes the following IOCTLs:

1. `IOCTL_SWISSKNIFE_GET_DRIVER_INFO` – returns metadata about the driver such as
   version, friendly name, and description.
2. `IOCTL_SWISSKNIFE_GET_SYSTEM_OVERVIEW` – delivers an overview of system state,
   including processor count, active processor mask, physical memory statistics, and
   uptime information.
3. `IOCTL_SWISSKNIFE_ENUMERATE_PROCESSES` – enumerates all running processes with ID,
   parent ID, thread count, and timing information.
4. `IOCTL_SWISSKNIFE_ENUMERATE_MODULES` – captures the kernel module list with base
   addresses, sizes, flags, and Unicode-converted image names.
5. `IOCTL_SWISSKNIFE_READ_FILE` – opens an arbitrary NT path from kernel mode and
   returns the requested byte range.
6. `IOCTL_SWISSKNIFE_QUERY_REGISTRY` – queries a registry value using full NT paths and
   returns type and raw bytes.
7. `IOCTL_SWISSKNIFE_RANDOM_BYTES` – produces deterministic or seeded random data using
   `RtlRandomEx`, suitable for entropy testing.

Each handler validates user buffers, performs the privileged operation, and returns a
buffered response so the user-mode tool can format the information.

## User-mode console client

Running the user-mode console on Windows will display a menu of the available
operations. Each option wraps a DeviceIoControl invocation, manages buffer resizing,
parses numeric input (supporting decimal and hexadecimal), and renders the driver
output:

- Process and module lists are displayed in tabular form.
- File reads and registry queries show hexadecimal dumps with offsets.
- Random byte generation includes the seed used and the hexdump of the result.

The application keeps the device handle open, allowing operators to execute multiple
operations in sequence without restarting.

## Building

The project is designed for the Windows Driver Kit (WDK) and Visual Studio toolchain.
A typical workflow would involve creating separate Visual Studio projects for the
kernel-mode driver (KMDF/WDM) and the user-mode console, both consuming
`include/SwissKnifeCommon.h` for shared definitions.

Because the code is delivered as reference source, you can integrate it into your
existing WDK solution structure and adapt signing, INF packaging, and deployment
according to your environment.

## Disclaimer

This repository provides illustrative code intended for learning and prototyping. Always
review and adapt the implementation to meet production security, performance, and
reliability requirements before deploying it to real systems.

