// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016, The Forknote developers
// Copyright (c) 2017-2019, The Karbo developers
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

#pragma once

#include "CryptoNoteProtocol/CryptoNoteProtocolDefinitions.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteCore/Difficulty.h"
#include "crypto/hash.h"

#include "Serialization/SerializationOverloads.h"
#include "Serialization/BlockchainExplorerDataSerialization.h"

namespace CryptoNote {
//-----------------------------------------------
#define CORE_RPC_STATUS_OK "OK"
#define CORE_RPC_STATUS_BUSY "BUSY"

struct EMPTY_STRUCT {
  void serialize(ISerializer &s) {}
};

struct STATUS_STRUCT {
  std::string status;

  void serialize(ISerializer &s) {
    KV_MEMBER(status)
  }
};

struct COMMAND_RPC_GET_HEIGHT {
  typedef EMPTY_STRUCT request;

  struct response {
    uint32_t height;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(height)
      KV_MEMBER(status)
    }
  };
};

struct COMMAND_RPC_GET_BLOCKS_FAST {

  struct request {
    std::vector<Crypto::Hash> block_ids; //*first 10 blocks id goes sequential, next goes in pow(2,n) offset, like 2, 4, 8, 16, 32, 64 and so on, and the last one is always genesis block */
    
    void serialize(ISerializer &s) {
      serializeAsBinary(block_ids, "block_ids", s);
    }
  };

  struct response {
    std::vector<block_complete_entry> blocks;
    uint32_t start_height;
    uint32_t current_height;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(blocks)
      KV_MEMBER(start_height)
      KV_MEMBER(current_height)
      KV_MEMBER(status)
    }
  };
};
//-----------------------------------------------
struct COMMAND_RPC_GET_TRANSACTIONS {
  struct request {
    std::vector<std::string> txs_hashes;

    void serialize(ISerializer &s) {
      KV_MEMBER(txs_hashes)
    }
  };

  struct response {
    std::vector<std::string> txs_as_hex;  // transactions blobs as hex
    std::vector<std::string> missed_txs;  // not found transactions
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(txs_as_hex)
      KV_MEMBER(missed_txs)
      KV_MEMBER(status)    
    }
  };
};
//-----------------------------------------------
struct COMMAND_RPC_GET_POOL_CHANGES {
  struct request {
    Crypto::Hash tailBlockId;
    std::vector<Crypto::Hash> knownTxsIds;

    void serialize(ISerializer &s) {
      KV_MEMBER(tailBlockId)
      serializeAsBinary(knownTxsIds, "knownTxsIds", s);
    }
  };

  struct response {
    bool isTailBlockActual;
    std::vector<BinaryArray> addedTxs;       // Added transactions blobs
    std::vector<Crypto::Hash> deletedTxsIds; // IDs of not found transactions
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(isTailBlockActual)
      KV_MEMBER(addedTxs)
      serializeAsBinary(deletedTxsIds, "deletedTxsIds", s);
      KV_MEMBER(status)
    }
  };
};

struct COMMAND_RPC_GET_POOL_CHANGES_LITE {
  struct request {
    Crypto::Hash tailBlockId;
    std::vector<Crypto::Hash> knownTxsIds;

    void serialize(ISerializer &s) {
      KV_MEMBER(tailBlockId)
      serializeAsBinary(knownTxsIds, "knownTxsIds", s);
    }
  };

  struct response {
    bool isTailBlockActual;
    std::vector<TransactionPrefixInfo> addedTxs; // Added transactions blobs
    std::vector<Crypto::Hash> deletedTxsIds;     // IDs of not found transactions
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(isTailBlockActual)
      KV_MEMBER(addedTxs)
      serializeAsBinary(deletedTxsIds, "deletedTxsIds", s);
      KV_MEMBER(status)
    }
  };
};

//-----------------------------------------------
struct COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES {
  
  struct request {
    Crypto::Hash txid;

    void serialize(ISerializer &s) {
      KV_MEMBER(txid)
    }
  };

  struct response {
    std::vector<uint64_t> o_indexes;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(o_indexes)
      KV_MEMBER(status)
    }
  };
};
//-----------------------------------------------
struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request {
  std::vector<uint64_t> amounts;
  uint64_t outs_count;

  void serialize(ISerializer &s) {
    KV_MEMBER(amounts)
    KV_MEMBER(outs_count)
  }
};

