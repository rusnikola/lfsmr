#!/bin/bash

SRCDIR=$(dirname "$BASH_SOURCE")

Rscript "$SRCDIR/genfigs_throughput.R"
Rscript "$SRCDIR/genfigs_retired.R"
Rscript "$SRCDIR/genfigs_throughput_read.R"
Rscript "$SRCDIR/genfigs_retired_read.R"
