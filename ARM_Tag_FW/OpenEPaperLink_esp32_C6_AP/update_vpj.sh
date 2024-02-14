#!/bin/sh

VPJ_FILE=../../vs/OpenEPaperLink_esp32_C6_AP.vpj

CWD=`pwd`
(cd build/;ninja -t deps |  \
sed -r 's!^([[:space:]]+)\.\./(.*)!\1'${CWD}/'\2!p' | \
sed -r 's!^([[:space:]]+)([^/.])(.*)!\1'${CWD}/build/'\2\3!p' > e.d)
dot_d_2vs.sh ${VPJ_FILE} build

