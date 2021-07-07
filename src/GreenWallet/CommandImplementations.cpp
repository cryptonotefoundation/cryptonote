// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2019, The Karbo Developers
// 
// Please see the included LICENSE file for more information.

/////////////////////////////////////////////
#include <GreenWallet/CommandImplementations.h>
/////////////////////////////////////////////

#include <atomic>
#include <initializer_list>
#include "Common/Base58.h"
#include "Common/StringTools.h"
#include "Common/FormatTools.h"
#include "Tools.h"
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"

#ifndef MSVC
#include <fstream>
#endif

#include "Mnemonics/electrum-words.h"

#include <GreenWallet/AddressBook.h>
#include <Common/ColouredMsg.h>
#include <GreenWallet/Commands.h>
#include <GreenWallet/Fusion.h>
#include <GreenWallet/Menu.h>
#include <GreenWallet/Open.h>
#include <GreenWallet/Sync.h>
#include <GreenWallet/Tools.h>
#include <GreenWallet/Transfer.h>
#include <GreenWallet/Types.h>
#include <GreenWallet/WalletConfig.h>

void changePassword(std::shared_ptr<WalletInfo> walletInfo)
{
    /* Check the user knows the current password */
    confirmPassword(walletInfo->walletPass, "Confirm your current password: ");

    /* Get a new password for the wallet */
    const std::string newPassword
        = getWalletPassword(true, "Enter your new password: ");

    /* Change the wallet password */
    walletInfo->wallet.changePassword(walletInfo->walletPass, newPassword);

    /* Change the stored wallet metadata */
    walletInfo->walletPass = newPassword;

    /* Make sure we save with the new password */
	walletInfo->wallet.save();

    std::cout << SuccessMsg("Your password has been changed!") << std::endl;
}

void exportKeys(std::shared_ptr<WalletInfo> walletInfo)
{
    confirmPassword(walletInfo->walletPass);
    printPrivateKeys(walletInfo->wallet, walletInfo->viewWallet);
}

std::string getGUIPrivateKey(CryptoNote::WalletGreen &wallet)
{
    auto viewKey = wallet.getViewKey();
    auto spendKey = wallet.getAddressSpendKey(0);

    CryptoNote::AccountPublicAddress addr
    {
        spendKey.publicKey,
        viewKey.publicKey,
    };

    CryptoNote::AccountKeys keys
    {
        addr,
        spendKey.secretKey,
        viewKey.secretKey,
    };

    return Tools::Base58::encode_addr
    (
        CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
        std::string(reinterpret_cast<char*>(&keys), sizeof(keys))
    );
}

void printPrivateKeys(CryptoNote::WalletGreen &wallet, bool viewWallet)
{
    auto privateViewKey = wallet.getViewKey().secretKey;

    if (viewWallet)
    {
        std::cout << SuccessMsg("Private view key:")
                  << std::endl
                  << SuccessMsg(Common::podToHex(privateViewKey))
                  << std::endl;
        return;
    }

	auto privateSpendKey = wallet.getAddressSpendKey(0).secretKey;

    Crypto::SecretKey derivedPrivateViewKey;

    CryptoNote::AccountBase::generateViewFromSpend(privateSpendKey,
                                                   derivedPrivateViewKey);

    const bool deterministicPrivateKeys
             = derivedPrivateViewKey == privateViewKey;

    std::cout << SuccessMsg("Private spend key:")
              << std::endl
              << SuccessMsg(Common::podToHex(privateSpendKey))
              << std::endl
              << std::endl
              << SuccessMsg("Private view key:")
              << std::endl
              << SuccessMsg(Common::podToHex(privateViewKey))
              << std::endl;

    if (deterministicPrivateKeys)
    {
        std::string mnemonicSeed;

        Crypto::ElectrumWords::bytes_to_words(privateSpendKey, 
                                              mnemonicSeed,
                                              "English");

        std::cout << std::endl
                  << SuccessMsg("Mnemonic seed:")
                  << std::endl
                  << SuccessMsg(mnemonicSeed)
                  << std::endl;
    }

    std::cout << std::endl
              << SuccessMsg("GUI Importable Private Key:")
              << std::endl
              << SuccessMsg(getGUIPrivateKey(wallet))
              << std::endl;
}

