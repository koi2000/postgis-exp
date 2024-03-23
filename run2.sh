#!/bin/bash
for ((i = 0; i <= 100; i++)); do
    ./build/nn -t $i -1 nuclei -2 vessel
done