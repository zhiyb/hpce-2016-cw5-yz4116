#!/bin/bash

for((i = 100; i < 1200; i += 100)); do
./bin/run_puzzle ising_spin $i 2 2>&1 | grep -E 'Executing puzzle|Executing reference|Finished' | awk '{print $2}' | tr -d ',' | paste - - | sed 's/^/-/;s/\s/+/' | bc | paste - - | sed "s/^/$i\t/"
done | tee ./results/ising.out
