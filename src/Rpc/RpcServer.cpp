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

#include "RpcServer.h"

#include <future>
#include <unordered_map>

// CryptoNote
#include "Common/StringTools.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/Core.h"
#include "CryptoNoteCore/Miner.h"
#include "CryptoNoteCore/TransactionExtra.h"

#include "CryptoNoteProtocol/CryptoNoteProtocolHandlerCommon.h"

#include "P2p/NetNode.h"

#include "CoreRpcServerErrorCodes.h"
#include "JsonRpc.h"

#undef ERROR

using namespace Logging;
using namespace Crypto;
using namespace Common;

namespace CryptoNote {

static inline void serialize(COMMAND_RPC_GET_BLOCKS_FAST::response& response, ISerializer &s) {
  KV_MEMBER(response.blocks)
  KV_MEMBER(response.start_height)
  KV_MEMBER(response.current_height)
  KV_MEMBER(response.status)
}

void serialize(BlockFullInfo& blockFullInfo, ISerializer& s) {
  KV_MEMBER(blockFullInfo.block_id);
  KV_MEMBER(blockFullInfo.block);
  s(blockFullInfo.transactions, "txs");
}

void serialize(TransactionPrefixInfo& transactionPrefixInfo, ISerializer& s) {
  KV_MEMBER(transactionPrefixInfo.txHash);
  KV_MEMBER(transactionPrefixInfo.txPrefix);
}

void serialize(BlockShortInfo& blockShortInfo, ISerializer& s) {
  KV_MEMBER(blockShortInfo.blockId);
  KV_MEMBER(blockShortInfo.block);
  KV_MEMBER(blockShortInfo.txPrefixes);
}

namespace {

template <typename Command>
RpcServer::HandlerFunction binMethod(bool (RpcServer::*handler)(typename Command::request const&, typename Command::response&)) {
  return [handler](RpcServer* obj, const HttpRequest& request, HttpResponse& response) {

    boost::value_initialized<typename Command::request> req;
    boost::value_initialized<typename Command::response> res;

    if (!loadFromBinaryKeyValue(static_cast<typename Command::request&>(req), request.getBody())) {
      return false;
    }

    bool result = (obj->*handler)(req, res);
    response.setBody(storeToBinaryKeyValue(res.data()));
    return result;
  };
}

template <typename Command>
RpcServer::HandlerFunction jsonMethod(bool (RpcServer::*handler)(typename Command::request const&, typename Command::response&)) {
  return [handler](RpcServer* obj, const HttpRequest& request, HttpResponse& response) {

    boost::value_initialized<typename Command::request> req;
    boost::value_initialized<typename Command::response> res;

    if (!loadFromJson(static_cast<typename Command::request&>(req), request.getBody())) {
      return false;
    }

    bool result = (obj->*handler)(req, res);
    response.setBody(storeToJson(res.data()));
    return result;
  };
}


}
  
std::unordered_map<std::string, RpcServer::RpcHandler<RpcServer::HandlerFunction>> RpcServer::s_handlers = {
  
  // binary handlers
  { "/getblocks.bin", { binMethod<COMMAND_RPC_GET_BLOCKS_FAST>(&RpcServer::on_get_blocks), false } },
  { "/queryblocks.bin", { binMethod<COMMAND_RPC_QUERY_BLOCKS>(&RpcServer::on_query_blocks), false } },
  { "/queryblockslite.bin", { binMethod<COMMAND_RPC_QUERY_BLOCKS_LITE>(&RpcServer::on_query_blocks_lite), false } },
  { "/get_o_indexes.bin", { binMethod<COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES>(&RpcServer::on_get_indexes), false } },
  { "/getrandom_outs.bin", { binMethod<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS>(&RpcServer::on_get_random_outs), false } },
  { "/get_pool_changes.bin", { binMethod<COMMAND_RPC_GET_POOL_CHANGES>(&RpcServer::onGetPoolChanges), false } },
  { "/get_pool_changes_lite.bin", { binMethod<COMMAND_RPC_GET_POOL_CHANGES_LITE>(&RpcServer::onGetPoolChangesLite), false } },
  { "/get_blocks_details_by_hashes.bin", { binMethod<COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES>(&RpcServer::onGetBlocksDetailsByHashes), false } },
  { "/get_blocks_hashes_by_timestamps.bin", { binMethod<COMMAND_RPC_GET_BLOCKS_HASHES_BY_TIMESTAMPS>(&RpcServer::onGetBlocksHashesByTimestamps), false } },
  { "/get_transaction_details_by_hashes.bin", { binMethod<COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASHES>(&RpcServer::onGetTransactionDetailsByHashes), false } },
  { "/get_transaction_hashes_by_payment_id.bin", { binMethod<COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID>(&RpcServer::onGetTransactionHashesByPaymentId), false } },

  // json handlers
  { "/getinfo", { jsonMethod<COMMAND_RPC_GET_INFO>(&RpcServer::on_get_info), true } },
  { "/getheight", { jsonMethod<COMMAND_RPC_GET_HEIGHT>(&RpcServer::on_get_height), true } },
  { "/gettransactions", { jsonMethod<COMMAND_RPC_GET_TRANSACTIONS>(&RpcServer::on_get_transactions), false } },
  { "/sendrawtransaction", { jsonMethod<COMMAND_RPC_SEND_RAW_TX>(&RpcServer::on_send_raw_tx), false } },
  { "/stop_daemon", { jsonMethod<COMMAND_RPC_STOP_DAEMON>(&RpcServer::on_stop_daemon), true } },

  // json rpc
  { "/json_rpc", { std::bind(&RpcServer::processJsonRpcRequest, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), true } }
};

RpcServer::RpcServer(System::Dispatcher& dispatcher, Logging::ILogger& log, Core& c, NodeServer& p2p, ICryptoNoteProtocolHandler& protocol) :
  HttpServer(dispatcher, log), logger(log, "RpcServer"), m_core(c), m_p2p(p2p), m_protocol(protocol) {
}

void RpcServer::processRequest(const HttpRequest& request, HttpResponse& response) {
  logger(TRACE) << "RPC request came: \n" << request << std::endl;

  auto url = request.getUrl();

  auto it = s_handlers.find(url);
  if (it == s_handlers.end()) {
    response.setStatus(HttpResponse::STATUS_404);
    return;
  }

  if (!it->second.allowBusyCore && !isCoreReady()) {
    response.setStatus(HttpResponse::STATUS_500);
    response.setBody("Core is busy");
    return;
  }

  it->second.handler(this, request, response);
}

bool RpcServer::processJsonRpcRequest(const HttpRequest& request, HttpResponse& response) {

  using namespace JsonRpc;

  response.addHeader("Content-Type", "application/json");

  JsonRpcRequest jsonRequest;
  JsonRpcResponse jsonResponse;

  try {
    logger(TRACE) << "JSON-RPC request: " << request.getBody();
    jsonRequest.parseRequest(request.getBody());
    jsonResponse.setId(jsonRequest.getId()); // copy id

    static std::unordered_map<std::string, RpcServer::RpcHandler<JsonMemberMethod>> jsonRpcHandlers = {
      { "getblockcount", { makeMemberMethod(&RpcServer::on_getblockcount), true } },
      { "on_getblockhash", { makeMemberMethod(&RpcServer::on_getblockhash), false } },
      { "getblocktemplate", { makeMemberMethod(&RpcServer::on_getblocktemplate), false } },
      { "getcurrencyid", { makeMemberMethod(&RpcServer::on_get_currency_id), true } },
      { "submitblock", { makeMemberMethod(&RpcServer::on_submitblock), false } },
      { "getlastblockheader", { makeMemberMethod(&RpcServer::on_get_last_block_header), false } },
      { "getblockheaderbyhash", { makeMemberMethod(&RpcServer::on_get_block_header_by_hash), false } },
      { "getblockheaderbyheight", { makeMemberMethod(&RpcServer::on_get_block_header_by_height), false } }
    };

    auto it = jsonRpcHandlers.find(jsonRequest.getMethod());
    if (it == jsonRpcHandlers.end()) {
      throw JsonRpcError(JsonRpc::errMethodNotFound);
    }

    if (!it->second.allowBusyCore && !isCoreReady()) {
      throw JsonRpcError(CORE_RPC_ERROR_CODE_CORE_BUSY, "Core is busy");
    }

    it->second.handler(this, jsonRequest, jsonResponse);

  } catch (const JsonRpcError& err) {
    jsonResponse.setError(err);
  } catch (const std::exception& e) {
    jsonResponse.setError(JsonRpcError(JsonRpc::errInternalError, e.what()));
  }

  response.setBody(jsonResponse.getBody());
  logger(TRACE) << "JSON-RPC response: " << jsonResponse.getBody();
  return true;
}

bool RpcServer::isCoreReady() {
  return m_core.getCurrency().isTestnet() || m_p2p.get_payload_object().isSynchronized();
}

//
// Binary handlers
//

bool RpcServer::on_get_blocks(const COMMAND_RPC_GET_BLOCKS_FAST::request& req, COMMAND_RPC_GET_BLOCKS_FAST::response& res) {
  // TODO code duplication see InProcessNode::doGetNewBlocks()
  if (req.block_ids.empty()) {
    res.status = "Failed";
    return false;
  }

  if (req.block_ids.back() != m_core.getBlockHashByIndex(0)) {
    res.status = "Failed";
    return false;
  }

  uint32_t totalBlockCount;
  uint32_t startBlockIndex;
  std::vector<Crypto::Hash> supplement = m_core.findBlockchainSupplement(req.block_ids, COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT, totalBlockCount, startBlockIndex);

  res.current_height = totalBlockCount;
  res.start_height = startBlockIndex;

  std::vector<Crypto::Hash> missedHashes;
  m_core.getBlocks(supplement, res.blocks, missedHashes);
  assert(missedHashes.empty());

  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_query_blocks(const COMMAND_RPC_QUERY_BLOCKS::request& req, COMMAND_RPC_QUERY_BLOCKS::response& res) {
  uint32_t startIndex;
  uint32_t currentIndex;
  uint32_t fullOffset;

  if (!m_core.queryBlocks(req.block_ids, req.timestamp, startIndex, currentIndex, fullOffset, res.items)) {
    res.status = "Failed to perform query";
    return false;
  }

  res.start_height = startIndex;
  res.current_height = currentIndex;
  res.full_offset = fullOffset;
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_query_blocks_lite(const COMMAND_RPC_QUERY_BLOCKS_LITE::request& req, COMMAND_RPC_QUERY_BLOCKS_LITE::response& res) {
  uint32_t startIndex;
  uint32_t currentIndex;
  uint32_t fullOffset;
  if (!m_core.queryBlocksLite(req.blockIds, req.timestamp, startIndex, currentIndex, fullOffset, res.items)) {
    res.status = "Failed to perform query";
    return false;
  }

  res.startHeight = startIndex;
  res.currentHeight = currentIndex;
  res.fullOffset = fullOffset;
  res.status = CORE_RPC_STATUS_OK;

  return true;
}

bool RpcServer::on_get_indexes(const COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::request& req, COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response& res) {
  std::vector<uint32_t> outputIndexes;
  if (!m_core.getTransactionGlobalIndexes(req.txid, outputIndexes)) {
    res.status = "Failed";
    return true;
  }

  res.o_indexes.assign(outputIndexes.begin(), outputIndexes.end());
  res.status = CORE_RPC_STATUS_OK;
  logger(TRACE) << "COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES: [" << res.o_indexes.size() << "]";
  return true;
}

bool RpcServer::on_get_random_outs(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res) {
  res.status = "Failed";

  for (uint64_t amount : req.amounts) {
    std::vector<uint32_t> globalIndexes;
    std::vector<Crypto::PublicKey> publicKeys;
    if (!m_core.getRandomOutputs(amount, static_cast<uint16_t>(req.outs_count), globalIndexes, publicKeys)) {
      return true;
    }

    assert(globalIndexes.size() == publicKeys.size());
    res.outs.emplace_back(COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_outs_for_amount{amount, {}});
    for (size_t i = 0; i < globalIndexes.size(); ++i) {
      res.outs.back().outs.push_back({globalIndexes[i], publicKeys[i]});
    }
  }

  res.status = CORE_RPC_STATUS_OK;

  std::stringstream ss;
  typedef COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount outs_for_amount;
  typedef COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry out_entry;

  std::for_each(res.outs.begin(), res.outs.end(), [&](outs_for_amount& ofa)  {
    ss << "[" << ofa.amount << "]:";

    assert(ofa.outs.size() && "internal error: ofa.outs.size() is empty");

    std::for_each(ofa.outs.begin(), ofa.outs.end(), [&](out_entry& oe)
    {
      ss << oe.global_amount_index << " ";
    });
    ss << ENDL;
  });
  std::string s = ss.str();
  logger(TRACE) << "COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS: " << ENDL << s;
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::onGetPoolChanges(const COMMAND_RPC_GET_POOL_CHANGES::request& req, COMMAND_RPC_GET_POOL_CHANGES::response& rsp) {
  rsp.status = CORE_RPC_STATUS_OK;
  rsp.isTailBlockActual = m_core.getPoolChanges(req.tailBlockId, req.knownTxsIds, rsp.addedTxs, rsp.deletedTxsIds);

  return true;
}

bool RpcServer::onGetPoolChangesLite(const COMMAND_RPC_GET_POOL_CHANGES_LITE::request& req, COMMAND_RPC_GET_POOL_CHANGES_LITE::response& rsp) {
  rsp.status = CORE_RPC_STATUS_OK;
  rsp.isTailBlockActual = m_core.getPoolChangesLite(req.tailBlockId, req.knownTxsIds, rsp.addedTxs, rsp.deletedTxsIds);

  return true;
}

bool RpcServer::onGetBlocksDetailsByHashes(const COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES::request& req, COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES::response& rsp) {
  try {
    std::vector<BlockDetails> blockDetails;
    for (const Crypto::Hash& hash : req.blockHashes) {
      blockDetails.push_back(m_core.getBlockDetails(hash));
    }

    rsp.blocks = std::move(blockDetails);
  } catch (std::system_error& e) {
    rsp.status = e.what();
    return false;
  } catch (std::exception& e) {
    rsp.status = "Error: " + std::string(e.what());
    return false;
  }

  rsp.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::onGetBlocksHashesByTimestamps(const COMMAND_RPC_GET_BLOCKS_HASHES_BY_TIMESTAMPS::request& req, COMMAND_RPC_GET_BLOCKS_HASHES_BY_TIMESTAMPS::response& rsp) {
  try {
    auto blockHashes = m_core.getBlockHashesByTimestamps(req.timestampBegin, req.secondsCount);
    rsp.blockHashes = std::move(blockHashes);
  } catch (std::system_error& e) {
    rsp.status = e.what();
    return false;
  } catch (std::exception& e) {
    rsp.status = "Error: " + std::string(e.what());
    return false;
  }

  rsp.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::onGetTransactionDetailsByHashes(const COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASHES::request& req, COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASHES::response& rsp) {
  try {
    std::vector<TransactionDetails> transactionDetails;
    transactionDetails.reserve(req.transactionHashes.size());

    for (const auto& hash: req.transactionHashes) {
      transactionDetails.push_back(m_core.getTransactionDetails(hash));
    }

    rsp.transactions = std::move(transactionDetails);
  } catch (std::system_error& e) {
    rsp.status = e.what();
    return false;
  } catch (std::exception& e) {
    rsp.status = "Error: " + std::string(e.what());
    return false;
  }

  rsp.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::onGetTransactionHashesByPaymentId(const COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID::request& req, COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID::response& rsp) {
  try {
    rsp.transactionHashes = m_core.getTransactionHashesByPaymentId(req.paymentId);
  } catch (std::system_error& e) {
    rsp.status = e.what();
    return false;
  } catch (std::exception& e) {
    rsp.status = "Error: " + std::string(e.what());
    return false;
  }

  rsp.status = CORE_RPC_STATUS_OK;
  return true;
}

//
// JSON handlers
//

bool RpcServer::on_get_info(const COMMAND_RPC_GET_INFO::request& req, COMMAND_RPC_GET_INFO::response& res) {
  res.height = m_core.getTopBlockIndex();
  res.difficulty = m_core.getDifficultyForNextBlock();
  res.tx_count = m_core.getBlockchainTransactionCount() - res.height; //without coinbase
  res.tx_pool_size = m_core.getPoolTransactionCount();
  res.alt_blocks_count = m_core.getAlternativeBlockCount();
  uint64_t total_conn = m_p2p.get_connections_count();
  res.outgoing_connections_count = m_p2p.get_outgoing_connections_count();
  res.incoming_connections_count = total_conn - res.outgoing_connections_count;
  res.white_peerlist_size = m_p2p.getPeerlistManager().get_white_peers_count();
  res.grey_peerlist_size = m_p2p.getPeerlistManager().get_gray_peers_count();
  res.last_known_block_index = std::max(static_cast<uint32_t>(1), m_protocol.getObservedHeight()) - 1;
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_get_height(const COMMAND_RPC_GET_HEIGHT::request& req, COMMAND_RPC_GET_HEIGHT::response& res) {
  res.height = m_core.getTopBlockIndex();
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_get_transactions(const COMMAND_RPC_GET_TRANSACTIONS::request& req, COMMAND_RPC_GET_TRANSACTIONS::response& res) {
  std::vector<Hash> vh;
  for (const auto& tx_hex_str : req.txs_hashes) {
    BinaryArray b;
    if (!fromHex(tx_hex_str, b)) {
      res.status = "Failed to parse hex representation of transaction hash";
      return true;
    }

    if (b.size() != sizeof(Hash)) {
      res.status = "Failed, size of data mismatch";
    }

    vh.push_back(*reinterpret_cast<const Hash*>(b.data()));
  }

  std::vector<Hash> missed_txs;
  std::vector<BinaryArray> txs;
  m_core.getTransactions(vh, txs, missed_txs);

  for (auto& tx : txs) {
    res.txs_as_hex.push_back(toHex(tx));
  }

  for (const auto& miss_tx : missed_txs) {
    res.missed_tx.push_back(Common::podToHex(miss_tx));
  }

  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_send_raw_tx(const COMMAND_RPC_SEND_RAW_TX::request& req, COMMAND_RPC_SEND_RAW_TX::response& res) {
  std::vector<BinaryArray> transactions(1);
  if (!fromHex(req.tx_as_hex, transactions.back())) {
    logger(INFO) << "[on_send_raw_tx]: Failed to parse tx from hexbuff: " << req.tx_as_hex;
    res.status = "Failed";
    return true;
  }

  Crypto::Hash transactionHash = Crypto::cn_fast_hash(transactions.back().data(), transactions.back().size());
  logger(DEBUGGING) << "transaction " << transactionHash << " came in on_send_raw_tx";

  if (!m_core.addTransactionToPool(transactions.back())) {
    logger(INFO) << "[on_send_raw_tx]: tx verification failed";
    res.status = "Failed";
    return true;
  }

  m_protocol.relayTransactions(transactions);
  //TODO: make sure that tx has reached other nodes here, probably wait to receive reflections from other nodes
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_stop_daemon(const COMMAND_RPC_STOP_DAEMON::request& req, COMMAND_RPC_STOP_DAEMON::response& res) {
  if (m_core.getCurrency().isTestnet()) {
    m_p2p.sendStopSignal();
    res.status = CORE_RPC_STATUS_OK;
  } else {
    res.status = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
    return false;
  }

  return true;
}

//------------------------------------------------------------------------------------------------------------------------------
// JSON RPC methods
//------------------------------------------------------------------------------------------------------------------------------
bool RpcServer::on_getblockcount(const COMMAND_RPC_GETBLOCKCOUNT::request& req, COMMAND_RPC_GETBLOCKCOUNT::response& res) {
  res.count = m_core.getTopBlockIndex() + 1;
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_getblockhash(const COMMAND_RPC_GETBLOCKHASH::request& req, COMMAND_RPC_GETBLOCKHASH::response& res) {
  if (req.size() != 1) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_PARAM, "Wrong parameters, expected height" };
  }

  uint32_t h = static_cast<uint32_t>(req[0]);
  Crypto::Hash blockId = m_core.getBlockHashByIndex(h - 1);
  if (blockId == NULL_HASH) {
    throw JsonRpc::JsonRpcError{ 
      CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT,
      std::string("Too big height: ") + std::to_string(h) + ", current blockchain height = " + std::to_string(m_core.getTopBlockIndex() + 1)
    };
  }

  res = Common::podToHex(blockId);
  return true;
}

namespace {
  uint64_t slow_memmem(void* start_buff, size_t buflen, void* pat, size_t patlen)
  {
    void* buf = start_buff;
    void* end = (char*)buf + buflen - patlen;
    while ((buf = memchr(buf, ((char*)pat)[0], buflen)))
    {
      if (buf>end)
        return 0;
      if (memcmp(buf, pat, patlen) == 0)
        return (char*)buf - (char*)start_buff;
      buf = (char*)buf + 1;
    }
    return 0;
  }
}

bool RpcServer::on_getblocktemplate(const COMMAND_RPC_GETBLOCKTEMPLATE::request& req, COMMAND_RPC_GETBLOCKTEMPLATE::response& res) {
  if (req.reserve_size > TX_EXTRA_NONCE_MAX_COUNT) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_TOO_BIG_RESERVE_SIZE, "To big reserved size, maximum 255" };
  }

  AccountPublicAddress acc = boost::value_initialized<AccountPublicAddress>();

  if (!req.wallet_address.size() || !m_core.getCurrency().parseAccountAddressString(req.wallet_address, acc)) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_WALLET_ADDRESS, "Failed to parse wallet address" };
  }

  BlockTemplate blockTemplate = boost::value_initialized<BlockTemplate>();
  CryptoNote::BinaryArray blob_reserve;
  blob_reserve.resize(req.reserve_size, 0);

  if (!m_core.getBlockTemplate(blockTemplate, acc, blob_reserve, res.difficulty, res.height)) {
    logger(ERROR) << "Failed to create block template";
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Internal error: failed to create block template" };
  }

  BinaryArray block_blob = toBinaryArray(blockTemplate);
  PublicKey tx_pub_key = CryptoNote::getTransactionPublicKeyFromExtra(blockTemplate.baseTransaction.extra);
  if (tx_pub_key == NULL_PUBLIC_KEY) {
    logger(ERROR) << "Failed to find tx pub key in coinbase extra";
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Internal error: failed to find tx pub key in coinbase extra" };
  }

  if (0 < req.reserve_size) {
    res.reserved_offset = slow_memmem((void*)block_blob.data(), block_blob.size(), &tx_pub_key, sizeof(tx_pub_key));
    if (!res.reserved_offset) {
      logger(ERROR) << "Failed to find tx pub key in blockblob";
      throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Internal error: failed to create block template" };
    }
    res.reserved_offset += sizeof(tx_pub_key) + 3; //3 bytes: tag for TX_EXTRA_TAG_PUBKEY(1 byte), tag for TX_EXTRA_NONCE(1 byte), counter in TX_EXTRA_NONCE(1 byte)
    if (res.reserved_offset + req.reserve_size > block_blob.size()) {
      logger(ERROR) << "Failed to calculate offset for reserved bytes";
      throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Internal error: failed to create block template" };
    }
  } else {
    res.reserved_offset = 0;
  }

  res.blocktemplate_blob = toHex(block_blob);
  res.status = CORE_RPC_STATUS_OK;

  return true;
}

bool RpcServer::on_get_currency_id(const COMMAND_RPC_GET_CURRENCY_ID::request& /*req*/, COMMAND_RPC_GET_CURRENCY_ID::response& res) {
  Hash genesisBlockHash = m_core.getCurrency().genesisBlockHash();
  res.currency_id_blob = Common::podToHex(genesisBlockHash);
  return true;
}

bool RpcServer::on_submitblock(const COMMAND_RPC_SUBMITBLOCK::request& req, COMMAND_RPC_SUBMITBLOCK::response& res) {
  if (req.size() != 1) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_PARAM, "Wrong param" };
  }

  BinaryArray blockblob;
  if (!fromHex(req[0], blockblob)) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB, "Wrong block blob" };
  }

  auto blockToSend = blockblob;
  auto submitResult = m_core.submitBlock(std::move(blockblob));
  if (submitResult != error::AddBlockErrorCondition::BLOCK_ADDED) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_BLOCK_NOT_ACCEPTED, "Block not accepted" };
  }

