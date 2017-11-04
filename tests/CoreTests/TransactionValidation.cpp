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

#include "TransactionValidation.h"
#include "TestGenerator.h"
#include "CryptoNoteCore/CryptoNoteTools.h"

using namespace CryptoNote;

namespace {
struct tx_builder {
  void step1_init(size_t version = CURRENT_TRANSACTION_VERSION, uint64_t unlock_time = 0) {
    m_tx.inputs.clear();
    m_tx.outputs.clear();
    m_tx.signatures.clear();

    m_tx.version = static_cast<uint8_t>(version);
    m_tx.unlockTime = unlock_time;

    m_tx_key = generateKeyPair();
    addTransactionPublicKeyToExtra(m_tx.extra, m_tx_key.publicKey);
  }

  void step2_fill_inputs(const AccountKeys& sender_account_keys, const std::vector<TransactionSourceEntry>& sources) {
    BOOST_FOREACH (const TransactionSourceEntry& src_entr, sources) {
      m_in_contexts.push_back(KeyPair());
      KeyPair& in_ephemeral = m_in_contexts.back();
      Crypto::KeyImage img;
      generate_key_image_helper(sender_account_keys, src_entr.realTransactionPublicKey,
                                src_entr.realOutputIndexInTransaction, in_ephemeral, img);

      // put key image into tx input
      KeyInput input_to_key;
      input_to_key.amount = src_entr.amount;
      input_to_key.keyImage = img;

      // fill outputs array and use relative offsets
      BOOST_FOREACH (const TransactionSourceEntry::OutputEntry& out_entry, src_entr.outputs)
        input_to_key.outputIndexes.push_back(out_entry.first);

      input_to_key.outputIndexes = absolute_output_offsets_to_relative(input_to_key.outputIndexes);
      m_tx.inputs.push_back(input_to_key);
    }
  }

  void step3_fill_outputs(const std::vector<TransactionDestinationEntry>& destinations) {
    size_t output_index = 0;
    BOOST_FOREACH (const TransactionDestinationEntry& dst_entr, destinations) {
      Crypto::KeyDerivation derivation;
      Crypto::PublicKey out_eph_public_key;
      Crypto::generate_key_derivation(dst_entr.addr.viewPublicKey, m_tx_key.secretKey, derivation);
      Crypto::derive_public_key(derivation, output_index, dst_entr.addr.spendPublicKey, out_eph_public_key);

      TransactionOutput out;
      out.amount = dst_entr.amount;
      KeyOutput tk;
      tk.key = out_eph_public_key;
      out.target = tk;
      m_tx.outputs.push_back(out);
      output_index++;
    }
  }

  void step4_calc_hash() {
    getObjectHash(*static_cast<TransactionPrefix*>(&m_tx), m_tx_prefix_hash);
  }

  void step5_sign(const std::vector<TransactionSourceEntry>& sources) {
    m_tx.signatures.clear();

    size_t i = 0;
    BOOST_FOREACH (const TransactionSourceEntry& src_entr, sources) {
      std::vector<const Crypto::PublicKey*> keys_ptrs;
      BOOST_FOREACH (const TransactionSourceEntry::OutputEntry& o, src_entr.outputs) { keys_ptrs.push_back(&o.second); }

      m_tx.signatures.push_back(std::vector<Crypto::Signature>());
      std::vector<Crypto::Signature>& sigs = m_tx.signatures.back();
      sigs.resize(src_entr.outputs.size());
      generate_ring_signature(m_tx_prefix_hash, boost::get<KeyInput>(m_tx.inputs[i]).keyImage, keys_ptrs,
                              m_in_contexts[i].secretKey, src_entr.realOutput, sigs.data());
      i++;
    }
  }

