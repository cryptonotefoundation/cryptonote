// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#include "gtest/gtest.h"

#include "IWalletLegacy.h"

#include "crypto/crypto.h"
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/TransactionApi.h"
#include "Logging/ConsoleLogger.h"
#include "Transfers/TransfersContainer.h"

#include "TransactionApiHelpers.h"

using namespace CryptoNote;

namespace {
  const size_t TEST_TRANSACTION_SPENDABLE_AGE = 1;
  const uint64_t TEST_OUTPUT_AMOUNT = 100;
  const uint64_t TEST_BLOCK_HEIGHT = 99;
  const uint32_t TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX = 113;

  class TransfersContainerTest : public ::testing::Test {
  public:
    enum : uint64_t {
      TEST_CONTAINER_CURRENT_HEIGHT = 1000
    };

    TransfersContainerTest() : 
      currency(CurrencyBuilder(logger).currency()), 
      container(currency, logger, TEST_TRANSACTION_SPENDABLE_AGE), 
      account(generateAccountKeys()) {   
    }

  protected:

    TransactionBlockInfo blockInfo(uint32_t height) const {
      return TransactionBlockInfo{ height, 1000000 };
    }

    std::unique_ptr<ITransactionReader> addTransaction(uint32_t height = WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT,
                                                 uint64_t outputAmount = TEST_OUTPUT_AMOUNT) {
      TestTransactionBuilder builder;

      // auto tx = createTransaction();
      builder.addTestInput(outputAmount + 1, account);

      auto outputIndex = (height == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT) ? UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX : TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX;
      auto outInfo = builder.addTestKeyOutput(outputAmount, outputIndex, account);
      std::vector<TransactionOutputInformationIn> outputs = { outInfo };

      auto tx = builder.build();

      EXPECT_TRUE(container.addTransaction(blockInfo(height), *tx, outputs));
      return tx;
    }

    std::unique_ptr<ITransactionReader> addSpendingTransaction(const Hash& sourceTx, uint32_t height, uint32_t outputIndex, uint64_t amount = TEST_OUTPUT_AMOUNT) {
      auto outputs = container.getTransactionOutputs(sourceTx, ITransfersContainer::IncludeTypeAll |
        ITransfersContainer::IncludeStateUnlocked | ITransfersContainer::IncludeStateSoftLocked);

      EXPECT_FALSE(outputs.empty());

      if (outputs.empty())
        return std::unique_ptr<ITransactionReader>();

      TestTransactionBuilder builder;

      size_t inputAmount = 0;
      for (const auto& t : outputs) {
        inputAmount += t.amount;
        builder.addInput(account, t);
      }

      EXPECT_GE(inputAmount, amount);

      std::vector<TransactionOutputInformationIn> transfers;

      builder.addTestKeyOutput(amount, outputIndex); // output to some random address

      if (inputAmount > amount) {
        transfers.emplace_back(builder.addTestKeyOutput(inputAmount - amount, outputIndex + 1, account)); // change
      }

      auto tx = builder.build();
      EXPECT_TRUE(container.addTransaction(blockInfo(height), *tx, transfers));
      return tx;
    }

    Logging::ConsoleLogger logger;
    Currency currency;
    TransfersContainer container;
    AccountKeys account;
  };

}

//--------------------------------------------------------------------------- 
// TransfersContainer_addTransaction
//--------------------------------------------------------------------------- 
class TransfersContainer_addTransaction : public TransfersContainerTest {};

TEST_F(TransfersContainer_addTransaction, orderIsRequired_sameHeight) {
  ASSERT_NO_THROW(addTransaction(TEST_BLOCK_HEIGHT));
  ASSERT_NO_THROW(addTransaction(TEST_BLOCK_HEIGHT));
}

TEST_F(TransfersContainer_addTransaction, orderIsRequired_confirmed) {
  ASSERT_NO_THROW(addTransaction(TEST_BLOCK_HEIGHT));
  ASSERT_NO_THROW(addTransaction(TEST_BLOCK_HEIGHT + 1));
  ASSERT_ANY_THROW(addTransaction(TEST_BLOCK_HEIGHT));
}

TEST_F(TransfersContainer_addTransaction, orderIsRequired_unconfirmedAtAnyHeight) {
  ASSERT_NO_THROW(addTransaction(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT));
  ASSERT_NO_THROW(addTransaction(TEST_BLOCK_HEIGHT));
  ASSERT_NO_THROW(addTransaction(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT));
  ASSERT_NO_THROW(addTransaction(TEST_BLOCK_HEIGHT + 1));
  ASSERT_NO_THROW(addTransaction(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT));
}

TEST_F(TransfersContainer_addTransaction, orderIsRequired_afterDetach) {
  ASSERT_NO_THROW(addTransaction(TEST_BLOCK_HEIGHT));
  ASSERT_NO_THROW(addTransaction(TEST_BLOCK_HEIGHT + 1));
  container.detach(TEST_BLOCK_HEIGHT + 1);
  ASSERT_NO_THROW(addTransaction(TEST_BLOCK_HEIGHT));
}

TEST_F(TransfersContainer_addTransaction, addingTransactionTwiceCausesException) {
  TestTransactionBuilder builder;
  builder.addTestInput(TEST_OUTPUT_AMOUNT + 1, account);
  auto outInfo = builder.addTestKeyOutput(TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, account);

  auto tx = builder.build();

  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx, { outInfo }));
  ASSERT_ANY_THROW(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT + 1), *tx, { outInfo }));
}

