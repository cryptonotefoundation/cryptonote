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
  const uint32_t UNCONFIRMED = std::numeric_limits<uint32_t>::max();
  const uint64_t TEST_TIMESTAMP = 1000000;

  class TransfersContainerKeyImage : public ::testing::Test {
  public:

    TransfersContainerKeyImage() :
      currency(CurrencyBuilder(logger).currency()), 
      container(currency, logger, TEST_TRANSACTION_SPENDABLE_AGE), 
      account(generateAccountKeys()),
      txTemplate(createTransaction()) {
      txTemplate->getTransactionSecretKey(txSecretKey);
    }

  protected:

    std::vector<TransactionOutputInformation> getOutputs(uint32_t flags) {
      std::vector<TransactionOutputInformation> outs;
      container.getOutputs(outs, flags);
      return outs;
    }

    size_t outputsCount(uint32_t flags) {
      return getOutputs(flags).size();
    }

    TransactionBlockInfo blockInfo(uint32_t height) const {
      return TransactionBlockInfo{ height, 1000000 };
    }

    TestTransactionBuilder createTransactionWithFixedKey() {
      return TestTransactionBuilder(txTemplate->getTransactionData(), txSecretKey);
    }

    std::unique_ptr<ITransactionReader> addTransactionWithFixedKey(uint32_t height, size_t inputs = 1, uint64_t amount = TEST_OUTPUT_AMOUNT, uint32_t txIndex = 0) {
      auto tx = createTransactionWithFixedKey();
      
      while (inputs--) {
        tx.addTestInput(amount + 1);
      }

      auto outputIndex = (height == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT) ? 
        UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX : TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX;
      auto outInfo = tx.addTestKeyOutput(amount, outputIndex, account);

      auto finalTx = tx.build();
      EXPECT_TRUE(container.addTransaction(TransactionBlockInfo{ height, 1000000, txIndex }, *finalTx, { outInfo }));
      return finalTx;
    }

    std::unique_ptr<ITransactionReader> addTransaction(uint32_t height = WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT) {
      TestTransactionBuilder tx;
      tx.addTestInput(TEST_OUTPUT_AMOUNT + 1);
      auto outputIndex = (height == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT) ? UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX : TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX;
      auto outInfo = tx.addTestKeyOutput(TEST_OUTPUT_AMOUNT, outputIndex, account);
      auto finalTx = tx.build();
      EXPECT_TRUE(container.addTransaction(blockInfo(height), *finalTx, { outInfo }));
      return finalTx;
    }

    std::unique_ptr<ITransactionReader> addSpendingTransaction(const Hash& sourceTx, uint32_t height, uint32_t outputIndex, uint64_t amount = TEST_OUTPUT_AMOUNT, bool fixedKey = false) {
      auto outputs = container.getTransactionOutputs(sourceTx, ITransfersContainer::IncludeTypeAll |
        ITransfersContainer::IncludeStateUnlocked | ITransfersContainer::IncludeStateSoftLocked);

      EXPECT_FALSE(outputs.empty());

      if (outputs.empty())
        return std::unique_ptr<ITransactionReader>();

      TestTransactionBuilder normalTx;
      TestTransactionBuilder fixedTx(createTransactionWithFixedKey());

      auto& tx = fixedKey ? fixedTx : normalTx;

      size_t inputAmount = 0;
      for (const auto& t : outputs) {
        inputAmount += t.amount;
        tx.addInput(account, t);
      }

      EXPECT_GE(inputAmount, amount);

      std::vector<TransactionOutputInformationIn> transfers;

      tx.addTestKeyOutput(amount, outputIndex); // output to some random address

      if (inputAmount > amount) {
        transfers.emplace_back(tx.addTestKeyOutput(inputAmount - amount, outputIndex + 1, account)); // change
      }

      auto finalTx = tx.build();
      EXPECT_TRUE(container.addTransaction(blockInfo(height), *finalTx, transfers));
      return finalTx;
    }

    Logging::ConsoleLogger logger;
    Currency currency;
    TransfersContainer container;
    AccountKeys account;
    std::unique_ptr<ITransaction> txTemplate;
    SecretKey txSecretKey;
  };

}