  Transaction m_tx;
  KeyPair m_tx_key;
  std::vector<KeyPair> m_in_contexts;
  Crypto::Hash m_tx_prefix_hash;
};

Transaction make_simple_tx_with_unlock_time(const std::vector<test_event_entry>& events,
                                            const CryptoNote::BlockTemplate& blk_head,
                                            const CryptoNote::AccountBase& from, const CryptoNote::AccountBase& to,
                                            uint64_t amount, uint64_t fee, uint64_t unlock_time) {
  std::vector<TransactionSourceEntry> sources;
  std::vector<TransactionDestinationEntry> destinations;
  fill_tx_sources_and_destinations(events, blk_head, from, to, amount, fee, 0, sources, destinations);

  tx_builder builder;
  builder.step1_init(CURRENT_TRANSACTION_VERSION, unlock_time);
  builder.step2_fill_inputs(from.getAccountKeys(), sources);
  builder.step3_fill_outputs(destinations);
  builder.step4_calc_hash();
  builder.step5_sign(sources);
  return builder.m_tx;
};

Crypto::PublicKey generate_invalid_pub_key() {
  for (int i = 0; i <= 0xFF; ++i) {
    Crypto::PublicKey key;
    memset(&key, i, sizeof(Crypto::PublicKey));
    if (!Crypto::check_key(key)) {
      return key;
    }
  }

  throw std::runtime_error("invalid public key wasn't found");
  return Crypto::PublicKey();
}
}

//----------------------------------------------------------------------------------------------------------------------
// Tests

bool gen_tx_big_version::generate(std::vector<test_event_entry>& events) const {
  /* uint64_t ts_start = 1338224400; */

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  MAKE_NEXT_BLOCK(events, blk_0r, blk_0, miner_account);
  REWIND_BLOCKS(events, blk_0rr, blk_0r, miner_account);

  std::vector<TransactionSourceEntry> sources;
  std::vector<TransactionDestinationEntry> destinations;
  fill_tx_sources_and_destinations(events, blk_0r, miner_account, miner_account, MK_COINS(1), m_currency->minimumFee(),
                                   0, sources, destinations);

  tx_builder builder;
  builder.step1_init(CURRENT_TRANSACTION_VERSION + 1, 0);
  builder.step2_fill_inputs(miner_account.getAccountKeys(), sources);
  builder.step3_fill_outputs(destinations);
  builder.step4_calc_hash();
  builder.step5_sign(sources);

  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(builder.m_tx);

  return true;
}

bool gen_tx_unlock_time::generate(std::vector<test_event_entry>& events) const {
  uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  REWIND_BLOCKS_N(events, blk_1, blk_0, miner_account, 10);
  REWIND_BLOCKS(events, blk_1r, blk_1, miner_account);

  auto make_tx_with_unlock_time = [&](uint64_t unlock_time) -> Transaction {
    return make_simple_tx_with_unlock_time(events, blk_1, miner_account, miner_account, MK_COINS(1),
                                           m_currency->minimumFee(), unlock_time);
  };

  std::list<Transaction> txs_0;

  auto last = std::prev(std::end(events), 1);
  txs_0.push_back(make_tx_with_unlock_time(0));
  events.push_back(txs_0.back());

  txs_0.push_back(make_tx_with_unlock_time(CachedBlock(blk_1r).getBlockIndex() - 1));
  events.push_back(txs_0.back());

  txs_0.push_back(make_tx_with_unlock_time(CachedBlock(blk_1r).getBlockIndex()));
  events.push_back(txs_0.back());

  txs_0.push_back(make_tx_with_unlock_time(CachedBlock(blk_1r).getBlockIndex() + 1));
  events.push_back(txs_0.back());

  txs_0.push_back(make_tx_with_unlock_time(CachedBlock(blk_1r).getBlockIndex() + 2));
  events.push_back(txs_0.back());

  txs_0.push_back(make_tx_with_unlock_time(ts_start - 1));
  events.push_back(txs_0.back());

  txs_0.push_back(make_tx_with_unlock_time(time(0) + 60 * 60));
  events.push_back(txs_0.back());
  events.erase(std::next(last), std::end(events));

  MAKE_NEXT_BLOCK_TX_LIST(events, blk_2, blk_1r, miner_account, txs_0);

  return true;
}

bool gen_tx_no_inputs_no_outputs::generate(std::vector<test_event_entry>& events) const {
  //uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);

  tx_builder builder;
  builder.step1_init();

  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(builder.m_tx);

  return true;
}

