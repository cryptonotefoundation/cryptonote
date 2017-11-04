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

#include "ITransaction.h"
#include "TransactionApiExtra.h"
#include "TransactionUtils.h"

#include "Account.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteConfig.h"

#include <boost/optional.hpp>
#include <numeric>
#include <unordered_set>

using namespace Crypto;

namespace {

  using namespace CryptoNote;

  void derivePublicKey(const AccountPublicAddress& to, const SecretKey& txKey, size_t outputIndex, PublicKey& ephemeralKey) {
    KeyDerivation derivation;
    generate_key_derivation(to.viewPublicKey, txKey, derivation);
    derive_public_key(derivation, outputIndex, to.spendPublicKey, ephemeralKey);
  }

}

namespace CryptoNote {

  using namespace Crypto;

  ////////////////////////////////////////////////////////////////////////
  // class Transaction declaration
  ////////////////////////////////////////////////////////////////////////

  class TransactionImpl : public ITransaction {
  public:
    TransactionImpl();
    TransactionImpl(const BinaryArray& txblob);
    TransactionImpl(const CryptoNote::Transaction& tx);
  
    // ITransactionReader
    virtual Hash getTransactionHash() const override;
    virtual Hash getTransactionPrefixHash() const override;
    virtual PublicKey getTransactionPublicKey() const override;
    virtual uint64_t getUnlockTime() const override;
    virtual bool getPaymentId(Hash& hash) const override;
    virtual bool getExtraNonce(BinaryArray& nonce) const override;
    virtual BinaryArray getExtra() const override;

    // inputs
    virtual size_t getInputCount() const override;
    virtual uint64_t getInputTotalAmount() const override;
    virtual TransactionTypes::InputType getInputType(size_t index) const override;
    virtual void getInput(size_t index, KeyInput& input) const override;
    virtual void getInput(size_t index, MultisignatureInput& input) const override;

    // outputs
    virtual size_t getOutputCount() const override;
    virtual uint64_t getOutputTotalAmount() const override;
    virtual TransactionTypes::OutputType getOutputType(size_t index) const override;
    virtual void getOutput(size_t index, KeyOutput& output, uint64_t& amount) const override;
    virtual void getOutput(size_t index, MultisignatureOutput& output, uint64_t& amount) const override;

    virtual size_t getRequiredSignaturesCount(size_t index) const override;
    virtual bool findOutputsToAccount(const AccountPublicAddress& addr, const SecretKey& viewSecretKey, std::vector<uint32_t>& outs, uint64_t& outputAmount) const override;

    // various checks
    virtual bool validateInputs() const override;
    virtual bool validateOutputs() const override;
    virtual bool validateSignatures() const override;

    // get serialized transaction
    virtual BinaryArray getTransactionData() const override;

    // ITransactionWriter

    virtual void setUnlockTime(uint64_t unlockTime) override;
    virtual void setPaymentId(const Hash& hash) override;
    virtual void setExtraNonce(const BinaryArray& nonce) override;
    virtual void appendExtra(const BinaryArray& extraData) override;

    // Inputs/Outputs 
    virtual size_t addInput(const KeyInput& input) override;
    virtual size_t addInput(const MultisignatureInput& input) override;
    virtual size_t addInput(const AccountKeys& senderKeys, const TransactionTypes::InputKeyInfo& info, KeyPair& ephKeys) override;

    virtual size_t addOutput(uint64_t amount, const AccountPublicAddress& to) override;
    virtual size_t addOutput(uint64_t amount, const std::vector<AccountPublicAddress>& to, uint32_t requiredSignatures) override;
    virtual size_t addOutput(uint64_t amount, const KeyOutput& out) override;
    virtual size_t addOutput(uint64_t amount, const MultisignatureOutput& out) override;

    virtual void signInputKey(size_t input, const TransactionTypes::InputKeyInfo& info, const KeyPair& ephKeys) override;
    virtual void signInputMultisignature(size_t input, const PublicKey& sourceTransactionKey, size_t outputIndex, const AccountKeys& accountKeys) override;
    virtual void signInputMultisignature(size_t input, const KeyPair& ephemeralKeys) override;


    // secret key
    virtual bool getTransactionSecretKey(SecretKey& key) const override;
    virtual void setTransactionSecretKey(const SecretKey& key) override;

  private:

    void invalidateHash();

    std::vector<Signature>& getSignatures(size_t input);

    const SecretKey& txSecretKey() const {
      if (!secretKey) {
        throw std::runtime_error("Operation requires transaction secret key");
      }
      return *secretKey;
    }

    void checkIfSigning() const {
      if (!transaction.signatures.empty()) {
        throw std::runtime_error("Cannot perform requested operation, since it will invalidate transaction signatures");
      }
    }

