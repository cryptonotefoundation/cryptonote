// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
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

#include "IntegerOverflow.h"
#include "CryptoNoteCore/CryptoNoteSerialization.h"
#include "Wallet/TransactionBuilder.h"

using namespace CryptoNote;

namespace
{
  void split_miner_tx_outs(Transaction& miner_tx, uint64_t amount_1)
  {
    uint64_t total_amount = get_outs_money_amount(miner_tx);
    uint64_t amount_2 = total_amount - amount_1;
    TransactionOutputTarget target = miner_tx.outputs[0].target;

    miner_tx.outputs.clear();

    TransactionOutput out1;
    out1.amount = amount_1;
    out1.target = target;
    miner_tx.outputs.push_back(out1);

    TransactionOutput out2;
    out2.amount = amount_2;
    out2.target = target;
    miner_tx.outputs.push_back(out2);
  }

  void append_TxBuildInput(std::vector<CryptoNote::TxBuildInput>& sources, const Transaction& tx, size_t out_idx, const AccountKeys& senderKeys)
  {
    CryptoNote::TxBuildInput se;
    se.keyInfo.amount = tx.outputs[out_idx].amount;
    TransactionTypes::GlobalOutput go; go.outputIndex = 0; go.targetKey = boost::get<CryptoNote::KeyOutput>(tx.outputs[out_idx].target).key;
    se.keyInfo.outputs.push_back(go);
    se.keyInfo.realOutput.transactionIndex = 0;
    se.keyInfo.realOutput.transactionPublicKey = getTransactionPublicKeyFromExtra(tx.extra);
    se.keyInfo.realOutput.outputInTransaction = static_cast<uint32_t>(out_idx);
    se.senderKeys = senderKeys;

    sources.push_back(se);
  }
}

//======================================================================================================================

gen_uint_overflow_base::gen_uint_overflow_base()
  : m_last_valid_block_event_idx(static_cast<size_t>(-1))
{
  REGISTER_CALLBACK_METHOD(gen_uint_overflow_1, mark_last_valid_block);
}

bool gen_uint_overflow_base::check_tx_verification_context(const CryptoNote::tx_verification_context& tvc, bool tx_added, size_t event_idx, const CryptoNote::Transaction& /*tx*/)
{
  return m_last_valid_block_event_idx < event_idx ? !tx_added && tvc.m_verification_failed : tx_added && !tvc.m_verification_failed;
}

bool gen_uint_overflow_base::check_block_verification_context(const CryptoNote::block_verification_context& bvc, size_t event_idx, const CryptoNote::Block& /*block*/)
{
  return m_last_valid_block_event_idx < event_idx ? bvc.m_verification_failed | bvc.m_marked_as_orphaned : !bvc.m_verification_failed;
}

bool gen_uint_overflow_base::mark_last_valid_block(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  m_last_valid_block_event_idx = ev_index - 1;
  return true;
}

//======================================================================================================================

bool gen_uint_overflow_1::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  DO_CALLBACK(events, "mark_last_valid_block");
  MAKE_ACCOUNT(events, bob_account);
  MAKE_ACCOUNT(events, alice_account);

  // Problem 1. Miner tx output overflow
  MAKE_MINER_TX_MANUALLY(miner_tx_0, blk_0);
  split_miner_tx_outs(miner_tx_0, m_currency.moneySupply());
  Block blk_1;
  if (!generator.constructBlockManually(blk_1, blk_0, miner_account, test_generator::bf_miner_tx, 0, 0, 0, Crypto::Hash(), 0, miner_tx_0))
    return false;
  events.push_back(blk_1);

  // Problem 1. Miner tx outputs overflow
  MAKE_MINER_TX_MANUALLY(miner_tx_1, blk_1);
  split_miner_tx_outs(miner_tx_1, m_currency.moneySupply());
  Block blk_2;
  if (!generator.constructBlockManually(blk_2, blk_1, miner_account, test_generator::bf_miner_tx, 0, 0, 0, Crypto::Hash(), 0, miner_tx_1))
    return false;
  events.push_back(blk_2);

  return true;
}

//======================================================================================================================

bool gen_uint_overflow_2::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  MAKE_ACCOUNT(events, bob_account);
  MAKE_ACCOUNT(events, alice_account);
  REWIND_BLOCKS(events, blk_0r, blk_0, miner_account);
  DO_CALLBACK(events, "mark_last_valid_block");

  // Problem 1. Regular tx outputs overflow
  std::vector<CryptoNote::TxBuildInput> sources;
  for (size_t i = 0; i < blk_0.baseTransaction.outputs.size(); ++i)
  {
    if (m_currency.minimumFee() < blk_0.baseTransaction.outputs[i].amount)
    {
      append_TxBuildInput(sources, blk_0.baseTransaction, i, miner_account.getAccountKeys());
      break;
    }
  }
  if (sources.empty())
  {
    return false;
  }

  std::vector<CryptoNote::TxBuildOutput> destinations;
  const AccountPublicAddress& bob_addr = bob_account.getAccountKeys().address;
  destinations.push_back(TxBuildOutput{bob_addr, m_currency.moneySupply()});
  destinations.push_back(TxBuildOutput{bob_addr, m_currency.moneySupply() - 1});
  // sources.front().keyInfo.amount = destinations[0].amount + destinations[2].amount + destinations[3].amount + m_currency.minimumFee()
  destinations.push_back(TxBuildOutput{bob_addr, sources.front().keyInfo.amount - m_currency.moneySupply() - m_currency.moneySupply() + 1 - m_currency.minimumFee()});

  CryptoNote::Transaction tx_1;
  try {
    Crypto::SecretKey txkey;
    auto itx = buildTransaction(sources, destinations, miner_account.getAccountKeys().viewSecretKey, {}, 0, 0, txkey);
    if (!fromBinaryArray(tx_1, itx->getTransactionData())) return false;
  } catch (...) { return false; }
  events.push_back(tx_1);

  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0r, miner_account, tx_1);
  REWIND_BLOCKS(events, blk_1r, blk_1, miner_account);

  // Problem 2. Regular tx inputs overflow
  sources.clear();
  for (size_t i = 0; i < tx_1.outputs.size(); ++i)
  {
    auto& tx_1_out = tx_1.outputs[i];
    if (tx_1_out.amount < m_currency.moneySupply() - 1)
      continue;

    append_TxBuildInput(sources, tx_1, i, bob_account.getAccountKeys());
  }

  destinations.clear();
  CryptoNote::TxBuildOutput de;
  de.destination = alice_account.getAccountKeys().address;
  de.amount = m_currency.moneySupply() - m_currency.minimumFee();
  destinations.push_back(de);
  destinations.push_back(de);

  CryptoNote::Transaction tx_2;
  try {
    Crypto::SecretKey txkey;
    auto itx = buildTransaction(sources, destinations, bob_account.getAccountKeys().viewSecretKey, {}, 0, 0, txkey);
    if (!fromBinaryArray(tx_2, itx->getTransactionData())) return false;
  } catch (...) { return false; }
  events.push_back(tx_2);

  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1r, miner_account, tx_2);

  return true;
}