TEST_F(TransfersContainer_addTransaction, addingTwoIdenticalUnconfirmedMultisignatureOutputsDoesNotCauseException) {

  CryptoNote::TransactionBlockInfo blockInfo{ WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 1000000 };

  TestTransactionBuilder tx1;
  tx1.addTestInput(TEST_OUTPUT_AMOUNT + 1, account);
  auto outInfo1 = tx1.addTestMultisignatureOutput(TEST_OUTPUT_AMOUNT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
  std::vector<TransactionOutputInformationIn> outputs1;
  outputs1.emplace_back(outInfo1);

  ASSERT_TRUE(container.addTransaction(blockInfo, *tx1.build(), outputs1));
  
  TestTransactionBuilder tx2;
  tx2.addTestInput(TEST_OUTPUT_AMOUNT + 1, account);
  auto outInfo2 = tx2.addTestMultisignatureOutput(TEST_OUTPUT_AMOUNT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
  std::vector<TransactionOutputInformationIn> outputs2;
  outputs2.emplace_back(outInfo2);

  ASSERT_TRUE(container.addTransaction(blockInfo, *tx2.build(), outputs2));

  container.advanceHeight(1000);

  ASSERT_EQ(2, container.transfersCount());
  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(2 * TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_addTransaction, addingConfirmedMultisignatureOutputIdenticalAnotherUnspentOuputCausesException) {
  CryptoNote::TransactionBlockInfo blockInfo{ TEST_BLOCK_HEIGHT, 1000000 };

  TestTransactionBuilder tx1;
  tx1.addTestInput(TEST_OUTPUT_AMOUNT + 1, account);
  auto outInfo1 = tx1.addTestMultisignatureOutput(TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  std::vector<TransactionOutputInformationIn> outputs1;
  outputs1.emplace_back(outInfo1);

  ASSERT_TRUE(container.addTransaction(blockInfo, *tx1.build(), outputs1));

  TestTransactionBuilder tx2;
  tx2.addTestInput(TEST_OUTPUT_AMOUNT + 1, account);
  auto outInfo2 = tx2.addTestMultisignatureOutput(TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  std::vector<TransactionOutputInformationIn> outputs2;
  outputs2.emplace_back(outInfo2);

  ASSERT_ANY_THROW(container.addTransaction(blockInfo, *tx2.build(), outputs2));

  container.advanceHeight(1000);

  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_addTransaction, addingConfirmedMultisignatureOutputIdenticalAnotherSpentOuputCausesException) {
  CryptoNote::TransactionBlockInfo blockInfo1{ TEST_BLOCK_HEIGHT, 1000000 };
  TestTransactionBuilder tx1;
  tx1.addTestInput(TEST_OUTPUT_AMOUNT + 1, account);
  auto outInfo1 = tx1.addTestMultisignatureOutput(TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  ASSERT_TRUE(container.addTransaction(blockInfo1, *tx1.build(), {outInfo1}));

  // Spend output
  {
    CryptoNote::TransactionBlockInfo blockInfo2{ WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 1000000 };
    TestTransactionBuilder tx2;
    tx2.addTestMultisignatureInput(TEST_OUTPUT_AMOUNT, outInfo1);
    ASSERT_TRUE(container.addTransaction(blockInfo2, *tx2.build(), std::vector<TransactionOutputInformationIn>()));
  }

  {
    CryptoNote::TransactionBlockInfo blockInfo3{ TEST_BLOCK_HEIGHT + 3, 1000000 };
    TestTransactionBuilder tx3;
    tx3.addTestInput(TEST_OUTPUT_AMOUNT + 1, account);
    auto outInfo3 = tx3.addTestMultisignatureOutput(TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
    ASSERT_ANY_THROW(container.addTransaction(blockInfo3, *tx3.build(), { outInfo3 }));
  }

  container.advanceHeight(1000);

  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_addTransaction, addingConfirmedBlockAndUnconfirmedOutputCausesException) {
  CryptoNote::TransactionBlockInfo blockInfo{ TEST_BLOCK_HEIGHT, 1000000 };

  TestTransactionBuilder tx;
  tx.addTestInput(TEST_OUTPUT_AMOUNT + 1, account);
  auto outInfo = tx.addTestKeyOutput(TEST_OUTPUT_AMOUNT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
  std::vector<TransactionOutputInformationIn> outputs;
  outputs.emplace_back(outInfo);

  ASSERT_ANY_THROW(container.addTransaction(blockInfo, *tx.build(), outputs));
}

TEST_F(TransfersContainer_addTransaction, addingUnconfirmedBlockAndConfirmedOutputCausesException) {
  CryptoNote::TransactionBlockInfo blockInfo{ WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 1000000 };

  TestTransactionBuilder tx;
  tx.addTestInput(TEST_OUTPUT_AMOUNT + 1, account);
  auto outInfo = tx.addTestKeyOutput(TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  std::vector<TransactionOutputInformationIn> outputs;
  outputs.emplace_back(outInfo);

  ASSERT_ANY_THROW(container.addTransaction(blockInfo, *tx.build(), outputs));
}

TEST_F(TransfersContainer_addTransaction, handlesAddingUnconfirmedOutputToKey) {
  CryptoNote::TransactionBlockInfo blockInfo{ WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 1000000 };

  TestTransactionBuilder txbuilder;
  txbuilder.addTestInput(TEST_OUTPUT_AMOUNT + 1, account);
  auto outInfo = txbuilder.addTestKeyOutput(TEST_OUTPUT_AMOUNT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
  std::vector<TransactionOutputInformationIn> outputs = { outInfo };

  auto tx = txbuilder.build();

  ASSERT_TRUE(container.addTransaction(blockInfo, *tx, outputs));

  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeAllLocked);
  ASSERT_EQ(1, transfers.size());

  transfers.clear();
  container.getOutputs(transfers, ITransfersContainer::IncludeAllUnlocked);
  ASSERT_TRUE(transfers.empty());

  transfers = container.getTransactionOutputs(tx->getTransactionHash(), ITransfersContainer::IncludeAllLocked);
  ASSERT_EQ(1, transfers.size());

  transfers = container.getTransactionOutputs(tx->getTransactionHash(), ITransfersContainer::IncludeAllUnlocked);
  ASSERT_TRUE(transfers.empty());

  TransactionInformation txInfo;
  uint64_t amountIn;
  uint64_t amountOut;
  ASSERT_TRUE(container.getTransactionInformation(tx->getTransactionHash(), txInfo, &amountIn, &amountOut));
  ASSERT_EQ(blockInfo.height, txInfo.blockHeight);
  ASSERT_EQ(0, amountIn);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, amountOut);

  std::vector<Crypto::Hash> unconfirmedTransactions;
  container.getUnconfirmedTransactions(unconfirmedTransactions);
  ASSERT_EQ(1, unconfirmedTransactions.size());
}

TEST_F(TransfersContainer_addTransaction, handlesAddingConfirmedOutputToKey) {
  CryptoNote::TransactionBlockInfo blockInfo{ TEST_BLOCK_HEIGHT, 1000000 };

  TestTransactionBuilder txbuilder;
  txbuilder.addTestInput(TEST_OUTPUT_AMOUNT + 1, account);
  auto outInfo = txbuilder.addTestKeyOutput(TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  std::vector<TransactionOutputInformationIn> outputs = { outInfo };
  
  auto tx = txbuilder.build();

  ASSERT_TRUE(container.addTransaction(blockInfo, *tx, outputs));

  container.advanceHeight(1000);

  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeAllLocked);
  ASSERT_TRUE(transfers.empty());

  transfers.clear();
  container.getOutputs(transfers, ITransfersContainer::IncludeAllUnlocked);
  ASSERT_EQ(1, transfers.size());

  transfers = container.getTransactionOutputs(tx->getTransactionHash(), ITransfersContainer::IncludeAllLocked);
  ASSERT_TRUE(transfers.empty());

  transfers = container.getTransactionOutputs(tx->getTransactionHash(), ITransfersContainer::IncludeAllUnlocked);
  ASSERT_EQ(1, transfers.size());

  TransactionInformation txInfo;
  uint64_t amountIn;
  uint64_t amountOut;
  ASSERT_TRUE(container.getTransactionInformation(tx->getTransactionHash(), txInfo, &amountIn, &amountOut));
  ASSERT_EQ(blockInfo.height, txInfo.blockHeight);
  ASSERT_EQ(0, amountIn);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, amountOut);

  std::vector<Crypto::Hash> unconfirmedTransactions;
  container.getUnconfirmedTransactions(unconfirmedTransactions);
  ASSERT_TRUE(unconfirmedTransactions.empty());
}

TEST_F(TransfersContainer_addTransaction, addingEmptyTransactionOuptutsDoesNotChaingeContainer) {
  CryptoNote::TransactionBlockInfo blockInfo{ WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 1000000 };

  TestTransactionBuilder builder;
  builder.addTestInput(TEST_OUTPUT_AMOUNT + 1, account);
  builder.addTestKeyOutput(TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);

  auto tx = builder.build();

  std::vector<TransactionOutputInformationIn> outputs;

  ASSERT_FALSE(container.addTransaction(blockInfo, *tx, outputs));

  ASSERT_EQ(0, container.transfersCount());
  ASSERT_EQ(0, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeAllLocked);
  ASSERT_TRUE(transfers.empty());

  transfers.clear();
  container.getOutputs(transfers, ITransfersContainer::IncludeAllUnlocked);
  ASSERT_TRUE(transfers.empty());

  transfers = container.getTransactionOutputs(tx->getTransactionHash(), ITransfersContainer::IncludeAll);
  ASSERT_TRUE(transfers.empty());

  TransactionInformation txInfo;
  ASSERT_FALSE(container.getTransactionInformation(tx->getTransactionHash(), txInfo));

  std::vector<Crypto::Hash> unconfirmedTransactions;
  container.getUnconfirmedTransactions(unconfirmedTransactions);
  ASSERT_TRUE(unconfirmedTransactions.empty());
}


TEST_F(TransfersContainer_addTransaction, handlesAddingUnconfirmedOutputMultisignature) {
  TestTransactionBuilder tx;
  auto out = tx.addTestMultisignatureOutput(TEST_OUTPUT_AMOUNT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);

  ASSERT_TRUE(container.addTransaction(blockInfo(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT), *tx.build(), { out }));

  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(1, container.transfersCount());

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeTypeMultisignature | ITransfersContainer::IncludeStateLocked));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeTypeMultisignature | ITransfersContainer::IncludeStateUnlocked));
}

TEST_F(TransfersContainer_addTransaction, handlesAddingConfirmedOutputMultisignature) {
  TestTransactionBuilder tx;
  auto out = tx.addTestMultisignatureOutput(TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);

  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx.build(), { out }));

  container.advanceHeight(1000);

  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(1, container.transfersCount());

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeTypeMultisignature | ITransfersContainer::IncludeStateUnlocked));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeTypeMultisignature | ITransfersContainer::IncludeStateLocked));
}

