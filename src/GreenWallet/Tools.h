// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2019, The Karbo Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <iterator>

#include <GreenWallet/Types.h>

void confirmPassword(std::string walletPass, std::string msg = "");

bool confirm(std::string msg);
bool confirm(std::string msg, bool defaultReturn);

std::string formatAmountBasic(uint64_t amount);
std::string formatAmount(uint64_t amount);
std::string formatDollars(uint64_t amount);
std::string formatCents(uint64_t amount);

uint64_t calculateNodeFee(uint64_t amount);

std::string getPaymentIDFromExtra(std::string extra);

std::string unixTimeToDate(uint64_t timestamp);

std::string createIntegratedAddress(std::string address, std::string paymentID);

bool shutdown(std::shared_ptr<WalletInfo> walletInfo, CryptoNote::INode &node,
              bool &alreadyShuttingDown);

uint64_t getDivisor();

uint64_t getScanHeight();

template <typename T, typename Function>
std::vector<T> filter(std::vector<T> input, Function predicate)
{
	std::vector<T> result;

	std::copy_if(
		input.begin(), input.end(), std::back_inserter(result), predicate
	);

	return result;
}
