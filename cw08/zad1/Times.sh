#!/usr/bin/env bash

RESULTS="Times.txt"
INPUT="baboon"

> $RESULTS
make all > /dev/null

for mode in block interleaved ; do \
    for nthreads in 0 1 2 4 8 ; do \
        for c in 3 5 8 16 32 65 ; do \
            echo "nthreads=$nthreads, mode=$mode, c=$c" | tee -a $RESULTS
            FILTER="rand_$c"
            OUTPUT="${INPUT}_$FILTER"
            ./main $nthreads $mode "inputs/$INPUT.ascii.pgm" "filters/$FILTER.txt" "outputs/$OUTPUT.ascii.pgm" >> $RESULTS
            echo "" >> $RESULTS
        done
    done
done
