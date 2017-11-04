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

#include "Chaingen.h"

#include <iostream>
#include <stdint.h>
#include <vector>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/program_options.hpp>

#include "Common/CommandLine.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/Core.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/UpgradeDetector.h"

//#include "AccountBoostSerialization.h"
//#include "cryptonote_boost_serialization.h"

using namespace std;
using namespace CryptoNote;

struct output_index {
  const CryptoNote::TransactionOutputTarget out;
  uint64_t amount;
  size_t blk_height; // block height
  size_t tx_no;      // index of transaction in block
  size_t out_no;     // index of out in transaction
  uint32_t idx;
  bool spent;
  CryptoNote::BlockTemplate p_blk;
  CryptoNote::Transaction p_tx;

  output_index(const CryptoNote::TransactionOutputTarget& _out, uint64_t _a, size_t _h, size_t tno, size_t ono,
               const CryptoNote::BlockTemplate& _pb, const CryptoNote::Transaction& _pt)
      : out(_out), amount(_a), blk_height(_h), tx_no(tno), out_no(ono), idx(0), spent(false), p_blk(_pb), p_tx(_pt) {
  }

  output_index(const output_index& other)
      : out(other.out), amount(other.amount), blk_height(other.blk_height), tx_no(other.tx_no), out_no(other.out_no),
        idx(other.idx), spent(other.spent), p_blk(other.p_blk), p_tx(other.p_tx) {
  }

  const std::string toString() const {
    std::stringstream ss;

    ss << "output_index{blk_height=" << blk_height << " tx_no=" << tx_no << " out_no=" << out_no << " amount=" << amount
       << " idx=" << idx << " spent=" << spent << "}";

    return ss.str();
  }

  output_index& operator=(const output_index& other) {
    new (this) output_index(other);
    return *this;
  }
};

typedef std::map<uint64_t, std::vector<size_t>> map_output_t;
typedef std::map<uint64_t, std::vector<output_index>> map_output_idx_t;
typedef pair<uint64_t, size_t> outloc_t;

namespace {
uint64_t get_inputs_amount(const vector<TransactionSourceEntry>& s) {
  return std::accumulate(std::begin(s), std::end(s), uint64_t(0),
                         [&](uint64_t sum, const TransactionSourceEntry& e) { return sum + e.amount; });
}
}

bool init_output_indices(map_output_idx_t& outs, std::map<uint64_t, std::vector<size_t>>& outs_mine,
                         const std::vector<CryptoNote::BlockTemplate>& blockchain, const map_hash2tx_t& mtx,
                         const CryptoNote::AccountBase& from) {

  for (const auto& blk : blockchain) {
    vector<Transaction> vtx;
    vtx.push_back(blk.baseTransaction);

    for (const auto& h : blk.transactionHashes) {
      const auto cit = mtx.find(h);
      if (mtx.end() == cit)
        throw std::runtime_error("block contains an unknown tx hash");

      vtx.push_back(cit->second);
    }

    // vtx.insert(vtx.end(), blk.);
    // TODO: add all other txes
    for (size_t i = 0; i < vtx.size(); i++) {
      const Transaction& tx = vtx[i];

      size_t keyIndex = 0;
      for (size_t j = 0; j < tx.outputs.size(); ++j) {
        const TransactionOutput& out = tx.outputs[j];
        if (out.target.type() == typeid(KeyOutput)) {
          output_index oi(out.target, out.amount, boost::get<BaseInput>(*blk.baseTransaction.inputs.begin()).blockIndex,
                          i, j, blk, vtx[i]);
          outs[out.amount].push_back(oi);
          uint32_t tx_global_idx = static_cast<uint32_t>(outs[out.amount].size() - 1);
          outs[out.amount][tx_global_idx].idx = tx_global_idx;
          // Is out to me?
          if (is_out_to_acc(from.getAccountKeys(), boost::get<KeyOutput>(out.target),
                            getTransactionPublicKeyFromExtra(tx.extra), keyIndex)) {
            outs_mine[out.amount].push_back(tx_global_idx);
          }

          ++keyIndex;
        } else if (out.target.type() == typeid(MultisignatureOutput)) {
          keyIndex += boost::get<MultisignatureOutput>(out.target).keys.size();
        }
      }
    }
  }

  return true;
}

bool init_spent_output_indices(map_output_idx_t& outs, const map_output_t& outs_mine,
                               const std::vector<CryptoNote::BlockTemplate>& blockchain, const map_hash2tx_t& mtx,
                               const CryptoNote::AccountBase& from) {

  for (const auto& o : outs_mine) {
    for (size_t i = 0; i < o.second.size(); ++i) {
      output_index& oi = outs[o.first][o.second[i]];

      // construct key image for this output
      Crypto::KeyImage img;
      KeyPair in_ephemeral;
      generate_key_image_helper(from.getAccountKeys(), getTransactionPublicKeyFromExtra(oi.p_tx.extra), oi.out_no,
                                in_ephemeral, img);

      // lookup for this key image in the events vector
      for (auto& tx_pair : mtx) {
        const Transaction& tx = tx_pair.second;
        for (const auto& in : tx.inputs) {
          if (in.type() == typeid(KeyInput)) {
            const KeyInput& itk = boost::get<KeyInput>(in);
            if (itk.keyImage == img) {
              oi.spent = true;
            }
          }
          // TODO: multisig?
        }
      }
    }
  }

  return true;
}

