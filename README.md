# MinerCoin - An Open Source CryptoCurrency
## base on Proof-of-Work CPU Mining

### MinerCoin Current Status
Working:
1. Daemon to seed node - tested on ubuntu 14.04
2. RPC connection to daemon, pool & wallets - tested on ubuntu 14.04
3. CPU Mining on MinerCoin pool - tested on Windows 10
4. Solo mining using Simplewallet via terminal - tested on ubuntu 14.04

Not tested
1. Compile on Windows
2. Compile on other Ubuntu version except 14.04
3. GUI wallet
4. Integration with Pool API

Need Attention
1. Extend MinerCoin code. to be working in manners. Low fees, fast transaction, a secure anonymous payment system.
2. Blockchain website and domain hosting - unmodified public ledger of all MinerCoin tx.
3. Cloud wallet service
4. Listed on centralized exchange to be pair with fiat.
5. More seed nodes hosting.

## Description
MinerCoin is to be use as electronic payment system and not limited to it. With feauture such as anonymity, secure, borderless, decentralize, very low fees and fast transfer capabilities, giving Users their financial flexibility and experience MinerCoin Use and Value. MinerCoin code is Open Source and always avalaible to be maintain by Community ♥️ Consensus. You can contribute by seeding MinerCoin nodes on your PC while at the same time taking advantage of CPU Mining just within wallet. 

## Preparation for Developers Environment

1. Create an account on [GitHub.com](github.com)
2. Fork [Minercoin repository](https://github.com/nidscom/minercoin)
3. Buy one or two Ubuntu-based dedicated servers (at least 2Gb of RAM) for seed nodes.
   ==> You can use VPS from [Vultr](https://www.vultr.com/?ref=7283997) with $5 or $10 VPS to seed nodes and support MinerCoin development.


### First step. FORK MinerCoin Repo
on [GitHub.com](https://github.com/nidscom/minercoin)

### Secondly, Build Environment 
Currently tested on UBUNTU 14.04, LINUX & WINDOWS

#### On Ubuntu, Linux and nix

Dependencies: GCC 4.7.3 or later, CMake 2.8.6 or later, and Boost 1.55.

You may download them from:

* http://gcc.gnu.org/
* http://www.cmake.org/
* http://www.boost.org/
* Alternatively, it may be possible to install them using a package manager.

To build, change to a directory where this file is located, and run `make`. The resulting executables can be found in `build/release/src`.

**Advanced options:**

* Parallel build: run `make -j<number of threads>` instead of `make`.
* Debug build: run `make build-debug`.
* Test suite: run `make test-release` to run tests in addition to building. Running `make test-debug` will do the same to the debug version.
* Building with Clang: it may be possible to use Clang instead of GCC, but this may not work everywhere. To build, run `export CC=clang CXX=clang++` before running `make`.

#### On Windows
Dependencies: MSVC 2013 or later, CMake 2.8.6 or later, and Boost 1.55. You may download them from:

* http://www.microsoft.com/
* http://www.cmake.org/
* http://www.boost.org/

To build, change to a directory where this file is located, and run theas commands: 
```
mkdir build
cd build
cmake -G "Visual Studio 12 Win64" ..
```

And then do Build.
Good luck!

==> MinerCoin
[Facebook](https://www.fb.com/nidscom.io)
[Maintainers](https://www.nidscom.io)