    CryptoNote::Transaction transaction;
    boost::optional<SecretKey> secretKey;
    mutable boost::optional<Hash> transactionHash;
    TransactionExtra extra;
  };


  ////////////////////////////////////////////////////////////////////////
  // class Transaction implementation
  ////////////////////////////////////////////////////////////////////////

  std::unique_ptr<ITransaction> createTransaction() {
    return std::unique_ptr<ITransaction>(new TransactionImpl());
  }

  std::unique_ptr<ITransaction> createTransaction(const BinaryArray& transactionBlob) {
    return std::unique_ptr<ITransaction>(new TransactionImpl(transactionBlob));
  }

  std::unique_ptr<ITransaction> createTransaction(const CryptoNote::Transaction& tx) {
    return std::unique_ptr<ITransaction>(new TransactionImpl(tx));
  }

  TransactionImpl::TransactionImpl() {   
    CryptoNote::KeyPair txKeys(CryptoNote::generateKeyPair());

    TransactionExtraPublicKey pk = { txKeys.publicKey };
    extra.set(pk);

    transaction.version = CURRENT_TRANSACTION_VERSION;
    transaction.unlockTime = 0;
    transaction.extra = extra.serialize();

    secretKey = txKeys.secretKey;
  }

  TransactionImpl::TransactionImpl(const BinaryArray& ba) {
    if (!fromBinaryArray(transaction, ba)) {
      throw std::runtime_error("Invalid transaction data");
    }
    
    extra.parse(transaction.extra);
    transactionHash = getBinaryArrayHash(ba); // avoid serialization if we already have blob
  }

  TransactionImpl::TransactionImpl(const CryptoNote::Transaction& tx) : transaction(tx) {
    extra.parse(transaction.extra);
  }

  void TransactionImpl::invalidateHash() {
    if (transactionHash.is_initialized()) {
      transactionHash = decltype(transactionHash)();
    }
  }

  Hash TransactionImpl::getTransactionHash() const {
    if (!transactionHash.is_initialized()) {
      transactionHash = getObjectHash(transaction);
    }

    return transactionHash.get();   
  }

  Hash TransactionImpl::getTransactionPrefixHash() const {
    return getObjectHash(*static_cast<const TransactionPrefix*>(&transaction));
  }

  PublicKey TransactionImpl::getTransactionPublicKey() const {
    PublicKey pk(NULL_PUBLIC_KEY);
    extra.getPublicKey(pk);
    return pk;
  }

  uint64_t TransactionImpl::getUnlockTime() const {
    return transaction.unlockTime;
  }

  void TransactionImpl::setUnlockTime(uint64_t unlockTime) {
    checkIfSigning();
    transaction.unlockTime = unlockTime;
    invalidateHash();
  }

  bool TransactionImpl::getTransactionSecretKey(SecretKey& key) const {
    if (!secretKey) {
      return false;
    }
    key = reinterpret_cast<const SecretKey&>(secretKey.get());
    return true;
  }

  void TransactionImpl::setTransactionSecretKey(const SecretKey& key) {
    const auto& sk = reinterpret_cast<const SecretKey&>(key);
    PublicKey pk;
    PublicKey txPubKey;

    secret_key_to_public_key(sk, pk);
    extra.getPublicKey(txPubKey);

    if (txPubKey != pk) {
      throw std::runtime_error("Secret transaction key does not match public key");
    }

    secretKey = key;
  }

  size_t TransactionImpl::addInput(const KeyInput& input) {
    checkIfSigning();
    transaction.inputs.emplace_back(input);
    invalidateHash();
    return transaction.inputs.size() - 1;
  }

  size_t TransactionImpl::addInput(const AccountKeys& senderKeys, const TransactionTypes::InputKeyInfo& info, KeyPair& ephKeys) {
    checkIfSigning();
    KeyInput input;
    input.amount = info.amount;

    generate_key_image_helper(
      senderKeys,
      info.realOutput.transactionPublicKey,
      info.realOutput.outputInTransaction,
      ephKeys,
      input.keyImage);

    // fill outputs array and use relative offsets
    for (const auto& out : info.outputs) {
      input.outputIndexes.push_back(out.outputIndex);
    }

    input.outputIndexes = absolute_output_offsets_to_relative(input.outputIndexes);
    return addInput(input);
  }

  size_t TransactionImpl::addInput(const MultisignatureInput& input) {
    checkIfSigning();
    transaction.inputs.push_back(input);
    invalidateHash();
    return transaction.inputs.size() - 1;
  }

