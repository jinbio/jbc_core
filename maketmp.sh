#!/bin/bash
rm -Rf ../tmp/*
mkdir ../tmp/data
cp -f ./jbcoin.tmp.conf ../tmp/data/jbcoin.conf
cp src/jbcoind ../tmp/
cp src/qt/jbcoin-qt ../tmp/


