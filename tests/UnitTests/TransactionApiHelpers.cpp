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

#include "TransactionApiHelpers.h"
#include "CryptoNoteCore/TransactionApi.h"

using namespace CryptoNote;
using namespace Crypto;

namespace {

  const std::vector<AccountBase>& getMsigAccounts() {
    static std::vector<AccountBase> msigAccounts = { generateAccount(), generateAccount() };
    return msigAccounts;
  }

}

TestTransactionBuilder::TestTransactionBuilder() {
  tx = createTransaction();
}

TestTransactionBuilder::TestTransactionBuilder(const BinaryArray& txTemplate, const Crypto::SecretKey& secretKey) {
  tx = createTransaction(txTemplate);
  tx->setTransactionSecretKey(secretKey);
}

PublicKey TestTransactionBuilder::getTransactionPublicKey() const {
  return tx->getTransactionPublicKey();
}

void TestTransactionBuilder::appendExtra(const BinaryArray& extraData) {
  tx->appendExtra(extraData);
}

void TestTransactionBuilder::setUnlockTime(uint64_t time) {
  tx->setUnlockTime(time);
}

size_t TestTransactionBuilder::addTestInput(uint64_t amount, const AccountKeys& senderKeys) {
  using namespace TransactionTypes;

  TransactionTypes::InputKeyInfo info;
  PublicKey targetKey;

  CryptoNote::KeyPair srcTxKeys = CryptoNote::generateKeyPair();
  derivePublicKey(senderKeys, srcTxKeys.publicKey, 5, targetKey);

  TransactionTypes::GlobalOutput gout = { targetKey, 0 };

  info.amount = amount;
  info.outputs.push_back(gout);

  info.realOutput.transactionIndex = 0;
  info.realOutput.outputInTransaction = 5;
  info.realOutput.transactionPublicKey = reinterpret_cast<const PublicKey&>(srcTxKeys.publicKey);

  KeyPair ephKeys;
  size_t idx = tx->addInput(senderKeys, info, ephKeys);
  keys[idx] = std::make_pair(info, ephKeys);
  return idx;
}

size_t TestTransactionBuilder::addTestInput(uint64_t amount, std::vector<uint32_t> gouts, const AccountKeys& senderKeys) {
  using namespace TransactionTypes;

  TransactionTypes::InputKeyInfo info;
  PublicKey targetKey;

  CryptoNote::KeyPair srcTxKeys = CryptoNote::generateKeyPair();
  derivePublicKey(senderKeys, srcTxKeys.publicKey, 5, targetKey);

  TransactionTypes::GlobalOutput gout = { targetKey, 0 };

  info.amount = amount;
  info.outputs.push_back(gout);
  PublicKey pk;
  SecretKey sk;
  for (auto out : gouts) {
    Crypto::generate_keys(pk, sk);
    info.outputs.push_back(TransactionTypes::GlobalOutput{ pk, out });
  }

  info.realOutput.transactionIndex = 0;
  info.realOutput.outputInTransaction = 5;
  info.realOutput.transactionPublicKey = reinterpret_cast<const PublicKey&>(srcTxKeys.publicKey);

  KeyPair ephKeys;
  size_t idx = tx->addInput(senderKeys, info, ephKeys);
  keys[idx] = std::make_pair(info, ephKeys);
  return idx;
}

void TestTransactionBuilder::addInput(const AccountKeys& senderKeys, const TransactionOutputInformation& t) {
  TransactionTypes::InputKeyInfo info;
  info.amount = t.amount;

  TransactionTypes::GlobalOutput globalOut;
  globalOut.outputIndex = t.globalOutputIndex;
  globalOut.targetKey = t.outputKey;
  info.outputs.push_back(globalOut);

  info.realOutput.outputInTransaction = t.outputInTransaction;
  info.realOutput.transactionIndex = 0;
  info.realOutput.transactionPublicKey = t.transactionPublicKey;

  KeyPair ephKeys;
  size_t idx = tx->addInput(senderKeys, info, ephKeys);
  keys[idx] = std::make_pair(info, ephKeys);
}