  size_t TransactionImpl::addOutput(uint64_t amount, const AccountPublicAddress& to) {
    checkIfSigning();

    KeyOutput outKey;
    derivePublicKey(to, txSecretKey(), transaction.outputs.size(), outKey.key);
    TransactionOutput out = { amount, outKey };
    transaction.outputs.emplace_back(out);
    invalidateHash();

    return transaction.outputs.size() - 1;
  }

  size_t TransactionImpl::addOutput(uint64_t amount, const std::vector<AccountPublicAddress>& to, uint32_t requiredSignatures) {
    checkIfSigning();

    const auto& txKey = txSecretKey();
    size_t outputIndex = transaction.outputs.size();
    MultisignatureOutput outMsig;
    outMsig.requiredSignatureCount = requiredSignatures;
    outMsig.keys.resize(to.size());
    
    for (size_t i = 0; i < to.size(); ++i) {
      derivePublicKey(to[i], txKey, outputIndex, outMsig.keys[i]);
    }

    TransactionOutput out = { amount, outMsig };
    transaction.outputs.emplace_back(out);
    invalidateHash();

    return outputIndex;
  }

  size_t TransactionImpl::addOutput(uint64_t amount, const KeyOutput& out) {
    checkIfSigning();
    size_t outputIndex = transaction.outputs.size();
    TransactionOutput realOut = { amount, out };
    transaction.outputs.emplace_back(realOut);
    invalidateHash();
    return outputIndex;
  }

  size_t TransactionImpl::addOutput(uint64_t amount, const MultisignatureOutput& out) {
    checkIfSigning();
    size_t outputIndex = transaction.outputs.size();
    TransactionOutput realOut = { amount, out };
    transaction.outputs.emplace_back(realOut);
    invalidateHash();
    return outputIndex;
  }

  void TransactionImpl::signInputKey(size_t index, const TransactionTypes::InputKeyInfo& info, const KeyPair& ephKeys) {
    const auto& input = boost::get<KeyInput>(getInputChecked(transaction, index, TransactionTypes::InputType::Key));
    Hash prefixHash = getTransactionPrefixHash();

    std::vector<Signature> signatures;
    std::vector<const PublicKey*> keysPtrs;

    for (const auto& o : info.outputs) {
      keysPtrs.push_back(reinterpret_cast<const PublicKey*>(&o.targetKey));
    }

    signatures.resize(keysPtrs.size());

    generate_ring_signature(
      reinterpret_cast<const Hash&>(prefixHash),
      reinterpret_cast<const KeyImage&>(input.keyImage),
      keysPtrs,
      reinterpret_cast<const SecretKey&>(ephKeys.secretKey),
      info.realOutput.transactionIndex,
      signatures.data());

    getSignatures(index) = signatures;
    invalidateHash();
  }

  void TransactionImpl::signInputMultisignature(size_t index, const PublicKey& sourceTransactionKey, size_t outputIndex, const AccountKeys& accountKeys) {
    KeyDerivation derivation;
    PublicKey ephemeralPublicKey;
    SecretKey ephemeralSecretKey;

    generate_key_derivation(
      reinterpret_cast<const PublicKey&>(sourceTransactionKey),
      reinterpret_cast<const SecretKey&>(accountKeys.viewSecretKey),
      derivation);

    derive_public_key(derivation, outputIndex,
      reinterpret_cast<const PublicKey&>(accountKeys.address.spendPublicKey), ephemeralPublicKey);
    derive_secret_key(derivation, outputIndex,
      reinterpret_cast<const SecretKey&>(accountKeys.spendSecretKey), ephemeralSecretKey);

    Signature signature;
    auto txPrefixHash = getTransactionPrefixHash();

    generate_signature(reinterpret_cast<const Hash&>(txPrefixHash),
      ephemeralPublicKey, ephemeralSecretKey, signature);

    getSignatures(index).push_back(signature);
    invalidateHash();
  }

  void TransactionImpl::signInputMultisignature(size_t index, const KeyPair& ephemeralKeys) {
    Signature signature;
    auto txPrefixHash = getTransactionPrefixHash();

    generate_signature(txPrefixHash, ephemeralKeys.publicKey, ephemeralKeys.secretKey, signature);

    getSignatures(index).push_back(signature);
    invalidateHash();
  }

  std::vector<Signature>& TransactionImpl::getSignatures(size_t input) {
    // update signatures container size if needed
    if (transaction.signatures.size() < transaction.inputs.size()) {
      transaction.signatures.resize(transaction.inputs.size());
    }
    // check range
    if (input >= transaction.signatures.size()) {
      throw std::runtime_error("Invalid input index");
    }

    return transaction.signatures[input];
  }

  BinaryArray TransactionImpl::getTransactionData() const {
    return toBinaryArray(transaction);
  }

