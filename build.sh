#!/bin/bash
cmake -H. -Bbuild && cmake --build build
ln -sf `pwd`/build/compile_commands.json `pwd`/compile_commands.json
