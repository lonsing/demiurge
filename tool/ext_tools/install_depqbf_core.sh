#!/bin/bash

if [ "$DEMIURGETP" = "" ]
then
   echo "The Environment variable DEMIURGETP is undefined."
   exit 1
fi

echo "Installing DepQBF ..."

DEPQBF="depqbf-cleanup-pre-release-assumptions-commit820fc3"
DEPQBF_ACHRIVE="$DEPQBF.tar.gz"

echo " Unpacking DepQBF ..."
rm -rf $DEMIURGETP/depqbf
tar -xzf $DEPQBF_ACHRIVE -C $DEMIURGETP
mv $DEMIURGETP/$DEPQBF $DEMIURGETP/depqbf
echo "#define DEPQBF_CORE_AVAILABLE" >> $DEMIURGETP/depqbf/qdpll.h
 
echo " Compiling DepQBF ..."
(cd $DEMIURGETP/depqbf; make)
