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

#pragma once

#include <boost/utility/value_init.hpp>

#include "CryptoNoteBasic.h"
#include "CryptoNoteSerialization.h"

#include "Serialization/BinaryOutputStreamSerializer.h"
#include "Serialization/BinaryInputStreamSerializer.h"

namespace Logging {
class ILogger;
}

namespace CryptoNote {

bool parseAndValidateTransactionFromBinaryArray(const BinaryArray& transactionBinaryArray, Transaction& transaction, Crypto::Hash& transactionHash, Crypto::Hash& transactionPrefixHash);

struct TransactionSourceEntry {
  typedef std::pair<uint32_t, Crypto::PublicKey> OutputEntry;

  std::vector<OutputEntry> outputs;           //index + key
  size_t realOutput;                          //index in outputs vector of real output_entry
  Crypto::PublicKey realTransactionPublicKey; //incoming real tx public key
  size_t realOutputIndexInTransaction;        //index in transaction outputs vector
  uint64_t amount;                            //money
};

struct TransactionDestinationEntry {
  uint64_t amount;                    //money
  AccountPublicAddress addr;          //destination address

  TransactionDestinationEntry() : amount(0), addr(boost::value_initialized<AccountPublicAddress>()) {}
  TransactionDestinationEntry(uint64_t amount, const AccountPublicAddress &addr) : amount(amount), addr(addr) {}
};


bool constructTransaction(
  const AccountKeys& senderAccountKeys,
  const std::vector<TransactionSourceEntry>& sources,
  const std::vector<TransactionDestinationEntry>& destinations,
  std::vector<uint8_t> extra, Transaction& transaction, uint64_t unlock_time, Logging::ILogger& log);


bool is_out_to_acc(const AccountKeys& acc, const KeyOutput& out_key, const Crypto::PublicKey& tx_pub_key, size_t keyIndex);
bool is_out_to_acc(const AccountKeys& acc, const KeyOutput& out_key, const Crypto::KeyDerivation& derivation, size_t keyIndex);
bool lookup_acc_outs(const AccountKeys& acc, const Transaction& tx, const Crypto::PublicKey& tx_pub_key, std::vector<size_t>& outs, uint64_t& money_transfered);
bool lookup_acc_outs(const AccountKeys& acc, const Transaction& tx, std::vector<size_t>& outs, uint64_t& money_transfered);
bool get_tx_fee(const Transaction& tx, uint64_t & fee);
uint64_t get_tx_fee(const Transaction& tx);
bool generate_key_image_helper(const AccountKeys& ack, const Crypto::PublicKey& tx_public_key, size_t real_output_index, KeyPair& in_ephemeral, Crypto::KeyImage& ki);
bool getInputsMoneyAmount(const Transaction& tx, uint64_t& money);
bool checkInputTypesSupported(const TransactionPrefix& tx);
bool checkOutsValid(const TransactionPrefix& tx, std::string* error = nullptr);
bool checkMultisignatureInputsDiff(const TransactionPrefix& tx);
bool checkMoneyOverflow(const TransactionPrefix &tx);
bool checkInputsOverflow(const TransactionPrefix &tx);
bool checkOutsOverflow(const TransactionPrefix& tx);
uint64_t get_outs_money_amount(const Transaction& tx);
std::string short_hash_str(const Crypto::Hash& h);

std::vector<uint32_t> relativeOutputOffsetsToAbsolute(const std::vector<uint32_t>& off);
std::vector<uint32_t> absolute_output_offsets_to_relative(const std::vector<uint32_t>& off);


// 62387455827 -> 455827 + 7000000 + 80000000 + 300000000 + 2000000000 + 60000000000, where 455827 <= dust_threshold
template<typename chunk_handler_t, typename dust_handler_t>
void decompose_amount_into_digits(uint64_t amount, uint64_t dust_threshold, const chunk_handler_t& chunk_handler, const dust_handler_t& dust_handler) {
  if (0 == amount) {
    return;
  }

  bool is_dust_handled = false;
  uint64_t dust = 0;
  uint64_t order = 1;
  while (0 != amount) {
    uint64_t chunk = (amount % 10) * order;
    amount /= 10;
    order *= 10;

    if (dust + chunk <= dust_threshold) {
      dust += chunk;
    } else {
      if (!is_dust_handled && 0 != dust) {
        dust_handler(dust);
        is_dust_handled = true;
      }
      if (0 != chunk) {
        chunk_handler(chunk);
      }
    }
  }

  if (!is_dust_handled && 0 != dust) {
    dust_handler(dust);
  }
}

}