void balance(CryptoNote::INode &node, CryptoNote::WalletGreen &wallet,
             bool viewWallet)
{
    const uint64_t unconfirmedBalance = wallet.getPendingBalance();
    const uint64_t confirmedBalance = wallet.getActualBalance();
    const uint64_t totalBalance = unconfirmedBalance + confirmedBalance;

    const uint32_t localHeight = node.getLastLocalBlockHeight();
    const uint32_t remoteHeight = node.getLastKnownBlockHeight();
    const uint32_t walletHeight = wallet.getBlockCount();

    std::cout << "Available balance: "
              << SuccessMsg(formatAmount(confirmedBalance)) << std::endl
              << "Locked (unconfirmed) balance: "
              << WarningMsg(formatAmount(unconfirmedBalance))
              << std::endl << "Total balance: "
              << InformationMsg(formatAmount(totalBalance)) << std::endl;

    if (viewWallet)
    {
        std::cout << std::endl 
                  << InformationMsg("Please note that view only wallets "
                                    "can only track incoming transactions,")
                  << std::endl
                  << InformationMsg("and so your wallet balance may appear "
                                    "inflated.") << std::endl;
    }

    if (localHeight < remoteHeight)
    {
        std::cout << std::endl
                  << InformationMsg("Your daemon is not fully synced with "
                                    "the network!")
                  << std::endl
                  << "Your balance may be incorrect until you are fully "
                  << "synced!" << std::endl;
    }
    /* Small buffer because wallet height doesn't update instantly like node
       height does */
    else if (walletHeight + 1000 < remoteHeight)
    {
        std::cout << std::endl
                  << InformationMsg("The blockchain is still being scanned for "
                                    "your transactions.")
                  << std::endl
                  << "Balances might be incorrect whilst this is ongoing."
                  << std::endl;
    }
}

void blockchainHeight(CryptoNote::INode &node, CryptoNote::WalletGreen &wallet)
{
    const uint32_t localHeight = node.getLastLocalBlockHeight();
    const uint32_t remoteHeight = node.getLastKnownBlockHeight();
    const uint32_t walletHeight = wallet.getBlockCount() - 1;

    /* This is the height that the wallet has been scanned to. The blockchain
       can be fully updated, but we have to walk the chain to find our
       transactions, and this number indicates that progress. */
    std::cout << "Wallet blockchain height: ";

    /* Small buffer because wallet height doesn't update instantly like node
       height does */
    if (walletHeight + 1000 > remoteHeight)
    {
        std::cout << SuccessMsg(std::to_string(walletHeight));
    }
    else
    {
        std::cout << WarningMsg(std::to_string(walletHeight));
    }

    std::cout << std::endl << "Local blockchain height: ";

    if (localHeight == remoteHeight)
    {
        std::cout << SuccessMsg(std::to_string(localHeight));
    }
    else
    {
        std::cout << WarningMsg(std::to_string(localHeight));
    }

    std::cout << std::endl << "Network blockchain height: "
              << SuccessMsg(std::to_string(remoteHeight)) << std::endl;

    if (localHeight == 0 && remoteHeight == 0)
    {
        std::cout << WarningMsg("Uh oh, it looks like you don't have ")
                  << WarningMsg(WalletConfig::daemonName)
                  << WarningMsg(" open!")
                  << std::endl;
    }
    else if (walletHeight + 1000 < remoteHeight && localHeight == remoteHeight)
    {
        std::cout << InformationMsg("You are synced with the network, but the "
                                    "blockchain is still being scanned for "
                                    "your transactions.")
                  << std::endl
                  << "Balances might be incorrect whilst this is ongoing."
                  << std::endl;
    }
    else if (localHeight == remoteHeight)
    {
        std::cout << SuccessMsg("Yay! You are synced!") << std::endl;
    }
    else
    {
        std::cout << WarningMsg("Be patient, you are still syncing with the "
                                "network!") << std::endl;
    }
}

