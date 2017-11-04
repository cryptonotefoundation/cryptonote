#!/bin/bash
STR="0-0-0-3"

#rm -Rf ~/.bipcoin

#make it
cmake . -DSTATIC=True
make

#archive it
rm -Rf build
mkdir build
mkdir build/bipcoin
cp ___Read_Me_Damn_It.txt build/bipcoin
cp BipCoin-RadioAd-60sec-Stereo-1.mp3 build/bipcoin
cp StickerArtPurple1000px.png build/bipcoin
cp src/bipcoind build/bipcoin
cp src/connectivity_tool build/bipcoin
cp src/simplewallet build/bipcoin
cp src/walletd build/bipcoin
cd build
tar -cvf bipcoin.linux.x64.v$STR.tar  bipcoin/* 
gzip -9 bipcoin.linux.x64.v$STR.tar