bool gen_tx_no_inputs_has_outputs::generate(std::vector<test_event_entry>& events) const {
  //uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  MAKE_NEXT_BLOCK(events, blk_1, blk_0, miner_account);

  std::vector<TransactionSourceEntry> sources;
  std::vector<TransactionDestinationEntry> destinations;
  fill_tx_sources_and_destinations(events, blk_1, miner_account, miner_account, MK_COINS(1), m_currency->minimumFee(),
                                   0, sources, destinations);

  tx_builder builder;
  builder.step1_init();
  builder.step3_fill_outputs(destinations);

  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(builder.m_tx);

  return true;
}

bool gen_tx_has_inputs_no_outputs::generate(std::vector<test_event_entry>& events) const {
  //uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  MAKE_NEXT_BLOCK(events, blk_1, blk_0, miner_account);
  REWIND_BLOCKS(events, blk_0r, blk_1, miner_account);

  std::vector<TransactionSourceEntry> sources;
  std::vector<TransactionDestinationEntry> destinations;
  fill_tx_sources_and_destinations(events, blk_1, miner_account, miner_account, MK_COINS(1), m_currency->minimumFee(),
                                   0, sources, destinations);
  destinations.clear();

  tx_builder builder;
  builder.step1_init();
  builder.step2_fill_inputs(miner_account.getAccountKeys(), sources);
  builder.step3_fill_outputs(destinations);
  builder.step4_calc_hash();
  builder.step5_sign(sources);

  events.push_back(builder.m_tx);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_0r, miner_account, builder.m_tx);

  return true;
}

bool gen_tx_invalid_input_amount::generate(std::vector<test_event_entry>& events) const {
  //uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  MAKE_NEXT_BLOCK(events, blk_1, blk_0, miner_account);
  REWIND_BLOCKS(events, blk_0r, blk_1, miner_account);

  std::vector<TransactionSourceEntry> sources;
  std::vector<TransactionDestinationEntry> destinations;
  fill_tx_sources_and_destinations(events, blk_1, miner_account, miner_account, MK_COINS(1), m_currency->minimumFee(),
                                   0, sources, destinations);
  sources.front().amount++;

  tx_builder builder;
  builder.step1_init();
  builder.step2_fill_inputs(miner_account.getAccountKeys(), sources);
  builder.step3_fill_outputs(destinations);
  builder.step4_calc_hash();
  builder.step5_sign(sources);

  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(builder.m_tx);

  return true;
}

bool gen_tx_in_to_key_wo_key_offsets::generate(std::vector<test_event_entry>& events) const {
  //uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  MAKE_NEXT_BLOCK(events, blk_1, blk_0, miner_account);
  REWIND_BLOCKS(events, blk_0r, blk_1, miner_account);

  std::vector<TransactionSourceEntry> sources;
  std::vector<TransactionDestinationEntry> destinations;
  fill_tx_sources_and_destinations(events, blk_1, miner_account, miner_account, MK_COINS(1), m_currency->minimumFee(),
                                   0, sources, destinations);

  tx_builder builder;
  builder.step1_init();
  builder.step2_fill_inputs(miner_account.getAccountKeys(), sources);
  builder.step3_fill_outputs(destinations);
  KeyInput& in_to_key = boost::get<KeyInput>(builder.m_tx.inputs.front());
  uint32_t key_offset = in_to_key.outputIndexes.front();
  in_to_key.outputIndexes.pop_back();
  CHECK_AND_ASSERT_MES(in_to_key.outputIndexes.empty(), false, "txin contained more than one key_offset");
  builder.step4_calc_hash();
  in_to_key.outputIndexes.push_back(key_offset);
  builder.step5_sign(sources);
  in_to_key.outputIndexes.pop_back();

  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(builder.m_tx);

  return true;
}

