// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin developers
// Copyright (c) 2016-2026, The Karbo developers
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

#include <cstdlib>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <cstring>
#include <string>
#include <string.h>
#include <sstream>
#include <vector>
#include <iterator>
#include <chrono>
#include <thread>
#include <future>

#include "Checkpoints.h"
#include "../CryptoNoteConfig.h"
#include "Common/StringTools.h"
#include "Common/DnsTools.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"

using namespace Logging;
#undef ERROR

namespace CryptoNote {
//---------------------------------------------------------------------------
Checkpoints::Checkpoints(Logging::ILogger &log, uint32_t reject_deep_reorg_depth) : logger(log, "checkpoints"), m_reject_deep_reorg_depth(reject_deep_reorg_depth) {
  
}
//---------------------------------------------------------------------------
bool Checkpoints::add_checkpoint(uint32_t height, const std::string &hash_str) {
  Crypto::Hash h = NULL_HASH;

  if (!Common::podFromHex(hash_str, h)) {
    logger(WARNING) << "Wrong hash in checkpoint for height " << height;
    return false;
  }

  if (!m_points.insert({ height, h }).second) {
    logger(WARNING) << "Checkpoint already exists.";
    return false;
  }

  return true;
}
//---------------------------------------------------------------------------
bool Checkpoints::load_checkpoints_from_file(const std::string& fileName) {
  std::ifstream file(fileName);
  if (!file) {
    logger(Logging::ERROR, BRIGHT_RED) << "Could not load checkpoints file: " << fileName;
    return false;
  }
  std::string indexString;
  std::string hash;
  uint32_t height;
  while (std::getline(file, indexString, ','), std::getline(file, hash)) {
    try {
      height = std::stoi(indexString);
    } catch (const std::invalid_argument &) {
      logger(Logging::ERROR, BRIGHT_RED) << "Invalid checkpoint file format - "
        << "could not parse height as a number";
      return false;
    }
    if (!add_checkpoint(height, hash)) {
      return false;
    }
  }
  logger(Logging::INFO) << "Loaded " << m_points.size() << " checkpoints from "	<< fileName;
  return true;
}

//---------------------------------------------------------------------------
bool Checkpoints::is_in_checkpoint_zone(uint32_t  height) const {
  return !m_points.empty() && (height <= (--m_points.end())->first);
}
//---------------------------------------------------------------------------
bool Checkpoints::check_block(uint32_t  height, const Crypto::Hash &h,
                              bool &is_a_checkpoint) const {
  auto it = m_points.find(height);
  is_a_checkpoint = it != m_points.end();
  if (!is_a_checkpoint)
    return true;

  if (it->second == h) {
    logger(Logging::DEBUGGING, Logging::GREEN)
      << "CHECKPOINT PASSED FOR HEIGHT " << height << " " << h;
    return true;
  } else {
    logger(Logging::ERROR) << "CHECKPOINT FAILED FOR HEIGHT " << height
                           << ". EXPECTED HASH: " << it->second
                           << ", FETCHED HASH: " << h;
    return false;
  }
}
//---------------------------------------------------------------------------
bool Checkpoints::check_block(uint32_t  height, const Crypto::Hash &h) const {
  bool ignored;
  return check_block(height, h, ignored);
}
//---------------------------------------------------------------------------
bool Checkpoints::is_alternative_block_allowed(uint32_t  blockchain_height,
                                               uint32_t  block_height) const {
  if (0 == block_height)
    return false;

  if (m_reject_deep_reorg_depth > 0 && block_height < blockchain_height - m_reject_deep_reorg_depth
    && !is_in_checkpoint_zone(block_height)) {
    logger(Logging::WARNING, Logging::WHITE) << "An attempt of too deep reorganization: "
      << blockchain_height - block_height << ", BLOCK REJECTED";

    return false;
  }

  auto it = m_points.upper_bound(blockchain_height);
  // Is blockchain_height before the first checkpoint?
  if (it == m_points.begin())
    return true;

  --it;
  uint32_t  checkpoint_height = it->first;
  return checkpoint_height < block_height;
}

std::vector<uint32_t> Checkpoints::getCheckpointHeights() const {
  std::vector<uint32_t> checkpointHeights;
  checkpointHeights.reserve(m_points.size());
  for (const auto& it : m_points) {
    checkpointHeights.push_back(it.first);
  }

  return checkpointHeights;
}

//---------------------------------------------------------------------------
bool Checkpoints::load_checkpoints_from_dns()
{
#if defined(__ANDROID__)
  return false;
#else
  std::string domain(CryptoNote::DNS_CHECKPOINTS_HOST);
  std::vector<std::string>records;
  bool res = true;
  auto start = std::chrono::steady_clock::now();
  logger(Logging::DEBUGGING) << "Fetching DNS checkpoint records from " << domain;

  try {
    auto future = std::async(std::launch::async, [this, &res, &domain, &records]() {
      res = Common::fetch_dns_txt(domain, records);
    });

    std::future_status status;

    status = future.wait_for(std::chrono::milliseconds(200));

    if (status == std::future_status::timeout) {
      logger(Logging::DEBUGGING) << "Timeout lookup DNS checkpoint records from " << domain;
      return false;
    }
    else if (status == std::future_status::ready) {
      future.get();
    }
  }
  catch (std::runtime_error& e) {
    logger(Logging::DEBUGGING) << e.what();
    return false;
  }

  auto dur = std::chrono::steady_clock::now() - start;
  logger(Logging::DEBUGGING) << "DNS query time: " << std::chrono::duration_cast<std::chrono::milliseconds>(dur).count() << " ms";

  // Fail-closed: if no signer addresses are baked into this build, every DNS
  // record is dropped without trying to verify it. Tampered DNS or an
  // accidentally-misconfigured release can't sneak past the signature gate
  // by simply omitting the signature field. The one-shot warning makes the
  // misconfiguration visible to operators reading the log.
  if (CryptoNote::DNS_CHECKPOINT_SIGNERS_COUNT == 0) {
    logger(Logging::WARNING) << "DNS checkpoints fetched but no DNS_CHECKPOINT_SIGNERS "
                                "configured in this build; ignoring " << records.size()
                             << " record(s). Set DNS_CHECKPOINT_SIGNERS in CryptoNoteConfig.h "
                                "to enable.";
    return true;
  }

  // Pre-parse the approved signer list once per DNS fetch. Addresses that
  // fail Base58/curve validation are logged and dropped so an accidentally-
  // mistyped address in the config does not silently lock the verifier
  // open to attacker signatures (it can't — Common::Base58::decode_addr
  // would just reject — but we want the diagnostic to point at the bad
  // entry).
  std::vector<CryptoNote::AccountPublicAddress> signers;
  signers.reserve(CryptoNote::DNS_CHECKPOINT_SIGNERS_COUNT);
  for (size_t i = 0; i < CryptoNote::DNS_CHECKPOINT_SIGNERS_COUNT; ++i) {
    CryptoNote::AccountPublicAddress addr;
    uint64_t prefix = 0;
    if (CryptoNote::parseAccountAddressString(prefix, addr,
            std::string(CryptoNote::DNS_CHECKPOINT_SIGNERS[i]))) {
      signers.push_back(addr);
    } else {
      logger(Logging::ERROR, BRIGHT_RED)
          << "DNS_CHECKPOINT_SIGNERS[" << i << "]='"
          << CryptoNote::DNS_CHECKPOINT_SIGNERS[i]
          << "' is not a valid Karbo address; skipping.";
    }
  }
  if (signers.empty()) {
    logger(Logging::WARNING) << "No usable DNS checkpoint signers after parsing; "
                                "ignoring all DNS records.";
    return true;
  }

  for (const auto& record : records) {
    // Required wire format: "<height>:<block_hash_64hex>:<signature>"
    // The legacy 2-field "<height>:<hash>" format is rejected — it has no
    // signature and so cannot be trusted to add even an anchor.
    const size_t del1 = record.find(':');
    if (del1 == std::string::npos) {
      logger(Logging::WARNING) << "Malformed DNS checkpoint (no field delimiter): " << record;
      continue;
    }
    const size_t del2 = record.find(':', del1 + 1);
    if (del2 == std::string::npos) {
      logger(Logging::WARNING) << "Malformed DNS checkpoint (legacy unsigned format, rejected): " << record;
      continue;
    }

    const std::string height_str = record.substr(0, del1);
    const std::string hash_str   = record.substr(del1 + 1, del2 - del1 - 1);
    const std::string sig_str    = record.substr(del2 + 1);

    if (hash_str.size() != 64) {
      logger(Logging::WARNING) << "Malformed DNS checkpoint (hash length " << hash_str.size()
                               << " != 64): " << record;
      continue;
    }

    uint32_t height = 0;
    {
      std::stringstream ss(height_str);
      char trailing;
      ss >> height;
      if (ss.fail() || ss.get(trailing)) {
        logger(Logging::WARNING) << "Malformed DNS checkpoint (height not a clean number): " << record;
        continue;
      }
    }

    Crypto::Hash hash{};
    if (!Common::podFromHex(hash_str, hash)) {
      logger(Logging::WARNING) << "Malformed DNS checkpoint (hash not hex): " << record;
      continue;
    }

    // Verify the signature against any one of the approved signers. The
    // signed payload is the literal "<height>:<hash>" string — what the
    // maintainer types into simplewallet's sign_message prompt.
    const std::string signed_payload = height_str + ":" + hash_str;
    bool verified = false;
    for (const auto& signer : signers) {
      if (CryptoNote::verifyMessage(signed_payload, signer, sig_str, logger.getLogger())) {
        verified = true;
        break;
      }
    }
    if (!verified) {
      logger(Logging::ERROR, BRIGHT_RED)
          << "DNS checkpoint signature did not match any approved signer; "
             "rejecting record: " << record;
      continue;
    }

    if (m_points.count(height) != 0) {
      logger(Logging::DEBUGGING) << "Checkpoint already exists for height: " << height
                                 << ". Ignoring DNS checkpoint.";
      continue;
    }
    add_checkpoint(height, hash_str);
    logger(Logging::DEBUGGING) << "Added signed DNS checkpoint: " << height_str << ":" << hash_str;
  }

  return true;
#endif
}

}
