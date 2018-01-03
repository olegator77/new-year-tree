#!/bin/sh


mkdir -p ../data
find *.png | sed "s/\.png//" | xargs -n1 -Ixxx convert xxx.png -background white -alpha remove ../data/xxx.bmp