TEST_F(TransfersContainer_addTransaction, addingConfirmedOutputMultisignatureTwiceFails) {

  {
    TestTransactionBuilder tx;
    auto out = tx.addTestMultisignatureOutput(TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
    ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx.build(), { out }));
  }

  {
    TestTransactionBuilder tx;
    auto out = tx.addTestMultisignatureOutput(TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
    ASSERT_ANY_THROW(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT + 1), *tx.build(), { out }));
  }
}


TEST_F(TransfersContainer_addTransaction, ignoresUnrelatedTransactionsWithKeyInput) {
  TestTransactionBuilder tx;
  tx.addTestInput(TEST_OUTPUT_AMOUNT, account);
  ASSERT_FALSE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx.build(), {}));
}

TEST_F(TransfersContainer_addTransaction, ignoresUnrelatedTransactionsWithMultisignatureInput) {
  TestTransactionBuilder tx;
  tx.addFakeMultisignatureInput(TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, 1);
  ASSERT_FALSE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx.build(), {}));
}

TEST_F(TransfersContainer_addTransaction, spendingUnconfirmedOutputFails) {
  auto tx = addTransaction(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT);

  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(1, container.transfersCount());

  auto outputs = container.getTransactionOutputs(
    tx->getTransactionHash(), ITransfersContainer::IncludeAll);

  ASSERT_EQ(1, outputs.size());

  TestTransactionBuilder spendingTx;
  for (const auto& t : outputs) {
    spendingTx.addInput(account, t);
  }

  ASSERT_ANY_THROW(container.addTransaction(blockInfo(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT), *spendingTx.build() , {}));
}