void printHeights(uint32_t localHeight, uint32_t remoteHeight,
	uint32_t walletHeight)
{
	/* This is the height that the wallet has been scanned to. The blockchain
	   can be fully updated, but we have to walk the chain to find our
	   transactions, and this number indicates that progress. */
	std::cout << "Wallet blockchain height: ";

	/* Small buffer because wallet height doesn't update instantly like node
	   height does */
	if (walletHeight + 1000 > remoteHeight)
	{
		std::cout << SuccessMsg(std::to_string(walletHeight));
	}
	else
	{
		std::cout << WarningMsg(std::to_string(walletHeight));
	}

	std::cout << std::endl << "Local blockchain height: ";

	if (localHeight == remoteHeight)
	{
		std::cout << SuccessMsg(std::to_string(localHeight));
	}
	else
	{
		std::cout << WarningMsg(std::to_string(localHeight));
	}

	std::cout << std::endl << "Network blockchain height: "
		<< SuccessMsg(std::to_string(remoteHeight)) << std::endl;
}

void printSyncStatus(uint32_t localHeight, uint32_t remoteHeight,
	uint32_t walletHeight)
{
	std::string networkSyncPercentage
		= Common::Format::get_sync_percentage(localHeight, remoteHeight) + "%";

	std::string walletSyncPercentage
		= Common::Format::get_sync_percentage(walletHeight, remoteHeight) + "%";

	std::cout << "Network sync status: ";

	if (localHeight == remoteHeight)
	{
		std::cout << SuccessMsg(networkSyncPercentage) << std::endl;
	}
	else
	{
		std::cout << WarningMsg(networkSyncPercentage) << std::endl;
	}

	std::cout << "Wallet sync status: ";

	/* Small buffer because wallet height is not always completely accurate */
	if (walletHeight + 1000 > remoteHeight)
	{
		std::cout << SuccessMsg(walletSyncPercentage) << std::endl;
	}
	else
	{
		std::cout << WarningMsg(walletSyncPercentage) << std::endl;
	}
}

void printSyncSummary(uint32_t localHeight, uint32_t remoteHeight,
	uint32_t walletHeight)
{
	if (localHeight == 0 && remoteHeight == 0)
	{
		std::cout << WarningMsg("Uh oh, it looks like you don't have ")
			<< WarningMsg(WalletConfig::daemonName)
			<< WarningMsg(" open!")
			<< std::endl;
	}
	else if (walletHeight + 1000 < remoteHeight && localHeight == remoteHeight)
	{
		std::cout << InformationMsg("You are synced with the network, but the "
			"blockchain is still being scanned for "
			"your transactions.")
			<< std::endl
			<< "Balances might be incorrect whilst this is ongoing."
			<< std::endl;
	}
	else if (localHeight == remoteHeight)
	{
		std::cout << SuccessMsg("Yay! You are synced!") << std::endl;
	}
	else
	{
		std::cout << WarningMsg("Be patient, you are still syncing with the "
			"network!") << std::endl;
	}
}

void printPeerCount(size_t peerCount)
{
    std::cout << "Peers: " << SuccessMsg(std::to_string(peerCount))
              << std::endl;
}

void printHashrate(uint64_t difficulty)
{
    /* Offline node / not responding */
    if (difficulty == 0)
    {
        return;
    }

    /* Hashrate is difficulty divided by block target time */
    uint32_t hashrate = static_cast<uint32_t>(
        round(difficulty / CryptoNote::parameters::DIFFICULTY_TARGET)
    );

    std::cout << "Network hashrate: "
              << SuccessMsg(Common::Format::get_mining_speed(hashrate))
              << " (Based on the last local block)" << std::endl;
}

/* This makes sure to call functions on the node which only return cached
   data. This ensures it returns promptly, and doesn't hang waiting for a
   response when the node is having issues. */
void status(CryptoNote::INode &node, CryptoNote::WalletGreen &wallet)
{
    uint32_t localHeight = node.getLastLocalBlockHeight();
    uint32_t remoteHeight = node.getLastKnownBlockHeight();
    uint32_t walletHeight = wallet.getBlockCount() - 1;

    /* Print the heights of local, remote, and wallet */
    printHeights(localHeight, remoteHeight, walletHeight);

    std::cout << std::endl;

    /* Print the network and wallet sync status in percentage */
    printSyncStatus(localHeight, remoteHeight, walletHeight);

    std::cout << std::endl;

    /* Print the network hashrate, based on the last local block */
    printHashrate(node.getLastLocalBlockHeaderInfo().difficulty);

    /* Print the amount of peers we have */
    printPeerCount(node.getPeerCount());

    std::cout << std::endl;

    /* Print a summary of the sync status */
    printSyncSummary(localHeight, remoteHeight, walletHeight);
}