  if (submitResult == error::AddBlockErrorCode::ADDED_TO_MAIN
      || submitResult == error::AddBlockErrorCode::ADDED_TO_ALTERNATIVE_AND_SWITCHED) {
    NOTIFY_NEW_BLOCK::request newBlockMessage;
    newBlockMessage.b = prepareRawBlockLegacy(std::move(blockToSend));
    newBlockMessage.hop = 0;
  newBlockMessage.current_blockchain_height = m_core.getTopBlockIndex();

    m_protocol.relayBlock(newBlockMessage);
  }

  res.status = CORE_RPC_STATUS_OK;
  return true;
}

RawBlockLegacy RpcServer::prepareRawBlockLegacy(BinaryArray&& blockBlob) {
  BlockTemplate blockTemplate;
  bool result = fromBinaryArray(blockTemplate, blockBlob);
  assert(result);

  RawBlockLegacy rawBlock;
  rawBlock.block = std::move(blockBlob);

  if (blockTemplate.transactionHashes.empty()) {
    return rawBlock;
  }

  rawBlock.transactions.reserve(blockTemplate.transactionHashes.size());
  std::vector<Crypto::Hash> missedTransactions;
  m_core.getTransactions(blockTemplate.transactionHashes, rawBlock.transactions, missedTransactions);
  assert(missedTransactions.empty());

  return rawBlock;
}