/////////////////////////////////////////////////////////////////////////////
// addTransaction
/////////////////////////////////////////////////////////////////////////////

TEST_F(TransfersContainerKeyImage, addTransaction_addingSecondUnconfirmedTransferHidesBothUnconfirmedTransfers) {
  // add first transaction
  auto tx1b = createTransactionWithFixedKey();
  auto tx2b = createTransactionWithFixedKey();

  ASSERT_EQ(tx1b.getTransactionPublicKey(), tx2b.getTransactionPublicKey());

  tx1b.addTestInput(TEST_OUTPUT_AMOUNT);
  auto tx1out = tx1b.addTestKeyOutput(TEST_OUTPUT_AMOUNT, UNCONFIRMED, account);
  auto tx1 = tx1b.build();

  ASSERT_TRUE(container.addTransaction({ WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 100000 }, *tx1, { tx1out }));
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(1, outputsCount(ITransfersContainer::IncludeAllLocked));
  
  // fill tx2
  tx2b.addTestInput(TEST_OUTPUT_AMOUNT);
  tx2b.addTestInput(TEST_OUTPUT_AMOUNT);
  auto tx2out = tx2b.addTestKeyOutput(TEST_OUTPUT_AMOUNT, UNCONFIRMED, account);
  auto tx2 = tx2b.build();

  ASSERT_EQ(tx1out.keyImage, tx2out.keyImage);
  ASSERT_NE(tx1->getTransactionPrefixHash(), tx2->getTransactionPrefixHash());

  ASSERT_TRUE(container.addTransaction({ WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 100000 }, *tx2, { tx2out }));

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllLocked)); // transactions' outputs should shadow one another
  ASSERT_EQ(0, outputsCount(ITransfersContainer::IncludeAllLocked));
}

TEST_F(TransfersContainerKeyImage, addTransaction_unconfirmedTransferAddedAfterConfirmedBecomeHidden) {
  // fill tx1
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
  ASSERT_EQ(1, outputsCount(ITransfersContainer::IncludeAllUnlocked));

  auto tx2 = createTransactionWithFixedKey();
  ASSERT_EQ(tx1->getTransactionPublicKey(), tx2.getTransactionPublicKey());

  // fill tx2
  tx2.addTestInput(TEST_OUTPUT_AMOUNT);
  tx2.addTestInput(TEST_OUTPUT_AMOUNT);
  auto tx2out = tx2.addTestKeyOutput(TEST_OUTPUT_AMOUNT, UNCONFIRMED, account);

  ASSERT_TRUE(container.addTransaction({ WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 100000 }, *tx2.build(), { tx2out }));

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(1, outputsCount(ITransfersContainer::IncludeAllUnlocked));
  ASSERT_EQ(0, outputsCount(ITransfersContainer::IncludeAllLocked));
}


TEST_F(TransfersContainerKeyImage, addTransaction_unconfirmedTransferAddedAfterSpentBecomeHidden) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  addSpendingTransaction(tx1->getTransactionHash(), TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE, 
    TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllUnlocked));
  ASSERT_EQ(0, outputsCount(ITransfersContainer::IncludeAll));

  addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 2);

  ASSERT_EQ(3, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
  ASSERT_EQ(0, outputsCount(ITransfersContainer::IncludeAll));
}


TEST_F(TransfersContainerKeyImage, addTransaction_confirmedTransferAddedAfterUnconfirmedHidesUnconfirmed) {
  auto tx1 = addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));

  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT * 2, container.balance(ITransfersContainer::IncludeAllUnlocked)); // confirmed added
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllLocked)); // unconfirmed shadowed
}