TEST_F(TransfersContainer_addTransaction, spendingConfirmedOutputWithUnconfirmedTxSucceed) {
  auto tx = addTransaction(TEST_BLOCK_HEIGHT);
  container.advanceHeight(1000);
  auto spendingTx = addSpendingTransaction(tx->getTransactionHash(), 
    WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(1, container.transfersCount()); // no new outputs
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_addTransaction, spendingConfirmedOutputWithConfirmedTxSucceed) {
  auto tx = addTransaction(TEST_BLOCK_HEIGHT);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  auto spendingTx = addSpendingTransaction(tx->getTransactionHash(),
    TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX + 1);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE*2);
  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(1, container.transfersCount()); // no new outputs
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_addTransaction, spendingConfirmedMultisignatureOutputWithUnconfirmedTxSucceed) {
  TestTransactionBuilder tx;
  tx.addTestInput(TEST_OUTPUT_AMOUNT + 1, account);
  auto out = tx.addTestMultisignatureOutput(TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx.build(), { out }));
  
  container.advanceHeight(1000);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
  
  TestTransactionBuilder spendingTx;
  spendingTx.addTestMultisignatureInput(TEST_OUTPUT_AMOUNT, out);
  ASSERT_TRUE(container.addTransaction(blockInfo(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT), *spendingTx.build(), {}));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_addTransaction, spendingConfirmedMultisignatureOutputWithConfirmedTxSucceed) {
  TestTransactionBuilder tx;
  tx.addTestInput(TEST_OUTPUT_AMOUNT + 1, account);
  auto out = tx.addTestMultisignatureOutput(TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx.build(), { out }));

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  TestTransactionBuilder spendingTx;
  spendingTx.addTestMultisignatureInput(TEST_OUTPUT_AMOUNT, out);
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE), *spendingTx.build(), {}));

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE*2);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

//--------------------------------------------------------------------------- 
// TransfersContainer_deleteUnconfirmedTransaction
//--------------------------------------------------------------------------- 
class TransfersContainer_deleteUnconfirmedTransaction : public TransfersContainerTest{};

TEST_F(TransfersContainer_deleteUnconfirmedTransaction, tryDeleteNonExistingTx) {
  addTransaction();
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_FALSE(container.deleteUnconfirmedTransaction(Crypto::rand<Crypto::Hash>()));
  ASSERT_EQ(1, container.transactionsCount());
}

TEST_F(TransfersContainer_deleteUnconfirmedTransaction, tryDeleteConfirmedTx) {
  auto txHash = addTransaction(TEST_BLOCK_HEIGHT)->getTransactionHash();
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_FALSE(container.deleteUnconfirmedTransaction(txHash));
  ASSERT_EQ(1, container.transactionsCount());
}