#pragma pack(push, 1)
struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_out_entry {
  uint64_t global_amount_index;
  Crypto::PublicKey out_key;
};
#pragma pack(pop)

struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_outs_for_amount {
  uint64_t amount;
  std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_out_entry> outs;

  void serialize(ISerializer &s) {
    KV_MEMBER(amount)
    serializeAsBinary(outs, "outs", s);
  }
};

struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response {
  std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_outs_for_amount> outs;
  std::string status;

  void serialize(ISerializer &s) {
    KV_MEMBER(outs);
    KV_MEMBER(status)
  }
};

struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS {
  typedef COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request request;
  typedef COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response response;

  typedef COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_out_entry out_entry;
  typedef COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_outs_for_amount outs_for_amount;
};

//-----------------------------------------------
struct COMMAND_RPC_SEND_RAW_TRANSACTION {
  struct request {
    std::string tx_as_hex;

    request() {}
    explicit request(const Transaction &);

    void serialize(ISerializer &s) {
      KV_MEMBER(tx_as_hex)
    }
  };

  struct response {
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(status)
    }
  };
};
//-----------------------------------------------
struct COMMAND_RPC_START_MINING {
  struct request {
    std::string miner_address;
    uint64_t threads_count;

    void serialize(ISerializer &s) {
      KV_MEMBER(miner_address)
      KV_MEMBER(threads_count)
    }
  };

  struct response {
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(status)
    }
  };
};

struct COMMAND_HTTP {
  typedef EMPTY_STRUCT request;

  typedef std::string response;
};

//-----------------------------------------------
struct COMMAND_RPC_GET_INFO {
  typedef EMPTY_STRUCT request;

  struct response {
    std::string status;
    std::string version;
    uint32_t height;
    std::string top_block_hash;
    uint64_t difficulty;
    uint64_t cumulative_difficulty;
    uint64_t max_cumulative_block_size;
    uint64_t next_reward;
    uint64_t min_fee;
    uint64_t transactions_count;
    uint64_t transactions_pool_size;
    uint64_t alt_blocks_count;
    uint64_t outgoing_connections_count;
    uint64_t incoming_connections_count;
    uint64_t rpc_connections_count;
    uint64_t white_peerlist_size;
    uint64_t grey_peerlist_size;
    uint32_t last_known_block_index;
    uint64_t start_time;
    uint8_t block_major_version;
    std::string already_generated_coins;
    std::string contact;   

    void serialize(ISerializer &s) {
      KV_MEMBER(status)
      KV_MEMBER(version)
      KV_MEMBER(height)
      KV_MEMBER(top_block_hash)
      KV_MEMBER(difficulty)
      KV_MEMBER(cumulative_difficulty)
      KV_MEMBER(max_cumulative_block_size)
      KV_MEMBER(next_reward)
      KV_MEMBER(min_fee)
      KV_MEMBER(transactions_count)
      KV_MEMBER(transactions_pool_size)
      KV_MEMBER(alt_blocks_count)
      KV_MEMBER(outgoing_connections_count)
      KV_MEMBER(incoming_connections_count)
      KV_MEMBER(rpc_connections_count)
      KV_MEMBER(white_peerlist_size)
      KV_MEMBER(grey_peerlist_size)
      KV_MEMBER(last_known_block_index)
      KV_MEMBER(start_time)
      KV_MEMBER(block_major_version)
      KV_MEMBER(already_generated_coins)
      KV_MEMBER(contact)      
    }
  };
};

//-----------------------------------------------
struct COMMAND_RPC_STOP_MINING {
  typedef EMPTY_STRUCT request;
  typedef STATUS_STRUCT response;
};

//-----------------------------------------------
struct COMMAND_RPC_STOP_DAEMON {
  typedef EMPTY_STRUCT request;
  typedef STATUS_STRUCT response;
};

//-----------------------------------------------
struct COMMAND_RPC_GET_PEER_LIST {
  typedef EMPTY_STRUCT request;

  struct response {
    std::vector<std::string> anchor_peers;
    std::vector<std::string> white_peers;
    std::vector<std::string> gray_peers;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(anchor_peers)
      KV_MEMBER(white_peers)
      KV_MEMBER(gray_peers)
      KV_MEMBER(status)
    }
  };
};