bool gen_tx_key_offest_points_to_foreign_key::generate(std::vector<test_event_entry>& events) const {
  //uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  MAKE_NEXT_BLOCK(events, blk_1, blk_0, miner_account);
  REWIND_BLOCKS(events, blk_1r, blk_1, miner_account);
  MAKE_ACCOUNT(events, alice_account);
  MAKE_ACCOUNT(events, bob_account);
  MAKE_TX_LIST_START(events, txs_0, miner_account, bob_account, MK_COINS(60) + 1, blk_1);
  MAKE_TX_LIST(events, txs_0, miner_account, alice_account, MK_COINS(60) + 1, blk_1);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_2, blk_1r, miner_account, txs_0);

  std::vector<TransactionSourceEntry> sources_bob;
  std::vector<TransactionDestinationEntry> destinations_bob;
  fill_tx_sources_and_destinations(events, blk_2, bob_account, miner_account,
                                   MK_COINS(60) + 1 - m_currency->minimumFee(), m_currency->minimumFee(), 0,
                                   sources_bob, destinations_bob);

  std::vector<TransactionSourceEntry> sources_alice;
  std::vector<TransactionDestinationEntry> destinations_alice;
  fill_tx_sources_and_destinations(events, blk_2, alice_account, miner_account,
                                   MK_COINS(60) + 1 - m_currency->minimumFee(), m_currency->minimumFee(), 0,
                                   sources_alice, destinations_alice);

  tx_builder builder;
  builder.step1_init();
  builder.step2_fill_inputs(bob_account.getAccountKeys(), sources_bob);
  KeyInput& in_to_key = boost::get<KeyInput>(builder.m_tx.inputs.front());
  in_to_key.outputIndexes.front() = sources_alice.front().outputs.front().first;
  builder.step3_fill_outputs(destinations_bob);
  builder.step4_calc_hash();
  builder.step5_sign(sources_bob);

  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(builder.m_tx);

  return true;
}

bool gen_tx_sender_key_offest_not_exist::generate(std::vector<test_event_entry>& events) const {
  //uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  MAKE_NEXT_BLOCK(events, blk_1, blk_0, miner_account);
  REWIND_BLOCKS(events, blk_0r, blk_1, miner_account);

  std::vector<TransactionSourceEntry> sources;
  std::vector<TransactionDestinationEntry> destinations;
  fill_tx_sources_and_destinations(events, blk_1, miner_account, miner_account, MK_COINS(1), m_currency->minimumFee(),
                                   0, sources, destinations);

  tx_builder builder;
  builder.step1_init();
  builder.step2_fill_inputs(miner_account.getAccountKeys(), sources);
  KeyInput& in_to_key = boost::get<KeyInput>(builder.m_tx.inputs.front());
  in_to_key.outputIndexes.front() = std::numeric_limits<uint32_t>::max();
  builder.step3_fill_outputs(destinations);
  builder.step4_calc_hash();
  builder.step5_sign(sources);

  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(builder.m_tx);

  return true;
}

bool gen_tx_mixed_key_offest_not_exist::generate(std::vector<test_event_entry>& events) const {
  //uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  MAKE_NEXT_BLOCK(events, blk_1, blk_0, miner_account);
  REWIND_BLOCKS(events, blk_1r, blk_1, miner_account);
  MAKE_ACCOUNT(events, alice_account);
  MAKE_ACCOUNT(events, bob_account);
  MAKE_TX_LIST_START(events, txs_0, miner_account, bob_account, MK_COINS(1) + m_currency->minimumFee(), blk_1);
  MAKE_TX_LIST(events, txs_0, miner_account, alice_account, MK_COINS(1) + m_currency->minimumFee(), blk_1);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_2, blk_1r, miner_account, txs_0);

  std::vector<TransactionSourceEntry> sources;
  std::vector<TransactionDestinationEntry> destinations;
  fill_tx_sources_and_destinations(events, blk_2, bob_account, miner_account, MK_COINS(1), m_currency->minimumFee(), 1,
                                   sources, destinations);

  sources.front().outputs[(sources.front().realOutput + 1) % 2].first = std::numeric_limits<uint32_t>::max();

  tx_builder builder;
  builder.step1_init();
  builder.step2_fill_inputs(bob_account.getAccountKeys(), sources);
  builder.step3_fill_outputs(destinations);
  builder.step4_calc_hash();
  builder.step5_sign(sources);

  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(builder.m_tx);

  return true;
}

