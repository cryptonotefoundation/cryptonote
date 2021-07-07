// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2019, The Karbo Developers
// 
// Please see the included LICENSE file for more information.

////////////////////////////////////////
#include <GreenWallet/CommandDispatcher.h>
////////////////////////////////////////

#include <GreenWallet/AddressBook.h>
#include <Common/ColouredMsg.h>
#include <GreenWallet/CommandImplementations.h>
#include <GreenWallet/Fusion.h>
#include <GreenWallet/Open.h>
#include <GreenWallet/Transfer.h>

bool handleCommand(const std::string command,
                   std::shared_ptr<WalletInfo> walletInfo,
                   CryptoNote::INode &node)
{
    /* Basic commands */
    if (command == "advanced")
    {
        advanced(walletInfo);
    }
    else if (command == "address")
    {
        std::cout << SuccessMsg(walletInfo->walletAddress) << std::endl;
    }
    else if (command == "balance")
    {
        balance(node, walletInfo->wallet, walletInfo->viewWallet);
    }
    else if (command == "backup")
    {
        exportKeys(walletInfo);
    }
    else if (command == "exit")
    {
        return false;
    }
    else if (command == "help")
    {
        help(walletInfo);
    }
    else if (command == "transfer")
    {
        transfer(walletInfo, node.getLastKnownBlockHeight(), false, node.feeAddress(), node.feeAmount());
    }
    /* Advanced commands */
    else if (command == "ab_add")
    {
        addToAddressBook();
    }
    else if (command == "ab_delete")
    {
        deleteFromAddressBook();
    }
    else if (command == "ab_list")
    {
        listAddressBook();
    }
    else if (command == "ab_send")
    {
        sendFromAddressBook(walletInfo, node.getLastKnownBlockHeight(),
                            node.feeAddress(), node.feeAmount());
    }
    else if (command == "change_password")
    {
        changePassword(walletInfo);
    }
    else if (command == "incoming_transfers")
    {
        listTransfers(true, false, walletInfo->wallet, node);
    }
    else if (command == "list_transfers")
    {
        listTransfers(true, true, walletInfo->wallet, node);
    }
    else if (command == "optimize")
    {
        fullOptimize(walletInfo->wallet);
    }
    else if (command == "outgoing_transfers")
    {
        listTransfers(false, true, walletInfo->wallet, node);
    }
    else if (command == "reset")
    {
        reset(node, walletInfo);
    }
    else if (command == "reserve_proof")
    {
        reserveProof(walletInfo, walletInfo->viewWallet);
    }
    else if (command == "save")
    {
		save(walletInfo->wallet);
    }
    else if (command == "save_csv")
    {
        saveCSV(walletInfo->wallet, node);
    }
    else if (command == "send_all")
    {
        transfer(walletInfo, node.getLastKnownBlockHeight(), true, node.feeAddress());
    }
    else if (command == "status")
    {
        status(node, walletInfo->wallet);
    }
    else if (command == "tx_key")
    {
        txSecretKey(walletInfo->wallet);
    }
    else if (command == "tx_proof")
    {
        txProof(walletInfo->wallet);
    }
    else if (command == "sign_message")
    {
      signMessage(walletInfo, walletInfo->viewWallet);
    }
    else if (command == "verify_message")
    {
      verifyMessage(walletInfo->wallet);
    }
    /* This should never happen */
    else
    {
        throw std::runtime_error("Command was defined but not hooked up!");
    }

    return true;
}

std::shared_ptr<WalletInfo> handleLaunchCommand(CryptoNote::WalletGreen &wallet,
                                                std::string launchCommand,
                                                Config &config)
{
    if (launchCommand == "create")
    {
        return generateWallet(wallet);
    }
    else if (launchCommand == "open")
    {
        return openWallet(wallet, config);
    }
    else if (launchCommand == "seed_restore")
    {
        return mnemonicImportWallet(wallet);
    }
    else if (launchCommand == "key_restore")
    {
        return importWallet(wallet);
    }
    else if (launchCommand == "view_wallet")
    {
        return createViewWallet(wallet);
    }
    /* This should never happen */
    else
    {
        throw std::runtime_error("Command was defined but not hooked up!");
    }
}