void reset(CryptoNote::INode &node, std::shared_ptr<WalletInfo> walletInfo)
{
	uint64_t scanHeight = getScanHeight();

	std::cout << std::endl
		<< InformationMsg("This process may take some time to complete.")
		<< std::endl
		<< InformationMsg("You can't make any transactions during the ")
		<< InformationMsg("process.")
		<< std::endl << std::endl;

	if (!confirm("Are you sure?"))
	{
		return;
	}

	std::cout << InformationMsg("Resetting wallet...") << std::endl;

	walletInfo->wallet.reset(scanHeight);

	syncWallet(node, walletInfo);
}

void saveCSV(CryptoNote::WalletGreen &wallet, CryptoNote::INode &node)
{
    const size_t numTransactions = wallet.getTransactionCount();

    std::ofstream csv;
    csv.open(WalletConfig::csvFilename);

    if (!csv)
    {
        std::cout << WarningMsg("Couldn't open transactions.csv file for "
                                "saving!")
                  << std::endl
                  << WarningMsg("Ensure it is not open in any other "
                                "application.")
                  << std::endl;
        return;
    }

    std::cout << InformationMsg("Saving CSV file...") << std::endl;

    /* Create CSV header */
    csv << "Timestamp,Block Height,Hash,Amount,In/Out"
        << std::endl;

    /* Loop through transactions */
    for (size_t i = 0; i < numTransactions; i++)
    {
        const CryptoNote::WalletTransaction t = wallet.getTransaction(i);

        /* Ignore fusion transactions */
        if (t.totalAmount == 0)
        {
            continue;
        }

        const std::string amount = formatAmountBasic(std::abs(t.totalAmount));

        const std::string direction = t.totalAmount > 0 ? "IN" : "OUT";

        csv << unixTimeToDate(t.timestamp) << ","       /* Timestamp */
            << t.blockHeight << ","                     /* Block Height */
            << Common::podToHex(t.hash) << ","          /* Hash */
            << amount << ","                            /* Amount */
            << direction                                /* In/Out */
            << std::endl;
    }

    csv.close();

    std::cout << SuccessMsg("CSV successfully written to ")
              << SuccessMsg(WalletConfig::csvFilename)
              << SuccessMsg("!")
              << std::endl;
}

void printOutgoingTransfer(CryptoNote::WalletTransaction t,
                           CryptoNote::INode &node)
{
    std::cout << WarningMsg("Outgoing transfer:")
              << std::endl
              << WarningMsg("Hash: " + Common::podToHex(t.hash))
              << std::endl;

    /* Block height will be garbage from memory if not confirmed yet */
    if (t.timestamp != 0)
    {
        std::cout << WarningMsg("Block height: ")
                  << WarningMsg(std::to_string(t.blockHeight))
                  << std::endl
                  << WarningMsg("Timestamp: ")
                  << WarningMsg(unixTimeToDate(t.timestamp))
                  << std::endl;
    }

    std::cout << WarningMsg("Spent: " + formatAmount(-t.totalAmount - t.fee))
              << std::endl
              << WarningMsg("Fee: " + formatAmount(t.fee))
              << std::endl
              << WarningMsg("Total Spent: " + formatAmount(-t.totalAmount))
              << std::endl;

    const std::string paymentID = getPaymentIDFromExtra(t.extra);

    if (paymentID != "")
    {
        std::cout << WarningMsg("Payment ID: " + paymentID) << std::endl;
    }

    std::cout << std::endl;
}