bool gen_tx_key_image_not_derive_from_tx_key::generate(std::vector<test_event_entry>& events) const {
  //uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  MAKE_NEXT_BLOCK(events, blk_0f, blk_0, miner_account);
  REWIND_BLOCKS(events, blk_0r, blk_0f, miner_account);

  std::vector<TransactionSourceEntry> sources;
  std::vector<TransactionDestinationEntry> destinations;
  fill_tx_sources_and_destinations(events, blk_0f, miner_account, miner_account, MK_COINS(1), m_currency->minimumFee(),
                                   0, sources, destinations);

  tx_builder builder;
  builder.step1_init();
  builder.step2_fill_inputs(miner_account.getAccountKeys(), sources);

  KeyInput& in_to_key = boost::get<KeyInput>(builder.m_tx.inputs.front());
  KeyPair kp = generateKeyPair();
  Crypto::KeyImage another_ki;
  Crypto::generate_key_image(kp.publicKey, kp.secretKey, another_ki);
  in_to_key.keyImage = another_ki;

  builder.step3_fill_outputs(destinations);
  builder.step4_calc_hash();

  // Tx with invalid key image can't be subscribed, so create empty signature
  builder.m_tx.signatures.resize(1);
  builder.m_tx.signatures[0].resize(1);
  builder.m_tx.signatures[0][0] = boost::value_initialized<Crypto::Signature>();

  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(builder.m_tx);

  return true;
}

bool gen_tx_key_image_is_invalid::generate(std::vector<test_event_entry>& events) const {
  //uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  MAKE_NEXT_BLOCK(events, blk_0f, blk_0, miner_account);
  REWIND_BLOCKS(events, blk_0r, blk_0f, miner_account);

  std::vector<TransactionSourceEntry> sources;
  std::vector<TransactionDestinationEntry> destinations;
  fill_tx_sources_and_destinations(events, blk_0f, miner_account, miner_account, MK_COINS(1), m_currency->minimumFee(),
                                   0, sources, destinations);

  tx_builder builder;
  builder.step1_init();
  builder.step2_fill_inputs(miner_account.getAccountKeys(), sources);

  KeyInput& in_to_key = boost::get<KeyInput>(builder.m_tx.inputs.front());
  Crypto::PublicKey pub = generate_invalid_pub_key();
  memcpy(&in_to_key.keyImage, &pub, sizeof(Crypto::EllipticCurvePoint));

  builder.step3_fill_outputs(destinations);
  builder.step4_calc_hash();

  // Tx with invalid key image can't be subscribed, so create empty signature
  builder.m_tx.signatures.resize(1);
  builder.m_tx.signatures[0].resize(1);
  builder.m_tx.signatures[0][0] = boost::value_initialized<Crypto::Signature>();

  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(builder.m_tx);

  return true;
}

bool gen_tx_check_input_unlock_time::generate(std::vector<test_event_entry>& events) const {
  static const size_t tests_count = 6;

  //uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  REWIND_BLOCKS_N(events, blk_1, blk_0, miner_account, tests_count - 1);
  REWIND_BLOCKS(events, blk_1r, blk_1, miner_account);

  std::array<AccountBase, tests_count> accounts;
  for (size_t i = 0; i < tests_count; ++i) {
    MAKE_ACCOUNT(events, acc);
    accounts[i] = acc;
  }

  std::list<Transaction> txs_0;
  auto make_tx_to_acc = [&](size_t acc_idx, uint64_t unlock_time) {
    txs_0.push_back(make_simple_tx_with_unlock_time(events, blk_1, miner_account, accounts[acc_idx],
                                                    MK_COINS(1) + m_currency->minimumFee(), m_currency->minimumFee(),
                                                    unlock_time));
    events.push_back(txs_0.back());
  };

  uint64_t blk_3_height = CachedBlock(blk_1r).getBlockIndex() + 2;
  make_tx_to_acc(0, 0);
  make_tx_to_acc(1, blk_3_height - 1);
  make_tx_to_acc(2, blk_3_height);
  make_tx_to_acc(3, blk_3_height + 1);
  make_tx_to_acc(4, time(0) - 1);
  make_tx_to_acc(5, time(0) + 60 * 60);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_2, blk_1r, miner_account, txs_0);

  std::list<Transaction> txs_1;
  auto make_tx_from_acc = [&](size_t acc_idx, bool invalid) {
    Transaction tx = make_simple_tx_with_unlock_time(events, blk_2, accounts[acc_idx], miner_account, MK_COINS(1),
                                                     m_currency->minimumFee(), 0);
    if (invalid) {
      DO_CALLBACK(events, "mark_invalid_tx");
    } else {
      txs_1.push_back(tx);
    }
    events.push_back(tx);
  };

  make_tx_from_acc(0, false);
  make_tx_from_acc(1, false);
  make_tx_from_acc(2, false);
  make_tx_from_acc(3, true);
  make_tx_from_acc(4, false);
  make_tx_from_acc(5, true);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_3, blk_2, miner_account, txs_1);

  return true;
}