TEST_F(TransfersContainer_deleteUnconfirmedTransaction, deleteUnconfirmedSpendingTx) {
  addTransaction(TEST_BLOCK_HEIGHT);

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeAllUnlocked);

  ASSERT_EQ(1, transfers.size());

  TestTransactionBuilder spendingTx;
  spendingTx.addInput(account, transfers[0]);
  spendingTx.addTestKeyOutput(TEST_OUTPUT_AMOUNT - 1, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
  auto tx = spendingTx.build();

  {
    CryptoNote::TransactionBlockInfo blockInfo{ WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 1000000 };
    ASSERT_TRUE(container.addTransaction(blockInfo, *tx, {}));
  }

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
  ASSERT_TRUE(container.deleteUnconfirmedTransaction(tx->getTransactionHash()));

  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_deleteUnconfirmedTransaction, deleteTx) {
  auto txHash = addTransaction()->getTransactionHash();
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_TRUE(container.deleteUnconfirmedTransaction(txHash));
  ASSERT_EQ(0, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllLocked));
}


//--------------------------------------------------------------------------- 
// TransfersContainer_markTransactionConfirmed
//--------------------------------------------------------------------------- 
class TransfersContainer_markTransactionConfirmed : public TransfersContainerTest {
public:
  bool markConfirmed(const Hash& txHash, uint32_t height = TEST_BLOCK_HEIGHT, 
    const std::vector<uint32_t>& globalIndices = { TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX }) {
    return container.markTransactionConfirmed(blockInfo(height), txHash, globalIndices);
  }
};

TEST_F(TransfersContainer_markTransactionConfirmed, unconfirmedBlockHeight) {
  ASSERT_ANY_THROW(markConfirmed(addTransaction()->getTransactionHash(), WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT));
}

TEST_F(TransfersContainer_markTransactionConfirmed, nonExistingTransaction) {
  addTransaction();
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_FALSE(markConfirmed(Crypto::rand<Hash>()));
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
}

TEST_F(TransfersContainer_markTransactionConfirmed, confirmedTransaction) {
  auto txHash = addTransaction(TEST_BLOCK_HEIGHT)->getTransactionHash();
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
  ASSERT_FALSE(markConfirmed(txHash));
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
}


TEST_F(TransfersContainer_markTransactionConfirmed, globalIndicesSmaller) {
  
  TestTransactionBuilder builder;
  builder.addTestInput(TEST_OUTPUT_AMOUNT + 1, account);

  auto outputs = {
    builder.addTestKeyOutput(TEST_OUTPUT_AMOUNT / 2, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX, account),
    builder.addTestKeyOutput(TEST_OUTPUT_AMOUNT / 2, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX, account)
  };

  auto tx = builder.build();

  ASSERT_TRUE(container.addTransaction(blockInfo(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT), *tx, outputs));
  ASSERT_EQ(2, container.transfersCount());
  ASSERT_ANY_THROW(markConfirmed(tx->getTransactionHash(), TEST_BLOCK_HEIGHT));
}

