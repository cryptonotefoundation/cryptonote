// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2026, Karbo developers
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

#include "CryptoNoteFormatUtils.h"

#include <set>

#include <Logging/LoggerRef.h>
#include <Common/BinaryArray.hpp>
#include <Common/Varint.h>
#include "Common/Base58.h"

#include "Serialization/BinaryOutputStreamSerializer.h"
#include "Serialization/BinaryInputStreamSerializer.h"
#include "CryptoNoteSerialization.h"

#include "Account.h"
#include "CryptoNoteBasicImpl.h"
#include "CryptoNoteSerialization.h"
#include "TransactionExtra.h"
#include "CryptoNoteTools.h"
#include "Currency.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"

#include "CryptoNoteConfig.h"

using namespace Logging;
using namespace Crypto;
using namespace Common;

namespace CryptoNote {

bool parseAndValidateTransactionFromBinaryArray(const BinaryArray& tx_blob, Transaction& tx, Hash& tx_hash, Hash& tx_prefix_hash) {
  if (!fromBinaryArray(tx, tx_blob)) {
    return false;
  }

  //TODO: validate tx
  cn_fast_hash(tx_blob.data(), tx_blob.size(), tx_hash);
  getObjectHash(*static_cast<TransactionPrefix*>(&tx), tx_prefix_hash);
  return true;
}

bool generate_key_image_helper(const AccountKeys& ack, const PublicKey& tx_public_key, size_t real_output_index, KeyPair& in_ephemeral, KeyImage& ki) {
  KeyDerivation recv_derivation;
  bool r = generate_key_derivation(tx_public_key, ack.viewSecretKey, recv_derivation);

  assert(r && "key image helper: failed to generate_key_derivation");

  if (!r) {
    return false;
  }

  r = derive_public_key(recv_derivation, real_output_index, ack.address.spendPublicKey, in_ephemeral.publicKey);

  assert(r && "key image helper: failed to derive_public_key");

  if (!r) {
    return false;
  }

  derive_secret_key(recv_derivation, real_output_index, ack.spendSecretKey, in_ephemeral.secretKey);
  generate_key_image(in_ephemeral.publicKey, in_ephemeral.secretKey, ki);
  return true;
}

uint64_t power_integral(uint64_t a, uint64_t b) {
  if (b == 0)
    return 1;
  uint64_t total = a;
  for (uint64_t i = 1; i != b; i++)
    total *= a;
  return total;
}

bool get_tx_fee(const Transaction& tx, uint64_t & fee) {
  uint64_t amount_in = 0;
  uint64_t amount_out = 0;

  for (const auto& in : tx.inputs) {
    if (in.type() == typeid(KeyInput)) {
      amount_in += boost::get<KeyInput>(in).amount;
    }
  }

  for (const auto& o : tx.outputs) {
    amount_out += o.amount;
  }

  if (!(amount_in >= amount_out)) {
    return false;
  }

  fee = amount_in - amount_out;
  return true;
}

uint64_t get_tx_fee(const Transaction& tx) {
  uint64_t r = 0;
  if (!get_tx_fee(tx, r))
    return 0;
  return r;
}

bool get_inputs_money_amount(const Transaction& tx, uint64_t& money) {
  money = 0;

  for (const auto& in : tx.inputs) {
    uint64_t amount = 0;

    if (in.type() == typeid(KeyInput)) {
      amount = boost::get<KeyInput>(in).amount;
    }

    money += amount;
  }
  return true;
}

uint32_t get_block_height(const Block& b) {
  if (b.baseTransaction.inputs.size() != 1) {
    return 0;
  }
  const auto& in = b.baseTransaction.inputs[0];
  if (in.type() != typeid(BaseInput)) {
    return 0;
  }
  return boost::get<BaseInput>(in).blockIndex;
}

bool check_inputs_types_supported(const TransactionPrefix& tx) {
  for (const auto& in : tx.inputs) {
    if (in.type() != typeid(KeyInput)) {
      return false;
    }
  }

  return true;
}

bool check_outs_valid(const TransactionPrefix& tx, std::string* error) {
  std::unordered_set<PublicKey> keys_seen;
  for (const TransactionOutput& out : tx.outputs) {
    if (out.target.type() == typeid(KeyOutput)) {
 
      if (out.amount == 0) {
        if (error) {
          *error = "Zero amount ouput";
        }
        return false;
      }

      if (!check_key(boost::get<KeyOutput>(out.target).key)) {
        if (error) {
          *error = "Output with invalid key";
        }
        return false;
      }

      if (keys_seen.find(boost::get<KeyOutput>(out.target).key) != keys_seen.end()) {
        if (error) {
          *error = "The same output target is present more than once";
        }
        return false;
      }
      keys_seen.insert(boost::get<KeyOutput>(out.target).key);
    } else {
      if (error) {
        *error = "Output with invalid type";
      }
      return false;
    }
  }

  return true;
}

bool check_money_overflow(const TransactionPrefix &tx) {
  return check_inputs_overflow(tx) && check_outs_overflow(tx);
}

bool check_inputs_overflow(const TransactionPrefix &tx) {
  uint64_t money = 0;

  for (const auto &in : tx.inputs) {
    uint64_t amount = 0;

    if (in.type() == typeid(KeyInput)) {
      amount = boost::get<KeyInput>(in).amount;
    }

    if (money > amount + money)
      return false;

    money += amount;
  }
  return true;
}

bool check_outs_overflow(const TransactionPrefix& tx) {
  uint64_t money = 0;
  for (const auto& o : tx.outputs) {
    if (money > o.amount + money)
      return false;
    money += o.amount;
  }
  return true;
}

uint64_t get_outs_money_amount(const Transaction& tx) {
  uint64_t outputs_amount = 0;
  for (const auto& o : tx.outputs) {
    outputs_amount += o.amount;
  }
  return outputs_amount;
}

std::string short_hash_str(const Hash& h) {
  std::string res = Common::podToHex(h);

  if (res.size() == 64) {
    auto erased_pos = res.erase(8, 48);
    res.insert(8, "....");
  }

  return res;
}

bool is_out_to_acc(const AccountKeys& acc, const KeyOutput& out_key, const KeyDerivation& derivation, size_t keyIndex) {
  PublicKey pk;
  derive_public_key(derivation, keyIndex, acc.address.spendPublicKey, pk);
  return pk == out_key.key;
}

bool is_out_to_acc(const AccountKeys& acc, const KeyOutput& out_key, const PublicKey& tx_pub_key, size_t keyIndex) {
  KeyDerivation derivation;
  generate_key_derivation(tx_pub_key, acc.viewSecretKey, derivation);
  return is_out_to_acc(acc, out_key, derivation, keyIndex);
}

bool lookup_acc_outs(const AccountKeys& acc, const Transaction& tx, std::vector<size_t>& outs, uint64_t& money_transfered) {
  PublicKey transactionPublicKey = getTransactionPublicKeyFromExtra(tx.extra);
  if (transactionPublicKey == NULL_PUBLIC_KEY)
    return false;
  return lookup_acc_outs(acc, tx, transactionPublicKey, outs, money_transfered);
}

bool lookup_acc_outs(const AccountKeys& acc, const Transaction& tx, const PublicKey& tx_pub_key, std::vector<size_t>& outs, uint64_t& money_transfered) {
  money_transfered = 0;
  size_t keyIndex = 0;
  size_t outputIndex = 0;

  KeyDerivation derivation;
  generate_key_derivation(tx_pub_key, acc.viewSecretKey, derivation);

  for (const TransactionOutput& o : tx.outputs) {
    assert(o.target.type() == typeid(KeyOutput));
    if (o.target.type() == typeid(KeyOutput)) {
      if (is_out_to_acc(acc, boost::get<KeyOutput>(o.target), derivation, keyIndex)) {
        outs.push_back(outputIndex);
        money_transfered += o.amount;
      }

      ++keyIndex;
    }

    ++outputIndex;
  }
  return true;
}

bool get_block_hashing_blob(const Block& b, BinaryArray& ba) {
  if (!toBinaryArray(static_cast<const BlockHeader&>(b), ba)) {
    return false;
  }

  Hash treeRootHash = get_tx_tree_hash(b);
  ba.insert(ba.end(), treeRootHash.data, treeRootHash.data + 32);
  auto transactionCount = asBinaryArray(Tools::get_varint_data(b.transactionHashes.size() + 1));
  ba.insert(ba.end(), transactionCount.begin(), transactionCount.end());
  return true;
}

bool get_signed_block_hashing_blob(const Block& b, BinaryArray& ba) {
  if (!toBinaryArray(static_cast<const BlockHeader&>(b), ba)) {
    return false;
  }

  Hash treeRootHash = get_tx_tree_hash(b);
  ba.insert(ba.end(), treeRootHash.data, treeRootHash.data + 32);
  auto transactionCount = asBinaryArray(Tools::get_varint_data(b.transactionHashes.size() + 1));
  ba.insert(ba.end(), transactionCount.begin(), transactionCount.end());
  BinaryArray sig = Common::asBinaryArray(std::string((const char *)&b.signature, sizeof(Crypto::Signature)));
  ba.insert(ba.end(), sig.begin(), sig.end());
  return true;
}

bool get_parent_block_hashing_blob(const Block& b, BinaryArray& blob) {
  auto serializer = makeParentBlockSerializer(b, true, true);
  return toBinaryArray(serializer, blob);
}

bool get_block_hash(const Block& b, Hash& res) {
  BinaryArray ba;
  if (!get_block_hashing_blob(b, ba)) {
    return false;
  }

  // The header of block version 1 differs from headers of blocks starting from v.2
  if (BLOCK_MAJOR_VERSION_2 == b.majorVersion || BLOCK_MAJOR_VERSION_3 == b.majorVersion) {
    BinaryArray parent_blob;
    auto serializer = makeParentBlockSerializer(b, true, false);
    if (!toBinaryArray(serializer, parent_blob))
      return false;

    ba.insert(ba.end(), parent_blob.begin(), parent_blob.end());
  }

  return getObjectHash(ba, res);
}

Hash get_block_hash(const Block& b) {
  Hash p = NULL_HASH;
  get_block_hash(b, p);
  return p;
}

bool get_aux_block_header_hash(const Block& b, Hash& res) {
  BinaryArray blob;
  if (!get_block_hashing_blob(b, blob)) {
    return false;
  }

  return getObjectHash(blob, res);
}

bool get_block_longhash(cn_context &context, const Block& b, Hash& res) {
  BinaryArray bd;
  if (b.majorVersion == BLOCK_MAJOR_VERSION_1 || b.majorVersion >= BLOCK_MAJOR_VERSION_4) {
    if (!get_block_hashing_blob(b, bd)) {
      return false;
    }
  } else if (b.majorVersion == BLOCK_MAJOR_VERSION_2 || b.majorVersion == BLOCK_MAJOR_VERSION_3) {
    if (!get_parent_block_hashing_blob(b, bd)) {
      return false;
    }
  } else {
    return false;
  }

  if (b.majorVersion <= BLOCK_MAJOR_VERSION_4) {
    cn_slow_hash(context, bd.data(), bd.size(), res);
  }
  else {
    return false;
  }
  
  return true;
}

std::vector<uint32_t> relative_output_offsets_to_absolute(const std::vector<uint32_t>& off) {
  std::vector<uint32_t> res = off;
  for (size_t i = 1; i < res.size(); i++)
    res[i] += res[i - 1];
  return res;
}

std::vector<uint32_t> absolute_output_offsets_to_relative(const std::vector<uint32_t>& off) {
  std::vector<uint32_t> res = off;
  if (!off.size())
    return res;
  std::sort(res.begin(), res.end());//just to be sure, actually it is already should be sorted
  for (size_t i = res.size() - 1; i != 0; i--)
    res[i] -= res[i - 1];

  return res;
}

void get_tx_tree_hash(const std::vector<Hash>& tx_hashes, Hash& h) {
  tree_hash(tx_hashes.data(), tx_hashes.size(), h);
}

Hash get_tx_tree_hash(const std::vector<Hash>& tx_hashes) {
  Hash h = NULL_HASH;
  get_tx_tree_hash(tx_hashes, h);
  return h;
}

Hash get_tx_tree_hash(const Block& b) {
  std::vector<Hash> txs_ids;
  Hash h = NULL_HASH;
  getObjectHash(b.baseTransaction, h);
  txs_ids.push_back(h);
  for (auto& th : b.transactionHashes) {
    txs_ids.push_back(th);
  }
  return get_tx_tree_hash(txs_ids);
}

bool is_valid_decomposed_amount(uint64_t amount) {
  auto it = std::lower_bound(Currency::PRETTY_AMOUNTS.begin(), Currency::PRETTY_AMOUNTS.end(), amount);
  if (it == Currency::PRETTY_AMOUNTS.end() || amount != *it) {
    return false;
  }
  return true;
}

bool getTransactionProof(const Crypto::Hash& transactionHash, const CryptoNote::AccountPublicAddress& destinationAddress, const Crypto::SecretKey& transactionKey, std::string& transactionProof, Logging::ILogger& log) {
  LoggerRef logger(log, "get_tx_proof"); 
  Crypto::KeyImage p = *reinterpret_cast<const Crypto::KeyImage*>(&destinationAddress.viewPublicKey);
  Crypto::KeyImage k = *reinterpret_cast<const Crypto::KeyImage*>(&transactionKey);
  Crypto::KeyImage pk = Crypto::scalarmultKey(p, k);
  Crypto::PublicKey R;
  Crypto::PublicKey rA = reinterpret_cast<const PublicKey&>(pk);
  Crypto::secret_key_to_public_key(transactionKey, R);
  Crypto::Signature sig;

  try {
    Crypto::generate_tx_proof(transactionHash, R, destinationAddress.viewPublicKey, rA, transactionKey, sig);
  } catch (const std::runtime_error &e) {
    logger(ERROR, BRIGHT_RED) << "Proof generation error: " << *e.what();
    return false;
  }

  transactionProof = Tools::Base58::encode_addr(CryptoNote::parameters::CRYPTONOTE_TX_PROOF_BASE58_PREFIX, std::string((const char *)&rA, sizeof(Crypto::PublicKey)) + std::string((const char *)&sig, sizeof(Crypto::Signature)));

  return true;
}

bool getReserveProof(const std::vector<TransactionOutputInformation>& selectedTransfers, const CryptoNote::AccountKeys& accountKeys, const uint64_t& amount, const std::string& message, std::string& reserveProof, Logging::ILogger& log) {
  LoggerRef logger(log, "get_reserve_proof");

  // compute signature prefix hash
  std::string prefix_data = message;
  prefix_data.append((const char*)&accountKeys.address, sizeof(CryptoNote::AccountPublicAddress));

  std::vector<Crypto::KeyImage> kimages;
  CryptoNote::KeyPair ephemeral;

  // have to repeat this to get key image as we don't store m_key_image
  for (size_t i = 0; i < selectedTransfers.size(); ++i) {
    const TransactionOutputInformation &td = selectedTransfers[i];

    // derive ephemeral secret key
    Crypto::KeyImage ki;
    const bool r = CryptoNote::generate_key_image_helper(accountKeys, td.transactionPublicKey, td.outputInTransaction, ephemeral, ki);
    if (!r) {
      logger(ERROR) << "Failed to generate key image";
      return false;
    }
    // now we can insert key image
    prefix_data.append((const char*)&ki, sizeof(Crypto::PublicKey));
    kimages.push_back(ki);
  }

  Crypto::Hash prefix_hash;
  Crypto::cn_fast_hash(prefix_data.data(), prefix_data.size(), prefix_hash);

  // generate proof entries
  std::vector<reserve_proof_entry> proofs(selectedTransfers.size());

  for (size_t i = 0; i < selectedTransfers.size(); ++i) {
    const TransactionOutputInformation &td = selectedTransfers[i];
    reserve_proof_entry& proof = proofs[i];
    proof.key_image = kimages[i];
    proof.transaction_id = td.transactionHash;
    proof.index_in_transaction = td.outputInTransaction;

    auto txPubKey = td.transactionPublicKey;

    for (int i = 0; i < 2; ++i) {
      Crypto::KeyImage sk = Crypto::scalarmultKey(*reinterpret_cast<const Crypto::KeyImage*>(&txPubKey), *reinterpret_cast<const Crypto::KeyImage*>(&accountKeys.viewSecretKey));
      proof.shared_secret = *reinterpret_cast<const Crypto::PublicKey *>(&sk);

      Crypto::KeyDerivation derivation;
      if (!Crypto::generate_key_derivation(proof.shared_secret, accountKeys.viewSecretKey, derivation)) {
        logger(ERROR) << "Failed to generate key derivation";
        return false;
      }
    }

    // generate signature for shared secret
    Crypto::generate_tx_proof(prefix_hash, accountKeys.address.viewPublicKey, txPubKey, proof.shared_secret, accountKeys.viewSecretKey, proof.shared_secret_sig);

    // derive ephemeral secret key
    Crypto::KeyImage ki;
    CryptoNote::KeyPair ephemeral;

    const bool r = CryptoNote::generate_key_image_helper(accountKeys, td.transactionPublicKey, td.outputInTransaction, ephemeral, ki);
    if (!r) {
      logger(ERROR) << "Failed to generate key image";
      return false;
    }

    if (ephemeral.publicKey != td.outputKey) {
      logger(ERROR) << "Derived public key doesn't agree with the stored one";
      return false;
    }

    // generate signature for key image
    const std::vector<const Crypto::PublicKey *>& pubs = { &ephemeral.publicKey };

    Crypto::generate_ring_signature(prefix_hash, proof.key_image, &pubs[0], 1, ephemeral.secretKey, 0, &proof.key_image_sig);
  }

  // generate signature for the spend key that received those outputs
  Crypto::Signature signature;
  Crypto::generate_signature(prefix_hash, accountKeys.address.spendPublicKey, accountKeys.spendSecretKey, signature);

  // serialize & encode
  reserve_proof p;
  p.proofs.assign(proofs.begin(), proofs.end());
  memcpy(&p.signature, &signature, sizeof(signature));

  BinaryArray ba = toBinaryArray(p);
  std::string ret(ba.begin(), ba.end());

  reserveProof = Tools::Base58::encode_addr(CryptoNote::parameters::CRYPTONOTE_RESERVE_PROOF_BASE58_PREFIX, ret);

  return true;
}

std::string signMessage(const std::string &data, const CryptoNote::AccountKeys &keys) {
  Crypto::Hash hash;
  Crypto::cn_fast_hash(data.data(), data.size(), hash);
  
  Crypto::Signature signature;
  Crypto::generate_signature(hash, keys.address.spendPublicKey, keys.spendSecretKey, signature);
  return Tools::Base58::encode_addr(CryptoNote::parameters::CRYPTONOTE_KEYS_SIGNATURE_BASE58_PREFIX, std::string((const char *)&signature, sizeof(signature)));
}

bool verifyMessage(const std::string &data, const CryptoNote::AccountPublicAddress &address, const std::string &signature, Logging::ILogger& log) {
  LoggerRef logger(log, "verify_message");

  std::string decoded;
  uint64_t prefix;
  if (!Tools::Base58::decode_addr(signature, prefix, decoded) || prefix != CryptoNote::parameters::CRYPTONOTE_KEYS_SIGNATURE_BASE58_PREFIX) {
    logger(Logging::ERROR) << "Signature decoding error";
    return false;
  }

  Crypto::Signature s;
  if (sizeof(s) != decoded.size()) {
    logger(Logging::ERROR) << "Signature size wrong";
    return false;
  }

  Crypto::Hash hash;
  Crypto::cn_fast_hash(data.data(), data.size(), hash);

  memcpy(&s, decoded.data(), sizeof(s));
  return Crypto::check_signature(hash, address.spendPublicKey, s);
}

bool generateDeterministicTransactionKeys(const Crypto::Hash& inputsHash,
    const Crypto::SecretKey& secretKey, CryptoNote::KeyPair& keys) {
  BinaryArray ba;
  Common::append(ba, std::begin(secretKey.data), std::end(secretKey.data));
  Common::append(ba, std::begin(inputsHash.data), std::end(inputsHash.data));
  Crypto::hash_to_scalar(ba.data(), ba.size(), keys.secretKey);
  return Crypto::secret_key_to_public_key(keys.secretKey, keys.publicKey);
}

bool generateDeterministicTransactionKeys(const Transaction& tx,
    const Crypto::SecretKey& secretKey, CryptoNote::KeyPair& keys) {
  return generateDeterministicTransactionKeys(getObjectHash(tx.inputs), secretKey, keys);
}

}