void TestTransactionBuilder::addTestMultisignatureInput(uint64_t amount, const TransactionOutputInformation& t) {
  MultisignatureInput input;
  input.amount = amount;
  input.outputIndex = t.globalOutputIndex;
  input.signatureCount = t.requiredSignatures;
  size_t idx = tx->addInput(input);
 
  msigInputs[idx] = MsigInfo{ t.transactionPublicKey, t.outputInTransaction, getMsigAccounts() };
}

size_t TestTransactionBuilder::addFakeMultisignatureInput(uint64_t amount, uint32_t globalOutputIndex, size_t signatureCount) {
  MultisignatureInput input;
  input.amount = amount;
  input.outputIndex = globalOutputIndex;
  input.signatureCount = static_cast<uint8_t>(signatureCount);
  size_t idx = tx->addInput(input);

  std::vector<AccountBase> accs;
  for (size_t i = 0; i < signatureCount; ++i) {
    accs.push_back(generateAccount());
  }

  msigInputs[idx] = MsigInfo{ Crypto::rand<PublicKey>(), 0, std::move(accs) };
  return idx;
}

TransactionOutputInformationIn TestTransactionBuilder::addTestKeyOutput(uint64_t amount, uint32_t globalOutputIndex, const AccountKeys& senderKeys) {
  uint32_t index = static_cast<uint32_t>(tx->addOutput(amount, senderKeys.address));

  uint64_t amount_;
  KeyOutput output;
  tx->getOutput(index, output, amount_);

  TransactionOutputInformationIn outputInfo;
  outputInfo.type = TransactionTypes::OutputType::Key;
  outputInfo.amount = amount_;
  outputInfo.globalOutputIndex = globalOutputIndex;
  outputInfo.outputInTransaction = index;
  outputInfo.transactionPublicKey = tx->getTransactionPublicKey();
  outputInfo.outputKey = output.key;
  outputInfo.keyImage = generateKeyImage(senderKeys, index, tx->getTransactionPublicKey());

  return outputInfo;
}

TransactionOutputInformationIn TestTransactionBuilder::addTestMultisignatureOutput(uint64_t amount, std::vector<AccountPublicAddress>& addresses, uint32_t globalOutputIndex) {
  uint32_t index = static_cast<uint32_t>(tx->addOutput(amount, addresses, static_cast<uint32_t>(addresses.size())));

  uint64_t _amount;
  MultisignatureOutput output;
  tx->getOutput(index, output, _amount);

  TransactionOutputInformationIn outputInfo;
  outputInfo.type = TransactionTypes::OutputType::Multisignature;
  outputInfo.amount = _amount;
  outputInfo.globalOutputIndex = globalOutputIndex;
  outputInfo.outputInTransaction = index;
  outputInfo.transactionPublicKey = tx->getTransactionPublicKey();
  // Doesn't used in multisignature output, so can contain garbage
  outputInfo.keyImage = generateKeyImage();
  outputInfo.requiredSignatures = output.requiredSignatureCount;
  return outputInfo;
}

TransactionOutputInformationIn TestTransactionBuilder::addTestMultisignatureOutput(uint64_t amount, uint32_t globalOutputIndex) {
  std::vector<AccountPublicAddress> multisigAddresses;
  for (const auto& acc : getMsigAccounts()) {
    multisigAddresses.push_back(acc.getAccountKeys().address);
  }

  return addTestMultisignatureOutput(amount, multisigAddresses, globalOutputIndex);
}

size_t TestTransactionBuilder::addOutput(uint64_t amount, const AccountPublicAddress& to) {
  return tx->addOutput(amount, to);
}

size_t TestTransactionBuilder::addOutput(uint64_t amount, const KeyOutput& out) {
  return tx->addOutput(amount, out);
}

size_t TestTransactionBuilder::addOutput(uint64_t amount, const MultisignatureOutput& out) {
  return tx->addOutput(amount, out);
}

