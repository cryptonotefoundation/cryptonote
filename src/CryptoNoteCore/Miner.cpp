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

#include "Miner.h"

#include <iostream>
#include <functional>
#include <thread>

#include "crypto/crypto.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"

namespace CryptoNote {

Miner::Miner(System::Dispatcher& dispatcher, Logging::ILogger& logger) :
  m_dispatcher(dispatcher),
  m_miningStopped(dispatcher),
  m_logger(logger, "Miner"),
  m_template(NoBlockTemplate()),
  m_template_no(0),
  m_difficulty(0),
  m_miner_address(nullptr),
  m_threads_total(0),
  m_starter_nonce(0) {
}

Miner::~Miner() {
  assert(m_threads.empty());
  stop();
}

void Miner::start(const AccountPublicAddress& miner_address, size_t threads_count) {
  stop();
  m_miner_address = &miner_address;
  m_threads_total = static_cast<uint32_t>(threads_count);
  m_miningStopped.clear();
  m_threads.clear();
  for (uint32_t i = 0; i != m_threads_total; ++i) {
    m_threads.push_back(std::thread(std::bind(&Miner::worker, this, i)));
  }
}

void Miner::stop() {
  m_miningStopped.set();
  for (auto& th : m_threads) {
    th.join();
  }
  m_threads.clear();
}

void Miner::on_block_chain_update() {
  if (!is_mining()) {
    return;
  }

  if (m_miner_address) {
    m_miner_address->thunderScore += 10; // +10 per ogni blocco minato
    std::cout << "Thunder Score updated: " << m_miner_address->thunderScore << std::endl;
  }

  BlockTemplate block_template;
  uint64_t difficulty;
  uint32_t height;

  if (m_blockchain.get_block_template(block_template, m_miner_address, difficulty, height)) {
    m_difficulty = difficulty;
    m_template = block_template;
    m_template_no++;
    m_starter_nonce = Crypto::rand<uint32_t>();
  }
}

void Miner::on_synchronized() {
  if (!is_mining()) {
    return;
  }

  on_block_chain_update();
}

bool Miner::is_mining() const {
  return !m_threads.empty();
}

void Miner::worker(uint32_t th_local_index) {
  m_logger(Logging::INFO) << "Miner thread " << th_local_index << " started";

  uint32_t nonce = m_starter_nonce + th_local_index;
  uint64_t template_no = m_template_no;
  BlockTemplate block_template = m_template;
  uint64_t difficulty = m_difficulty;

  while (!m_miningStopped) {
    if (template_no != m_template_no) {
      nonce = m_starter_nonce + th_local_index;
      template_no = m_template_no;
      block_template = m_template;
      difficulty = m_difficulty;
    }

    if (block_template == NoBlockTemplate()) {
      m_logger(Logging::INFO) << "No block template, miner thread " << th_local_index << " stopped";
      return;
    }

    block_template.nonce = nonce;

    Crypto::Hash hash;
    if (!CryptoNote::get_block_hash(block_template, hash)) {
      m_logger(Logging::WARNING) << "Failed to get block hash, miner thread " << th_local_index << " stopped";
      return;
    }

    if (check_hash(hash, difficulty)) {
      m_logger(Logging::INFO) << "Found block with hash " << hash << " at difficulty " << difficulty;
      m_blockchain.add_new_block(block_template);
      on_block_chain_update();
    }

    nonce += m_threads_total;
  }

  m_logger(Logging::INFO) << "Miner thread " << th_local_index << " stopped";
}

} // namespace CryptoNote
