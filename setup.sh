#!/bin/sh

RM=/bin/rm
MKDIR=/bin/mkdir
MAKE=/usr/bin/make
DIR=build2
LIBNAME=libssm.so.0

pt=`pwd`

${MKDIR} ../build2
./configure --preifx=${pt}/../${DIR}/lib64/
${MAKE}
${MAKE} install

mv ../${DIR}/lib64/${LIBNAME} ../${DIR}/lib64/${LIBNAME}.org
cd ../build2/lib64
ln -s ${pt}/src/.libs/libssm.so.0 .