//-----------------------------------------------
struct p2p_connection_entry
{
  uint8_t version;
  std::string state;
  std::string connection_id;
  std::string remote_ip;
  uint32_t remote_port = 0;
  bool is_incoming = false;
  uint64_t started = 0;
  uint32_t remote_blockchain_height = 0;
  uint32_t last_response_height = 0;

  void serialize(ISerializer& s)
  {
    KV_MEMBER(version)
    KV_MEMBER(state)
    KV_MEMBER(connection_id)
    KV_MEMBER(remote_ip)
    KV_MEMBER(remote_port)
    KV_MEMBER(is_incoming)
    KV_MEMBER(started)
    KV_MEMBER(remote_blockchain_height)
    KV_MEMBER(last_response_height)
  }
};

struct COMMAND_RPC_GET_CONNECTIONS {
  typedef EMPTY_STRUCT request;

  struct response {
    std::vector<p2p_connection_entry> connections;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(connections)
      KV_MEMBER(status)
    }
  };
};

//-----------------------------------------------
struct COMMAND_RPC_GET_FEE_ADDRESS {
  typedef EMPTY_STRUCT request;

  struct response {
    std::string fee_address;
    uint64_t    fee_amount;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(fee_address)
      KV_MEMBER(fee_amount)
      KV_MEMBER(status)
    }
  };
};

struct COMMAND_RPC_GETBLOCKCOUNT {
  typedef std::vector<std::string> request;

  struct response {
    uint64_t count;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(count)
      KV_MEMBER(status)
    }
  };
};

struct COMMAND_RPC_GETBLOCKHASH {
  typedef std::vector<uint64_t> request;
  typedef std::string response;
};

struct COMMAND_RPC_GETBLOCKTEMPLATE {
  struct request {
    uint64_t reserve_size; //max 255 bytes
    std::string wallet_address;

    void serialize(ISerializer &s) {
      KV_MEMBER(reserve_size)
      KV_MEMBER(wallet_address)
    }
  };

  struct response {
    uint64_t difficulty;
    uint32_t height;
    uint64_t reserved_offset;
    std::string blocktemplate_blob;
	std::string blockhashing_blob;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(difficulty)
      KV_MEMBER(height)
      KV_MEMBER(reserved_offset)
      KV_MEMBER(blocktemplate_blob)
	  KV_MEMBER(blockhashing_blob)
      KV_MEMBER(status)
    }
  };
};

struct COMMAND_RPC_GET_CURRENCY_ID {
  typedef EMPTY_STRUCT request;

  struct response {
    std::string currency_id_blob;

    void serialize(ISerializer &s) {
      KV_MEMBER(currency_id_blob)
    }
  };
};

struct COMMAND_RPC_SUBMITBLOCK {
  typedef std::vector<std::string> request;
  typedef STATUS_STRUCT response;
};

struct block_header_response {
  uint8_t major_version;
  uint8_t minor_version;
  uint64_t timestamp;
  std::string prev_hash;
  uint32_t nonce;
  bool orphan_status;
  uint32_t height;
  uint32_t depth;
  std::string hash;
  difficulty_type difficulty;
  uint64_t reward;

  void serialize(ISerializer &s) {
    KV_MEMBER(major_version)
    KV_MEMBER(minor_version)
    KV_MEMBER(timestamp)
    KV_MEMBER(prev_hash)
    KV_MEMBER(nonce)
    KV_MEMBER(orphan_status)
    KV_MEMBER(height)
    KV_MEMBER(depth)
    KV_MEMBER(hash)
    KV_MEMBER(difficulty)
    KV_MEMBER(reward)
  }
};

struct BLOCK_HEADER_RESPONSE {
  std::string status;
  block_header_response block_header;

  void serialize(ISerializer &s) {
    KV_MEMBER(block_header)
    KV_MEMBER(status)
  }
};

struct transaction_short_response {
  std::string hash;
  uint64_t fee;
  uint64_t amount_out;
  uint64_t size;

  void serialize(ISerializer &s) {
    KV_MEMBER(hash)
    KV_MEMBER(fee)
    KV_MEMBER(amount_out)
    KV_MEMBER(size)
  }
};

