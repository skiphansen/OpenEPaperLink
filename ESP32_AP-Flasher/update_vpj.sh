#!/bin/sh

ENV=OpenEPaperLink_Mini_AP_v4

VPJ_FILE=../vs/${ENV}.vpj

dot_d_2vs.sh ${VPJ_FILE} .pio/build/${ENV}
cp ${VPJ_FILE} .
cat ${ENV}.vpj | sed -e 's!\\ ! !'g > e.xml
mv e.xml ${VPJ_FILE}

