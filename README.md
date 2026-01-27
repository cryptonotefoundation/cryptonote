For an immediately usable, binary version of the Luxcoin Core software, see:

<a href="https://github.com/luxxcoin/luxcoin/releases/download/luxcoin-auto/luxcoin-core-windows.zip">Windows Download</a>

<a href="https://github.com/luxxcoin/luxcoin/releases/download/luxcoin-linux/luxcoin-core-linux.sh">Linux Download</a>

<a href="URL">Buy/use/invesment Luxcoin</a>

What is Luxcoin Core?
Luxcoin Core connects to the Luxcoin peer-to-peer network to download and fully validate blocks and transactions. It also includes a wallet and graphical user interface, which can be optionally built.

Further information about Luxcoin Core is available in the doc folder.

License
Luxcoin Core is released under the terms of the MIT license. See COPYING for more information or see .
<a href="https://opensource.org/license/MIT">https://opensource.org/license/MIT</a>
Development Process
The master branch is regularly built (see doc/build-*.md for instructions) and tested, but it is not guaranteed to be completely stable. Tags are created regularly from release branches to indicate new official, stable release versions of Luxcoin Core.

The https://github.com/luxxcoin/luxcoin/gui repository is used exclusively for the development of the GUI. Its master branch is identical in all monotree repositories. Release branches and tags do not exist, so please do not fork that repository unless it is for development reasons.

The contribution workflow is described in CONTRIBUTING.md and useful hints for developers can be found in doc/developer-notes.md.

Testing
Testing and code review is the bottleneck for development; we get more pull requests than we can review and test on short notice. Please be patient and help out by testing other people's pull requests, and remember this is a security-critical project where any mistake might cost people lots of money.

Automated Testing
Developers are strongly encouraged to write unit tests for new code, and to submit new unit tests for old code. Unit tests can be compiled and run (assuming they weren't disabled during the generation of the build system) with: ctest. Further details on running and extending unit tests can be found in /src/test/README.md.

There are also regression and integration tests, written in Python. These tests can be run (if the test dependencies are installed) with: build/test/functional/test_runner.py (assuming build is your build directory).

The CI (Continuous Integration) systems make sure that every pull request is tested on Windows, Linux, and macOS. The CI must pass on all commits before merge to avoid unrelated CI failures on new pull requests.

Manual Quality Assurance (QA) Testing
Changes should be tested by somebody other than the developer who wrote the code. This is especially important for large or high-risk changes. It is useful to add a test plan to the pull request description if testing the changes is not straightforward.

Translations
Changes to translations as well as new translations can be submitted to Luxcoin Core's Transifex page.

Translations are periodically pulled from Transifex and merged into the git repository. See the translation process for details on how this works.

Important: We do not accept translation changes as GitHub pull requests because the next pull from Transifex would automatically overwrite them again.

<a href="https://buymeacoffee.com/luxcoin">Donate Support</a>
<a href="https://patreon.com/luxcoin">Donate Support</a>