TEST_F(TransfersContainerKeyImage, addTransaction_secondConfirmedTransferAddedAsHidden_BothTransfersInTheSameBlock) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 1, TEST_OUTPUT_AMOUNT, 1);
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllLocked));

  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2, 2);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainerKeyImage, addTransaction_secondConfirmedTransferAddedAsHidden_TransfersInDifferentBlocks) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  ASSERT_EQ(1, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeStateSoftLocked | ITransfersContainer::IncludeTypeAll));

  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 2, TEST_OUTPUT_AMOUNT * 2);
  container.advanceHeight(TEST_BLOCK_HEIGHT + 1 + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainerKeyImage, addTransaction_confirmedTransferAddedAfterSpentBecomeHidden) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  addSpendingTransaction(tx1->getTransactionHash(), TEST_BLOCK_HEIGHT + 1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll)); // all spent

  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE + 1, 2, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(3, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll)); // all spent
}

/////////////////////////////////////////////////////////////////////////////
// markTransactionConfirmed
/////////////////////////////////////////////////////////////////////////////

TEST_F(TransfersContainerKeyImage, markTransactionConfirmed_confirmingOneOfAFewUnconfirmedTransfersMakesThisTransferVisible) {
  addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 1, TEST_OUTPUT_AMOUNT);
  auto tx2 = addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2);
  addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 3, TEST_OUTPUT_AMOUNT * 3);

  ASSERT_EQ(3, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_TRUE(container.markTransactionConfirmed(
    { TEST_BLOCK_HEIGHT, 100000 }, tx2->getTransactionHash(), { TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX }));

  ASSERT_EQ(TEST_OUTPUT_AMOUNT * 2, container.balance(ITransfersContainer::IncludeStateSoftLocked | ITransfersContainer::IncludeTypeAll));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeAll));
}

TEST_F(TransfersContainerKeyImage, markTransactionConfirmed_oneConfirmedOtherUnconfirmed_confirmingOneUnconfirmed) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 1, TEST_OUTPUT_AMOUNT, 1);
  auto tx2 = addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2);
  addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 3, TEST_OUTPUT_AMOUNT * 3);

  ASSERT_EQ(3, container.transactionsCount());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeStateSoftLocked | ITransfersContainer::IncludeTypeAll));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeAll));

  // same block, but smaller transactionIndex
  ASSERT_TRUE(container.markTransactionConfirmed(
    { TEST_BLOCK_HEIGHT, TEST_TIMESTAMP, 2 }, tx2->getTransactionHash(), { TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX }));

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage, markTransactionConfirmed_oneSpentOtherUnconfirmed_confirmingOneUnconfirmed) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 1, TEST_OUTPUT_AMOUNT);
  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
  addSpendingTransaction(tx1->getTransactionHash(), TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE, 
    TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  auto tx2 = addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2);
  auto tx3 = addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 3, TEST_OUTPUT_AMOUNT * 3);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_TRUE(container.markTransactionConfirmed(
  { TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE + 1, TEST_TIMESTAMP, 0 }, tx2->getTransactionHash(), {TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX}));

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
}

/////////////////////////////////////////////////////////////////////////////
// addTransaction - spending fails
/////////////////////////////////////////////////////////////////////////////

class TransfersContainerKeyImage_Spend : public TransfersContainerKeyImage {
public:

  bool spendOutput(const TransactionOutputInformation& outInfo) {
    auto amount = outInfo.amount;
    TestTransactionBuilder spendTx;
    spendTx.addInput(account, outInfo);
    spendTx.addTestKeyOutput(amount, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
    return container.addTransaction({ TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE, TEST_TIMESTAMP, 0 }, *spendTx.build(), {});
  }
};


TEST_F(TransfersContainerKeyImage_Spend, spendingKeyImageWithWrongAmountCausesException) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);

  auto outputs = getOutputs(ITransfersContainer::IncludeTypeAll | ITransfersContainer::IncludeStateSoftLocked);

  ASSERT_EQ(1, outputs.size());
  // mess with amount
  outputs[0].amount = TEST_OUTPUT_AMOUNT * 2;
  ASSERT_ANY_THROW(spendOutput(outputs[0]));
  ASSERT_EQ(1, container.transactionsCount());
}