TEST_F(TransfersContainer_markTransactionConfirmed, confirmationWorks) {
  auto txHash = addTransaction()->getTransactionHash();
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_TRUE(markConfirmed(txHash));
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_markTransactionConfirmed, confirmationTxWithNoOutputs) {
  addTransaction(TEST_BLOCK_HEIGHT);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeAllUnlocked);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  TestTransactionBuilder builder;
  builder.addInput(account, transfers[0]);
  auto tx = builder.build();

  ASSERT_TRUE(container.addTransaction(blockInfo(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT), *tx, {}));

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
  ASSERT_TRUE(markConfirmed(tx->getTransactionHash()));
  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainer_markTransactionConfirmed, confirmingMultisignatureOutputIdenticalAnotherUnspentOuputCausesException) {
  // Add tx1
  CryptoNote::TransactionBlockInfo blockInfo1{ TEST_BLOCK_HEIGHT, 1000000 };
  TestTransactionBuilder tx1;
  tx1.addTestInput(TEST_OUTPUT_AMOUNT + 1, account);
  auto outInfo1 = tx1.addTestMultisignatureOutput(TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  ASSERT_TRUE(container.addTransaction(blockInfo1, *tx1.build(), {outInfo1}));

  // Spend output, add tx2
  CryptoNote::TransactionBlockInfo blockInfo2{ WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 1000000 };
  TestTransactionBuilder tx2;
  tx2.addTestMultisignatureInput(TEST_OUTPUT_AMOUNT, outInfo1);
  ASSERT_TRUE(container.addTransaction(blockInfo2, *tx2.build(), {}));

  // Add tx3
  CryptoNote::TransactionBlockInfo blockInfo3{ WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 1000000 };
  TestTransactionBuilder tx3;
  tx3.addTestInput(TEST_OUTPUT_AMOUNT + 1, account);
  auto outInfo3 = tx3.addTestMultisignatureOutput(TEST_OUTPUT_AMOUNT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
  ASSERT_TRUE(container.addTransaction(blockInfo3, *tx3.build(), {outInfo3}));

  // Confirm tx3
  blockInfo3.height = TEST_BLOCK_HEIGHT + 2;
  std::vector<uint32_t> globalIndices3;
  globalIndices3.emplace_back(TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  ASSERT_ANY_THROW(container.markTransactionConfirmed(blockInfo3, tx3.getTransactionHash(), globalIndices3));

  container.advanceHeight(1000);

  ASSERT_EQ(2, container.transfersCount());
  ASSERT_EQ(3, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainer_markTransactionConfirmed, confirmingMultisignatureOutputIdenticalAnotherSpentOuputCausesException) {
  CryptoNote::TransactionBlockInfo blockInfo1{ TEST_BLOCK_HEIGHT, 1000000 };
  TestTransactionBuilder tx1;
  tx1.addTestInput(TEST_OUTPUT_AMOUNT + 1);
  auto outInfo1 = tx1.addTestMultisignatureOutput(TEST_OUTPUT_AMOUNT, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  ASSERT_TRUE(container.addTransaction(blockInfo1, *tx1.build(), {outInfo1}));

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  CryptoNote::TransactionBlockInfo blockInfo2{ WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 1000000 };
  TestTransactionBuilder tx2;
  tx2.addTestInput(TEST_OUTPUT_AMOUNT + 1);
  auto outInfo2 = tx2.addTestMultisignatureOutput(TEST_OUTPUT_AMOUNT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
  ASSERT_TRUE(container.addTransaction(blockInfo2, *tx2.build(), { outInfo2 }));

  blockInfo2.height = TEST_BLOCK_HEIGHT + 2;
  std::vector<uint32_t> globalIndices2;
  globalIndices2.emplace_back(TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  ASSERT_ANY_THROW(container.markTransactionConfirmed(blockInfo2, tx2.getTransactionHash(), globalIndices2));

  ASSERT_EQ(2, container.transfersCount());
  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
}


//--------------------------------------------------------------------------- 
// TransfersContainer_detach
//--------------------------------------------------------------------------- 
class TransfersContainer_detach : public TransfersContainerTest {
public:
  
};

TEST_F(TransfersContainer_detach, detachConfirmed) {
  addTransaction(TEST_BLOCK_HEIGHT);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(1, container.transactionsCount());
  container.detach(TEST_BLOCK_HEIGHT);
  ASSERT_EQ(0, container.transfersCount());
  ASSERT_EQ(0, container.transactionsCount());
}

TEST_F(TransfersContainer_detach, detachConfirmedSpendingTransaction) {
  auto tx = addTransaction(TEST_BLOCK_HEIGHT);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  auto spendingTx = addSpendingTransaction(
    tx->getTransactionHash(), TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE*2);

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  container.detach(TEST_BLOCK_HEIGHT+1);
  
  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainer_detach, threeRelatedTransactions) {
  auto tx = addTransaction(TEST_BLOCK_HEIGHT);

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  auto spendingTx1 = addSpendingTransaction(
    tx->getTransactionHash(), TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT / 2);

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE*2);

  auto spendingTx2 = addSpendingTransaction(
    spendingTx1->getTransactionHash(), TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE*2, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX + 2, TEST_OUTPUT_AMOUNT / 2);

  ASSERT_EQ(3, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  container.detach(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE*2);

  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainer_detach, detachConfirmedTransactionWithUnrelatedUnconfirmed) {
  auto tx1 = addTransaction(TEST_BLOCK_HEIGHT);
  auto tx2 = addTransaction(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT);

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT * 2, container.balance(ITransfersContainer::IncludeAll));

  container.detach(TEST_BLOCK_HEIGHT);

  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainer_detach, confirmedWithUnconfirmedSpendingTransaction_H1) {

  auto tx = addTransaction(TEST_BLOCK_HEIGHT);

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  auto spendingTx = addSpendingTransaction(
    tx->getTransactionHash(), WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  container.detach(TEST_BLOCK_HEIGHT + 1);

  ASSERT_EQ(1, container.transfersCount());
  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainer_detach, confirmedWithUnconfirmedSpendingTransaction_H0) {
  auto tx = addTransaction(TEST_BLOCK_HEIGHT);
  
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  auto spendingTx = addSpendingTransaction(
    tx->getTransactionHash(), WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  container.detach(TEST_BLOCK_HEIGHT);

  ASSERT_EQ(0, container.transfersCount());
  ASSERT_EQ(0, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainer_detach, confirmedTwoOfThree) {
  auto txHash = addTransaction(TEST_BLOCK_HEIGHT - 1)->getTransactionHash();
  addTransaction(TEST_BLOCK_HEIGHT);
  addTransaction(TEST_BLOCK_HEIGHT + 1);

  ASSERT_EQ(3, container.transactionsCount());

  container.detach(TEST_BLOCK_HEIGHT);

  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(1, container.getTransactionOutputs(txHash, ITransfersContainer::IncludeAll).size());
}

TEST_F(TransfersContainer_detach, transactionDetachAfterAdvance) {
  container.detach(TEST_BLOCK_HEIGHT);
  addTransaction(TEST_BLOCK_HEIGHT);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
  container.detach(TEST_BLOCK_HEIGHT);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
}


//--------------------------------------------------------------------------- 
// TransfersContainer_advanceHeight
//--------------------------------------------------------------------------- 
class TransfersContainer_advanceHeight : public TransfersContainerTest {
public:
  TransfersContainer_advanceHeight(){}
};


TEST_F(TransfersContainer_advanceHeight, advanceFailed) {
  ASSERT_TRUE(container.advanceHeight(1000));
  ASSERT_FALSE(container.advanceHeight(999)); // 1000 -> 999
}

TEST_F(TransfersContainer_advanceHeight, advanceSucceeded) {
  ASSERT_TRUE(container.advanceHeight(1000)); // 1000 -> 1000
  ASSERT_TRUE(container.advanceHeight(1001)); // 1000 -> 1001
}

TEST_F(TransfersContainer_advanceHeight, advanceUnlocksTransaction) {
  container.detach(TEST_BLOCK_HEIGHT);
  addTransaction(TEST_BLOCK_HEIGHT);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
  addTransaction(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
}


//--------------------------------------------------------------------------- 
// TransfersContainer_balance
//--------------------------------------------------------------------------- 

class TransfersContainer_balance : public TransfersContainerTest {
public:
  TransfersContainer_balance() {
  }

  enum TestAmounts : uint64_t {
    AMOUNT_1 = 13,
    AMOUNT_2 = 17
  };
};


TEST_F(TransfersContainer_balance, treatsUnconfirmedTransfersAsLocked) {
  auto tx1 = addTransaction(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, AMOUNT_1);
  auto tx2 = addTransaction(TEST_BLOCK_HEIGHT, AMOUNT_2);

  ASSERT_EQ(AMOUNT_1, container.balance(ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeAll));
}

TEST_F(TransfersContainer_balance, handlesLockedByTimeTransferAsLocked) {
  TestTransactionBuilder tx1;
  tx1.setUnlockTime(time(nullptr) + 60 * 60 * 24);
  tx1.addTestInput(AMOUNT_1 + 1, account);
  auto outInfo = tx1.addTestKeyOutput(AMOUNT_1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, account);
  std::vector<TransactionOutputInformationIn> outputs = { outInfo };
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx1.build(), outputs));

  auto tx2 = addTransaction(TEST_BLOCK_HEIGHT, AMOUNT_2);

  ASSERT_EQ(AMOUNT_1, container.balance(ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeAll));
}

TEST_F(TransfersContainer_balance, handlesLockedByHeightTransferAsLocked) {
  TestTransactionBuilder tx1;
  tx1.setUnlockTime(TEST_CONTAINER_CURRENT_HEIGHT + 1);
  tx1.addTestInput(AMOUNT_1 + 1);
  auto outInfo = tx1.addTestKeyOutput(AMOUNT_1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, account);
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx1.build(), { outInfo }));

  auto tx2 = addTransaction(TEST_BLOCK_HEIGHT, AMOUNT_2);

  ASSERT_EQ(AMOUNT_1, container.balance(ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeAll));
}

TEST_F(TransfersContainer_balance, handlesTransferStateSoftLocked) {
  auto tx1 = addTransaction(TEST_CONTAINER_CURRENT_HEIGHT - TEST_TRANSACTION_SPENDABLE_AGE, AMOUNT_2);
  auto tx2 = addTransaction(TEST_CONTAINER_CURRENT_HEIGHT, AMOUNT_1);

  ASSERT_EQ(AMOUNT_1, container.balance(ITransfersContainer::IncludeStateSoftLocked | ITransfersContainer::IncludeTypeAll));
}

TEST_F(TransfersContainer_balance, handlesTransferStateUnLocked) {
  auto tx1 = addTransaction(TEST_CONTAINER_CURRENT_HEIGHT - TEST_TRANSACTION_SPENDABLE_AGE, AMOUNT_2);
  auto tx2 = addTransaction(TEST_CONTAINER_CURRENT_HEIGHT, AMOUNT_1);

  ASSERT_EQ(AMOUNT_2, container.balance(ITransfersContainer::IncludeStateUnlocked | ITransfersContainer::IncludeTypeAll));
}

TEST_F(TransfersContainer_balance, handlesTransferTypeKey) {
  TestTransactionBuilder tx;
  tx.addTestInput(AMOUNT_1 + AMOUNT_2 + 1);
  auto outInfo1 = tx.addTestKeyOutput(AMOUNT_1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, account);
  auto outInfo2 = tx.addTestMultisignatureOutput(AMOUNT_2, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx.build(), { outInfo1, outInfo2 }));
  ASSERT_EQ(AMOUNT_1, container.balance(ITransfersContainer::IncludeStateAll | ITransfersContainer::IncludeTypeKey));
}

TEST_F(TransfersContainer_balance, handlesTransferTypeMultisignature) {
  TestTransactionBuilder tx;
  tx.addTestInput(AMOUNT_1 + AMOUNT_2 + 1);
  auto outInfo1 = tx.addTestKeyOutput(AMOUNT_1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, account);
  auto outInfo2 = tx.addTestMultisignatureOutput(AMOUNT_2, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx.build(), { outInfo1, outInfo2 }));

  ASSERT_EQ(AMOUNT_2, container.balance(ITransfersContainer::IncludeStateAll | ITransfersContainer::IncludeTypeMultisignature));
}

TEST_F(TransfersContainer_balance, filtersByStateAndKeySimultaneously) {
  TestTransactionBuilder tx;
  tx.addTestInput(AMOUNT_1 + AMOUNT_2 + 1);
  auto outInfo1 = tx.addTestKeyOutput(AMOUNT_1, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX, account);
  auto outInfo2 = tx.addTestMultisignatureOutput(AMOUNT_2, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
  ASSERT_TRUE(container.addTransaction(blockInfo(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT), *tx.build(), { outInfo1, outInfo2 }));

  auto tx2 = addTransaction(TEST_BLOCK_HEIGHT, AMOUNT_1 + AMOUNT_2);

  container.advanceHeight(TEST_CONTAINER_CURRENT_HEIGHT);

  ASSERT_EQ(AMOUNT_1, container.balance(ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeKey));
  ASSERT_EQ(AMOUNT_2, container.balance(ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeMultisignature));
  ASSERT_EQ(AMOUNT_1 + AMOUNT_2, container.balance(ITransfersContainer::IncludeStateUnlocked | ITransfersContainer::IncludeTypeKey));
}


//--------------------------------------------------------------------------- 
// TransfersContainer_getOutputs
//--------------------------------------------------------------------------- 

class TransfersContainer_getOutputs : public TransfersContainerTest {
public:
  TransfersContainer_getOutputs() {
  }

  enum TestAmounts : uint64_t {
    AMOUNT_1 = 13,
    AMOUNT_2 = 17
  };
};


TEST_F(TransfersContainer_getOutputs, treatsUnconfirmedTransfersAsLocked) {
  auto tx1 = addTransaction(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, AMOUNT_1);
  auto tx2 = addTransaction(TEST_BLOCK_HEIGHT, AMOUNT_2);

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeAll);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_1, transfers.front().amount);
}

TEST_F(TransfersContainer_getOutputs, handlesLockedByTimeTransferAsLocked) {
  TestTransactionBuilder tx1;
  tx1.setUnlockTime(time(nullptr) + 60 * 60 * 24);
  tx1.addTestInput(AMOUNT_1 + 1);
  auto outInfo = tx1.addTestKeyOutput(AMOUNT_1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, account);
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx1.build(), { outInfo }));

  auto tx2 = addTransaction(TEST_BLOCK_HEIGHT, AMOUNT_2);

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeAll);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_1, transfers.front().amount);
}

