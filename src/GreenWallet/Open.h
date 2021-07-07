// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2019, The Karbo Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <GreenWallet/Types.h>

std::shared_ptr<WalletInfo> importFromKeys(CryptoNote::WalletGreen &wallet, 
                                           Crypto::SecretKey privateSpendKey,
                                           Crypto::SecretKey privateViewKey);

std::shared_ptr<WalletInfo> openWallet(CryptoNote::WalletGreen &wallet,
                                       Config &config);

std::shared_ptr<WalletInfo> createViewWallet(CryptoNote::WalletGreen &wallet);

std::shared_ptr<WalletInfo> importWallet(CryptoNote::WalletGreen &wallet);

std::shared_ptr<WalletInfo> createViewWallet(CryptoNote::WalletGreen &wallet);

std::shared_ptr<WalletInfo> mnemonicImportWallet(CryptoNote::WalletGreen 
                                                 &wallet);

std::shared_ptr<WalletInfo> generateWallet(CryptoNote::WalletGreen &wallet);

std::shared_ptr<WalletInfo> importGUIWallet(CryptoNote::WalletGreen &wallet);

Crypto::SecretKey getPrivateKey(std::string outputMsg);

std::string getNewWalletFileName();

std::string getExistingWalletFileName(Config &config);

std::string getWalletPassword(bool verifyPwd, std::string msg);

bool isValidMnemonic(std::string &mnemonic_phrase, 
                     Crypto::SecretKey &private_spend_key);

void logIncorrectMnemonicWords(std::vector<std::string> words);

void viewWalletMsg();

void connectingMsg();

void promptSaveKeys(CryptoNote::WalletGreen &wallet);
