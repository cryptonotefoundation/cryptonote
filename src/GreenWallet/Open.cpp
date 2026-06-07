// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2016-2026, The Karbo developers
//
// Please see the included LICENSE file for more information.

///////////////////////////
#include <GreenWallet/Open.h>
///////////////////////////

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <Common/Base58.h>
#include <Common/StringTools.h>

#include <crypto/crypto.h>
#include <CryptoNoteCore/Account.h>
#include <CryptoNoteCore/CryptoNoteBasicImpl.h>
#include <CryptoNoteCore/CryptoNoteTools.h>

#include <Mnemonics/electrum-words.h>

#include <Wallet/WalletErrors.h>

#include <Common/ColouredMsg.h>
#include <GreenWallet/CommandImplementations.h>
#include <GreenWallet/Tools.h>
#include <GreenWallet/Transfer.h>
#include <GreenWallet/Types.h>
#include <Common/PasswordContainer.h>
#include <GreenWallet/WalletConfig.h>

std::shared_ptr<WalletInfo> createViewWallet(CryptoNote::WalletGreen &wallet)
{
    // Accept either:
    //   (A) A Tracking Key (192 hex chars):
    //       spendPublicKey | viewPublicKey | viewSecretKey
    //       — tracks incoming transactions only.
    //   (B) A Private View Key (64 hex chars):
    //       — tracking-only; wallet address must be entered separately.
    std::cout << InformationMsg("A TRACKING wallet accepts a Tracking Key or Private View Key and")
              << std::endl
              << InformationMsg("allows audit of transactions.")
              << std::endl
              << WarningMsg("This wallet type does not allow spending funds.")
              << std::endl;

    bool create = confirm("Continue?");

    std::cout << std::endl;

    if (!create)
    {
        return nullptr;
    }

    std::string keyInput;
    while (true)
    {
        std::cout << InformationMsg("Paste your Tracking Key (192 hex) or Private View Key (64 hex): ");
        std::getline(std::cin, keyInput);
        boost::algorithm::trim(keyInput);

        if (keyInput.size() == 64 || keyInput.size() == 192)
            break;

        std::cout << WarningMsg("Invalid key length — expected 64 or 192 hex characters. Try again.")
                  << std::endl;
    }

    const std::string walletFileName = getNewWalletFileName();
    const std::string walletPass = getWalletPassword(true, "Give your new wallet a password: ");

    if (keyInput.size() == 192)
    {
        // Parse the three 32-byte fields from the 192-hex string.
        size_t sz;
        Crypto::Hash spendPubHash, viewPubHash, viewSecretHash;

        if (!Common::fromHex(keyInput.substr(0, 64),   &spendPubHash,   sizeof(spendPubHash),   sz) ||
            !Common::fromHex(keyInput.substr(64, 64),  &viewPubHash,    sizeof(viewPubHash),    sz) ||
            !Common::fromHex(keyInput.substr(128, 64), &viewSecretHash, sizeof(viewSecretHash), sz))
        {
            std::cout << WarningMsg("Invalid key — could not decode hex fields.") << std::endl;
            return nullptr;
        }

        Crypto::PublicKey spendPublicKey = *reinterpret_cast<Crypto::PublicKey*>(&spendPubHash);
        Crypto::PublicKey viewPublicKey  = *reinterpret_cast<Crypto::PublicKey*>(&viewPubHash);
        Crypto::SecretKey viewSecretKey  = *reinterpret_cast<Crypto::SecretKey*>(&viewSecretHash);

        // Reconstruct the address string from the embedded public keys.
        CryptoNote::AccountPublicAddress addrKeys { spendPublicKey, viewPublicKey };
        CryptoNote::BinaryArray addrData;
        toBinaryArray(addrKeys, addrData);
        std::string address = Tools::Base58::encode_addr(
            CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
            std::string(reinterpret_cast<const char*>(addrData.data()), addrData.size()));

        wallet.createViewWallet(walletPass, address, viewSecretKey, walletFileName);

        std::cout << std::endl
                  << InformationMsg("Your view wallet ")
                  << InformationMsg(address)
                  << InformationMsg(" has been successfully imported!")
                  << std::endl << std::endl;
        viewWalletMsg();

        return std::make_shared<WalletInfo>(walletFileName, walletPass,
                                            address, true, wallet);
    }
    else
    {
        // 64-hex: legacy private view key — ask for address separately.
        Crypto::Hash viewKeyHash;
        size_t sz;
        if (!Common::fromHex(keyInput, &viewKeyHash, sizeof(viewKeyHash), sz) ||
            sz != sizeof(viewKeyHash))
        {
            std::cout << WarningMsg("Invalid private view key — not a valid hex string.") << std::endl;
            return nullptr;
        }

        Crypto::SecretKey privateViewKey = *reinterpret_cast<Crypto::SecretKey*>(&viewKeyHash);
        Crypto::PublicKey derivedPub;
        if (!Crypto::secret_key_to_public_key(privateViewKey, derivedPub))
        {
            std::cout << WarningMsg("Invalid private view key — not on the ed25519 curve.") << std::endl;
            return nullptr;
        }

        std::string address;
        while (true)
        {
            std::cout << InformationMsg("Enter your public ")
                      << InformationMsg(WalletConfig::ticker)
                      << InformationMsg(" address: ");

            std::getline(std::cin, address);
            boost::algorithm::trim(address);

            if (parseAddress(address))
                break;
        }

        wallet.createViewWallet(walletPass, address, privateViewKey, walletFileName);

        std::cout << std::endl
                  << InformationMsg("Your view wallet ")
                  << InformationMsg(address)
                  << InformationMsg(" has been successfully imported!")
                  << std::endl << std::endl;
        viewWalletMsg();

        return std::make_shared<WalletInfo>(walletFileName, walletPass,
                                            address, true, wallet);
    }
}