TEST_F(TransfersContainerKeyImage_Spend, spendUnconfirmedeKeyImageCausesException) {
  auto tx1 = addTransactionWithFixedKey(UNCONFIRMED);
  auto outputs = getOutputs(ITransfersContainer::IncludeAllLocked);
  ASSERT_FALSE(outputs.empty());

  auto tx2 = addTransactionWithFixedKey(UNCONFIRMED, 2, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(2, container.transactionsCount());
  ASSERT_ANY_THROW(spendOutput(outputs[0]));
}

TEST_F(TransfersContainerKeyImage_Spend, spendingUnconfirmedTransferIfThereIsConfirmedWithAnotherAmountCausesException) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  auto tx2 = addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(2, container.transactionsCount());

  auto outputs = getOutputs(ITransfersContainer::IncludeTypeAll | ITransfersContainer::IncludeStateSoftLocked);
  ASSERT_FALSE(outputs.empty());
  outputs[0].amount = TEST_OUTPUT_AMOUNT * 2;

  ASSERT_ANY_THROW(spendOutput(outputs[0]));
}

TEST_F(TransfersContainerKeyImage_Spend, spendingTransferIfThereIsSpentTransferWithAnotherAmountCausesException) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  auto outputs = container.getTransactionOutputs(tx1->getTransactionHash(), ITransfersContainer::IncludeAll);
  ASSERT_EQ(1, outputs.size());

  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 2, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  addSpendingTransaction(tx1->getTransactionHash(), 
    TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_ANY_THROW(spendOutput(outputs[0]));
}


/////////////////////////////////////////////////////////////////////////////
// addTransaction - spending succeeds
/////////////////////////////////////////////////////////////////////////////
TEST_F(TransfersContainerKeyImage_Spend, spendingVisibleConfirmedTransfer) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 2, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_NE(tx1->getTransactionHash(), tx2->getTransactionHash());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  // spend first confirmed transaction
  addSpendingTransaction(tx1->getTransactionHash(), TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE,
    TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  auto spentOutputs = container.getSpentOutputs();
  ASSERT_EQ(1, spentOutputs.size());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, spentOutputs[0].amount);
  ASSERT_EQ(tx1->getTransactionHash(), spentOutputs[0].transactionHash);
}

TEST_F(TransfersContainerKeyImage_Spend, spendHiddenConfirmedTransfer) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 2, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_NE(tx1->getTransactionHash(), tx2->getTransactionHash());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  // spend second confirmed transaction (hidden)
  addSpendingTransaction(tx2->getTransactionHash(), TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE,
    TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  auto spentOutputs = container.getSpentOutputs();
  ASSERT_EQ(1, spentOutputs.size());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT * 2, spentOutputs[0].amount);
  ASSERT_EQ(tx2->getTransactionHash(), spentOutputs[0].transactionHash);
}

TEST_F(TransfersContainerKeyImage_Spend, spendSecondHiddenConfirmedOutput) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 2, TEST_OUTPUT_AMOUNT * 2);
  auto tx3 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 2, 3, TEST_OUTPUT_AMOUNT * 3);

  // spend third confirmed transaction (hidden)
  addSpendingTransaction(tx3->getTransactionHash(), TEST_BLOCK_HEIGHT + 2 + TEST_TRANSACTION_SPENDABLE_AGE,
    TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT * 3);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  auto spentOutputs = container.getSpentOutputs();
  ASSERT_EQ(1, spentOutputs.size());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT * 3, spentOutputs[0].amount);
  ASSERT_EQ(tx3->getTransactionHash(), spentOutputs[0].transactionHash);
}

TEST_F(TransfersContainerKeyImage_Spend, spendHiddenWithSameAmount_oneBlock) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT - 1);
  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2, 1);
  auto tx3 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 3, TEST_OUTPUT_AMOUNT * 2, 2);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));

  addSpendingTransaction(tx3->getTransactionHash(), TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE,
    TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  auto spentOutputs = container.getSpentOutputs();
  ASSERT_EQ(1, spentOutputs.size());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT * 2, spentOutputs[0].amount);
  ASSERT_EQ(tx2->getTransactionHash(), spentOutputs[0].transactionHash);
}