void printIncomingTransfer(CryptoNote::WalletTransaction t,
                           CryptoNote::INode &node)
{
    std::cout << SuccessMsg("Incoming transfer:")
              << std::endl
              << SuccessMsg("Block height: " + std::to_string(t.blockHeight))
              << std::endl;

    /* Block height will be garbage from memory if not confirmed yet */
    if (t.timestamp != 0)
    {
        std::cout << SuccessMsg("Block height: ")
                  << SuccessMsg(std::to_string(t.blockHeight))
                  << std::endl
                  << SuccessMsg("Timestamp: ")
                  << SuccessMsg(unixTimeToDate(t.timestamp))
                  << std::endl;
    }

    std::cout << SuccessMsg("Hash: " + Common::podToHex(t.hash))
              << std::endl
              << SuccessMsg("Amount: " + formatAmount(t.totalAmount))
              << std::endl;

    const std::string paymentID = getPaymentIDFromExtra(t.extra);

    if (paymentID != "")
    {
        std::cout << SuccessMsg("Payment ID: " + paymentID) << std::endl;
    }

    std::cout << std::endl;
}

void listTransfers(bool incoming, bool outgoing, 
                   CryptoNote::WalletGreen &wallet, CryptoNote::INode &node)
{
    const size_t numTransactions = wallet.getTransactionCount();

    int64_t totalSpent = 0;
    int64_t totalReceived = 0;

    for (size_t i = 0; i < numTransactions; i++)
    {
        const CryptoNote::WalletTransaction t = wallet.getTransaction(i);

        if (t.totalAmount < 0 && outgoing)
        {
            printOutgoingTransfer(t, node);
            totalSpent += -t.totalAmount;
        }
        else if (t.totalAmount > 0 && incoming)
        {
            printIncomingTransfer(t, node);
            totalReceived += t.totalAmount;
        }
    }

    if (incoming)
    {
        std::cout << SuccessMsg("Total received: " 
                              + formatAmount(totalReceived))
                  << std::endl;
    }

    if (outgoing)
    {
        std::cout << WarningMsg("Total spent: " + formatAmount(totalSpent))
                  << std::endl;
    }
}

void save(CryptoNote::WalletGreen &wallet)
{
	std::cout << InformationMsg("Saving.") << std::endl;
	wallet.save();
	std::cout << InformationMsg("Saved.") << std::endl;
}

void help(std::shared_ptr<WalletInfo> wallet)
{
    if (wallet->viewWallet)
    {
        printCommands(basicViewWalletCommands());
    }
    else
    {
        printCommands(basicCommands());
    }
}

void advanced(std::shared_ptr<WalletInfo> wallet)
{
    /* We pass the offset of the command to know what index to print for
       command numbers */
    if (wallet->viewWallet)
    {
        printCommands(advancedViewWalletCommands(),
                      (int)basicViewWalletCommands().size());
    }
    else
    {
        printCommands(advancedCommands(),
                      (int)basicCommands().size());
    }
}

void txSecretKey(CryptoNote::WalletGreen &wallet)
{
    std::string hashStr;
	Crypto::Hash txid;

    while (true)
    {
        std::cout << InformationMsg("Enter transaction hash: ");

        std::getline(std::cin, hashStr);
        boost::algorithm::trim(hashStr);

        if (!parse_hash256(hashStr, txid)) {
            std::cout << WarningMsg("Failed to parse txid") << std::endl;
            return;
        }
        else {
            break;
        }

        if (std::cin.fail() || std::cin.eof()) {
            std::cin.clear();
            break;
        }        
    }

    Crypto::SecretKey txSecretKey = wallet.getTransactionSecretKey(txid);

    if (txSecretKey == CryptoNote::NULL_SECRET_KEY) {
        std::cout << WarningMsg("Transaction ") 
                  << WarningMsg(hashStr)
                  << WarningMsg(" secret key is not available")
                  << std::endl;
        return;
    }
		
    std::cout << SuccessMsg("Transaction secret key: ")
              << std::endl
              << SuccessMsg(Common::podToHex(txSecretKey))
              << std::endl;
}