std::shared_ptr<WalletInfo> importWallet(CryptoNote::WalletGreen &wallet)
{
    const Crypto::SecretKey privateSpendKey
        = getPrivateKey("Enter your private spend key: ");

    const Crypto::SecretKey privateViewKey
        = getPrivateKey("Enter your private view key: ");

    return importFromKeys(wallet, privateSpendKey, privateViewKey);
}

std::shared_ptr<WalletInfo> importGUIWallet(CryptoNote::WalletGreen &wallet)
{
    // GUI private key is base58-encoded AccountKeys (spend+view keys).
    // Format: 128 bytes (spendPub|viewPub|spendSec|viewSec) -> 184 base58 chars.
    static constexpr size_t kLegacyKeyBytes = 128;  // old format without extra fields
    static constexpr size_t kNewKeyBytes    = sizeof(CryptoNote::AccountKeys);

    std::string guiPrivateKey;

    uint64_t addressPrefix;
    std::string data;

    while (true)
    {
        std::cout << "GUI Private Key: ";
        std::getline(std::cin, guiPrivateKey);
        boost::algorithm::trim(guiPrivateKey);

        if (!Tools::Base58::decode_addr(guiPrivateKey, addressPrefix, data)
          || (data.size() != kLegacyKeyBytes && data.size() != kNewKeyBytes))
        {
            std::cout << WarningMsg("Invalid GUI Private Key — failed to decode or wrong size. "
                                    "Try again.")
                      << std::endl;

            continue;
        }

        if (addressPrefix !=
            CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX)
        {
            std::cout << WarningMsg("Invalid GUI Private Key, it should begin ")
                      << WarningMsg("with ")
                      << WarningMsg(WalletConfig::addressPrefix)
                      << WarningMsg("! Try again.")
                      << std::endl;

            continue;
        }

        break;
    }

    // Extract keys from known fixed offsets (identical in both legacy and new format):
    //   offset  0: spendPublicKey  (32 bytes)
    //   offset 32: viewPublicKey   (32 bytes)
    //   offset 64: spendSecretKey  (32 bytes)
    //   offset 96: viewSecretKey   (32 bytes)
    Crypto::SecretKey spendSecretKey, viewSecretKey;
    memcpy(&spendSecretKey, data.data() + 64, sizeof(Crypto::SecretKey));
    memcpy(&viewSecretKey,  data.data() + 96, sizeof(Crypto::SecretKey));

    return importFromKeys(wallet, spendSecretKey, viewSecretKey);
}