TEST_F(TransfersContainerKeyImage_Spend, spendHiddenWithSameAmount_differentBlocks) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 3, TEST_OUTPUT_AMOUNT * 2);
  auto tx3 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 2, 2, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));

  addSpendingTransaction(tx3->getTransactionHash(), TEST_BLOCK_HEIGHT + 2 + TEST_TRANSACTION_SPENDABLE_AGE,
    TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  auto spentOutputs = container.getSpentOutputs();
  ASSERT_EQ(1, spentOutputs.size());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT * 2, spentOutputs[0].amount);
  ASSERT_EQ(tx2->getTransactionHash(), spentOutputs[0].transactionHash);
}

/////////////////////////////////////////////////////////////////////////////
// remove spending transaction
/////////////////////////////////////////////////////////////////////////////
class TransfersContainerKeyImage_Remove : public TransfersContainerKeyImage {
public: 
  void checkSpentOutputs(const Hash& expectedTxHash) {
    auto spentOutputs = container.getSpentOutputs();
    ASSERT_EQ(1, spentOutputs.size());
    ASSERT_EQ(expectedTxHash, spentOutputs[0].transactionHash);
  }
};

TEST_F(TransfersContainerKeyImage_Remove, removeUnconfirmedTxSpendingVisibleOutput) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 1, TEST_OUTPUT_AMOUNT, 1);
  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2, 2);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));

  auto spendingTx = addSpendingTransaction(tx1->getTransactionHash(),
    WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, UNCONFIRMED, TEST_OUTPUT_AMOUNT);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
  ASSERT_NO_FATAL_FAILURE(checkSpentOutputs(tx1->getTransactionHash()));

  ASSERT_TRUE(container.deleteUnconfirmedTransaction(spendingTx->getTransactionHash()));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage_Remove, removeUnconfirmedTxSpendingHiddenOut) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 1, TEST_OUTPUT_AMOUNT, 1);
  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2, 2);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));

  auto spendingTx = addSpendingTransaction(tx2->getTransactionHash(),
    WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, UNCONFIRMED, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
  ASSERT_NO_FATAL_FAILURE(checkSpentOutputs(tx2->getTransactionHash()));
  ASSERT_TRUE(container.deleteUnconfirmedTransaction(spendingTx->getTransactionHash()));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage_Remove, removeUnconfirmedTxAfterAddingMoreTx) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT, 1, TEST_OUTPUT_AMOUNT, 1);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
  auto spendingTx = addSpendingTransaction(tx1->getTransactionHash(),
    WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, UNCONFIRMED, TEST_OUTPUT_AMOUNT);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 2, TEST_OUTPUT_AMOUNT * 2);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_TRUE(container.deleteUnconfirmedTransaction(spendingTx->getTransactionHash()));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

/////////////////////////////////////////////////////////////////////////////
// remove unconfirmed output
/////////////////////////////////////////////////////////////////////////////
TEST_F(TransfersContainerKeyImage, removingOneOfTwoUnconfirmedTransfersMakesAnotherVisible) {
  auto tx1 = addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT);
  auto tx2 = addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2, 2);
  
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_TRUE(container.deleteUnconfirmedTransaction(tx2->getTransactionHash()));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage, removingOneOfThreeUnconfirmedTransfersDoesNotMakeVisibleAnyOfRemaining) {
  auto tx1 = addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT);
  auto tx2 = addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2);
  auto tx3 = addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 3, TEST_OUTPUT_AMOUNT * 3);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_TRUE(container.deleteUnconfirmedTransaction(tx2->getTransactionHash()));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage, removingOneOfTwoUnconfirmedTransfersIfThereIsConfirmedTransferDoesNotAffectBalance) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  auto tx2 = addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2);
  auto tx3 = addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 3, TEST_OUTPUT_AMOUNT * 3);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_TRUE(container.deleteUnconfirmedTransaction(tx2->getTransactionHash()));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage, removingOnlyUnconfirmedTransfersIfThereIsConfirmedTransferDoesNotAffectBalance) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  auto tx2 = addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeStateSoftLocked | ITransfersContainer::IncludeTypeAll));

  ASSERT_TRUE(container.deleteUnconfirmedTransaction(tx2->getTransactionHash()));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage, removingOneOfTwoUnconfirmedTransfersIfThereIsSpentTransferDoesNotAffectBalance) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  auto tx2 = addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2);
  auto tx3 = addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 3, TEST_OUTPUT_AMOUNT * 3);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeStateSoftLocked | ITransfersContainer::IncludeTypeAll));

  container.advanceHeight(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE);
  auto spendingTx = addSpendingTransaction(tx1->getTransactionHash(),
    TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE + 1, TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_TRUE(container.deleteUnconfirmedTransaction(tx2->getTransactionHash()));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
}