TEST_F(TransfersContainer_getOutputs, handlesLockedByHeightTransferAsLocked) {
  TestTransactionBuilder tx1;
  tx1.setUnlockTime(TEST_CONTAINER_CURRENT_HEIGHT + 1);
  tx1.addTestInput(AMOUNT_1 + 1);
  auto outInfo = tx1.addTestKeyOutput(AMOUNT_1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, account);
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx1.build(), { outInfo }));

  auto tx2 = addTransaction(TEST_BLOCK_HEIGHT, AMOUNT_2);

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeAll);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_1, transfers.front().amount);
}

TEST_F(TransfersContainer_getOutputs, handlesTransferStateSoftLocked) {
  addTransaction(TEST_CONTAINER_CURRENT_HEIGHT - TEST_TRANSACTION_SPENDABLE_AGE, AMOUNT_2);
  addTransaction(TEST_CONTAINER_CURRENT_HEIGHT, AMOUNT_1);
  
  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeStateSoftLocked | ITransfersContainer::IncludeTypeAll);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_1, transfers.front().amount);
}

TEST_F(TransfersContainer_getOutputs, handlesTransferStateUnLocked) {
  addTransaction(TEST_CONTAINER_CURRENT_HEIGHT - TEST_TRANSACTION_SPENDABLE_AGE, AMOUNT_2);
  addTransaction(TEST_CONTAINER_CURRENT_HEIGHT, AMOUNT_1);

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeStateUnlocked | ITransfersContainer::IncludeTypeAll);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_2, transfers.front().amount);
}

