#!/bin/bash

if [ -d __build__ ]; then
    rm -rf __build__
fi

mkdir __build__
cd __build__
cmake ..
make -j

cd ..
rm compile_commands.json
ln -s __build__/compile_commands.json compile_commands.json 

