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

#include <list>
#include "CryptoNoteCore/CryptoNoteBasic.h"

// ISerializer-based serialization
#include "Serialization/ISerializer.h"
#include "Serialization/SerializationOverloads.h"
#include "CryptoNoteCore/CryptoNoteSerialization.h"

namespace CryptoNote
{

#define BC_COMMANDS_POOL_BASE 2000

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/

  //just to keep backward compatibility with BlockCompleteEntry serialization
  struct RawBlockLegacy {
    BinaryArray block;
    std::vector<BinaryArray> transactions;
  };

  struct NOTIFY_NEW_BLOCK_request
  {
    RawBlockLegacy b;
    uint32_t current_blockchain_height;
    uint32_t hop;
  };

  struct NOTIFY_NEW_BLOCK
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 1;
    typedef NOTIFY_NEW_BLOCK_request request;
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct NOTIFY_NEW_TRANSACTIONS_request
  {
    std::vector<BinaryArray> txs;
  };

  struct NOTIFY_NEW_TRANSACTIONS
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 2;
    typedef NOTIFY_NEW_TRANSACTIONS_request request;
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct NOTIFY_REQUEST_GET_OBJECTS_request
  {
    std::vector<Crypto::Hash> txs;
    std::vector<Crypto::Hash> blocks;

    void serialize(ISerializer& s) {
      serializeAsBinary(txs, "txs", s);
      serializeAsBinary(blocks, "blocks", s);
    }
  };

  struct NOTIFY_REQUEST_GET_OBJECTS
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 3;
    typedef NOTIFY_REQUEST_GET_OBJECTS_request request;
  };

  struct NOTIFY_RESPONSE_GET_OBJECTS_request
  {
    std::vector<std::string> txs;
    std::vector<RawBlockLegacy> blocks;
    std::vector<Crypto::Hash> missed_ids;
    uint32_t current_blockchain_height;
  };

  struct NOTIFY_RESPONSE_GET_OBJECTS
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 4;
    typedef NOTIFY_RESPONSE_GET_OBJECTS_request request;
  };

  struct NOTIFY_REQUEST_CHAIN
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 6;

    struct request
    {
      std::vector<Crypto::Hash> block_ids; /*IDs of the first 10 blocks are sequential, next goes with pow(2,n) offset, like 2, 4, 8, 16, 32, 64 and so on, and the last one is always genesis block */

      void serialize(ISerializer& s) {
        serializeAsBinary(block_ids, "block_ids", s);
      }
    };
  };

  struct NOTIFY_RESPONSE_CHAIN_ENTRY_request
  {
    uint32_t start_height;
    uint32_t total_height;
    std::vector<Crypto::Hash> m_block_ids;

    void serialize(ISerializer& s) {
      KV_MEMBER(start_height)
      KV_MEMBER(total_height)
      serializeAsBinary(m_block_ids, "m_block_ids", s);
    }
  };

  struct NOTIFY_RESPONSE_CHAIN_ENTRY
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 7;
    typedef NOTIFY_RESPONSE_CHAIN_ENTRY_request request;
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct NOTIFY_REQUEST_TX_POOL_request {
    std::vector<Crypto::Hash> txs;

    void serialize(ISerializer& s) {
      serializeAsBinary(txs, "txs", s);
    }
  };

  struct NOTIFY_REQUEST_TX_POOL {
    const static int ID = BC_COMMANDS_POOL_BASE + 8;
    typedef NOTIFY_REQUEST_TX_POOL_request request;
  };
}
