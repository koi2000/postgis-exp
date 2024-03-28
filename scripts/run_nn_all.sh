#!/bin/bash
for ((i = 0; i <= 9551; i++)); do
    ./build/nn_origin -t $i -1 nuclei -2 nuclei
done

for ((i = 0; i <= 9551; i++)); do
    ./build/nn_origin -t $i -1 nuclei -2 vessel
done