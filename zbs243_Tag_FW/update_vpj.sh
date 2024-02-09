#!/bin/sh

echo -e "\n\nBuilding 2.9 (UC8151) version..."
make BUILD=zbs29_uc8151 CPU=8051 SOC=zbs243 > build.log

if [ ! -e ../temp_dir ]; then mkdir ../temp_dir; fi
cat `find ../zbs243_shared . -name "*.d"` > ../temp_dir/e.d
dot_d_2vs.sh ~/esl/vs/zbs243_Tag_FW.vpj ../temp_dir


