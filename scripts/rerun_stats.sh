#!/bin/bash

#DIR=~/studia/doktoranckie/tager/dane/wypluwka_2009-12-16-1756.xml.split

if [ -z "$DIR" ]; then
    echo "DIR environment variable not set."
    exit 1
fi

SUFFIX="$1"
if [ -z "$SUFFIX" ]; then
    echo "please specify directory name suffix as an argument"
    exit 1
fi

THRS="2"

export OMP_NUM_THREADS=1
PROCESSES=6

export LC_ALL="pl_PL.UTF-8"

for t in $THRS; do
    for ((i=0; i<10; i++)); do
        nice /usr/bin/time ./src/btagger \
            $DIR-$t-$SUFFIX/npart$i.xml.btengine \
            $DIR-$t-$SUFFIX/part$i.xml \
            $t 2>&1 | tee $DIR-$t-$SUFFIX/part$i.out.stats
    done
done

