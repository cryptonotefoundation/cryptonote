The same text below in English...

Levcoin е проект за създаване на първата българска крипто валута. Основана е на технологията на CryptoNote, която се счита че е по-добра от оригиналната технология на "биткойна". Направени са и някои малки нетехнологични, а чисто икономически модификации. В началото на "копаенето" тази валута се добива съвсем леко по-бързо от "биткойна". Но с времето добивът става значително по-бавен. Това е необходимо, за да се гарантира фактически безсрочен добив, което ще осигури винаги да съществуват "копачки", което пък крепи самото съществуване на валутата. 
"Биткойнът" носи в себе си сериозния риск в един момент почти нищо вече да не може да се добива, копачките да спрат, и цялата система да рухне. Засега няма отговор какво ще се прави по този въпрос, но е възможно на някакъв етап да се намери решение.
При "левкойна" такъв проблем няма. Кривата на добив пресича тази на "биткойна" и в дори след 30 години, отново ще има какво да се добива. По този показател е твърде вероятно тази българска валута да е сред най-добре проектираните в света.
Общото колическо планирани "левкойни" ще е 9 милиона. Цифрата е избрана по броя на населението на нашата страна, когато е била най-многолюдна. Тази емисия е доста по-малка от на "биткойна" и на другите валути, базирани на CryptoNote. Но от друга страна, основната идея е да бъде национална валута, т.е. не е нужна чак толкова голяма емисия.
Разбира се, нищо не пречи тази валута да се добива и в чужбина. Единственото, което може да я запази предимно българска, е активността на българите като "копачи". Бъдете активни.
Технологията CryptoNote има и допълнителното предимство спрямо "биткойна", на по-голяма анонимност и непроследимост.
Друго важно предимство е, че може да се "копае" на по-обикновени компютри - домашни такива, и не са нужни сървърни ферми за милиони, за да добиеш нещо.
Надяваме се проектът да ви бъде полезен...
---
Levcoin is a project for creation of the first Bulgarian crypto-currency. It is based on CryptoNote source code, considered better than the code of "bitcoin". Some changes in Econimic model are done. At the start of mining it is a bit faster that with Bitcoin. But after that it becomes considerably slower. This is needed so "endless" mining is possible, that will ensure the presence of miners forever. And this will guarantee the survival of the currency.
Bitcoin brings the risk of emission-end that will shut-down the digging computers. And this may result in an entire currency crash.
May be this problem will be solved in future, but now the risk is not addressed.
With LevCoin no such problems. Even in 30 years there still be what to mine. Based on this characteristic the Bulgarian cryptocurrency is among the best in the world.
Total money supply is 9 million. It is based on the population of Bulgaria in its best times when it was 9 million. This is less that 21 million of Bitcoin and about 18 million of other CryptoNote coins.
CryptoNote technology is better than Bitcoin in anonymity and tracking protection.
Another advantage is it can be mined on home computers and no server farms needed.
We hope the project will be useful for you...



