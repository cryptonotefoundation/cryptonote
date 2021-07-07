// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Forknote project
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The Unprll Project
// Copyright (c) 2018-2019, The TurtleCoin Developers
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

#include "CryptoNoteProtocolHandler.h"

#include <future>
#include <random>
#include <boost/optional.hpp>
#include <boost/scope_exit.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <System/Dispatcher.h>

#include "Common/ShuffleGenerator.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/VerificationContext.h"
#include "P2p/LevinProtocol.h"

#include "crypto/random.h"

using namespace Logging;
using namespace Common;

namespace CryptoNote {

namespace {

template<class t_parametr>
bool post_notify(IP2pEndpoint& p2p, typename t_parametr::request& arg, const CryptoNoteConnectionContext& context) {
  return p2p.invoke_notify_to_peer(t_parametr::ID, LevinProtocol::encode(arg), context);
}

template<class t_parametr>
void relay_post_notify(IP2pEndpoint& p2p, typename t_parametr::request& arg, const net_connection_id* excludeConnection = nullptr) {
  p2p.externalRelayNotifyToAll(t_parametr::ID, LevinProtocol::encode(arg), excludeConnection);
}

}

CryptoNoteProtocolHandler::CryptoNoteProtocolHandler(const Currency& currency, System::Dispatcher& dispatcher, ICore& rcore, IP2pEndpoint* p_net_layout, Logging::ILogger& log) :
  m_dispatcher(dispatcher),
  m_currency(currency),
  m_core(rcore),
  m_p2p(p_net_layout),
  m_synchronized(false),
  m_stop(false),
  m_observedHeight(0),
  m_peersCount(0),
  m_dandelionStemSelectInterval(CryptoNote::parameters::DANDELION_EPOCH),
  m_dandelionStemFluffInterval(CryptoNote::parameters::DANDELION_STEM_EMBARGO),
  logger(log, "protocol"),
  m_stemPool() {
  
  if (!m_p2p) {
    m_p2p = &m_p2p_stub;
  }
}

size_t CryptoNoteProtocolHandler::getPeerCount() const {
  return m_peersCount;
}

void CryptoNoteProtocolHandler::set_p2p_endpoint(IP2pEndpoint* p2p) {
  if (p2p)
    m_p2p = p2p;
  else
    m_p2p = &m_p2p_stub;
}

void CryptoNoteProtocolHandler::onConnectionOpened(CryptoNoteConnectionContext& context) {
}

void CryptoNoteProtocolHandler::onConnectionClosed(CryptoNoteConnectionContext& context) {
  bool updated = false;
  {
    std::lock_guard<std::mutex> lock(m_observedHeightMutex);
    uint64_t prevHeight = m_observedHeight;
    recalculateMaxObservedHeight(context);
    if (prevHeight != m_observedHeight) {
      updated = true;
    }
  }

  if (updated) {
    logger(TRACE) << "Observed height updated: " << m_observedHeight;
    m_observerManager.notify(&ICryptoNoteProtocolObserver::lastKnownBlockHeightUpdated, m_observedHeight);
  }

  if (context.m_state != CryptoNoteConnectionContext::state_befor_handshake) {
    m_peersCount--;
    m_observerManager.notify(&ICryptoNoteProtocolObserver::peerCountUpdated, m_peersCount.load());
  }
}

void CryptoNoteProtocolHandler::stop() {
  m_stop = true;
}

bool CryptoNoteProtocolHandler::start_sync(CryptoNoteConnectionContext& context) {
  logger(Logging::TRACE) << context << "Starting synchronization";

  if (context.m_state == CryptoNoteConnectionContext::state_synchronizing) {
    assert(context.m_needed_objects.empty());
    assert(context.m_requested_objects.empty());

    NOTIFY_REQUEST_CHAIN::request r = boost::value_initialized<NOTIFY_REQUEST_CHAIN::request>();
    r.block_ids = m_core.buildSparseChain();
    logger(Logging::TRACE) << context << "-->>NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << r.block_ids.size();
    post_notify<NOTIFY_REQUEST_CHAIN>(*m_p2p, r, context);
  }

  return true;
}

bool CryptoNoteProtocolHandler::get_stat_info(core_stat_info& stat_inf) {
  return m_core.get_stat_info(stat_inf);
}

void CryptoNoteProtocolHandler::log_connections() {
  std::stringstream ss;

  ss << std::setw(25) << std::left << "Remote Host"
    << std::setw(20) << "Peer id"
    << std::setw(25) << "Recv/Sent (inactive,sec)"
    << std::setw(25) << "State"
    << std::setw(20) << "Lifetime(seconds)" << ENDL;

  m_p2p->for_each_connection([&](const CryptoNoteConnectionContext& cntxt, PeerIdType peer_id) {
    ss << std::setw(25) << std::left << std::string(cntxt.m_is_income ? "[INC]" : "[OUT]") +
      Common::ipAddressToString(cntxt.m_remote_ip) + ":" + std::to_string(cntxt.m_remote_port)
      << std::setw(20) << std::hex << peer_id
      // << std::setw(25) << std::to_string(cntxt.m_recv_cnt) + "(" + std::to_string(time(NULL) - cntxt.m_last_recv) + ")" + "/" + std::to_string(cntxt.m_send_cnt) + "(" + std::to_string(time(NULL) - cntxt.m_last_send) + ")"
      << std::setw(25) << get_protocol_state_string(cntxt.m_state)
      << std::setw(20) << std::to_string(time(NULL) - cntxt.m_started) << ENDL;
  });
  logger(INFO) << "Connections: " << ENDL << ss.str();
}

bool CryptoNoteProtocolHandler::getConnections(std::vector<CryptoNoteConnectionContext>& connections) const {
  m_p2p->for_each_connection([&](const CryptoNoteConnectionContext& cntxt, PeerIdType peer_id) {
    connections.push_back(cntxt);
  });

  return true;
}

uint32_t CryptoNoteProtocolHandler::get_current_blockchain_height() {
  uint32_t height;
  Crypto::Hash blockId;
  m_core.get_blockchain_top(height, blockId);
  return height;
}

bool CryptoNoteProtocolHandler::process_payload_sync_data(const CORE_SYNC_DATA& hshd, CryptoNoteConnectionContext& context, bool is_initial) {
  if (context.m_state == CryptoNoteConnectionContext::state_befor_handshake && !is_initial)
    return true;

  if (context.m_state == CryptoNoteConnectionContext::state_synchronizing) {
  } else if (m_core.have_block(hshd.top_id)) {
    if (is_initial) {
      on_connection_synchronized();
      context.m_state = CryptoNoteConnectionContext::state_pool_sync_required;
    } else {
      context.m_state = CryptoNoteConnectionContext::state_normal;
    }
  } else {
    int64_t diff = static_cast<int64_t>(hshd.current_height - 1) - static_cast<int64_t>(get_current_blockchain_height());

    // drop and eventually ban if peer is on fork too deep behind us 
    if (diff < 0 && std::abs(diff) > CryptoNote::parameters::CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW && m_core.isInCheckpointZone(hshd.current_height)) {
      logger(Logging::DEBUGGING) << context << "Sync data returned a new top block candidate: " << get_current_blockchain_height() << " -> " << hshd.current_height - 1
        << ". Your node is " << std::abs(diff) << " blocks (" << std::abs(diff) / (24 * 60 * 60 / m_currency.difficultyTarget()) << " days) "
        << "ahead. The block candidate is too deep behind and in checkpoint zone, dropping connection";
      m_p2p->drop_connection(context, true);
    }

    logger(diff >= 0 ? (is_initial ? Logging::INFO : Logging::DEBUGGING) : Logging::TRACE, Logging::BRIGHT_YELLOW) << context <<
      "Sync data returned a new top block candidate: " << get_current_blockchain_height() << " -> " << hshd.current_height - 1
      << " [Your node is " << std::abs(diff) << " blocks (" << std::abs(diff) / (24 * 60 * 60 / m_currency.difficultyTarget()) << " days) "
      << (diff >= 0 ? std::string("behind") : std::string("ahead")) << "] " << std::endl << "Synchronization started";

    logger(Logging::DEBUGGING) << "Remote top block height: " << hshd.current_height - 1 << ", id: " << hshd.top_id;
    //let the socket to send response to handshake, but request callback, to let send request data after response
    logger(Logging::TRACE) << context << "requesting synchronization";
    context.m_state = CryptoNoteConnectionContext::state_sync_required;
  }

  updateObservedHeight(hshd.current_height, context);
  context.m_remote_blockchain_height = hshd.current_height;

  if (is_initial) {
    m_peersCount++;
    m_observerManager.notify(&ICryptoNoteProtocolObserver::peerCountUpdated, m_peersCount.load());
  }

  return true;
}

bool CryptoNoteProtocolHandler::get_payload_sync_data(CORE_SYNC_DATA& hshd) {
  uint32_t current_height;
  m_core.get_blockchain_top(current_height, hshd.top_id);
  hshd.current_height = current_height;
  hshd.current_height += 1;
  return true;
}

template <typename Command, typename Handler>
int notifyAdaptor(const BinaryArray& reqBuf, CryptoNoteConnectionContext& ctx, Handler handler) {

  typedef typename Command::request Request;
  int command = Command::ID;

  Request req = boost::value_initialized<Request>();
  if (!LevinProtocol::decode(reqBuf, req)) {
    throw std::runtime_error("Failed to load_from_binary in command " + std::to_string(command));
  }

  return handler(command, req, ctx);
}

#define HANDLE_NOTIFY(CMD, Handler) case CMD::ID: { ret = notifyAdaptor<CMD>(in, ctx, std::bind(Handler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)); break; }

int CryptoNoteProtocolHandler::handleCommand(bool is_notify, int command, const BinaryArray& in, BinaryArray& out, CryptoNoteConnectionContext& ctx, bool& handled) {
  int ret = 0;
  handled = true;

  switch (command) {
    HANDLE_NOTIFY(NOTIFY_NEW_BLOCK, &CryptoNoteProtocolHandler::handle_notify_new_block)
    HANDLE_NOTIFY(NOTIFY_NEW_TRANSACTIONS, &CryptoNoteProtocolHandler::handle_notify_new_transactions)
    HANDLE_NOTIFY(NOTIFY_REQUEST_GET_OBJECTS, &CryptoNoteProtocolHandler::handle_request_get_objects)
    HANDLE_NOTIFY(NOTIFY_RESPONSE_GET_OBJECTS, &CryptoNoteProtocolHandler::handle_response_get_objects)
    HANDLE_NOTIFY(NOTIFY_REQUEST_CHAIN, &CryptoNoteProtocolHandler::handle_request_chain)
    HANDLE_NOTIFY(NOTIFY_RESPONSE_CHAIN_ENTRY, &CryptoNoteProtocolHandler::handle_response_chain_entry)
    HANDLE_NOTIFY(NOTIFY_REQUEST_TX_POOL, &CryptoNoteProtocolHandler::handle_request_tx_pool)
    HANDLE_NOTIFY(NOTIFY_NEW_LITE_BLOCK, &CryptoNoteProtocolHandler::handle_notify_new_lite_block)
    HANDLE_NOTIFY(NOTIFY_MISSING_TXS, &CryptoNoteProtocolHandler::handle_notify_missing_txs)

  default:
    handled = false;
  }

  return ret;
}

#undef HANDLE_NOTIFY

int CryptoNoteProtocolHandler::handle_notify_new_block(int command, NOTIFY_NEW_BLOCK::request& arg, CryptoNoteConnectionContext& context) {
  logger(Logging::TRACE) << context << "NOTIFY_NEW_BLOCK (hop " << arg.hop << ")";

  updateObservedHeight(arg.current_blockchain_height, context);

  context.m_remote_blockchain_height = arg.current_blockchain_height;

  if (context.m_state != CryptoNoteConnectionContext::state_normal) {
    return 1;
  }

  for (auto tx_blob_it = arg.b.txs.begin(); tx_blob_it != arg.b.txs.end(); tx_blob_it++) {
    CryptoNote::tx_verification_context tvc = boost::value_initialized<decltype(tvc)>();

    auto transactionBinary = asBinaryArray(*tx_blob_it);
    //Crypto::Hash transactionHash = Crypto::cn_fast_hash(transactionBinary.data(), transactionBinary.size());
    //logger(DEBUGGING) << "transaction " << transactionHash << " came in NOTIFY_NEW_BLOCK";

    m_core.handle_incoming_tx(transactionBinary, tvc, true);
    if (tvc.m_verification_failed) {
      logger(Logging::INFO) << context << "Block verification failed: transaction verification failed, dropping connection";
      m_p2p->drop_connection(context, true);
      return 1;
    }
  }

  block_verification_context bvc = boost::value_initialized<block_verification_context>();
  m_core.handle_incoming_block_blob(asBinaryArray(arg.b.block), bvc, true, false);
  if (bvc.m_verification_failed) {
    logger(Logging::DEBUGGING) << context << "Block verification failed, dropping connection";
    m_p2p->drop_connection(context, true);
    return 1;
  }
  if (bvc.m_added_to_main_chain) {
    ++arg.hop;
    //TODO: Add here announce protocol usage
    //relay_post_notify<NOTIFY_NEW_BLOCK>(*m_p2p, arg, &context.m_connection_id);
    relay_block(arg);
    // relay_block(arg, context);

    if (bvc.m_switched_to_alt_chain) {
      requestMissingPoolTransactions(context);
    }
  } else if (bvc.m_marked_as_orphaned) {
    context.m_state = CryptoNoteConnectionContext::state_synchronizing;
    NOTIFY_REQUEST_CHAIN::request r = boost::value_initialized<NOTIFY_REQUEST_CHAIN::request>();
    r.block_ids = m_core.buildSparseChain();
    logger(Logging::TRACE) << context << "-->>NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << r.block_ids.size();
    post_notify<NOTIFY_REQUEST_CHAIN>(*m_p2p, r, context);
  }

  return 1;
}

int CryptoNoteProtocolHandler::handle_notify_new_transactions(int command, NOTIFY_NEW_TRANSACTIONS::request& arg, CryptoNoteConnectionContext& context) {
  logger(Logging::TRACE) << context << "NOTIFY_NEW_TRANSACTIONS";
  if (context.m_state != CryptoNoteConnectionContext::state_normal)
    return 1;

  std::vector<Crypto::Hash> txHashes;

  if (context.m_pending_lite_block) {
    logger(Logging::TRACE) << context
      << " Pending lite block detected, handling request as missing lite block transactions response";
    std::vector<BinaryArray> _txs;
    for (const auto& tx : arg.txs) {
      _txs.push_back(asBinaryArray(tx));
    }
    return doPushLiteBlock(context.m_pending_lite_block->request, context, std::move(_txs));
  } else {
    for (auto tx_blob_it = arg.txs.begin(); tx_blob_it != arg.txs.end();) {
      auto transactionBinary = asBinaryArray(*tx_blob_it);
      Crypto::Hash transactionHash = Crypto::cn_fast_hash(transactionBinary.data(), transactionBinary.size());
      logger(DEBUGGING) << "Transaction " << transactionHash << " came in NOTIFY_NEW_TRANSACTIONS"
                        << " as " << (arg.stem ? "stem" : "fluff");
      CryptoNote::tx_verification_context tvc = boost::value_initialized<decltype(tvc)>();
      m_core.handle_incoming_tx(transactionBinary, tvc, false);
      if (tvc.m_verification_failed) {
        logger(Logging::DEBUGGING) << context << "Transaction verification failed";
      }
      if (!tvc.m_verification_failed && tvc.m_should_be_relayed) {
        if (!arg.stem) {
          if (m_stemPool.hasTransaction(transactionHash)) {
            logger(Logging::DEBUGGING) << "Removing transaction " << transactionHash << " from stempool as already broadcasted";
            m_stemPool.removeTransaction(transactionHash);
          }
        }
        else {
          txHashes.push_back(transactionHash);
          if (!m_stemPool.hasTransaction(transactionHash)) {
            logger(Logging::DEBUGGING) << "Adding transaction " << transactionHash << " to stempool";
            m_stemPool.addTransaction(transactionHash, *tx_blob_it);
          }
          else { // tx made roundtrip as stem, fluff it
            logger(Logging::DEBUGGING) << "Removing transaction " << transactionHash << " from stempool and fluff";
            m_stemPool.removeTransaction(transactionHash);
            txHashes.erase(std::remove(txHashes.begin(), txHashes.end(), transactionHash), txHashes.end());
            arg.stem = false;
          }
        }
        ++tx_blob_it;
      }
      else {
        if (m_stemPool.hasTransaction(transactionHash)) {
          logger(Logging::DEBUGGING) << "Removing transaction " << transactionHash << " from stempool as already broadcasted";
          m_stemPool.removeTransaction(transactionHash);
        }
        tx_blob_it = arg.txs.erase(tx_blob_it);
      }
    }
  }

  if (arg.txs.size()) {
    //TODO: add announce usage here
    if (arg.stem && !m_dandelion_stem.empty()) {
      std::mt19937 rng = Random::generator();
      std::uniform_int_distribution<> dis(0, 100);
      auto coin_flip = dis(rng);
      if (coin_flip < CryptoNote::parameters::DANDELION_STEM_TX_PROPAGATION_PROBABILITY) { // Stem propagation
        for (const auto& dandelion_peer : m_dandelion_stem) {
          if (dandelion_peer.m_state == CryptoNoteConnectionContext::state_normal || dandelion_peer.m_state == CryptoNoteConnectionContext::state_synchronizing) {
            if (!post_notify<NOTIFY_NEW_TRANSACTIONS>(*m_p2p, arg, dandelion_peer)) {
              arg.stem = false;
              logger(Logging::DEBUGGING) << "Failed to relay transactions to Dandelion peer " << dandelion_peer.m_connection_id << ", remove from stempool and broadcast as fluff:";
              for (const auto& h : txHashes) {
                m_stemPool.removeTransaction(h);
                logger(Logging::DEBUGGING) << h;
              }
              relay_post_notify<NOTIFY_NEW_TRANSACTIONS>(*m_p2p, arg, &context.m_connection_id); // Fluff broadcast
              break;
            }
          }
        }
      } else { // Switch to fluff broadcast
        arg.stem = false;
        logger(Logging::DEBUGGING) << "Switching to fluff broadcast of stem transactions:";
        for (const auto& h : txHashes) {
          m_stemPool.removeTransaction(h);
          logger(Logging::DEBUGGING) << h;
        }
        relay_post_notify<NOTIFY_NEW_TRANSACTIONS>(*m_p2p, arg, &context.m_connection_id);
      }
    } else { // Fluff broadcast
      arg.stem = false;
      relay_post_notify<NOTIFY_NEW_TRANSACTIONS>(*m_p2p, arg, &context.m_connection_id);
    }
  }

  return true;
}

int CryptoNoteProtocolHandler::handle_request_get_objects(int command, NOTIFY_REQUEST_GET_OBJECTS::request& arg, CryptoNoteConnectionContext& context) {
  logger(Logging::TRACE) << context << "Received NOTIFY_REQUEST_GET_OBJECTS";

  /* Essentially, one can send such a large amount of IDs that core exhausts
   * all free memory. This issue can theoretically be exploited using very
   * large CN blockchains, such as Monero.
   *
   * This is a partial fix. Thanks and credit given to CryptoNote author
   * 'cryptozoidberg' for collaboration and the fix. Also thanks to
   * 'moneromooo'. Referencing HackerOne report #506595.
   */
  if (arg.blocks.size() + arg.txs.size() > CURRENCY_PROTOCOL_MAX_OBJECT_REQUEST_COUNT)
  {
    logger(Logging::ERROR) << context << 
      "Requested objects count is too big ("
      << arg.blocks.size() << ") expected not more then "
      << CURRENCY_PROTOCOL_MAX_OBJECT_REQUEST_COUNT;
    m_p2p->drop_connection(context, true);
    return 1;
  }

  NOTIFY_RESPONSE_GET_OBJECTS::request rsp;
  if (!m_core.handle_get_objects(arg, rsp)) {
    logger(Logging::ERROR) << context << "failed to handle request NOTIFY_REQUEST_GET_OBJECTS, dropping connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
  }

  logger(Logging::TRACE) << context << "-->>NOTIFY_RESPONSE_GET_OBJECTS: blocks.size()=" << rsp.blocks.size() << ", txs.size()=" << rsp.txs.size()
    << ", rsp.m_current_blockchain_height=" << rsp.current_blockchain_height << ", missed_ids.size()=" << rsp.missed_ids.size();
  post_notify<NOTIFY_RESPONSE_GET_OBJECTS>(*m_p2p, rsp, context);
  return 1;
}

int CryptoNoteProtocolHandler::handle_response_get_objects(int command, NOTIFY_RESPONSE_GET_OBJECTS::request& arg, CryptoNoteConnectionContext& context) {
  logger(Logging::TRACE) << context << "NOTIFY_RESPONSE_GET_OBJECTS";

  if (arg.blocks.empty())
  {
    logger(Logging::ERROR) << context << "sent wrong NOTIFY_HAVE_OBJECTS: no blocks, dropping connection";
    m_p2p->drop_connection(context, true);
    return 1;
  }

  if (context.m_last_response_height > arg.current_blockchain_height) {
    logger(Logging::ERROR) << context << "sent wrong NOTIFY_HAVE_OBJECTS: arg.m_current_blockchain_height=" << arg.current_blockchain_height
      << " < m_last_response_height=" << context.m_last_response_height << ", dropping connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  updateObservedHeight(arg.current_blockchain_height, context);

  context.m_remote_blockchain_height = arg.current_blockchain_height;

  size_t count = 0;
  std::vector<Crypto::Hash> block_hashes;
  block_hashes.reserve(arg.blocks.size());
  std::vector<parsed_block_entry> parsed_blocks;
  parsed_blocks.reserve(arg.blocks.size());
  for (const block_complete_entry& block_entry : arg.blocks) {
    ++count;
    Block b;
    BinaryArray block_blob = asBinaryArray(block_entry.block);
    if (block_blob.size() > m_currency.maxBlockBlobSize()) {
      logger(Logging::ERROR) << context << "sent wrong block: too big size " << block_blob.size() << ", dropping connection";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    }
    if (!fromBinaryArray(b, block_blob)) {
      logger(Logging::ERROR) << context << "sent wrong block: failed to parse and validate block: \r\n"
        << toHex(block_blob) << "\r\n dropping connection";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    }

    //to avoid concurrency in core between connections, suspend connections which delivered block later then first one
    auto blockHash = get_block_hash(b);
    if (count == 2) {
      if (m_core.have_block(blockHash)) {
        context.m_state = CryptoNoteConnectionContext::state_idle;
        context.m_needed_objects.clear();
        context.m_requested_objects.clear();
        logger(Logging::DEBUGGING) << context << "Connection set to idle state.";
        return 1;
      }
    }

    auto req_it = context.m_requested_objects.find(blockHash);
    if (req_it == context.m_requested_objects.end()) {
      logger(Logging::ERROR) << context << "sent wrong NOTIFY_RESPONSE_GET_OBJECTS: block with id=" << Common::podToHex(blockHash)
        << " wasn't requested, dropping connection";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    }
    if (b.transactionHashes.size() != block_entry.txs.size()) {
      logger(Logging::ERROR) << context << "sent wrong NOTIFY_RESPONSE_GET_OBJECTS: block with id=" << Common::podToHex(blockHash)
        << ", transactionHashes.size()=" << b.transactionHashes.size() << " mismatch with block_complete_entry.m_txs.size()=" << block_entry.txs.size() << ", dropping connection";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    }

    context.m_requested_objects.erase(req_it);

    block_hashes.push_back(blockHash);

    parsed_block_entry parsedBlock;
    parsedBlock.block = std::move(b);
    for (auto& tx_blob : block_entry.txs) {
      auto transactionBinary = asBinaryArray(tx_blob);
      parsedBlock.txs.push_back(transactionBinary);
    }
    parsed_blocks.push_back(parsedBlock);
  }

  if (context.m_requested_objects.size()) {
    logger(Logging::ERROR, Logging::BRIGHT_RED) << context <<
      "returned not all requested objects (context.m_requested_objects.size()="
      << context.m_requested_objects.size() << "), dropping connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  uint32_t height;
  Crypto::Hash top;
  {
    m_core.pause_mining();

    // we lock all the rest to avoid having multiple connections redo a lot
    // of the same work, and one of them doing it for nothing: subsequent
    // connections will wait until the current one's added its blocks, then
    // will add any extra it has, if any
    std::lock_guard<std::recursive_mutex> lk(m_sync_lock);

    // dismiss what another connection might already have done (likely everything)
    m_core.get_blockchain_top(height, top);
    uint64_t dismiss = 1;
    for (const auto &h : block_hashes) {
      if (top == h) {
        logger(Logging::DEBUGGING) << "Found current top block in synced blocks, dismissing "
          << dismiss << "/" << arg.blocks.size() << " blocks";
        while (dismiss--) {
          arg.blocks.erase(arg.blocks.begin());
          parsed_blocks.erase(parsed_blocks.begin());
        }
        break;
      }
      ++dismiss;
    }

    BOOST_SCOPE_EXIT_ALL(this) { m_core.update_block_template_and_resume_mining(); };

    int result = processObjects(context, parsed_blocks);
    if (result != 0) {
      return result;
    }
  }

  m_core.get_blockchain_top(height, top);
  logger(DEBUGGING, BRIGHT_GREEN) << "Local blockchain updated, new height = " << height;

  if (!m_stop && context.m_state == CryptoNoteConnectionContext::state_synchronizing) {
    request_missing_objects(context, true);
  }

  return 1;
}

int CryptoNoteProtocolHandler::processObjects(CryptoNoteConnectionContext& context, const std::vector<parsed_block_entry>& blocks) {
  for (const parsed_block_entry& block_entry : blocks) {
    if (m_stop) {
      break;
    }

    //process transactions
    for (size_t i = 0; i < block_entry.txs.size(); ++i) {
      auto transactionBinary = block_entry.txs[i];
      Crypto::Hash transactionHash = Crypto::cn_fast_hash(transactionBinary.data(), transactionBinary.size());
      logger(DEBUGGING) << "transaction " << transactionHash << " came in processObjects";

      // check if tx hashes match
      if (transactionHash != block_entry.block.transactionHashes[i]) {
        logger(Logging::DEBUGGING) << context << "transaction mismatch on NOTIFY_RESPONSE_GET_OBJECTS, \r\ntx_id = "
          << Common::podToHex(transactionHash) << ", dropping connection";
        context.m_state = CryptoNoteConnectionContext::state_shutdown;
        return 1;
      }

      tx_verification_context tvc = boost::value_initialized<decltype(tvc)>();
      m_core.handle_incoming_tx(transactionBinary, tvc, true);
      if (tvc.m_verification_failed) {
        logger(Logging::DEBUGGING) << context << "transaction verification failed on NOTIFY_RESPONSE_GET_OBJECTS, \r\ntx_id = "
          << Common::podToHex(transactionHash) << ", dropping connection";
        context.m_state = CryptoNoteConnectionContext::state_shutdown;
        return 1;
      }
    }

    // process block
    block_verification_context bvc = boost::value_initialized<block_verification_context>();
    m_core.handle_incoming_block(block_entry.block, bvc, false, false);

    if (bvc.m_verification_failed) {
      logger(Logging::DEBUGGING) << context << "Block verification failed, dropping connection";
      m_p2p->drop_connection(context, true);
      return 1;
    } else if (bvc.m_marked_as_orphaned) {
      logger(Logging::INFO) << context << "Block received at sync phase was marked as orphaned, dropping connection";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    } else if (bvc.m_already_exists) {
      logger(Logging::DEBUGGING) << context << "Block already exists, switching to idle state";
      context.m_state = CryptoNoteConnectionContext::state_idle;
      context.m_needed_objects.clear();
      context.m_requested_objects.clear();
      return 1;
    }

    m_dispatcher.yield();
  }

  return 0;
}

bool CryptoNoteProtocolHandler::select_dandelion_stem() {
  m_dandelion_stem.clear();

  //TODO: select from outgoing connections preferably supporting Dandelion: context.version >= P2P_VERSION_4)

  std::vector<CryptoNoteConnectionContext> alive_peers;
  m_p2p->for_each_connection([&](const CryptoNoteConnectionContext& ctx, PeerIdType peer_id) {
    if ((ctx.m_state == CryptoNoteConnectionContext::state_normal || ctx.m_state == CryptoNoteConnectionContext::state_synchronizing) && !ctx.m_is_income) {
      alive_peers.push_back(ctx);
    }
  });

  if (alive_peers.size() > 0) {
    ShuffleGenerator<size_t> peersGenerator(alive_peers.size());
    while (m_dandelion_stem.size() < std::min<size_t>(CryptoNote::parameters::DANDELION_STEMS, alive_peers.size()) && !peersGenerator.empty()) {
      auto& it = alive_peers[peersGenerator()];
      m_dandelion_stem.push_back(it);
    }

    logger(Logging::DEBUGGING) << "Selected dandelion_stem peers:";
    for (const auto& dp : m_dandelion_stem) {
      logger(Logging::DEBUGGING) << Common::ipAddressToString(dp.m_remote_ip) + ":" + std::to_string(dp.m_remote_port);
    }
    logger(Logging::DEBUGGING) << "out of:";
    for (const auto& ap : alive_peers) {
      logger(Logging::DEBUGGING) << Common::ipAddressToString(ap.m_remote_ip) + ":" + std::to_string(ap.m_remote_port);
    }

    return true;
  }

  logger(Logging::WARNING) << "No alive peers for dandelion stem...";
  
  return false;
}

// Fail-safe to ensure stem txs are broadcasted
bool CryptoNoteProtocolHandler::fluffStemPool() {
  if (!m_stemPool.hasTransactions()) {
    NOTIFY_NEW_TRANSACTIONS::request notification;
    notification.stem = false;
    logger(Logging::DEBUGGING) << "Broadcasting as fluff " << m_stemPool.getTransactionsCount() << " timeout stem transaction(s):";
    std::vector<std::pair<Crypto::Hash, std::string>> stemTxs = m_stemPool.getTransactions();
    for (const auto & s : stemTxs) {
        notification.txs.push_back(s.second);
      logger(Logging::DEBUGGING) << s.first;
    }
    auto buf = LevinProtocol::encode(notification);
    m_p2p->externalRelayNotifyToAll(NOTIFY_NEW_TRANSACTIONS::ID, buf, nullptr);

    m_stemPool.clearStemPool();
  }
  else {
    logger(Logging::DEBUGGING) << "Nothing to broadcast in fluff mode...";
  }
  
  return true;
}

bool CryptoNoteProtocolHandler::on_idle() {
  m_dandelionStemSelectInterval.call([&]() { return select_dandelion_stem(); });
  m_dandelionStemFluffInterval.call([&]() { return fluffStemPool(); });
  return m_core.on_idle();
}

int CryptoNoteProtocolHandler::doPushLiteBlock(NOTIFY_NEW_LITE_BLOCK::request arg, CryptoNoteConnectionContext &context,
                                              std::vector<BinaryArray> missingTxs) {
  Block b;
  if (!fromBinaryArray(b, asBinaryArray(arg.block))) {
    logger(Logging::WARNING) << context << "Deserialization of Block Template failed, dropping connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  std::unordered_map<Crypto::Hash, BinaryArray> provided_txs;
  provided_txs.reserve(missingTxs.size());
  for (const auto &missingTx : missingTxs) {
    provided_txs[getBinaryArrayHash(missingTx)] = missingTx;
  }

  std::vector<BinaryArray> have_txs;
  std::vector<Crypto::Hash> need_txs;

  if (context.m_pending_lite_block) {
    for (const auto &requestedTxHash : context.m_pending_lite_block->missed_transactions) {
      if (provided_txs.find(requestedTxHash) == provided_txs.end()) {
        logger(Logging::DEBUGGING) << context
          << "Peer didn't provide a missing transaction, previously "
          "acquired for a lite block, dropping connection.";
        context.m_pending_lite_block = boost::none;
        context.m_state = CryptoNoteConnectionContext::state_shutdown;
        return 1;
      }
    }
  }

  /*
   * here we are finding out which txs are present in the pool and which are not
   * further we check for transactions in the blockchain to accept alternative blocks
   */
  for (const auto &transactionHash : b.transactionHashes) {
    auto providedSearch = provided_txs.find(transactionHash);
    if (providedSearch != provided_txs.end()) {
      have_txs.push_back(providedSearch->second);
    } else {
      Transaction tx;
      if (m_core.getTransaction(transactionHash, tx, true)) {
        have_txs.push_back(toBinaryArray(tx));
      } else {
        need_txs.push_back(transactionHash);
      }
    }
  }

  /*
   * if all txs are present then continue adding the block to
   * blockchain storage and relaying the lite-block to other peers
   *
   * if not request the missing txs from the sender
   * of the lite-block request
   */
  if (need_txs.empty()) {
    context.m_pending_lite_block = boost::none;

    for (auto transactionBinary : have_txs) {
      CryptoNote::tx_verification_context tvc = boost::value_initialized<decltype(tvc)>();

      m_core.handle_incoming_tx(transactionBinary, tvc, true);
      if (tvc.m_verification_failed) {
        logger(Logging::INFO) << context << "Lite block verification failed: transaction verification failed, dropping connection";
        m_p2p->drop_connection(context, true);
        return 1;
      }
    }

    block_verification_context bvc = boost::value_initialized<block_verification_context>();
    m_core.handle_incoming_block_blob(asBinaryArray(arg.block), bvc, true, false);
    if (bvc.m_verification_failed) {
      logger(Logging::DEBUGGING) << context << "Lite block verification failed, dropping connection";
      m_p2p->drop_connection(context, true);
      return 1;
    }
    if (bvc.m_added_to_main_chain) {
      ++arg.hop;
      //TODO: Add here announce protocol usage
      relay_post_notify<NOTIFY_NEW_LITE_BLOCK>(*m_p2p, arg, &context.m_connection_id);

      if (bvc.m_switched_to_alt_chain) {
        requestMissingPoolTransactions(context);
      }
    }
    else if (bvc.m_marked_as_orphaned) {
      context.m_state = CryptoNoteConnectionContext::state_synchronizing;
      NOTIFY_REQUEST_CHAIN::request r = boost::value_initialized<NOTIFY_REQUEST_CHAIN::request>();
      r.block_ids = m_core.buildSparseChain();
      logger(Logging::TRACE) << context << "-->>NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << r.block_ids.size();
      post_notify<NOTIFY_REQUEST_CHAIN>(*m_p2p, r, context);
    }
  } else {
    if (context.m_pending_lite_block) {
      context.m_pending_lite_block = boost::none;
      logger(Logging::DEBUGGING) << context
        << " Peer has a pending lite block but didn't provide all necessary "
        "transactions, dropping the connection.";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
    } else {
      NOTIFY_MISSING_TXS::request req;
      req.current_blockchain_height = arg.current_blockchain_height;
      req.blockHash = get_block_hash(b);
      req.missing_txs = std::move(need_txs);
      context.m_pending_lite_block = PendingLiteBlock{ arg, {req.missing_txs.begin(), req.missing_txs.end()} };

      if (!post_notify<NOTIFY_MISSING_TXS>(*m_p2p, req, context)) {
        logger(Logging::DEBUGGING) << context
          << "Lite block is missing transactions but the publisher is not "
          "reachable, dropping connection.";
        context.m_state = CryptoNoteConnectionContext::state_shutdown;
      }
    }
  }

  return 1;
}

int CryptoNoteProtocolHandler::handle_request_chain(int command, NOTIFY_REQUEST_CHAIN::request& arg, CryptoNoteConnectionContext& context) {
  logger(Logging::TRACE) << context << "NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << arg.block_ids.size();

  if (arg.block_ids.empty()) {
    logger(Logging::ERROR, Logging::BRIGHT_RED) << context << "Failed to handle NOTIFY_REQUEST_CHAIN. block_ids is empty";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  if (arg.block_ids.back() != m_core.getBlockIdByHeight(0)) {
    logger(Logging::ERROR) << context << "Failed to handle NOTIFY_REQUEST_CHAIN. block_ids doesn't end with genesis block ID";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  NOTIFY_RESPONSE_CHAIN_ENTRY::request r;
  r.m_block_ids = m_core.findBlockchainSupplement(arg.block_ids, BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT, r.total_height, r.start_height);

  logger(Logging::TRACE) << context << "-->>NOTIFY_RESPONSE_CHAIN_ENTRY: m_start_height=" << r.start_height << ", m_total_height=" << r.total_height << ", m_block_ids.size()=" << r.m_block_ids.size();
  post_notify<NOTIFY_RESPONSE_CHAIN_ENTRY>(*m_p2p, r, context);
  return 1;
}

bool CryptoNoteProtocolHandler::request_missing_objects(CryptoNoteConnectionContext& context, bool check_having_blocks) {
  if (context.m_needed_objects.size()) {
    //we know objects that we need, request this objects
    NOTIFY_REQUEST_GET_OBJECTS::request req;
    size_t count = 0;
    auto it = context.m_needed_objects.begin();

    while (it != context.m_needed_objects.end() && count < BLOCKS_SYNCHRONIZING_DEFAULT_COUNT) {
      if (!(check_having_blocks && m_core.have_block(*it))) {
        req.blocks.push_back(*it);
        ++count;
        context.m_requested_objects.insert(*it);
      }
      it = context.m_needed_objects.erase(it);
    }
    logger(Logging::TRACE) << context << "-->>NOTIFY_REQUEST_GET_OBJECTS: blocks.size()=" << req.blocks.size() << ", txs.size()=" << req.txs.size();
    post_notify<NOTIFY_REQUEST_GET_OBJECTS>(*m_p2p, req, context);
  } else if (context.m_last_response_height < context.m_remote_blockchain_height - 1) {//we have to fetch more objects ids, request blockchain entry

    NOTIFY_REQUEST_CHAIN::request r = boost::value_initialized<NOTIFY_REQUEST_CHAIN::request>();
    r.block_ids = m_core.buildSparseChain();
    logger(Logging::TRACE) << context << "-->>NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << r.block_ids.size();
    post_notify<NOTIFY_REQUEST_CHAIN>(*m_p2p, r, context);
  } else {
    if (!(context.m_last_response_height ==
      context.m_remote_blockchain_height - 1 &&
      !context.m_needed_objects.size() &&
      !context.m_requested_objects.size())) {
      logger(Logging::ERROR, Logging::BRIGHT_RED)
        << "request_missing_blocks final condition failed!"
        << "\r\nm_last_response_height=" << context.m_last_response_height
        << "\r\nm_remote_blockchain_height=" << context.m_remote_blockchain_height
        << "\r\nm_needed_objects.size()=" << context.m_needed_objects.size()
        << "\r\nm_requested_objects.size()=" << context.m_requested_objects.size() 
        << "\r\non connection [" << context << "]";
      return false;
    }

    requestMissingPoolTransactions(context);

    context.m_state = CryptoNoteConnectionContext::state_normal;
    logger(Logging::INFO, Logging::BRIGHT_GREEN) << context << "SYNCHRONIZED OK";
    on_connection_synchronized();
  }
  return true;
}

bool CryptoNoteProtocolHandler::on_connection_synchronized() {
  bool val_expected = false;
  if (m_synchronized.compare_exchange_strong(val_expected, true)) {
    std::cout << ENDL << "**********************************************************************" << ENDL
      << "You are now synchronized with the network. You may now start simplewallet." << ENDL
      << ENDL
      << "Please note, that the blockchain will be saved only after you quit the daemon with \"exit\" command or if you use \"save\" command." << ENDL
      << "Otherwise, you will possibly need to synchronize the blockchain again." << ENDL
      << ENDL
      << "Use \"help\" command to see the list of available commands." << ENDL
      << "**********************************************************************" << ENDL;
    m_core.on_synchronized();

    uint32_t height;
    Crypto::Hash hash;
    m_core.get_blockchain_top(height, hash);
    m_observerManager.notify(&ICryptoNoteProtocolObserver::blockchainSynchronized, height);
  }
  return true;
}

int CryptoNoteProtocolHandler::handle_response_chain_entry(int command, NOTIFY_RESPONSE_CHAIN_ENTRY::request& arg, CryptoNoteConnectionContext& context) {
  logger(Logging::TRACE) << context << "NOTIFY_RESPONSE_CHAIN_ENTRY: m_block_ids.size()=" << arg.m_block_ids.size()
    << ", m_start_height=" << arg.start_height << ", m_total_height=" << arg.total_height;

  if (!arg.m_block_ids.size()) {
    logger(Logging::ERROR) << context << "sent empty m_block_ids, dropping connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  if (!m_core.have_block(arg.m_block_ids.front())) {
    logger(Logging::ERROR)
      << context << "sent m_block_ids starting from unknown id: "
      << Common::podToHex(arg.m_block_ids.front())
      << " , dropping connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  context.m_remote_blockchain_height = arg.total_height;
  context.m_last_response_height = arg.start_height + static_cast<uint32_t>(arg.m_block_ids.size()) - 1;

  if (context.m_last_response_height > context.m_remote_blockchain_height) {
    logger(Logging::ERROR)
      << context
      << "sent wrong NOTIFY_RESPONSE_CHAIN_ENTRY, with \r\nm_total_height="
      << arg.total_height << "\r\nm_start_height=" << arg.start_height
      << "\r\nm_block_ids.size()=" << arg.m_block_ids.size();
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
  }

  for (auto& bl_id : arg.m_block_ids) {
    if (!m_core.have_block(bl_id))
      context.m_needed_objects.push_back(bl_id);
  }

  if (!request_missing_objects(context, false)) {
    logger(Logging::DEBUGGING) << context << "Failed to request missing objects, dropping connection";
    m_p2p->drop_connection(context, true);
  }

  return 1;
}

int CryptoNoteProtocolHandler::handle_request_tx_pool(int command, NOTIFY_REQUEST_TX_POOL::request& arg,
                                                     CryptoNoteConnectionContext& context) {
  logger(Logging::TRACE) << context << "NOTIFY_REQUEST_TX_POOL: txs.size() = " << arg.txs.size();

  std::vector<Transaction> addedTransactions;
  std::vector<Crypto::Hash> deletedTransactions;
  m_core.getPoolChanges(arg.txs, addedTransactions, deletedTransactions);

  if (!addedTransactions.empty()) {
    NOTIFY_NEW_TRANSACTIONS::request notification;
    notification.stem = false;
    for (auto& tx : addedTransactions) {
      notification.txs.push_back(asString(toBinaryArray(tx)));
    }

    bool ok = post_notify<NOTIFY_NEW_TRANSACTIONS>(*m_p2p, notification, context);
    if (!ok) {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Failed to post notification NOTIFY_NEW_TRANSACTIONS to " << context.m_connection_id;
    }
  }

  return 1;
}

int CryptoNoteProtocolHandler::handle_notify_new_lite_block(int command, NOTIFY_NEW_LITE_BLOCK::request &arg,
                                                           CryptoNoteConnectionContext &context) {
  logger(Logging::DEBUGGING) << context << "NOTIFY_NEW_LITE_BLOCK (hop " << arg.hop << ")";
  updateObservedHeight(arg.current_blockchain_height, context);
  context.m_remote_blockchain_height = arg.current_blockchain_height;
  if (context.m_state != CryptoNoteConnectionContext::state_normal) {
    return 1;
  }

  return doPushLiteBlock(std::move(arg), context, {});
}

int CryptoNoteProtocolHandler::handle_notify_missing_txs(int command,  NOTIFY_MISSING_TXS::request &arg,
                                                        CryptoNoteConnectionContext &context) {
  logger(Logging::DEBUGGING) << context << "NOTIFY_MISSING_TXS";

  NOTIFY_NEW_TRANSACTIONS::request req;

  std::list<Transaction> txs;
  std::list<Crypto::Hash> missedHashes;
  m_core.getTransactions(arg.missing_txs, txs, missedHashes, true);
  if (!missedHashes.empty()) {
    logger(Logging::DEBUGGING) << "Failed to Handle NOTIFY_MISSING_TXS, Unable to retrieve requested "
      "transactions, Dropping Connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  } else {
    for (auto& tx : txs) {
      req.txs.push_back(asString(toBinaryArray(tx)));
    }
  }

  logger(Logging::DEBUGGING) << "--> NOTIFY_RESPONSE_MISSING_TXS: "
    << "txs.size() = " << req.txs.size();

  if (post_notify<NOTIFY_NEW_TRANSACTIONS>(*m_p2p, req, context)) {
    logger(Logging::DEBUGGING) << "NOTIFY_MISSING_TXS response sent to peer successfully";
  } else {
    logger(Logging::DEBUGGING) << "Error while sending NOTIFY_MISSING_TXS response to peer";
  }

  return 1;
}

void CryptoNoteProtocolHandler::relay_block(NOTIFY_NEW_BLOCK::request& arg) {
  // generate a lite block request from the received normal block
  NOTIFY_NEW_LITE_BLOCK::request lite_arg;
  lite_arg.current_blockchain_height = arg.current_blockchain_height;
  lite_arg.block = arg.b.block;
  lite_arg.hop = arg.hop;

  // encoding the request for sending the blocks to peers
  auto buf = LevinProtocol::encode(arg);
  auto lite_buf = LevinProtocol::encode(lite_arg);

  // logging the msg size to see the difference in payload size
  logger(Logging::DEBUGGING) << "NOTIFY_NEW_BLOCK - MSG_SIZE = " << buf.size();
  logger(Logging::DEBUGGING) << "NOTIFY_NEW_LITE_BLOCK - MSG_SIZE = " << lite_buf.size();

  std::list<boost::uuids::uuid> liteBlockConnections, normalBlockConnections;

  // sort the peers into their support categories
  m_p2p->for_each_connection([this, &liteBlockConnections, &normalBlockConnections](
    const CryptoNoteConnectionContext &ctx, uint64_t peerId) {
    if (ctx.version >= P2P_LITE_BLOCKS_PROPOGATION_VERSION) {
      logger(Logging::DEBUGGING) << ctx << "Peer supports lite-blocks... adding peer to lite block list";
      liteBlockConnections.push_back(ctx.m_connection_id);
    } else {
      logger(Logging::DEBUGGING) << ctx << "Peer doesn't support lite-blocks... adding peer to normal block list";
      normalBlockConnections.push_back(ctx.m_connection_id);
    }
  });

  // first send lite blocks as it's faster
  if (!liteBlockConnections.empty()) {
    m_p2p->externalRelayNotifyToList(NOTIFY_NEW_LITE_BLOCK::ID, lite_buf, liteBlockConnections);
  }

  if (!normalBlockConnections.empty()) {
    auto buf = LevinProtocol::encode(arg);
    m_p2p->externalRelayNotifyToAll(NOTIFY_NEW_BLOCK::ID, buf, nullptr);
  }
}

void CryptoNoteProtocolHandler::relay_transactions(NOTIFY_NEW_TRANSACTIONS::request& arg) {
  if (arg.stem && !m_dandelion_stem.empty()) { // Dandelion broadcast
    std::vector<Crypto::Hash> txHashes;
    for (auto tx_blob_it = arg.txs.begin(); tx_blob_it != arg.txs.end(); tx_blob_it++) {
      auto transactionBinary = asBinaryArray(*tx_blob_it);
      Crypto::Hash transactionHash = Crypto::cn_fast_hash(transactionBinary.data(), transactionBinary.size());
      if (!m_stemPool.hasTransaction(transactionHash)) {
        logger(Logging::DEBUGGING) << "Adding relayed transaction " << transactionHash << " to stempool";      
        auto txblob = *tx_blob_it;
        m_dispatcher.remoteSpawn([this, transactionHash, txblob] {
          m_stemPool.addTransaction(transactionHash, txblob);
        });
        txHashes.push_back(transactionHash);
      }
    }

    std::mt19937 rng = Random::generator();
    std::uniform_int_distribution<> dis(0, 100);
    auto coin_flip = dis(rng);
    if (coin_flip < CryptoNote::parameters::DANDELION_STEM_TX_PROPAGATION_PROBABILITY) { // Stem propagation
      for (const auto& dandelion_peer : m_dandelion_stem) {
        if (dandelion_peer.m_state == CryptoNoteConnectionContext::state_normal || dandelion_peer.m_state == CryptoNoteConnectionContext::state_synchronizing) {
          if (!post_notify<NOTIFY_NEW_TRANSACTIONS>(*m_p2p, arg, dandelion_peer)) {
            logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Failed to relay transactions to Dandelion peer " << dandelion_peer.m_connection_id << ", broadcasting in dandelion fluff mode";
            arg.stem = false;
            for (const auto& h : txHashes) {
              m_stemPool.removeTransaction(h);
              logger(Logging::DEBUGGING) << h;
            }

            auto buf = LevinProtocol::encode(arg);
            m_p2p->externalRelayNotifyToAll(NOTIFY_NEW_TRANSACTIONS::ID, buf, nullptr);
            break;
          }
        }
      }
    } else { // Switch to fluff broadcast
      arg.stem = false;
      logger(Logging::DEBUGGING) << "Switching to fluff broadcast of stem transactions:";
      for (const auto& h : txHashes) {
        m_stemPool.removeTransaction(h);
        logger(Logging::DEBUGGING) << h;
      }
      auto buf = LevinProtocol::encode(arg);
      m_p2p->externalRelayNotifyToAll(NOTIFY_NEW_TRANSACTIONS::ID, buf, nullptr);
    }
  } else { // Fluff broadcast
    logger(Logging::DEBUGGING) << "Not stem or no stem peers, fluff broadcast of transactions...";
    arg.stem = false;
    auto buf = LevinProtocol::encode(arg);
    m_p2p->externalRelayNotifyToAll(NOTIFY_NEW_TRANSACTIONS::ID, buf, nullptr);
  }
}

void CryptoNoteProtocolHandler::requestMissingPoolTransactions(const CryptoNoteConnectionContext& context) {
  if (context.version < CryptoNote::P2P_VERSION_1) {
    return;
  }

  auto poolTxs = m_core.getPoolTransactions();

  NOTIFY_REQUEST_TX_POOL::request notification;
  for (auto& tx : poolTxs) {
    notification.txs.emplace_back(getObjectHash(tx));
  }

  bool ok = post_notify<NOTIFY_REQUEST_TX_POOL>(*m_p2p, notification, context);
  if (!ok) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Failed to post notification NOTIFY_REQUEST_TX_POOL to " << context.m_connection_id;
  }
}

void CryptoNoteProtocolHandler::updateObservedHeight(uint32_t peerHeight, const CryptoNoteConnectionContext& context) {
  bool updated = false;
  {
    std::lock_guard<std::mutex> lock(m_observedHeightMutex);

    uint32_t height = m_observedHeight;
    if (peerHeight > context.m_remote_blockchain_height) {
      m_observedHeight = std::max(m_observedHeight, peerHeight);
      if (m_observedHeight != height) {
        updated = true;
      }
    } else if (peerHeight != context.m_remote_blockchain_height && context.m_remote_blockchain_height == m_observedHeight) {
      //the client switched to alternative chain and had maximum observed height. need to recalculate max height
      recalculateMaxObservedHeight(context);
      if (m_observedHeight != height) {
        updated = true;
      }
    }
  }

  if (updated) {
    logger(TRACE) << "Observed height updated: " << m_observedHeight;
    m_observerManager.notify(&ICryptoNoteProtocolObserver::lastKnownBlockHeightUpdated, m_observedHeight);
  }
}

void CryptoNoteProtocolHandler::recalculateMaxObservedHeight(const CryptoNoteConnectionContext& context) {
  //should be locked outside
  uint32_t peerHeight = 0;
  m_p2p->for_each_connection([&peerHeight, &context](const CryptoNoteConnectionContext& ctx, PeerIdType peerId) {
    if (ctx.m_connection_id != context.m_connection_id) {
      peerHeight = std::max(peerHeight, ctx.m_remote_blockchain_height);
    }
  });

  uint32_t localHeight = 0;
  Crypto::Hash ignore;
  m_core.get_blockchain_top(localHeight, ignore);
  m_observedHeight = std::max(peerHeight, localHeight + 1);
}

uint32_t CryptoNoteProtocolHandler::getObservedHeight() const {
  std::lock_guard<std::mutex> lock(m_observedHeightMutex);
  return m_observedHeight;
};

bool CryptoNoteProtocolHandler::addObserver(ICryptoNoteProtocolObserver* observer) {
  return m_observerManager.add(observer);
}

bool CryptoNoteProtocolHandler::removeObserver(ICryptoNoteProtocolObserver* observer) {
  return m_observerManager.remove(observer);
}

};