std::shared_ptr<WalletInfo> mnemonicImportWallet(CryptoNote::WalletGreen
                                                 &wallet)
{
    std::string mnemonicPhrase;

    Crypto::SecretKey privateSpendKey;
    Crypto::SecretKey privateViewKey;

    do
    {
        std::cout << InformationMsg("Enter your mnemonic phrase (25 words): ");

        std::getline(std::cin, mnemonicPhrase);

        boost::algorithm::trim(mnemonicPhrase);
    }
    while (!Crypto::ElectrumWords::is_valid_mnemonic(mnemonicPhrase,
                                                     privateSpendKey,
                                                     std::cout));

    CryptoNote::AccountBase::generateViewFromSpend(privateSpendKey,
                                                   privateViewKey);

    return importFromKeys(wallet, privateSpendKey, privateViewKey);
}

std::shared_ptr<WalletInfo> importFromKeys(CryptoNote::WalletGreen &wallet,
                                           Crypto::SecretKey privateSpendKey,
                                           Crypto::SecretKey privateViewKey)
{
    const std::string walletFileName = getNewWalletFileName();

    const std::string msg = "Give your new wallet a password: ";

    const std::string walletPass = getWalletPassword(true, msg);

    connectingMsg();

    wallet.initializeWithViewKey(walletFileName, walletPass, privateViewKey);

    const std::string walletAddress = wallet.createAddress(privateSpendKey);

    std::cout << std::endl << InformationMsg("Your wallet ")
              << InformationMsg(walletAddress)
              << InformationMsg(" has been successfully imported!")
              << std::endl << std::endl;

    return std::make_shared<WalletInfo>(walletFileName, walletPass,
                                        walletAddress, false, wallet);
}

std::shared_ptr<WalletInfo> generateWallet(CryptoNote::WalletGreen &wallet)
{
    const std::string walletFileName = getNewWalletFileName();

    const std::string msg = "Give your new wallet a password: ";

    const std::string walletPass = getWalletPassword(true, msg);

    CryptoNote::KeyPair spendKey;
    Crypto::SecretKey privateViewKey;

    Crypto::generate_keys(spendKey.publicKey, spendKey.secretKey);

    CryptoNote::AccountBase::generateViewFromSpend(spendKey.secretKey,
                                                   privateViewKey);

    wallet.initializeWithViewKey(walletFileName, walletPass, privateViewKey);

    uint64_t creationTimestamp = static_cast<uint64_t>(time(nullptr));

    const std::string walletAddress = wallet.createAddress(spendKey.secretKey,
                                                           creationTimestamp);

    promptSaveKeys(wallet);

    std::cout << WarningMsg("If you lose these your wallet cannot be ")
              << WarningMsg("recreated!")
              << std::endl << std::endl;

    return std::make_shared<WalletInfo>(walletFileName, walletPass,
                                        walletAddress, false, wallet);
}

