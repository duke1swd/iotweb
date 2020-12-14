#!/bin/sh
BDIR=/cygdrive/c/users/danieste/appdata/local/temp
BUILD=`ls -tdr $BDIR/arduino_build* | tail -1`
FILE=`ls $BUILD/*.bin`
FILE=`basename $FILE`
#echo BDIR $BDIR
#echo BUILD $BUILD
#echo FILE `basename $FILE`

echo cp $BUILD/$FILE $FILE
cp $BUILD/$FILE $FILE