==============
This is the reference code for [CryptoNote](https://cryptonote.org) cryptocurrency protocol.

* Launch your own CryptoNote currency: [CryptoNote Starter](https://cryptonotestarter.org/)
* CryptoNote reference implementation: [CryptoNoteCoin](https://cryptonote-coin.org)
* Discussion board and support: [CryptoNote Forum](https://forum.cryptonote.org)

## CryptoNote forking how-to

### Preparation

1. Create an account on [GitHub.com](github.com)
2. Fork [CryptoNote repository](https://github.com/cryptonotefoundation/cryptonote)
3. Buy one or two Ubuntu-based dedicated servers (at least 2Gb of RAM) for seed nodes.



### First step. Give a name to your coin

**Good name must be unique.** Check uniqueness with [google](http://google.com) and [Map of Coins](mapofcoins.com) or any other similar service.

Name must be specified twice:

**1. in file src/CryptoNoteConfig.h** - `CRYPTONOTE_NAME` constant

Example: 
```
const char CRYPTONOTE_NAME[] = "furiouscoin";
```

**2. in src/CMakeList.txt file** - set_property(TARGET daemon PROPERTY OUTPUT_NAME "YOURCOINNAME**d**")

Example: 
```
set_property(TARGET daemon PROPERTY OUTPUT_NAME "furiouscoind")
```

**Note:** You should also change a repository name.


### Second step. Emission logic 

**1. Total money supply** (src/CryptoNoteConfig.h)

Total amount of coins to be emitted. Most of CryptoNote based coins use `(uint64_t)(-1)` (equals to 18446744073709551616). You can define number explicitly (for example `UINT64_C(858986905600000000)`).

Example:
```
const uint64_t MONEY_SUPPLY = (uint64_t)(-1);
```

**2. Emission curve** (src/CryptoNoteConfig.h)

Be default CryptoNote provides emission formula with slight decrease of block reward with each block. This is different from Bitcoin where block reward halves every 4 years.

`EMISSION_SPEED_FACTOR` constant defines emission curve slope. This parameter is required to calulate block reward. 

Example:
```
const unsigned EMISSION_SPEED_FACTOR = 18;
```

**3. Difficulty target** (src/CryptoNoteConfig.h)

Difficulty target is an ideal time period between blocks. In case an average time between blocks becomes less than difficulty target, the difficulty increases. Difficulty target is measured in seconds.

Difficulty target directly influences several aspects of coin's behavior:

- transaction confirmation speed: the longer the time between the blocks is, the slower transaction confirmation is
- emission speed: the longer the time between the blocks is the slower the emission process is
- orphan rate: chains with very fast blocks have greater orphan rate

For most coins difficulty target is 60 or 120 seconds.

Example:
```
const uint64_t DIFFICULTY_TARGET = 120;
```

**4. Block reward formula**

In case you are not satisfied with CryptoNote default implementation of block reward logic you can also change it. The implementation is in `src/CryptoNoteCore/Currency.cpp`:
```
bool Currency::getBlockReward(size_t medianSize, size_t currentBlockSize, uint64_t alreadyGeneratedCoins, uint64_t fee, uint64_t& reward, int64_t& emissionChange) const
```

This function has two parts:

- basic block reward calculation: `uint64_t baseReward = (m_moneySupply - alreadyGeneratedCoins) >> m_emissionSpeedFactor;`
- big block penalty calculation: this is the way CryptoNote protects the block chain from transaction flooding attacks and preserves opportunities for organic network growth at the same time.

Only the first part of this function is directly related to the emission logic. You can change it the way you want. See MonetaVerde and DuckNote as the examples where this function is modified.


### Third step. Networking

**1. Default ports for P2P and RPC networking** (src/CryptoNoteConfig.h)

P2P port is used by daemons to talk to each other through P2P protocol.
RPC port is used by wallet and other programs to talk to daemon.

It's better to choose ports that aren't used by other software or coins. See known TCP ports lists:

* http://www.speedguide.net/ports.php
* http://www.networksorcery.com/enp/protocol/ip/ports00000.htm
* http://keir.net/portlist.html

Example:
```
const int P2P_DEFAULT_PORT = 17236;
const int RPC_DEFAULT_PORT = 18236;
```


**2. Network identifier** (src/P2p/P2pNetworks.h)

This identifier is used in network packages in order not to mix two different cryptocoin networks. Change all the bytes to random values for your network:
```
const static boost::uuids::uuid CRYPTONOTE_NETWORK = { { 0xA1, 0x1A, 0xA1, 0x1A, 0xA1, 0x0A, 0xA1, 0x0A, 0xA0, 0x1A, 0xA0, 0x1A, 0xA0, 0x1A, 0xA1, 0x1A } };
```


**3. Seed nodes** (src/CryptoNoteConfig.h)

Add IP addresses of your seed nodes.

Example:
```
const std::initializer_list<const char*> SEED_NODES = {
  "111.11.11.11:17236",
  "222.22.22.22:17236",
};
```


### Fourth step. Transaction fee and related parameters

**1. Minimum transaction fee** (src/CryptoNoteConfig.h)

Zero minimum fee can lead to transaction flooding. Transactions cheaper than the minimum transaction fee wouldn't be accepted by daemons. 100000 value for `MINIMUM_FEE` is usually enough.

Example:
```
const uint64_t MINIMUM_FEE = 100000;
```


**2. Penalty free block size** (src/CryptoNoteConfig.h)

CryptoNote protects chain from tx flooding by reducing block reward for blocks larger than the median block size. However, this rule applies for blocks larger than `CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE` bytes.

Example:
```
const size_t CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE = 20000;
```


### Fifth step. Address prefix

You may choose a letter (in some cases several letters) all the coin's public addresses will start with. It is defined by `CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX` constant. Since the rules for address prefixes are nontrivial you may use the [prefix generator tool](https://cryptonotestarter.org/tools.html).

Example:
```
const uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 0xe9; // addresses start with "f"
```


### Sixth step. Genesis block

**1. Build the binaries with blank genesis tx hex** (src/CryptoNoteConfig.h)

You should leave `const char GENESIS_COINBASE_TX_HEX[]` blank and compile the binaries without it.

Example:
```
const char GENESIS_COINBASE_TX_HEX[] = "";
```


**2. Start the daemon to print out the genesis block**

Run your daemon with `--print-genesis-tx` argument. It will print out the genesis block coinbase transaction hash.

Example:
```
furiouscoind --print-genesis-tx
```


**3. Copy the printed transaction hash** (src/CryptoNoteConfig.h)

Copy the tx hash that has been printed by the daemon to `GENESIS_COINBASE_TX_HEX` in `src/CryptoNoteConfig.h`

Example:
```
const char GENESIS_COINBASE_TX_HEX[] = "013c01ff0001ffff...785a33d9ebdba68b0";
```


**4. Recompile the binaries**

Recompile everything again. Your coin code is ready now. Make an announcement for the potential users and enjoy!


## Building CryptoNote 

### On *nix

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

### On Windows
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
