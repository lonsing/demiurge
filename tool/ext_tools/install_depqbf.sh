#!/bin/bash

if [ "$DEMIURGETP" = "" ]
then
   echo "The Environment variable DEMIURGETP is undefined."
   exit 1
fi

echo "Installing DepQBF ..."

DEPQBF_ACHRIVE="lonsing-depqbf-version-2.0-0-gb79050a.zip"

if [ ! -e "$DEPQBF_ACHRIVE" ];
then
  echo " Downloading DepQBF ..."
  wget https://github.com/lonsing/depqbf/archive/$DEPQBF_ACHRIVE
fi

echo " Unpacking DepQBF ..."
rm -rf $DEMIURGETP/depqbf
unzip -d $DEMIURGETP $DEPQBF_ACHRIVE
mv $DEMIURGETP/depqbf-b79050aebc3fed2c0c8344bfe5155bac6a047bb4 $DEMIURGETP/depqbf

echo " Compiling DepQBF ..."
(cd $DEMIURGETP/depqbf; make)