void txProof(CryptoNote::WalletGreen &wallet)
{
    std::string txHashStr;
    Crypto::Hash txid;

    while (true)
    {
        std::cout << InformationMsg("Enter transaction hash: ");

        std::getline(std::cin, txHashStr);
        boost::algorithm::trim(txHashStr);

        if (!parse_hash256(txHashStr, txid)) {
            std::cout << WarningMsg("Failed to parse txid") << std::endl;
            return;
        }
        else {
            break;
        }

        if (std::cin.fail() || std::cin.eof()) {
            std::cin.clear();
            break;
        }
    }
	  
    Crypto::SecretKey txSecretKey = wallet.getTransactionSecretKey(txid);

    if (txSecretKey == CryptoNote::NULL_SECRET_KEY) {
        std::cout << InformationMsg("Transaction ")
                  << InformationMsg(txHashStr)
                  << InformationMsg(" secret key is not available.")
                  << std::endl
                  << InformationMsg("If you have it elsewhere, ")
                  << InformationMsg("enter it here to continue: ")
                  << std::endl;

        Crypto::Hash tx_key_hash;

        while (true)
        {
            std::string keyStr;
			
            std::getline(std::cin, keyStr);
            boost::algorithm::trim(keyStr);

            size_t size;

            if (!Common::fromHex(keyStr, &tx_key_hash, sizeof(tx_key_hash), size)
                || size != sizeof(tx_key_hash))
            {
                std::cout << WarningMsg("Failed to parse tx secret key ")
                          << WarningMsg(keyStr) << std::endl;
                return;
            }
            else {     
                txSecretKey = *(struct Crypto::SecretKey *) &tx_key_hash;
				break;
            }

            if (std::cin.fail() || std::cin.eof()) {
                std::cin.clear();
                break;
            }
        }
    }

    CryptoNote::AccountPublicAddress destAddress;

    while (true)
    {
        std::cout << InformationMsg("Enter destination address: ");

        std::string addrStr;
        uint64_t prefix;

        std::getline(std::cin, addrStr);
        boost::algorithm::trim(addrStr);

        if (!CryptoNote::parseAccountAddressString(prefix, destAddress, addrStr))
        {
            std::cout << WarningMsg("Failed to parse address") << std::endl;
        }
        else
        {
            break;
        }

        if (std::cin.fail() || std::cin.eof()) {
            std::cin.clear();
            break;
        }
    }

    try {
        std::string sig;

        if (wallet.getTransactionProof(txid, destAddress, txSecretKey, sig)) {
            std::cout << SuccessMsg("Transaction proof: ")
                      << std::endl
                      << SuccessMsg(sig)
                      << std::endl;
        }
        else {
            std::cout << WarningMsg("Failed to get transaction proof") << std::endl;
        }
    }
    catch (std::system_error& x) {
        std::cout << WarningMsg("Error while getting transaction proof: ")
                  << WarningMsg(x.what())
                  << std::endl;
    }
    catch (std::exception& x) {
        std::cout << WarningMsg("Error while getting transaction proof: ")
                  << WarningMsg(x.what())
                  << std::endl;
    }
}

void reserveProof(std::shared_ptr<WalletInfo> walletInfo, bool viewWallet)
{
    if (viewWallet)
    {
        std::cout << WarningMsg("This is tracking wallet. ")
                  << WarningMsg("The reserve proof can be generated ")
                  << WarningMsg("only by a full wallet.")
                  << std::endl;
        return;
    }

    uint64_t amount;
    uint64_t actualBalance = walletInfo->wallet.getActualBalance();

    while (true)
    {
        std::cout << InformationMsg("Enter amount to prove ")
                  << InformationMsg("or type ") << "\"all\" "
                  << InformationMsg("if you want to prove all balance: ");

        std::string amtStr;

        std::getline(std::cin, amtStr);

        if (amtStr == "all")
        {
            amount = actualBalance;
            break;
        }

        if (parseAmount(amtStr, amount))
        {
            if (amount > actualBalance)
            {
                std::cout << WarningMsg("Amount is bigger than ")
                          << WarningMsg("actual balance ")
                          << WarningMsg(formatAmount(actualBalance))
                          << std::endl;
            }
            else {
                break;
            }
        }

        if (std::cin.fail() || std::cin.eof()) {
            std::cin.clear();
            break;
        }
    }

    std::string message;

    while (true)
    {
        std::cout << InformationMsg("Enter optional challenge message: ");

        std::getline(std::cin, message);
        boost::algorithm::trim(message);

        if (message == "")
        {
            break;
        }

        if (std::cin.fail() || std::cin.eof()) {
            std::cin.clear();
            break;
        }
    }

    try
    {
        const std::string sig = 
            walletInfo->wallet.getReserveProof(amount, 
                                               walletInfo->walletAddress,
                                               message.empty() ? "" : message);

        std::string fileName;

        while (true)
        {
            std::cout << InformationMsg("Enter file name ")
                      << InformationMsg("where to save your proof: ");

            std::getline(std::cin, fileName);
            boost::algorithm::trim(fileName);

            if (boost::filesystem::portable_name(fileName))
            {
                break;
            }
            else {
                std::cout << WarningMsg("Enter valid file name") << std::endl;
            }

	        if (std::cin.fail() || std::cin.eof()) {
                std::cin.clear();
                break;
            }
        }

        fileName += ".txt";

        boost::system::error_code ec;
        if (boost::filesystem::exists(fileName, ec)) {
            boost::filesystem::remove(fileName, ec);
        }

        std::ofstream proofFile(fileName, 
                                std::ios::out | 
                                std::ios::trunc | 
                                std::ios::binary);
          
        if (!proofFile.good())
        {
            std::cout << WarningMsg("Failed to save reserve proof to file")
                      << std::endl;
            return;
        }

        proofFile << sig;

        std::cout << SuccessMsg("Proof signature saved to file: ") 
                  << SuccessMsg(fileName)
                  << std::endl;
    }
    catch (const std::exception &e) {
        std::cout << WarningMsg("Failed to get reserve proof: ")
                  << WarningMsg(e.what())
                  << std::endl;
    }
}