bool gen_tx_txout_to_key_has_invalid_key::generate(std::vector<test_event_entry>& events) const {
  //uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  MAKE_NEXT_BLOCK(events, blk_0f, blk_0, miner_account);
  REWIND_BLOCKS(events, blk_0r, blk_0f, miner_account);

  std::vector<TransactionSourceEntry> sources;
  std::vector<TransactionDestinationEntry> destinations;
  fill_tx_sources_and_destinations(events, blk_0f, miner_account, miner_account, MK_COINS(1), m_currency->minimumFee(),
                                   0, sources, destinations);

  tx_builder builder;
  builder.step1_init();
  builder.step2_fill_inputs(miner_account.getAccountKeys(), sources);
  builder.step3_fill_outputs(destinations);

  KeyOutput& out_to_key = boost::get<KeyOutput>(builder.m_tx.outputs.front().target);
  out_to_key.key = generate_invalid_pub_key();

  builder.step4_calc_hash();
  builder.step5_sign(sources);

  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(builder.m_tx);

  return true;
}

bool gen_tx_output_with_zero_amount::generate(std::vector<test_event_entry>& events) const {
  //uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  MAKE_NEXT_BLOCK(events, blk_0f, blk_0, miner_account);
  REWIND_BLOCKS(events, blk_0r, blk_0f, miner_account);

  std::vector<TransactionSourceEntry> sources;
  std::vector<TransactionDestinationEntry> destinations;
  fill_tx_sources_and_destinations(events, blk_0f, miner_account, miner_account, MK_COINS(1), m_currency->minimumFee(),
                                   0, sources, destinations);

  tx_builder builder;
  builder.step1_init();
  builder.step2_fill_inputs(miner_account.getAccountKeys(), sources);
  builder.step3_fill_outputs(destinations);

  builder.m_tx.outputs.front().amount = 0;

  builder.step4_calc_hash();
  builder.step5_sign(sources);

  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(builder.m_tx);

  return true;
}

bool gen_tx_signatures_are_invalid::generate(std::vector<test_event_entry>& events) const {
  //uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  MAKE_NEXT_BLOCK(events, blk_1, blk_0, miner_account);
  REWIND_BLOCKS(events, blk_1r, blk_1, miner_account);
  MAKE_ACCOUNT(events, alice_account);
  MAKE_ACCOUNT(events, bob_account);
  MAKE_TX_LIST_START(events, txs_0, miner_account, bob_account, MK_COINS(1) + m_currency->minimumFee(), blk_1);
  MAKE_TX_LIST(events, txs_0, miner_account, alice_account, MK_COINS(1) + m_currency->minimumFee(), blk_1);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_2, blk_1r, miner_account, txs_0);

  MAKE_TX(events, tx_0, miner_account, miner_account, MK_COINS(60), blk_2);
  events.pop_back();

  MAKE_TX_MIX(events, tx_1, bob_account, miner_account, MK_COINS(1), 1, blk_2);
  events.pop_back();

  // Tx with nmix = 0 without signatures
  DO_CALLBACK(events, "mark_invalid_tx");
  BinaryArray sr_tx = toBinaryArray(static_cast<TransactionPrefix>(tx_0));
  events.push_back(serialized_transaction(sr_tx));

  // Tx with nmix = 0 have a few inputs, and not enough signatures
  DO_CALLBACK(events, "mark_invalid_tx");
  sr_tx = toBinaryArray(tx_0);
  sr_tx.resize(sr_tx.size() - sizeof(Crypto::Signature));
  events.push_back(serialized_transaction(sr_tx));

  // Tx with nmix = 0 have a few inputs, and too many signatures
  DO_CALLBACK(events, "mark_invalid_tx");
  sr_tx = toBinaryArray(tx_0);
  sr_tx.insert(sr_tx.end(), sr_tx.end() - sizeof(Crypto::Signature), sr_tx.end());
  events.push_back(serialized_transaction(sr_tx));

  // Tx with nmix = 1 without signatures
  DO_CALLBACK(events, "mark_invalid_tx");
  sr_tx = toBinaryArray(static_cast<TransactionPrefix>(tx_1));
  events.push_back(serialized_transaction(sr_tx));

  // Tx with nmix = 1 have not enough signatures
  DO_CALLBACK(events, "mark_invalid_tx");
  sr_tx = toBinaryArray(tx_1);
  sr_tx.resize(sr_tx.size() - sizeof(Crypto::Signature));
  events.push_back(serialized_transaction(sr_tx));

  // Tx with nmix = 1 have too many signatures
  DO_CALLBACK(events, "mark_invalid_tx");
  sr_tx = toBinaryArray(tx_1);
  sr_tx.insert(sr_tx.end(), sr_tx.end() - sizeof(Crypto::Signature), sr_tx.end());
  events.push_back(serialized_transaction(sr_tx));

  return true;
}

