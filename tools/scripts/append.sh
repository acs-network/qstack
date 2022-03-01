#!/bin/bash

cat $2 > tmp
cat $1 > $2
cat tmp >> $2
