// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2019, The Karbo Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <Wallet/WalletGreen.h>

bool fusionTX(CryptoNote::WalletGreen &wallet, 
              CryptoNote::TransactionParameters p);

bool optimize(CryptoNote::WalletGreen &wallet, uint64_t threshold);

void fullOptimize(CryptoNote::WalletGreen &wallet);

size_t makeFusionTransaction(CryptoNote::WalletGreen &wallet, 
                             uint64_t threshold);