bool GenerateTransactionWithZeroFee::generate(std::vector<test_event_entry>& events) const {
  //uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(alice_account);
  GENERATE_ACCOUNT(bob_account);
  MAKE_GENESIS_BLOCK(events, blk_0, alice_account, ts_start);
  MAKE_NEXT_BLOCK(events, blk_1, blk_0, alice_account);
  REWIND_BLOCKS(events, blk_1r, blk_1, alice_account);

  CryptoNote::Transaction tx;
  construct_tx_to_key(m_logger, events, tx, blk_1, alice_account, bob_account, MK_COINS(1), 0, 0);

  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(tx);

  return true;
}

MultiSigTx_OutputSignatures::MultiSigTx_OutputSignatures(size_t givenKeys, uint32_t requiredSignatures, bool shouldSucceed) :
  m_givenKeys(givenKeys), m_requiredSignatures(requiredSignatures), m_shouldSucceed(shouldSucceed) {

  for (size_t i = 0; i < m_givenKeys; ++i) {
    AccountBase acc;
    acc.generate();
    m_outputAccounts.push_back(acc);
  }
}

bool MultiSigTx_OutputSignatures::generate(std::vector<test_event_entry>& events) const {
  TestGenerator generator(*m_currency, events);
  return generate(generator);
}

bool MultiSigTx_OutputSignatures::generate(TestGenerator& generator) const {

  generator.generateBlocks(m_currency->minedMoneyUnlockWindow());

  std::vector<TransactionSourceEntry> sources;
  std::vector<TransactionDestinationEntry> destinations;
  fill_tx_sources_and_destinations(generator.events, generator.lastBlock, generator.minerAccount,
                                   generator.minerAccount, MK_COINS(1), m_currency->minimumFee(), 0, sources,
                                   destinations);

  tx_builder builder;
  builder.step1_init();
  builder.step2_fill_inputs(generator.minerAccount.getAccountKeys(), sources);

  MultisignatureOutput target;

  for (const auto& acc : m_outputAccounts) {
    target.keys.push_back(acc.getAccountKeys().address.spendPublicKey);
  }
  target.requiredSignatureCount = m_requiredSignatures;
  TransactionOutput txOut = {MK_COINS(1), target};
  builder.m_tx.outputs.push_back(txOut);

  builder.step4_calc_hash();
  builder.step5_sign(sources);

  if (!m_shouldSucceed) {
    generator.addCallback("mark_invalid_tx");
  }

  generator.addEvent(builder.m_tx);

  if (!m_shouldSucceed) {
    generator.addCallback("mark_invalid_block");
  }

  generator.makeNextBlock(builder.m_tx);

  return true;
}

bool MultiSigTx_InvalidOutputSignature::generate(std::vector<test_event_entry>& events) const {
  //uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  MAKE_NEXT_BLOCK(events, blk_0f, blk_0, miner_account);
  REWIND_BLOCKS(events, blk_0r, blk_0f, miner_account);

  std::vector<TransactionSourceEntry> sources;
  std::vector<TransactionDestinationEntry> destinations;
  fill_tx_sources_and_destinations(events, blk_0f, miner_account, miner_account, MK_COINS(1), m_currency->minimumFee(),
                                   0, sources, destinations);

  tx_builder builder;
  builder.step1_init();
  builder.step2_fill_inputs(miner_account.getAccountKeys(), sources);

  MultisignatureOutput target;

  Crypto::PublicKey pk;
  Crypto::SecretKey sk;
  Crypto::generate_keys(pk, sk);

  // fill with 1 valid key
  target.keys.push_back(pk);
  // and 1 invalid
  target.keys.push_back(generate_invalid_pub_key());

  target.requiredSignatureCount = 2;

  TransactionOutput txOut = {MK_COINS(1), target};
  builder.m_tx.outputs.push_back(txOut);

  builder.step4_calc_hash();
  builder.step5_sign(sources);

  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(builder.m_tx);

  return true;
}