namespace {

uint64_t get_block_reward(const BlockTemplate& blk) {
  uint64_t reward = 0;
  for (const TransactionOutput& out : blk.baseTransaction.outputs) {
    reward += out.amount;
  }

  return reward;
}

}

void RpcServer::fill_block_header_response(const BlockTemplate& blk, bool orphan_status, uint32_t index, const Hash& hash, block_header_response& response) {
  response.major_version = blk.majorVersion;
  response.minor_version = blk.minorVersion;
  response.timestamp = blk.timestamp;
  response.prev_hash = Common::podToHex(blk.previousBlockHash);
  response.nonce = blk.nonce;
  response.orphan_status = orphan_status;
  response.height = index;
  response.depth = m_core.getTopBlockIndex() - index;
  response.hash = Common::podToHex(hash);
  response.difficulty = m_core.getBlockDifficulty(index);
  response.reward = get_block_reward(blk);
}

bool RpcServer::on_get_last_block_header(const COMMAND_RPC_GET_LAST_BLOCK_HEADER::request& req, COMMAND_RPC_GET_LAST_BLOCK_HEADER::response& res) {
  auto topBlock = m_core.getBlockByHash(m_core.getTopBlockHash());  
  fill_block_header_response(topBlock, false, m_core.getTopBlockIndex(), m_core.getTopBlockHash(), res.block_header);
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_get_block_header_by_hash(const COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::request& req, COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::response& res) {
  Hash blockHash;
  if (!parse_hash256(req.hash, blockHash)) {
    throw JsonRpc::JsonRpcError{
      CORE_RPC_ERROR_CODE_WRONG_PARAM,
      "Failed to parse hex representation of block hash. Hex = " + req.hash + '.' };
  }

  if (!m_core.hasBlock(blockHash)) {
    throw JsonRpc::JsonRpcError{
      CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
      "Internal error: can't get block by hash. Hash = " + req.hash + '.' };
  }

  auto block = m_core.getBlockByHash(blockHash);
  CachedBlock cachedBlock(block);
  assert(block.baseTransaction.inputs.front().type() != typeid(BaseInput));

  fill_block_header_response(block, false, cachedBlock.getBlockIndex(), cachedBlock.getBlockHash(), res.block_header);
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_get_block_header_by_height(const COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::request& req, COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::response& res) {
  if (m_core.getTopBlockIndex() + 1 < req.height) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT,
      std::string("To big height: ") + std::to_string(req.height) + ", current blockchain height = " + std::to_string(m_core.getTopBlockIndex() + 1) };
  }

  uint32_t index = static_cast<uint32_t>(req.height);
  auto block = m_core.getBlockByIndex(index);
  CachedBlock cachedBlock(block);
  assert(cachedBlock.getBlockIndex() == req.height);
  fill_block_header_response(block, false, index, cachedBlock.getBlockHash(), res.block_header);
  res.status = CORE_RPC_STATUS_OK;
  return true;
}


}