std::shared_ptr<WalletInfo> openWallet(CryptoNote::WalletGreen &wallet,
                                       Config &config)
{
    const std::string walletFileName = getExistingWalletFileName(config);

    bool initial = true;

    while (true)
    {
        std::string walletPass;

        /* Only use the command line pass once, otherwise we will infinite
           loop if it is incorrect */
        if (initial && config.passGiven)
        {
            walletPass = config.walletPass;
        }
        else
        {
            walletPass = getWalletPassword(false, "Enter password: ");
        }

        initial = false;

        connectingMsg();

        try
        {
            wallet.load(walletFileName, walletPass);

            const std::string walletAddress = wallet.getAddress(0);

            const Crypto::SecretKey privateSpendKey
                = wallet.getAddressSpendKey(0).secretKey;

            bool viewWallet = false;

            if (privateSpendKey == CryptoNote::NULL_SECRET_KEY)
            {
                std::cout << std::endl
                          << InformationMsg("Your view only wallet ")
                          << InformationMsg(walletAddress)
                          << InformationMsg(" has been successfully opened!")
                          << std::endl << std::endl;

                viewWalletMsg();

                viewWallet = true;

            }
            else
            {
                std::cout << std::endl
                          << InformationMsg("Your wallet ")
                          << InformationMsg(walletAddress)
                          << InformationMsg(" has been successfully opened!")
                          << std::endl << std::endl;
            }

            return std::make_shared<WalletInfo>(
                walletFileName, walletPass, walletAddress, viewWallet, wallet
            );

        }
        catch (const std::system_error& e)
        {
            bool handled = false;

            switch (e.code().value())
            {
                case CryptoNote::error::WRONG_PASSWORD:
                {
                    std::cout << std::endl
                              << WarningMsg("Incorrect password! Try again.")
                              << std::endl << std::endl;

                    handled = true;

                    break;
                }
                case CryptoNote::error::WRONG_VERSION:
                {
                    std::stringstream msg;

                    msg << "Could not open wallet file! It doesn't appear "
                        << "to be a valid wallet!" << std::endl
                        << "Ensure you are opening a wallet file, and the "
                        << "file has not gotten corrupted." << std::endl
                        << "Try reimporting via keys, and always close "
                        << WalletConfig::walletName << " with the exit "
                        << "command to prevent corruption." << std::endl;

                    std::cout << WarningMsg(msg.str()) << std::endl;

                    return nullptr;
                }
            }

            if (handled)
            {
                continue;
            }

            const std::string alreadyOpenMsg =
                "MemoryMappedFile::open: The process cannot access the file "
                "because it is being used by another process.";

            const std::string errorMsg = e.what();

            /* The message actually has a \r\n on the end but i'd prefer to
               keep just the raw string in the source so check the it starts
               with instead */
            if (boost::starts_with(errorMsg, alreadyOpenMsg))
            {
                std::cout << WarningMsg("Could not open wallet! It is already "
                                        "open in another process.")
                          << std::endl
                          << WarningMsg("Check with a task manager that you "
                                        "don't have ")
                          << WalletConfig::walletName
                          << WarningMsg(" open twice.")
                          << std::endl
                          << WarningMsg("Also check you don't have another "
                                        "wallet program open, such as a GUI "
                                        "wallet or ")
                          << WarningMsg(WalletConfig::walletdName)
                          << WarningMsg(".")
                          << std::endl << std::endl;

                return nullptr;
            }
            else
            {
                std::cout << "Unexpected error: " << errorMsg << std::endl;
                std::cout << "Please report this error message and what "
                          << "you did to cause it." << std::endl << std::endl;

                return nullptr;
            }
        }
    }
}

Crypto::SecretKey getPrivateKey(std::string msg)
{
    const size_t privateKeyLen = 64;
    size_t size;

    std::string privateKeyString;
    Crypto::Hash privateKeyHash;
    Crypto::SecretKey privateKey;
    Crypto::PublicKey publicKey;

    while (true)
    {
        std::cout << InformationMsg(msg);

        std::getline(std::cin, privateKeyString);
        boost::algorithm::trim(privateKeyString);

        if (privateKeyString.length() != privateKeyLen)
        {
            std::cout << std::endl
                      << WarningMsg("Invalid private key, should be 64 ")
                      << WarningMsg("characters! Try again.") << std::endl
                      << std::endl;

            continue;
        }
        else if (!Common::fromHex(privateKeyString, &privateKeyHash,
                  sizeof(privateKeyHash), size)
               || size != sizeof(privateKeyHash))
        {
            std::cout << WarningMsg("Invalid private key, it is not a valid ")
                      << WarningMsg("hex string! Try again.")
                      << std::endl << std::endl;

            continue;
        }

        privateKey = *(struct Crypto::SecretKey *) &privateKeyHash;

        /* Just used for verification purposes before we pass it to
           walletgreen */
        if (!Crypto::secret_key_to_public_key(privateKey, publicKey))
        {
            std::cout << std::endl
                      << WarningMsg("Invalid private key, is not on the ")
                      << WarningMsg("ed25519 curve!") << std::endl
                      << WarningMsg("Probably a typo - ensure you entered ")
                      << WarningMsg("it correctly.")
                      << std::endl << std::endl;

            continue;
        }

        return privateKey;
    }
}

