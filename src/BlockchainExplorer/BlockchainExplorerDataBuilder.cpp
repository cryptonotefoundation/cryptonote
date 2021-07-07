// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2020, The Karbo developers
//
// This file is part of Karbo.
//
// Karbo is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Karbo is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Karbo.  If not, see <http://www.gnu.org/licenses/>.

#include "BlockchainExplorerDataBuilder.h"

#include <boost/utility/value_init.hpp>
#include <boost/range/combine.hpp>

#include "Common/StringTools.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/TransactionExtra.h"
#include "CryptoNoteConfig.h"

namespace CryptoNote {

BlockchainExplorerDataBuilder::BlockchainExplorerDataBuilder(CryptoNote::ICore& core, CryptoNote::ICryptoNoteProtocolQuery& protocol) :
m_core(core),
protocol(protocol) {
}

bool BlockchainExplorerDataBuilder::getMixin(const Transaction& transaction, uint64_t& mixin) {
  mixin = 0;
  for (const TransactionInput& txin : transaction.inputs) {
    if (txin.type() != typeid(KeyInput)) {
      continue;
    }
    uint64_t currentMixin = boost::get<KeyInput>(txin).outputIndexes.size();
    if (currentMixin > mixin) {
      mixin = currentMixin;
    }
  }
  return true;
}

bool BlockchainExplorerDataBuilder::getPaymentId(const Transaction& transaction, Crypto::Hash& paymentId) {
  std::vector<TransactionExtraField> txExtraFields;
  parseTransactionExtra(transaction.extra, txExtraFields);
  TransactionExtraNonce extraNonce;
  if (!findTransactionExtraFieldByType(txExtraFields, extraNonce)) {
    return false;
  }
  return getPaymentIdFromTransactionExtraNonce(extraNonce.nonce, paymentId);
}

bool BlockchainExplorerDataBuilder::fillTxExtra(const std::vector<uint8_t>& rawExtra, TransactionExtraDetails2& extraDetails) {
  extraDetails.raw = rawExtra;
  std::vector<TransactionExtraField> txExtraFields;
  parseTransactionExtra(rawExtra, txExtraFields);
  for (const TransactionExtraField& field : txExtraFields) {
    if (typeid(TransactionExtraPadding) == field.type()) {
      extraDetails.padding.push_back(std::move(boost::get<TransactionExtraPadding>(field).size));
    } else if (typeid(TransactionExtraPublicKey) == field.type()) {
      extraDetails.publicKey = std::move(boost::get<TransactionExtraPublicKey>(field).publicKey);
    } else if (typeid(TransactionExtraNonce) == field.type()) {
      extraDetails.nonce = boost::get<TransactionExtraNonce>(field).nonce;
    }
  }
  extraDetails.size = rawExtra.size();
  return true;
}

size_t BlockchainExplorerDataBuilder::median(std::vector<size_t>& v) {
  if (v.empty())
    return boost::value_initialized<size_t>();
  if (v.size() == 1)
    return v[0];

  size_t n = (v.size()) / 2;
  std::sort(v.begin(), v.end());
  //nth_element(v.begin(), v.begin()+n-1, v.end());
  if (v.size() % 2) {//1, 3, 5...
    return v[n];
  } else {//2, 4, 6...
    return (v[n - 1] + v[n]) / 2;
  }
}

bool BlockchainExplorerDataBuilder::fillBlockDetails(const Block &block, BlockDetails& blockDetails, bool calculate_pow) {
  Crypto::Hash hash = get_block_hash(block);

  blockDetails.majorVersion = block.majorVersion;
  blockDetails.minorVersion = block.minorVersion;
  blockDetails.timestamp = block.timestamp;
  blockDetails.prevBlockHash = block.previousBlockHash;
  blockDetails.nonce = block.nonce;
  blockDetails.hash = hash;

  blockDetails.reward = 0;
  for (const TransactionOutput& out : block.baseTransaction.outputs) {
    blockDetails.reward += out.amount;
  }

  if (block.baseTransaction.inputs.front().type() != typeid(BaseInput))
    return false;
  blockDetails.height = boost::get<BaseInput>(block.baseTransaction.inputs.front()).blockIndex;
  blockDetails.depth = m_core.getCurrentBlockchainHeight() - blockDetails.height - 1;

  Crypto::Hash tmpHash = m_core.getBlockIdByHeight(blockDetails.height);
  blockDetails.isOrphaned = hash != tmpHash;

  blockDetails.proofOfWork = boost::value_initialized<Crypto::Hash>();
  if (calculate_pow) {
    Crypto::cn_context context;
    if (!get_block_longhash(context, block, blockDetails.proofOfWork)) {
      return false;
    }
  }

  if (!m_core.getBlockDifficulty(blockDetails.height, blockDetails.difficulty)) {
    return false;
  }

  if (!m_core.getBlockCumulativeDifficulty(blockDetails.height, blockDetails.cumulativeDifficulty)) {
    return false;
  }

  std::vector<size_t> blocksSizes;
  if (!m_core.getBackwardBlocksSizes(blockDetails.height, blocksSizes, parameters::CRYPTONOTE_REWARD_BLOCKS_WINDOW)) {
    return false;
  }
  blockDetails.sizeMedian = median(blocksSizes);

  size_t blockGrantedFullRewardZone = CryptoNote::parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;
  blockDetails.effectiveSizeMedian = std::max(blockDetails.sizeMedian, (uint64_t) blockGrantedFullRewardZone);

  size_t blockSize = 0;
  if (!m_core.getBlockSize(hash, blockSize)) {
    return false;
  }
  blockDetails.transactionsCumulativeSize = blockSize;

  size_t blokBlobSize = getObjectBinarySize(block);
  size_t minerTxBlobSize = getObjectBinarySize(block.baseTransaction);
  blockDetails.blockSize = blokBlobSize + blockDetails.transactionsCumulativeSize - minerTxBlobSize;

  if (!m_core.getAlreadyGeneratedCoins(hash, blockDetails.alreadyGeneratedCoins)) {
    return false;
  }

  if (!m_core.getGeneratedTransactionsNumber(blockDetails.height, blockDetails.alreadyGeneratedTransactions)) {
    return false;
  }

  uint64_t prevBlockGeneratedCoins = 0;
  if (blockDetails.height > 0) {
    if (!m_core.getAlreadyGeneratedCoins(block.previousBlockHash, prevBlockGeneratedCoins)) {
      return false;
    }
  }

  uint64_t maxReward = 0;
  uint64_t currentReward = 0;
  int64_t emissionChange = 0;
  if (!m_core.getBlockReward(block.majorVersion, blockDetails.sizeMedian, 0, prevBlockGeneratedCoins, 0, maxReward, emissionChange)) {
    return false;
  }

  if (!m_core.getBlockReward(block.majorVersion, blockDetails.sizeMedian, blockDetails.transactionsCumulativeSize, prevBlockGeneratedCoins, 0, currentReward, emissionChange)) {
    return false;
  }

  blockDetails.baseReward = maxReward;
  if (maxReward == 0 && currentReward == 0) {
    blockDetails.penalty = static_cast<double>(0);
  } else {
    if (maxReward < currentReward) {
      return false;
    }
    blockDetails.penalty = static_cast<double>(maxReward - currentReward) / static_cast<double>(maxReward);
  }


  blockDetails.transactions.reserve(block.transactionHashes.size() + 1);
  TransactionDetails transactionDetails;
  if (!fillTransactionDetails(block.baseTransaction, transactionDetails, block.timestamp)) {
    return false;
  }
  blockDetails.transactions.push_back(std::move(transactionDetails));

  std::list<Transaction> found;
  std::list<Crypto::Hash> missed;
  m_core.getTransactions(block.transactionHashes, found, missed, blockDetails.isOrphaned);
  if (found.size() != block.transactionHashes.size()) {
    return false;
  }

  blockDetails.totalFeeAmount = 0;

  for (const Transaction& tx : found) {
    TransactionDetails transactionDetails;
    if (!fillTransactionDetails(tx, transactionDetails, block.timestamp)) {
      return false;
    }
    blockDetails.transactions.push_back(std::move(transactionDetails));
    blockDetails.totalFeeAmount += transactionDetails.fee;
  }
  return true;
}

bool BlockchainExplorerDataBuilder::fillTransactionDetails(const Transaction& transaction, TransactionDetails& transactionDetails, uint64_t timestamp) {
  Crypto::Hash hash = getObjectHash(transaction);
  transactionDetails.hash = hash;
  transactionDetails.version = transaction.version;
  transactionDetails.timestamp = timestamp;

  Crypto::Hash blockHash;
  uint32_t blockHeight;
  if (!m_core.getBlockContainingTx(hash, blockHash, blockHeight)) {
    transactionDetails.inBlockchain = false;
    transactionDetails.blockHeight = boost::value_initialized<uint32_t>();
    transactionDetails.blockHash = boost::value_initialized<Crypto::Hash>();
  } else {
    transactionDetails.inBlockchain = true;
    transactionDetails.blockHeight = blockHeight;
    transactionDetails.blockHash = blockHash;
    if (timestamp == 0) {
      Block block;
      if (!m_core.getBlockByHash(blockHash, block)) {
        return false;
      }
      transactionDetails.timestamp = block.timestamp;
    }
  }
  transactionDetails.size = getObjectBinarySize(transaction);
  transactionDetails.unlockTime = transaction.unlockTime;
  transactionDetails.totalOutputsAmount = get_outs_money_amount(transaction);

  uint64_t inputsAmount;
  if (!get_inputs_money_amount(transaction, inputsAmount)) {
    return false;
  }
  transactionDetails.totalInputsAmount = inputsAmount;

  if (transaction.inputs.size() > 0 && transaction.inputs.front().type() == typeid(BaseInput)) {
    //It's gen transaction
    transactionDetails.fee = 0;
    transactionDetails.mixin = 0;
  } else {
    uint64_t fee;
    if (!get_tx_fee(transaction, fee)) {
      return false;
    }
    transactionDetails.fee = fee;
    uint64_t mixin;
    if (!m_core.getMixin(transaction, mixin)) {
      return false;
    }
    transactionDetails.mixin = mixin;
  }
  Crypto::Hash paymentId;
  if (getPaymentId(transaction, paymentId)) {
    transactionDetails.paymentId = paymentId;
  } else {
    transactionDetails.paymentId = boost::value_initialized<Crypto::Hash>();
  }
  fillTxExtra(transaction.extra, transactionDetails.extra);
  transactionDetails.signatures.reserve(transaction.signatures.size());
  for (const std::vector<Crypto::Signature>& signatures : transaction.signatures) {
    std::vector<Crypto::Signature> signaturesDetails;
    signaturesDetails.reserve(signatures.size());
    for (const Crypto::Signature& signature : signatures) {
      signaturesDetails.push_back(std::move(signature));
    }
    transactionDetails.signatures.push_back(std::move(signaturesDetails));
  }

  transactionDetails.inputs.reserve(transaction.inputs.size());
  for (const TransactionInput& txIn : transaction.inputs) {
    transactionInputDetails2 txInDetails;
    if (txIn.type() == typeid(BaseInput)) {
      BaseInputDetails txInGenDetails;
      txInGenDetails.input.blockIndex = boost::get<BaseInput>(txIn).blockIndex;
      txInGenDetails.amount = 0;
      for (const TransactionOutput& out : transaction.outputs) {
        txInGenDetails.amount += out.amount;
      }
	  txInDetails = txInGenDetails;
    } else if (txIn.type() == typeid(KeyInput)) {
      CryptoNote::KeyInputDetails txInToKeyDetails;
      const KeyInput& txInToKey = boost::get<KeyInput>(txIn);
      txInToKeyDetails.input = txInToKey; 
      std::list<std::pair<Crypto::Hash, size_t>> outputReferences;
      if (!m_core.scanOutputkeysForIndices(txInToKey, outputReferences)) {
        return false;
      }
      txInToKeyDetails.mixin = txInToKey.outputIndexes.size();
	  for (const auto& r : outputReferences) {
		  TransactionOutputReferenceDetails d;
		  d.number = r.second;
		  d.transactionHash = r.first;
		  txInToKeyDetails.outputs.push_back(d);
	  }
	  txInDetails = txInToKeyDetails;
    } else if (txIn.type() == typeid(MultisignatureInput)) {
      MultisignatureInputDetails txInMultisigDetails;
      const MultisignatureInput& txInMultisig = boost::get<MultisignatureInput>(txIn);
      txInMultisigDetails.input = txInMultisig;
      std::pair<Crypto::Hash, size_t> outputReference;
      if (!m_core.getMultisigOutputReference(txInMultisig, outputReference)) {
        return false;
      }
      txInMultisigDetails.output.number = outputReference.second;
      txInMultisigDetails.output.transactionHash = outputReference.first;
	  txInDetails = txInMultisigDetails;
    } else {
      return false;
    }
    transactionDetails.inputs.push_back(std::move(txInDetails));
  }

  transactionDetails.outputs.reserve(transaction.outputs.size());
  std::vector<uint32_t> globalIndices;
  globalIndices.reserve(transaction.outputs.size());
  if (!transactionDetails.inBlockchain || !m_core.get_tx_outputs_gindexs(hash, globalIndices)) {
    for (size_t i = 0; i < transaction.outputs.size(); ++i) {
      globalIndices.push_back(0);
    }
  }

  typedef boost::tuple<TransactionOutput, uint32_t> outputWithIndex;
  auto range = boost::combine(transaction.outputs, globalIndices);
  for (const outputWithIndex& txOutput : range) {
    transactionOutputDetails2 txOutDetails;
    txOutDetails.globalIndex = txOutput.get<1>();
	txOutDetails.output.amount = txOutput.get<0>().amount;
	txOutDetails.output.target = txOutput.get<0>().target;
    transactionDetails.outputs.push_back(std::move(txOutDetails));
  }

  return true;
}

}