bool fill_output_entries(std::vector<output_index>& out_indices, size_t sender_out, size_t nmix, size_t& real_entry_idx,
                         std::vector<TransactionSourceEntry::OutputEntry>& output_entries) {
  if (out_indices.size() <= nmix)
    return false;

  bool sender_out_found = false;
  size_t rest = nmix;
  for (size_t i = 0; i < out_indices.size() && (0 < rest || !sender_out_found); ++i) {
    const output_index& oi = out_indices[i];
    if (oi.spent)
      continue;

    bool append = false;
    if (i == sender_out) {
      append = true;
      sender_out_found = true;
      real_entry_idx = output_entries.size();
    } else if (0 < rest) {
      --rest;
      append = true;
    }

    if (append) {
      const KeyOutput& otk = boost::get<KeyOutput>(oi.out);
      output_entries.push_back(TransactionSourceEntry::OutputEntry(oi.idx, otk.key));
    }
  }

  return 0 == rest && sender_out_found;
}

bool fill_tx_sources(std::vector<TransactionSourceEntry>& sources, const std::vector<test_event_entry>& events,
                     const BlockTemplate& blk_head, const CryptoNote::AccountBase& from, uint64_t amount, size_t nmix) {
  map_output_idx_t outs;
  map_output_t outs_mine;

  std::vector<CryptoNote::BlockTemplate> blockchain;
  map_hash2tx_t mtx;
  CachedBlock cachedBlk(blk_head);
  if (!find_block_chain(events, blockchain, mtx, cachedBlk.getBlockHash()))
    return false;

  if (!init_output_indices(outs, outs_mine, blockchain, mtx, from))
    return false;

  if (!init_spent_output_indices(outs, outs_mine, blockchain, mtx, from))
    return false;

  // Iterate in reverse for efficiency
  uint64_t sources_amount = 0;
  bool sources_found = false;
  std::find_if(outs_mine.rbegin(), outs_mine.rend(), [&](const map_output_t::value_type& o) {
    for (size_t i = 0; i < o.second.size() && !sources_found; ++i) {
      size_t sender_out = o.second[i];
      const output_index& oi = outs[o.first][sender_out];
      if (oi.spent)
        continue;

      CryptoNote::TransactionSourceEntry ts;
      ts.amount = oi.amount;
      ts.realOutputIndexInTransaction = oi.out_no;
      ts.realTransactionPublicKey = getTransactionPublicKeyFromExtra(oi.p_tx.extra); // incoming tx public key
      size_t realOutput;
      if (!fill_output_entries(outs[o.first], sender_out, nmix, realOutput, ts.outputs))
        continue;

      ts.realOutput = realOutput;

      sources.push_back(ts);

      sources_amount += ts.amount;
      sources_found = amount <= sources_amount;
    }

    return sources_found;
  });

  return sources_found;
}

bool fill_tx_destination(TransactionDestinationEntry& de, const CryptoNote::AccountBase& to, uint64_t amount) {
  de.addr = to.getAccountKeys().address;
  de.amount = amount;
  return true;
}

void fill_tx_sources_and_destinations(const std::vector<test_event_entry>& events, const BlockTemplate& blk_head,
                                      const CryptoNote::AccountBase& from, const CryptoNote::AccountBase& to,
                                      uint64_t amount, uint64_t fee, size_t nmix,
                                      std::vector<TransactionSourceEntry>& sources,
                                      std::vector<TransactionDestinationEntry>& destinations) {
  sources.clear();
  destinations.clear();

  if (!fill_tx_sources(sources, events, blk_head, from, amount + fee, nmix))
    throw std::runtime_error("couldn't fill transaction sources");

  TransactionDestinationEntry de;
  if (!fill_tx_destination(de, to, amount))
    throw std::runtime_error("couldn't fill transaction destination");
  destinations.push_back(de);

  TransactionDestinationEntry de_change;
  uint64_t cache_back = get_inputs_amount(sources) - (amount + fee);
  if (0 < cache_back) {
    if (!fill_tx_destination(de_change, from, cache_back))
      throw std::runtime_error("couldn't fill transaction cache back destination");
    destinations.push_back(de_change);
  }
}

