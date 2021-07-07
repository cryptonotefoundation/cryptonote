// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2019, The Karbo Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <GreenWallet/Types.h>

template<typename T>
std::string parseCommand(const std::vector<T> &printableCommands,
                         const std::vector<T> &availableCommands,
                         std::string prompt,
                         bool backgroundRefresh,
                         std::shared_ptr<WalletInfo> walletInfo);

std::tuple<bool, std::shared_ptr<WalletInfo>>
    selectionScreen(Config &config, CryptoNote::WalletGreen &wallet,
                    CryptoNote::INode &node);

bool checkNodeStatus(CryptoNote::INode &node);

std::string getAction(Config &config);

void mainLoop(std::shared_ptr<WalletInfo> walletInfo, CryptoNote::INode &node);

template<typename T>
void printCommands(const std::vector<T> &commands, int offset = 0);
