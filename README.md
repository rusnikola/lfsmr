# Hyaline Reclamation

## Publications

* [Paper](https://rusnikola.github.io/files/hyaline-pldi21.pdf): Snapshot-Free, Transparent, and Robust Memory Reclamation for Lock-Free Data Structures. Ruslan Nikolaev, and Binoy Ravindran. In Proceedings of the 42nd ACM SIGPLAN Conference on Programming Language Design and Implementation (PLDI'21). Virtual, Canada.

* [Brief Announcement](https://rusnikola.github.io/files/hyaline-podc19.pdf): Hyaline: Fast and Transparent Lock-Free Memory Reclamation. Ruslan Nikolaev and Binoy Ravindran. In Proceedings of the 38th ACM Symposium on Principles of Distributed Computing (PODC'19). Toronto, ON, Canada.

## Source Code

Please see Hyaline's code in the 'hyaline' directory. We also used used an
existent Interval-Based-Reclamation benchmark and integrated our new Hyaline
schemes into the benchmark.

## Building

To build this benchmark suite, we use clang 11.0. You can
download it from (http://releases.llvm.org/download.html). You can use
Ubuntu 16.04 pre-built binaries on Ubuntu 18.04 LTS:

https://github.com/llvm/llvm-project/releases/download/llvmorg-11.0.1/clang+llvm-11.0.1-x86_64-linux-gnu-ubuntu-16.04.tar.xz

Please extract it to /opt/llvm/ (e.g., /opt/llvm/bin/clang for clang).
If extracted to a different directory, then change clang and
clang++ paths in the Makefile.

You also need to install hwloc and jemalloc. If running Ubuntu 18.04, you
can type:

sudo apt-get install build-essential libjemalloc-dev libhwloc-dev libc6-dev libc-dev make python

To compile the benchmark with Hyaline:

* Go to 'benchmark'
(This benchmark is already modified to use Hyaline.)

* Run 'make'

See the original IBR's instructions in the 'benchmark' directory.

## Unreclaimed Objects

We had to change the default method of counting uncreclaimed
objects in the benchmark, as the original approach would not work
as is with Hyaline due to the global retirement of objects.

However, measurements become more expensive and skew throughput
for some tests. For this reason, we introduced an extra '-c'
flag. By default, we do not count (properly) the number of
unreclaimed objects. If the -c flag is specified, we count
the number of objects (potentially skewing throughput). To count
the number of objects, you have to run a separate test with the -c flag.

## Usage Example

A sample command (not counting uncreclaimed objects):

./bin/main -i 1 -m 3 -v -r 1 -o hashmap_result.csv  -t 4 -d tracker=HyalineEL
