#!/bin/sh

VPJ_FILE=../vs/zbs243_Tag_FW.vpj

echo -n "Building 2.9 (UC8151) version..."
make BUILD=zbs29_uc8151 CPU=8051 SOC=zbs243 > build.log
echo

if [ -e ../temp_dir ]; then rm -rf ../temp_dir; fi
mkdir ../temp_dir
cat `find ../zbs243_shared . -name "*.d"` > ../temp_dir/e.d
dot_d_2vs.sh ${VPJ_FILE} ../temp_dir