struct transaction_pool_response {
  std::string hash;
  uint64_t fee;
  uint64_t amount_out;
  uint64_t size;
  uint64_t receive_time;

  void serialize(ISerializer &s) {
    KV_MEMBER(hash)
    KV_MEMBER(fee)
    KV_MEMBER(amount_out)
    KV_MEMBER(size)
    KV_MEMBER(receive_time)
  }
};

struct block_short_response {
  uint64_t timestamp;
  uint32_t height;
  std::string hash;
  uint64_t transactions_count;
  uint64_t cumulative_size;
  difficulty_type difficulty;
  uint64_t min_fee;

  void serialize(ISerializer &s) {
    KV_MEMBER(timestamp)
    KV_MEMBER(height)
    KV_MEMBER(hash)
    KV_MEMBER(cumulative_size)
    KV_MEMBER(transactions_count)
    KV_MEMBER(difficulty)
    KV_MEMBER(min_fee)
  }
};

struct COMMAND_RPC_GET_LAST_BLOCK_HEADER {
  typedef EMPTY_STRUCT request;
  typedef BLOCK_HEADER_RESPONSE response;
};

struct COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH {
  struct request {
    std::string hash;

    void serialize(ISerializer &s) {
      KV_MEMBER(hash)
    }
  };

  typedef BLOCK_HEADER_RESPONSE response;
};

struct COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT {
  struct request {
    uint32_t height;

    void serialize(ISerializer &s) {
      KV_MEMBER(height)
    }
  };

  typedef BLOCK_HEADER_RESPONSE response;
};

struct COMMAND_RPC_GET_BLOCK_TIMESTAMP_BY_HEIGHT {
  struct request {
    uint32_t height;

    void serialize(ISerializer &s) {
      KV_MEMBER(height)
    }
  };

  struct response {
    uint64_t timestamp;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(timestamp)
      KV_MEMBER(status)
    }
  };
};

struct COMMAND_RPC_GET_BLOCKS_LIST {
  struct request {
    uint32_t height;
    uint32_t count = 10;

    void serialize(ISerializer &s) {
      KV_MEMBER(height)
      KV_MEMBER(count)
    }
  };

  struct response {
    std::vector<block_short_response> blocks;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(blocks)
      KV_MEMBER(status)
    }
  };
};

struct COMMAND_RPC_GET_ALT_BLOCKS_LIST {
  typedef EMPTY_STRUCT request;

  struct response {
    std::vector<block_short_response> alt_blocks;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(alt_blocks)
      KV_MEMBER(status)
    }
  };
};

//-----------------------------------------------
struct COMMAND_RPC_GET_TRANSACTIONS_BY_PAYMENT_ID {
  struct request {
    std::string payment_id;

    void serialize(ISerializer &s) {
      KV_MEMBER(payment_id)
    }
  };

  struct response {
    std::vector<transaction_short_response> transactions;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(transactions)
      KV_MEMBER(status)
    }
  };
};

struct COMMAND_RPC_GET_TRANSACTIONS_POOL_SHORT {
  typedef EMPTY_STRUCT request;

  struct response {
    std::vector<transaction_pool_response> transactions;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(transactions)
      KV_MEMBER(status)
    }
  };
};

struct COMMAND_RPC_GET_TRANSACTIONS_POOL {
  typedef EMPTY_STRUCT request;

  struct response {
    std::vector<TransactionDetails> transactions;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(transactions)
      KV_MEMBER(status)
    }
  };
};

struct COMMAND_RPC_QUERY_BLOCKS {
  struct request {
    std::vector<Crypto::Hash> block_ids; //*first 10 blocks id goes sequential, next goes in pow(2,n) offset, like 2, 4, 8, 16, 32, 64 and so on, and the last one is always genesis block */
    uint64_t timestamp;

    void serialize(ISerializer &s) {
      serializeAsBinary(block_ids, "block_ids", s);
      KV_MEMBER(timestamp)
    }
  };

  struct response {
    std::string status;
    uint32_t start_height;
    uint32_t current_height;
    uint64_t full_offset;
    std::vector<BlockFullInfo> items;

    void serialize(ISerializer &s) {
      KV_MEMBER(status)
      KV_MEMBER(start_height)
      KV_MEMBER(current_height)
      KV_MEMBER(full_offset)
      KV_MEMBER(items)
    }
  };
};

