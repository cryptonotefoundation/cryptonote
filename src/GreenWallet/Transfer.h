// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2019, The Karbo Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <memory>

#include <GreenWallet/Types.h>
#include <GreenWallet/WalletConfig.h>

enum BalanceInfo { NotEnoughBalance, EnoughBalance, SetMixinToZero };
void transfer(std::shared_ptr<WalletInfo> walletInfo, uint32_t height,
	bool sendAll = false, std::string nodeAddress = std::string(), uint64_t nodeFee = 0);

void doTransfer(std::string address, uint64_t amount, uint64_t fee,
                std::string extra, std::shared_ptr<WalletInfo> walletInfo,
                uint32_t height, uint64_t mixin = WalletConfig::defaultMixin,
                std::string nodeAddress = std::string(), uint64_t nodeFee = 0);

void sendMultipleTransactions(CryptoNote::WalletGreen &wallet,
                              std::vector<CryptoNote::TransactionParameters>
                              transfers);

void splitTx(CryptoNote::WalletGreen &wallet,
             CryptoNote::TransactionParameters p);

bool confirmTransaction(CryptoNote::TransactionParameters t,
                        std::shared_ptr<WalletInfo> walletInfo, uint64_t nodeFee);

bool parseAmount(std::string strAmount, uint64_t &amount);

bool parseAmount(std::string amountString);

bool parseAddress(std::string address);

bool parseFee(std::string feeString);

#ifndef __ANDROID__
bool getOpenAlias(const std::string& alias, std::string& address);

bool processServerAliasResponse(const std::string& s, std::string& address);

bool askAliasesTransfersConfirmation(const std::string address);
#endif

std::string getExtraFromPaymentID(std::string paymentID);

#ifndef __ANDROID__
std::string resolveAlias(const std::string& aliasUrl);
#endif

Maybe<std::string> getPaymentID(std::string msg);

Maybe<std::string> getExtra();

Maybe<std::string> getDestinationAddress();

Maybe<uint64_t> getFee();

Maybe<uint64_t> getTransferAmount();

BalanceInfo doWeHaveEnoughBalance(uint64_t amount, uint64_t fee,
	std::shared_ptr<WalletInfo> walletInfo,
	uint32_t height, uint64_t nodeFee);

uint64_t calculateNodeFee(uint64_t amount);