void signMessage(std::shared_ptr<WalletInfo> walletInfo, bool viewWallet)
{
    if (viewWallet)
    {
        std::cout << WarningMsg("This is tracking wallet. ")
                  << WarningMsg("The message can be signed ")
                  << WarningMsg("only by a full wallet.")
                  << std::endl;
        return;
    }

    std::string message;

    while (true)
    {
        std::cout << InformationMsg("Enter message to sign: ");

        std::getline(std::cin, message);
        boost::algorithm::trim(message);

        if (!message.empty())
        {
          break;
        }

        if (std::cin.fail() || std::cin.eof()) {
            std::cin.clear();
            break;
        }
    }

    try
    {
        std::string walletAddress = walletInfo->walletAddress;

        std::string signature = walletInfo->wallet.signMessage(message, walletAddress);

        std::cout << SuccessMsg("Signature: ") 
                  << InformationMsg(signature)
                  << std::endl;
    }
    catch (const std::exception &e) {
        std::cout << WarningMsg("Failed to sign message: ")
                  << WarningMsg(e.what())
                  << std::endl;
    }
}

void verifyMessage(CryptoNote::WalletGreen &wallet)
{
    std::string addrStr;

    while (true)
    {
        std::cout << InformationMsg("Enter address: ");

        CryptoNote::AccountPublicAddress address;
        
        uint64_t prefix;

        std::getline(std::cin, addrStr);
        boost::algorithm::trim(addrStr);

        if (!CryptoNote::parseAccountAddressString(prefix, address, addrStr))
        {
            std::cout << WarningMsg("Failed to parse address") << std::endl;
        }
        else
        {
            break;
        }

        if (std::cin.fail() || std::cin.eof()) {
            std::cin.clear();
            break;
        }
    }

    std::string message;

    while (true)
    {
        std::cout << InformationMsg("Enter message: ");

        std::getline(std::cin, message);
        boost::algorithm::trim(message);

        if (!message.empty())
        {
            break;
        }

        if (std::cin.fail() || std::cin.eof()) {
            std::cin.clear();
            break;
        }
    }

    std::string signature;

    while (true)
    {
        std::cout << InformationMsg("Enter signature: ");

        std::getline(std::cin, signature);
        boost::algorithm::trim(signature);       

        if (!signature.empty())
        {
            break;
        }

        if (std::cin.fail() || std::cin.eof()) {
            std::cin.clear();
            break;
        }
    }

    try
    {
        bool r = wallet.verifyMessage(message, addrStr, signature);

        if (r)
        {
            std::cout << SuccessMsg("Signature is valid") 
                      << std::endl;
        }
        else
        {
            std::cout << WarningMsg("Signature is invalid")
                      << std::endl;
        }
    }
    catch (const std::exception &e) {
        std::cout << WarningMsg("Failed to verify message: ")
                  << WarningMsg(e.what())
                  << std::endl;
    }
}