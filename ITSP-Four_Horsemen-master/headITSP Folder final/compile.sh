#!/bin/bash
echo "compiling $1"
if [[ $1 == *.c ]]
then
    gcc -ggdb `pkg-config --cflags opencv` -o `basename $1 .c` -lcurlpp -lcurl  $1 -lcurl `pkg-config libxml++-2.6 --cflags` `pkg-config libxml++-2.6 --libs` `pkg-config --libs opencv`;
elif [[ $1 == *.cpp ]]
then
    g++ -ggdb `pkg-config --cflags opencv` -o `basename $1 .cpp` -lcurlpp -lcurl $1 -lcurl `pkg-config libxml++-2.6 --cflags` `pkg-config libxml++-2.6 --libs` `pkg-config --libs opencv`;
else
    echo "Please compile only .c or .cpp files"
fi
echo "Output file => ${1%.*}"