std::unique_ptr<ITransactionReader> TestTransactionBuilder::build() {
  for (const auto& kv : keys) {
    tx->signInputKey(kv.first, kv.second.first, kv.second.second);
  }

  for (const auto& kv : msigInputs) {
    for (const auto& acc : kv.second.accounts) {
      tx->signInputMultisignature(kv.first, kv.second.transactionKey, kv.second.outputIndex, acc.getAccountKeys());
    }
  }

  transactionHash = tx->getTransactionHash();

  keys.clear();
  return std::move(tx);
}

Crypto::Hash TestTransactionBuilder::getTransactionHash() const {
  return transactionHash;
}

FusionTransactionBuilder::FusionTransactionBuilder(const Currency& currency, uint64_t amount) :
  m_currency(currency),
  m_amount(amount),
  m_firstInput(0),
  m_firstOutput(0),
  m_fee(0),
  m_extraSize(0),
  m_inputCount(currency.fusionTxMinInputCount()) {
}

uint64_t FusionTransactionBuilder::getAmount() const {
  return m_amount;
}

void FusionTransactionBuilder::setAmount(uint64_t val) {
  m_amount = val;
}

uint64_t FusionTransactionBuilder::getFirstInput() const {
  return m_firstInput;
}

void FusionTransactionBuilder::setFirstInput(uint64_t val) {
  m_firstInput = val;
}

uint64_t FusionTransactionBuilder::getFirstOutput() const {
  return m_firstOutput;
}

void FusionTransactionBuilder::setFirstOutput(uint64_t val) {
  m_firstOutput = val;
}

uint64_t FusionTransactionBuilder::getFee() const {
  return m_fee;
}

void FusionTransactionBuilder::setFee(uint64_t val) {
  m_fee = val;
}

size_t FusionTransactionBuilder::getExtraSize() const {
  return m_extraSize;
}

void FusionTransactionBuilder::setExtraSize(size_t val) {
  m_extraSize = val;
}

size_t FusionTransactionBuilder::getInputCount() const {
  return m_inputCount;
}

void FusionTransactionBuilder::setInputCount(size_t val) {
  m_inputCount = val;
}

std::unique_ptr<ITransactionReader> FusionTransactionBuilder::buildReader() const {
  assert(m_inputCount > 0);
  assert(m_firstInput <= m_amount);
  assert(m_amount > m_currency.defaultDustThreshold());

  TestTransactionBuilder builder;

  if (m_extraSize != 0) {
    builder.appendExtra(BinaryArray(m_extraSize, 0));
  }

  if (m_firstInput != 0) {
    builder.addTestInput(m_firstInput);
  }

  if (m_amount > m_firstInput) {
    builder.addTestInput(m_amount - m_firstInput - (m_inputCount - 1) * m_currency.defaultDustThreshold());
    for (size_t i = 0; i < m_inputCount - 1; ++i) {
      builder.addTestInput(m_currency.defaultDustThreshold());
    }
  }

  AccountPublicAddress address = generateAddress();
  std::vector<uint64_t> outputAmounts;
  assert(m_amount >= m_firstOutput + m_fee);
  decomposeAmount(m_amount - m_firstOutput - m_fee, m_currency.defaultDustThreshold(), outputAmounts);
  std::sort(outputAmounts.begin(), outputAmounts.end());

  if (m_firstOutput != 0) {
    builder.addOutput(m_firstOutput, address);
  }

  for (auto outAmount : outputAmounts) {
    builder.addOutput(outAmount, address);
  }

  return builder.build();
}

Transaction FusionTransactionBuilder::buildTx() const {
  return convertTx(*buildReader());
}

Transaction FusionTransactionBuilder::createFusionTransactionBySize(size_t targetSize) {
  auto tx = buildReader();

  size_t realSize = tx->getTransactionData().size();
  if (realSize < targetSize) {
    setExtraSize(targetSize - realSize);
    tx = buildReader();

    realSize = tx->getTransactionData().size();
    if (realSize > targetSize) {
      setExtraSize(getExtraSize() - 1);
      tx = buildReader();
    }
  }

  return convertTx(*tx);
}
