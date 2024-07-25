## **Dogemone: The Future of Privacy Coins**

Dogemone is a cutting-edge privacy coin built on the robust Cryptonote protocol, ensuring unparalleled transaction confidentiality and security. With a maximum supply of 300 million, Dogemone follows a meticulously designed emission curve, providing a predictable and fair distribution over time. The coin leverages the Cryptonight algorithm, known for its ASIC resistance and efficiency in CPU and GPU mining, promoting decentralization. Dogemone is committed to protecting your financial privacy while maintaining a secure and decentralized network, making it a standout choice in the realm of cryptocurrency. Join us in shaping a future where privacy and security are paramount.

### **Key Features:**
- **Privacy-Focused**: Utilizes Cryptonote protocol for untraceable and unlinkable transactions.
- **Controlled Supply**: Capped at 300 million coins with a fair emission curve.
- **Efficient Mining**: Cryptonight algorithm ensures equitable mining opportunities.
- **No Premine**: 100% fair launch with no premine.
- **Developer Fee**: 10% of the block reward is allocated to development, taken slowly over time.
- **Difficulty Adjustment**: Adaptive difficulty adjustment every 120 seconds for network stability.
- **Decentralized Network**: Promotes decentralization and security.

Embrace the future of privacy with Dogemone.


## Building Dogemone 

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
We would recommend using the prebuild binaries from the release section, Good luck!