struct COMMAND_RPC_QUERY_BLOCKS_LITE {
  struct request {
    std::vector<Crypto::Hash> blockIds;
    uint64_t timestamp;

    void serialize(ISerializer &s) {
      serializeAsBinary(blockIds, "block_ids", s);
      KV_MEMBER(timestamp)
    }
  };

  struct response {
    std::string status;
    uint32_t startHeight;
    uint32_t currentHeight;
    uint64_t fullOffset;
    std::vector<BlockShortInfo> items;

    void serialize(ISerializer &s) {
      KV_MEMBER(status)
      KV_MEMBER(startHeight)
      KV_MEMBER(currentHeight)
      KV_MEMBER(fullOffset)
      KV_MEMBER(items)
    }
  };
};

//-----------------------------------------------
struct COMMAND_RPC_CHECK_TRANSACTION_KEY {
  struct request {
    std::string transaction_id;
    std::string transaction_key;
    std::string address;

    void serialize(ISerializer &s) {
      KV_MEMBER(transaction_id)
      KV_MEMBER(transaction_key)
      KV_MEMBER(address)
    }
  };

  struct response {
    uint64_t amount;
    std::vector<TransactionOutput> outputs;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(amount)
      KV_MEMBER(outputs)
      KV_MEMBER(status)
    }
  };
};

//-----------------------------------------------
struct COMMAND_RPC_CHECK_TRANSACTION_WITH_PRIVATE_VIEW_KEY {
	struct request {
		std::string transaction_id;
		std::string view_key;
		std::string address;

		void serialize(ISerializer &s) {
			KV_MEMBER(transaction_id)
			KV_MEMBER(view_key)
			KV_MEMBER(address)
		}
	};

	struct response {
		uint64_t amount;
		std::vector<TransactionOutput> outputs;
		uint32_t confirmations = 0;
		std::string status;

		void serialize(ISerializer &s) {
			KV_MEMBER(amount)
			KV_MEMBER(outputs)
			KV_MEMBER(confirmations)
			KV_MEMBER(status)
		}
	};
};

struct COMMAND_RPC_VALIDATE_ADDRESS {
  struct request {
    std::string address;

    void serialize(ISerializer &s) {
      KV_MEMBER(address)
    }
  };

  struct response {
    bool is_valid;
    std::string address;
    std::string spend_public_key;
    std::string view_public_key;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(is_valid)
      KV_MEMBER(address)
      KV_MEMBER(spend_public_key)
      KV_MEMBER(view_public_key)
      KV_MEMBER(status)
    }
  };
};

struct COMMAND_RPC_VERIFY_MESSAGE {
  struct request {
    std::string message;
    std::string address;
    std::string signature;

    void serialize(ISerializer &s) {
      KV_MEMBER(message)
      KV_MEMBER(address)
      KV_MEMBER(signature)
    }
  };

  struct response {
    bool sig_valid;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(sig_valid)
      KV_MEMBER(status)
    }
  };
};

struct COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HEIGHTS {
  struct request {
    std::vector<uint32_t> blockHeights;

    void serialize(ISerializer& s) {
      KV_MEMBER(blockHeights);
    }
  };

  struct response {
    std::vector<BlockDetails> blocks;
    std::string status;

    void serialize(ISerializer& s) {
      KV_MEMBER(status)
      KV_MEMBER(blocks)
    }
  };
};

struct COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES {
  struct request {
    std::vector<Crypto::Hash> blockHashes;

    void serialize(ISerializer& s) {
      KV_MEMBER(blockHashes);
    }
  };

  struct response {
    std::vector<BlockDetails> blocks;
    std::string status;

    void serialize(ISerializer& s) {
      KV_MEMBER(status)
      KV_MEMBER(blocks)
    }
  };
};

struct COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT {
  struct request {
    uint32_t blockHeight;

    void serialize(ISerializer& s) {
      KV_MEMBER(blockHeight)
    }
  };

  struct response {
    BlockDetails block;
    std::string status;

    void serialize(ISerializer& s) {
      KV_MEMBER(status)
      KV_MEMBER(block)
    }
  };
};

struct COMMAND_RPC_GET_BLOCK_DETAILS_BY_HASH {
  struct request {
    std::string hash;

    void serialize(ISerializer& s) {
      KV_MEMBER(hash)
    }
  };