namespace {
void fillMultisignatureInput(TestGenerator& generator, tx_builder& builder, uint64_t inputAmount,
                             uint32_t givenSignatures) {

  builder.step1_init();

  // create input
  MultisignatureInput input;
  input.amount = inputAmount;
  input.signatureCount = givenSignatures;
  input.outputIndex = 0;
  builder.m_tx.inputs.push_back(input);

  // create output
  std::vector<TransactionDestinationEntry> destinations;
  destinations.emplace_back(inputAmount - generator.currency().minimumFee(),
                            generator.minerAccount.getAccountKeys().address);
  builder.step3_fill_outputs(destinations);

  // calc hash
  builder.step4_calc_hash();
}
}

MultiSigTx_Input::MultiSigTx_Input(size_t givenKeys, uint32_t requiredSignatures, uint32_t givenSignatures,
                                   bool inputShouldSucceed)
    : MultiSigTx_OutputSignatures(givenKeys, requiredSignatures, true), m_givenSignatures(givenSignatures),
      m_inputShouldSucceed(inputShouldSucceed) {
}

bool MultiSigTx_Input::generate(std::vector<test_event_entry>& events) const {

  TestGenerator generator(*m_currency, events);

  // create outputs
  MultiSigTx_OutputSignatures::generate(generator);

  tx_builder builder;
  fillMultisignatureInput(generator, builder, MK_COINS(1), m_givenSignatures);

  // calc signatures
  builder.m_tx.signatures.resize(builder.m_tx.signatures.size() + 1);
  auto& outsigs = builder.m_tx.signatures.back();

  for (size_t i = 0; i < m_givenSignatures; ++i) {
    const auto& pk = m_outputAccounts[i].getAccountKeys().address.spendPublicKey;
    const auto& sk = m_outputAccounts[i].getAccountKeys().spendSecretKey;

    Crypto::Signature sig;
    Crypto::generate_signature(builder.m_tx_prefix_hash, pk, sk, sig);
    outsigs.push_back(sig);
  }

  if (!m_inputShouldSucceed) {
    generator.addCallback("mark_invalid_tx");
  }

  generator.addEvent(builder.m_tx);
  return true;
}

MultiSigTx_BadInputSignature::MultiSigTx_BadInputSignature() : MultiSigTx_OutputSignatures(1, 1, true) {
}

bool MultiSigTx_BadInputSignature::generate(std::vector<test_event_entry>& events) const {

  TestGenerator generator(*m_currency, events);

  // create outputs
  MultiSigTx_OutputSignatures::generate(generator);

  tx_builder builder;
  fillMultisignatureInput(generator, builder, MK_COINS(1), 1);

  // calc signatures
  builder.m_tx.signatures.resize(builder.m_tx.signatures.size() + 1);
  auto& outsigs = builder.m_tx.signatures.back();

  const auto& pk = m_outputAccounts[0].getAccountKeys().address.spendPublicKey;
  const auto& sk = m_outputAccounts[0].getAccountKeys().spendSecretKey;

  // modify the transaction prefix hash
  Crypto::Hash badHash = builder.m_tx_prefix_hash;
  *reinterpret_cast<uint16_t*>(&badHash) = 0xdead;

  // sign the hash
  Crypto::Signature sig;
  Crypto::generate_signature(badHash, pk, sk, sig);
  outsigs.push_back(sig);

  // transaction with bad signature should be rejected
  generator.addCallback("mark_invalid_tx");
  generator.addEvent(builder.m_tx);

  // blocks with transaction with bad signature should be rejected
  generator.addCallback("mark_invalid_block");
  generator.makeNextBlock(builder.m_tx);

  return true;
}