  void TransactionImpl::setPaymentId(const Hash& hash) {
    checkIfSigning();
    BinaryArray paymentIdBlob;
    setPaymentIdToTransactionExtraNonce(paymentIdBlob, reinterpret_cast<const Hash&>(hash));
    setExtraNonce(paymentIdBlob);
  }

  bool TransactionImpl::getPaymentId(Hash& hash) const {
    BinaryArray nonce;
    if (getExtraNonce(nonce)) {
      Hash paymentId;
      if (getPaymentIdFromTransactionExtraNonce(nonce, paymentId)) {
        hash = reinterpret_cast<const Hash&>(paymentId);
        return true;
      }
    }
    return false;
  }

  void TransactionImpl::setExtraNonce(const BinaryArray& nonce) {
    checkIfSigning();
    TransactionExtraNonce extraNonce = { nonce };
    extra.set(extraNonce);
    transaction.extra = extra.serialize();
    invalidateHash();
  }

  void TransactionImpl::appendExtra(const BinaryArray& extraData) {
    checkIfSigning();
    transaction.extra.insert(
      transaction.extra.end(), extraData.begin(), extraData.end());
  }

  bool TransactionImpl::getExtraNonce(BinaryArray& nonce) const {
    TransactionExtraNonce extraNonce;
    if (extra.get(extraNonce)) {
      nonce = extraNonce.nonce;
      return true;
    }
    return false;
  }

  BinaryArray TransactionImpl::getExtra() const {
    return transaction.extra;
  }

  size_t TransactionImpl::getInputCount() const {
    return transaction.inputs.size();
  }

  uint64_t TransactionImpl::getInputTotalAmount() const {
    return std::accumulate(transaction.inputs.begin(), transaction.inputs.end(), 0ULL, [](uint64_t val, const TransactionInput& in) {
      return val + getTransactionInputAmount(in); });
  }

  TransactionTypes::InputType TransactionImpl::getInputType(size_t index) const {
    return getTransactionInputType(getInputChecked(transaction, index));
  }

  void TransactionImpl::getInput(size_t index, KeyInput& input) const {
    input = boost::get<KeyInput>(getInputChecked(transaction, index, TransactionTypes::InputType::Key));
  }

  void TransactionImpl::getInput(size_t index, MultisignatureInput& input) const {
    input = boost::get<MultisignatureInput>(getInputChecked(transaction, index, TransactionTypes::InputType::Multisignature));
  }

  size_t TransactionImpl::getOutputCount() const {
    return transaction.outputs.size();
  }

  uint64_t TransactionImpl::getOutputTotalAmount() const {
    return std::accumulate(transaction.outputs.begin(), transaction.outputs.end(), 0ULL, [](uint64_t val, const TransactionOutput& out) {
      return val + out.amount; });
  }

  TransactionTypes::OutputType TransactionImpl::getOutputType(size_t index) const {
    return getTransactionOutputType(getOutputChecked(transaction, index).target);
  }

  void TransactionImpl::getOutput(size_t index, KeyOutput& output, uint64_t& amount) const {
    const auto& out = getOutputChecked(transaction, index, TransactionTypes::OutputType::Key);
    output = boost::get<KeyOutput>(out.target);
    amount = out.amount;
  }

  void TransactionImpl::getOutput(size_t index, MultisignatureOutput& output, uint64_t& amount) const {
    const auto& out = getOutputChecked(transaction, index, TransactionTypes::OutputType::Multisignature);
    output = boost::get<MultisignatureOutput>(out.target);
    amount = out.amount;
  }

  bool TransactionImpl::findOutputsToAccount(const AccountPublicAddress& addr, const SecretKey& viewSecretKey, std::vector<uint32_t>& out, uint64_t& amount) const {
    return ::CryptoNote::findOutputsToAccount(transaction, addr, viewSecretKey, out, amount);
  }

  size_t TransactionImpl::getRequiredSignaturesCount(size_t index) const {
    return ::getRequiredSignaturesCount(getInputChecked(transaction, index));
  }

  bool TransactionImpl::validateInputs() const {
    return
      checkInputTypesSupported(transaction) &&
      checkInputsOverflow(transaction) &&
      checkInputsKeyimagesDiff(transaction) &&
      checkMultisignatureInputsDiff(transaction);
  }

  bool TransactionImpl::validateOutputs() const {
    return
      checkOutsValid(transaction) &&
      checkOutsOverflow(transaction);
  }

  bool TransactionImpl::validateSignatures() const {
    if (transaction.signatures.size() < transaction.inputs.size()) {
      return false;
    }

    for (size_t i = 0; i < transaction.inputs.size(); ++i) {
      if (getRequiredSignaturesCount(i) > transaction.signatures[i].size()) {
        return false;
      }
    }

    return true;
  }
}