bool construct_tx_to_key(Logging::ILogger& logger, const std::vector<test_event_entry>& events,
                         CryptoNote::Transaction& tx, const BlockTemplate& blk_head,
                         const CryptoNote::AccountBase& from, const CryptoNote::AccountBase& to, uint64_t amount,
                         uint64_t fee, size_t nmix) {
  vector<TransactionSourceEntry> sources;
  vector<TransactionDestinationEntry> destinations;
  fill_tx_sources_and_destinations(events, blk_head, from, to, amount, fee, nmix, sources, destinations);

  return constructTransaction(from.getAccountKeys(), sources, destinations, std::vector<uint8_t>(), tx, 0, logger);
}

Transaction construct_tx_with_fee(Logging::ILogger& logger, std::vector<test_event_entry>& events,
                                  const BlockTemplate& blk_head, const AccountBase& acc_from, const AccountBase& acc_to,
                                  uint64_t amount, uint64_t fee) {
  Transaction tx;
  construct_tx_to_key(logger, events, tx, blk_head, acc_from, acc_to, amount, fee, 0);
  events.push_back(tx);
  return tx;
}

uint64_t get_balance(const CryptoNote::AccountBase& addr, const std::vector<CryptoNote::BlockTemplate>& blockchain,
                     const map_hash2tx_t& mtx) {
  uint64_t res = 0;
  std::map<uint64_t, std::vector<output_index>> outs;
  std::map<uint64_t, std::vector<size_t>> outs_mine;

  map_hash2tx_t confirmed_txs;
  get_confirmed_txs(blockchain, mtx, confirmed_txs);

  if (!init_output_indices(outs, outs_mine, blockchain, confirmed_txs, addr))
    return false;

  if (!init_spent_output_indices(outs, outs_mine, blockchain, confirmed_txs, addr))
    return false;

  for (const auto& o : outs_mine) {
    for (size_t i = 0; i < o.second.size(); ++i) {
      if (outs[o.first][o.second[i]].spent)
        continue;

      res += outs[o.first][o.second[i]].amount;
    }
  }

  return res;
}

void get_confirmed_txs(const std::vector<CryptoNote::BlockTemplate>& blockchain, const map_hash2tx_t& mtx,
                       map_hash2tx_t& confirmed_txs) {
  std::unordered_set<Crypto::Hash> confirmed_hashes;
  for (const BlockTemplate& blk : blockchain) {
    for (const Crypto::Hash& tx_hash : blk.transactionHashes) {
      confirmed_hashes.insert(tx_hash);
    }
  }

  for (const auto& tx_pair : mtx) {
    if (0 != confirmed_hashes.count(tx_pair.first)) {
      confirmed_txs.insert(tx_pair);
    }
  }
}

bool find_block_chain(const std::vector<test_event_entry>& events, std::vector<CryptoNote::BlockTemplate>& blockchain,
                      map_hash2tx_t& mtx, const Crypto::Hash& head) {
  std::unordered_map<Crypto::Hash, BlockTemplate> block_index;
  for (const auto& ev : events) {
    if (typeid(RawBlock) == ev.type()) {
      auto blk = fromBinaryArray<BlockTemplate>(boost::get<RawBlock>(ev).block);
      if (!block_index.insert({CachedBlock(blk).getBlockHash(), blk}).second)
        throw std::runtime_error("find_block_chain error, rawblock");
      for (auto& txblob : boost::get<RawBlock>(ev).transactions) {
        auto tx = fromBinaryArray<Transaction>(txblob);
        mtx[getObjectHash(tx)] = tx;
      }
    } else if (typeid(BlockTemplate) == ev.type()) {
      auto blk = boost::get<BlockTemplate>(ev);
      if (!block_index.insert({CachedBlock(blk).getBlockHash(), blk}).second)
        throw std::runtime_error("find_block_chain error, template");
    } else if (typeid(Transaction) == ev.type()) {
      const Transaction& tx = boost::get<Transaction>(ev);
      mtx[getObjectHash(tx)] = tx;
    }
  }

  bool b_success = false;
  Crypto::Hash id = head;
  for (auto it = block_index.find(id); block_index.end() != it; it = block_index.find(id)) {
    blockchain.push_back(it->second);
    id = it->second.previousBlockHash;
    if (NULL_HASH == id) {
      b_success = true;
      break;
    }
  }
  std::reverse(blockchain.begin(), blockchain.end());

  return b_success;
}

const CryptoNote::Currency& test_chain_unit_base::currency() const {
  return *m_currency;
}

void test_chain_unit_base::register_callback(const std::string& cb_name, verify_callback cb) {
  m_callbacks[cb_name] = cb;
}

bool test_chain_unit_base::verify(const std::string& cb_name, CryptoNote::Core& c, size_t ev_index,
                                  const std::vector<test_event_entry>& events) {
  auto cb_it = m_callbacks.find(cb_name);
  if (cb_it == m_callbacks.end()) {
    LOG_ERROR("Failed to find callback " << cb_name);
    return false;
  }
  return cb_it->second(c, ev_index, events);
}
