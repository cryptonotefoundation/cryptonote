This is the reference code for [DarkTaelon](https://taelon.io) cryptocurrency.

To install on windowsx64

Step 1:

Download : taelon.io/darktaelonx64.zip

Step 2:

Unzip the folder and start : darktaelond.exe 

Then start simplewallet.exe

Press 'g' to generate a wallet, then name it and enter a password.

Tip: On first open the wallet will show your private key, this is founf under the wallett address. Save it somewhere safe to secure the wallet.

To install on Ubuntu 16.04

Step 1:

Copy the source code onto your server, simply type :

'''
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

In root access, simply type:

$ cmake /root/darktaelon (path to darktaelon dir)

Afterwards run:

$ make


to permamently start a node : screen -S demon ./darktaelond (from src dir)