TEST_F(TransfersContainer_getOutputs, handlesTransferTypeKey) {
  TestTransactionBuilder tx;
  tx.addTestInput(AMOUNT_1 + AMOUNT_2 + 1);
  auto outInfo1 = tx.addTestKeyOutput(AMOUNT_1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, account);
  auto outInfo2 = tx.addTestMultisignatureOutput(AMOUNT_2, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx.build(), { outInfo1, outInfo2 }));

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeStateAll | ITransfersContainer::IncludeTypeKey);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_1, transfers.front().amount);
}

TEST_F(TransfersContainer_getOutputs, handlesTransferTypeMultisignature) {
  TestTransactionBuilder tx;
  tx.addTestInput(AMOUNT_1 + AMOUNT_2 + 1);
  auto outInfo1 = tx.addTestKeyOutput(AMOUNT_1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, account);
  auto outInfo2 = tx.addTestMultisignatureOutput(AMOUNT_2, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  ASSERT_TRUE(container.addTransaction(blockInfo(TEST_BLOCK_HEIGHT), *tx.build(), { outInfo1, outInfo2 }));

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeStateAll | ITransfersContainer::IncludeTypeMultisignature);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_2, transfers.front().amount);
}

TEST_F(TransfersContainer_getOutputs, filtersByStateAndKeySimultaneously) {
  TestTransactionBuilder tx;
  tx.addTestInput(AMOUNT_1 + AMOUNT_2 + 1);
  auto outInfo1 = tx.addTestKeyOutput(AMOUNT_1, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX, account);
  auto outInfo2 = tx.addTestMultisignatureOutput(AMOUNT_2, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
  ASSERT_TRUE(container.addTransaction(blockInfo(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT), *tx.build(), { outInfo1, outInfo2 }));

  auto tx2 = addTransaction(TEST_BLOCK_HEIGHT, AMOUNT_1 + AMOUNT_2);

  container.advanceHeight(TEST_CONTAINER_CURRENT_HEIGHT);

  std::vector<TransactionOutputInformation> transfers;
  container.getOutputs(transfers, ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeKey);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_1, transfers.front().amount);

  transfers.clear();
  container.getOutputs(transfers, ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeMultisignature);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_2, transfers.front().amount);

  transfers.clear();
  container.getOutputs(transfers, ITransfersContainer::IncludeStateUnlocked | ITransfersContainer::IncludeTypeKey);
  ASSERT_EQ(1, transfers.size());
  ASSERT_EQ(AMOUNT_1 + AMOUNT_2, transfers.front().amount);
}