/////////////////////////////////////////////////////////////////////////////
// remove confirmed output
/////////////////////////////////////////////////////////////////////////////
TEST_F(TransfersContainerKeyImage, removeConfirmed_oneOfThree) {
  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 2, TEST_OUTPUT_AMOUNT * 2);
  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 2, 3, TEST_OUTPUT_AMOUNT * 3);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  container.detach(TEST_BLOCK_HEIGHT + 2);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersContainerKeyImage, removeConfirmed_oneOfTwo) {
  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 2, TEST_OUTPUT_AMOUNT * 2);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAllUnlocked));

  container.detach(TEST_BLOCK_HEIGHT + 1);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeStateSoftLocked | ITransfersContainer::IncludeTypeAll));
}

TEST_F(TransfersContainerKeyImage, removeConfirmed_revealsUnconfirmed) {
  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_EQ(1, container.detach(TEST_BLOCK_HEIGHT).size());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT * 2, container.balance(ITransfersContainer::IncludeAll));
  ASSERT_EQ(TEST_OUTPUT_AMOUNT * 2, container.balance(ITransfersContainer::IncludeAllLocked));
}

TEST_F(TransfersContainerKeyImage, removeConfirmed_twoUnconfirmedHidden) {
  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 2, TEST_OUTPUT_AMOUNT * 2);
  addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 3, TEST_OUTPUT_AMOUNT * 3);

  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_EQ(1, container.detach(TEST_BLOCK_HEIGHT).size());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage, removeConfirmed_twoConfirmedOneUnconfirmedHidden) {
  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + 1, 2, TEST_OUTPUT_AMOUNT * 2);
  addTransactionWithFixedKey(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, 3, TEST_OUTPUT_AMOUNT * 3);
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_EQ(1, container.detach(TEST_BLOCK_HEIGHT + 1).size());
  ASSERT_EQ(TEST_OUTPUT_AMOUNT, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage, removeConfirmed_oneSpentOneConfirmed) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  addSpendingTransaction(tx1->getTransactionHash(), TEST_BLOCK_HEIGHT + 1,
    TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE + 1, 2, TEST_OUTPUT_AMOUNT * 2);
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));

  ASSERT_EQ(1, container.detach(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE + 1).size());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersContainerKeyImage, removeConfirmed_oneSpentTwoConfirmed) {
  auto tx1 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT);
  addSpendingTransaction(tx1->getTransactionHash(), TEST_BLOCK_HEIGHT + 1,
    TEST_TRANSACTION_OUTPUT_GLOBAL_INDEX, TEST_OUTPUT_AMOUNT);

  auto tx2 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE + 1, 2, TEST_OUTPUT_AMOUNT * 2);
  auto tx3 = addTransactionWithFixedKey(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE + 2, 3, TEST_OUTPUT_AMOUNT * 3);

  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
  ASSERT_EQ(4, container.transactionsCount());

  ASSERT_EQ(1, container.detach(TEST_BLOCK_HEIGHT + TEST_TRANSACTION_SPENDABLE_AGE + 2).size());
  ASSERT_EQ(3, container.transactionsCount());
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAll));
}