std::string getExistingWalletFileName(Config &config)
{
    bool initial = true;

    std::string walletName;

    while (true)
    {
        /* Only use wallet file once in case it is incorrect */
        if (config.walletGiven && initial)
        {
            walletName = config.walletFile;
        }
        else
        {
            std::cout << InformationMsg("What is the name of the wallet ")
                      << InformationMsg("you want to open?: ");

            std::getline(std::cin, walletName);
        }

        initial = false;

        const std::string walletFileName = walletName + ".wallet";

        if (walletName == "")
        {
            std::cout << std::endl
                      << WarningMsg("Wallet name can't be blank! Try again.")
                      << std::endl << std::endl;
        }
        /* Allow people to enter wallet name with or without file extension */
        else if (boost::filesystem::exists(walletName))
        {
            return walletName;
        }
        else if (boost::filesystem::exists(walletFileName))
        {
            return walletFileName;
        }
        else
        {
            std::cout << std::endl
                      << WarningMsg("A wallet with the filename ")
                      << InformationMsg(walletName)
                      << WarningMsg(" or ")
                      << InformationMsg(walletFileName)
                      << WarningMsg(" doesn't exist!")
                      << std::endl
                      << "Ensure you entered your wallet name correctly."
                      << std::endl << std::endl;
        }
    }
}

std::string getNewWalletFileName()
{
    std::string walletName;

    while (true)
    {
        std::cout << InformationMsg("What would you like to call your ")
                  << InformationMsg("new wallet?: ");

        std::getline(std::cin, walletName);

        const std::string walletFileName = walletName + ".wallet";

        if (boost::filesystem::exists(walletFileName))
        {
            std::cout << std::endl
                      << WarningMsg("A wallet with the filename " )
                      << InformationMsg(walletFileName)
                      << WarningMsg(" already exists!")
                      << std::endl
                      << "Try another name." << std::endl << std::endl;
        }
        else if (walletName == "")
        {
            std::cout << std::endl
                      << WarningMsg("Wallet name can't be blank! Try again.")
                      << std::endl << std::endl;
        }
        else
        {
            return walletFileName;
        }
    }
}

std::string getWalletPassword(bool verifyPwd, std::string msg)
{
    Tools::PasswordContainer pwdContainer;
    pwdContainer.read_password(verifyPwd, msg);
    return pwdContainer.password();
}

void viewWalletMsg()
{
    std::cout << InformationMsg("Please remember that when using a view wallet "
                                "you can only view transactions!")
              << std::endl
              << InformationMsg("If you used old software for sending, ")
              << InformationMsg("your balance may appear incorrect.") << std::endl;
}

void connectingMsg()
{
    std::cout << std::endl << "Making initial contact with "
              << WalletConfig::daemonName
              << "."
              << std::endl
              << "Please wait, this sometimes can take a long time..."
              << std::endl << std::endl;
}

void promptSaveKeys(CryptoNote::WalletGreen &wallet)
{
    std::cout << "Welcome to your new wallet, here is your payment address:"
              << std::endl << InformationMsg(wallet.getAddress(0))
              << std::endl << std::endl
              << "Please copy your secret keys and mnemonic seed and store "
              << "them in a secure location: " << std::endl;

    printPrivateKeys(wallet, false);

    std::cout << std::endl;
}