  struct response {
    BlockDetails block;
    std::string status;

    void serialize(ISerializer& s) {
      KV_MEMBER(status)
      KV_MEMBER(block)
    }
  };
};

struct COMMAND_RPC_GET_BLOCKS_HASHES_BY_TIMESTAMPS {
  struct request {
    uint64_t timestampBegin;
    uint64_t timestampEnd;
    uint32_t limit;

    void serialize(ISerializer &s) {
      KV_MEMBER(timestampBegin)
      KV_MEMBER(timestampEnd)
      KV_MEMBER(limit)
    }
  };

  struct response {
    std::vector<Crypto::Hash> blockHashes;
    uint32_t count;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(status)
      KV_MEMBER(count)
      KV_MEMBER(blockHashes)
    }
  };
};

struct COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID {
  struct request {
    std::string paymentId;

    void serialize(ISerializer &s) {
      KV_MEMBER(paymentId)
    }
  };

  struct response {
    std::vector<Crypto::Hash> transactionHashes;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(status)
      KV_MEMBER(transactionHashes);
    }
  };
};

struct COMMAND_RPC_GET_TRANSACTIONS_DETAILS_BY_HASHES {
  struct request {
    std::vector<Crypto::Hash> transactionHashes;

    void serialize(ISerializer &s) {
      KV_MEMBER(transactionHashes);
    }
  };

  struct response {
    std::vector<TransactionDetails> transactions;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(status)
      KV_MEMBER(transactions)
    }
  };
};

struct COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASH {
  struct request {
    std::string hash;

    void serialize(ISerializer &s) {
      KV_MEMBER(hash);
    }
  };

  struct response {
    TransactionDetails transaction;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(status)
      KV_MEMBER(transaction)
    }
  };
};

struct COMMAND_RPC_GET_TRANSACTIONS_DETAILS_BY_HEIGHTS {
  struct request {
    std::vector<uint32_t> heights;
    bool include_miner_txs = true;
    bool exclude_signatures = true;
    bool range = false;

    void serialize(ISerializer &s) {
      KV_MEMBER(heights)
      KV_MEMBER(include_miner_txs)
      KV_MEMBER(exclude_signatures)
      KV_MEMBER(range)
    };
  };

  struct response {
    std::vector<TransactionDetails> transactions;
    std::list<std::string> missed_txs;
    std::string status;

    void serialize(ISerializer &s)
    {
      KV_MEMBER(transactions)
      KV_MEMBER(missed_txs)
      KV_MEMBER(status)       
    }
  };
};

//-----------------------------------------------

struct tx_with_output_global_indexes {
  TransactionPrefix transaction;
  Crypto::Hash hash;
  Crypto::Hash block_hash;
  uint32_t height;
  uint64_t fee;
  uint64_t timestamp;
  std::vector<uint32_t> output_indexes;

  void serialize(ISerializer &s)
  {
    KV_MEMBER(transaction)
    KV_MEMBER(hash)
    KV_MEMBER(block_hash)
    KV_MEMBER(height)
    KV_MEMBER(fee)
    KV_MEMBER(timestamp)
    KV_MEMBER(output_indexes)
  }
};

struct COMMAND_RPC_GET_TRANSACTIONS_WITH_OUTPUT_GLOBAL_INDEXES_BY_HEIGHTS {
  struct request {
    std::vector<uint32_t> heights;
    bool include_miner_txs = true;
    bool range = false;

    void serialize(ISerializer &s) {
      KV_MEMBER(heights)
      KV_MEMBER(include_miner_txs)
      KV_MEMBER(range)
    };
  };

  struct response {
    std::vector<tx_with_output_global_indexes> transactions;
    std::list<std::string> missed_txs;
    std::string status;

    void serialize(ISerializer &s)
    {
      KV_MEMBER(transactions)
      KV_MEMBER(missed_txs)
      KV_MEMBER(status)
    }
  };
};

struct COMMAND_RPC_GET_RAW_TRANSACTIONS_POOL {
  typedef EMPTY_STRUCT request;

  struct response {
    std::vector<tx_with_output_global_indexes> transactions;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(transactions)
      KV_MEMBER(status)
    }
  };
};

