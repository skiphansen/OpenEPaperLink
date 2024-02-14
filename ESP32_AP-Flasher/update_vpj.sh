#!/bin/sh

VPJ_FILE=../vs/ESP32_S3_C6_NANO_AP.vpj

dot_d_2vs.sh ${VPJ_FILE} .pio/build/ESP32_S3_C6_NANO_AP
cp ${VPJ_FILE} .
cat ESP32_S3_C6_NANO_AP.vpj | sed -e 's!\\ ! !'g > e.xml
mv e.xml ${VPJ_FILE}

