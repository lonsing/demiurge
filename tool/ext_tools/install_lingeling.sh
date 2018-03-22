#!/bin/bash

if [ "$DEMIURGETP" = "" ]
then
   echo "The Environment variable DEMIURGETP is undefined."
   exit 1
fi

echo "Installing the Lingeling SAT-solver ..."

LINGELING="lingeling-ala-b02aa1a-121013"
LINGELING_ACHRIVE="$LINGELING.tar.gz"

if [ ! -e "$LINGELING_ACHRIVE" ];
then
  echo " Downloading Lingeling ..."
  wget http://fmv.jku.at/lingeling/$LINGELING_ACHRIVE
fi

echo " Unpacking Lingeling ..."
rm -rf $DEMIURGETP/lingeling
tar -xzf $LINGELING_ACHRIVE -C $DEMIURGETP
mv $DEMIURGETP/$LINGELING $DEMIURGETP/lingeling

echo " Compiling Lingeling ..."
cd $DEMIURGETP/lingeling
./configure
cp $DEMIURGETP/lingeling/makefile $DEMIURGETP/lingeling/makefile_orig
sed 's/AIGER=/AIGER=$(DEMIURGETP)\/aiger-1.9.4/' $DEMIURGETP/lingeling/makefile_orig > $DEMIURGETP/lingeling/makefile
make
make blimc
