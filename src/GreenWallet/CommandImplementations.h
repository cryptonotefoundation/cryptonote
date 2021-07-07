// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2019, The Karbo Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <GreenWallet/Types.h>

#include <Wallet/WalletGreen.h>

bool handleCommand(const std::string command,
                   std::shared_ptr<WalletInfo> walletInfo,
                   CryptoNote::INode &node);

void changePassword(std::shared_ptr<WalletInfo> walletInfo);

void printPrivateKeys(CryptoNote::WalletGreen &wallet, bool viewWallet);

void reset(CryptoNote::INode &node, std::shared_ptr<WalletInfo> walletInfo);

void status(CryptoNote::INode &node, CryptoNote::WalletGreen &wallet);

void printHeights(uint32_t localHeight, uint32_t remoteHeight,
	uint32_t walletHeight);

void printSyncStatus(uint32_t localHeight, uint32_t remoteHeight,
	uint32_t walletHeight);

void printSyncSummary(uint32_t localHeight, uint32_t remoteHeight,
	uint32_t walletHeight);

void printPeerCount(size_t peerCount);

void printHashrate(uint64_t difficulty);

void blockchainHeight(CryptoNote::INode &node, CryptoNote::WalletGreen &wallet);

void balance(CryptoNote::INode &node, CryptoNote::WalletGreen &wallet,
             bool viewWallet);

void exportKeys(std::shared_ptr<WalletInfo> walletInfo);

void saveCSV(CryptoNote::WalletGreen &wallet, CryptoNote::INode &node);

void save(CryptoNote::WalletGreen &wallet);

void listTransfers(bool incoming, bool outgoing, 
                   CryptoNote::WalletGreen &wallet, CryptoNote::INode &node);

void printOutgoingTransfer(CryptoNote::WalletTransaction t,
                           CryptoNote::INode &node);

void printIncomingTransfer(CryptoNote::WalletTransaction t,
                           CryptoNote::INode &node);

std::string getGUIPrivateKey(CryptoNote::WalletGreen &wallet);

void help(std::shared_ptr<WalletInfo> wallet);

void advanced(std::shared_ptr<WalletInfo> wallet);

void reserveProof(std::shared_ptr<WalletInfo> walletInfo, bool viewWallet);

void txSecretKey(CryptoNote::WalletGreen &wallet);

void txProof(CryptoNote::WalletGreen &wallet);

void signMessage(std::shared_ptr<WalletInfo> walletInfo, bool viewWallet);

void verifyMessage(CryptoNote::WalletGreen &wallet);
