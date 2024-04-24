#!/bin/sh

set -x

if [ "${BUILD}x" = "x" ]; then
  BUILD=chroma74y
fi

VPJ_FILE=../../vs/${BUILD}_OPEL.vpj
dot_d_2vs.sh ${VPJ_FILE} ../builds/${BUILD}
