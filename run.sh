#!/bin/bash
N=$1
# all or progressive
type1=$2
# within or nn
type2=$3
# nuclei or vessel
table2=$4

if [ $type1 == "all" ]; then
    nohup ./build/"$type2"_origin -n "$N" -1 nuclei -2 "$table2" > "$N"_"$type2"_"$type1"_"$table2".log &
elif [ $type1 == "progressive" ]; then
    nohup ./build/"$type2" -n "$N" -1 nuclei -2 "$table2" > "$N"_"$type2"_"$type1"_"$table2".log &
else
    echo "Invalid type: $type"
    exit 1
fi

# bash run.sh 9551 progressive within nuclei
# bash run.sh 9551 all within nuclei
# bash run.sh 9551 progressive within vessel
# bash run.sh 9551 all within vessel

# bash run.sh 9551 progressive nn nuclei
# bash run.sh 9551 all nn nuclei
# bash run.sh 9551 progressive nn vessel
# bash run.sh 9551 all nn vessel