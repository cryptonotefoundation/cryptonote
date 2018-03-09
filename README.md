This is the reference code for [CryptoNote](https://cryptonote.org) cryptocurrency protocol.

* Launch your own CryptoNote currency: [CryptoNote Starter](https://cryptonotestarter.org/)
* CryptoNote reference implementation: [CryptoNoteCoin](https://cryptonote-coin.org)
* Discussion board and support: [CryptoNote Forum](https://forum.cryptonote.org)

Step 1:

Copy the source code onto your server, simply type :

$ git clone https://github.com/darktaelon/darktaelon

Step 2:

Go to the newly created directory, simply type:

$ cd darktaelon

Step 3:

Get the environment setup properly, simply type:

$ apt-get install libpthread-stubs0-dev 
$ apt-get install doxygen
$ apt-get install libboost-all-dev
$ apt-get install g++

(If you have any issues here try typing: apt-get update after each install)

Step 4:

make and cmake

In folder darktaelon, simply type:

$ cmake /root/darktaelon

Afterwards run:

$ make


to permamently start a node : screen -S demon ./darktaelond