//-----------------------------------------------
struct reserve_proof_entry
{
  Crypto::Hash transaction_id;
  uint64_t index_in_transaction;
  Crypto::PublicKey shared_secret;
  Crypto::KeyImage key_image;
  Crypto::Signature shared_secret_sig;
  Crypto::Signature key_image_sig;

  void serialize(ISerializer& s)
  {
    KV_MEMBER(transaction_id)
    KV_MEMBER(index_in_transaction)
    KV_MEMBER(shared_secret)
    KV_MEMBER(key_image)
    KV_MEMBER(shared_secret_sig)
    KV_MEMBER(key_image_sig)
  }
};

struct reserve_proof {
	std::vector<reserve_proof_entry> proofs;
	Crypto::Signature signature;

	void serialize(ISerializer &s) {
		KV_MEMBER(proofs)
		KV_MEMBER(signature)
	}
};

struct COMMAND_RPC_CHECK_TRANSACTION_PROOF {
  struct request {
    std::string transaction_id;
    std::string destination_address;
    std::string signature;

    void serialize(ISerializer &s) {
      KV_MEMBER(transaction_id)
      KV_MEMBER(destination_address)
      KV_MEMBER(signature)
    }
  };

  struct response {
    bool signature_valid;
    uint64_t received_amount;
    std::vector<TransactionOutput> outputs;
    uint32_t confirmations = 0;
    std::string status;

    void serialize(ISerializer &s) {
      KV_MEMBER(signature_valid)
      KV_MEMBER(received_amount)
      KV_MEMBER(outputs)
      KV_MEMBER(confirmations)
      KV_MEMBER(status)
    }
  };
};

struct COMMAND_RPC_CHECK_RESERVE_PROOF {
  struct request {
    std::string address;
    std::string message;
    std::string signature;
    uint32_t height = 0;

    void serialize(ISerializer &s) {
      KV_MEMBER(address)
      KV_MEMBER(message)
      KV_MEMBER(signature)
      KV_MEMBER(height)
    }
  };

  struct response {
    bool good;
    uint64_t total;
    uint64_t spent;
    uint64_t locked;

    void serialize(ISerializer &s) {
      KV_MEMBER(good)
      KV_MEMBER(total)
      KV_MEMBER(spent)
      KV_MEMBER(locked)
    }
  };
};


struct block_stats_entry {
  uint32_t height;
  uint64_t already_generated_coins;
  uint64_t transactions_count;
  uint64_t block_size;
  uint64_t difficulty;
  uint64_t reward;
  uint64_t timestamp;
  //uint64_t min_fee;

  void serialize(ISerializer &s) {
    KV_MEMBER(height)
    KV_MEMBER(already_generated_coins)
    KV_MEMBER(transactions_count)
    KV_MEMBER(block_size)
    KV_MEMBER(difficulty)
    KV_MEMBER(reward)
    KV_MEMBER(timestamp)
    //KV_MEMBER(min_fee)
  }
};

struct COMMAND_RPC_GET_STATS_BY_HEIGHTS {
  struct request {
    std::vector<uint32_t> heights;

    void serialize(ISerializer& s) {
      KV_MEMBER(heights);
    }
  };

  struct response {
    std::vector<block_stats_entry> stats;
    double duration;
    std::string status;

    void serialize(ISerializer& s) {
      KV_MEMBER(stats);
      KV_MEMBER(duration);
      KV_MEMBER(status);
    }
  };
};

struct COMMAND_RPC_GET_STATS_BY_HEIGHTS_RANGE {
  struct request {
    uint32_t start_height;
    uint32_t end_height;

    void serialize(ISerializer& s) {
      KV_MEMBER(start_height);
      KV_MEMBER(end_height);
    }
  };

  struct response {
    std::vector<block_stats_entry> stats;
    double duration;
    std::string status;

    void serialize(ISerializer& s) {
      KV_MEMBER(stats);
      KV_MEMBER(duration);
      KV_MEMBER(status);
    }
  };
};

struct COMMAND_RPC_RESOLVE_OPEN_ALIAS {
  struct request {
    std::string url;

    void serialize(ISerializer& s) {
      KV_MEMBER(url);
    }
  };

  struct response {
    std::string address;
    std::string status;

    void serialize(ISerializer& s) {
      KV_MEMBER(address);
      KV_MEMBER(status);
    }
  };
};

}
