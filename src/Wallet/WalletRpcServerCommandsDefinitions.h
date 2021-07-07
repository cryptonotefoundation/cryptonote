// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2016, The Monero Project
// Copyright (c) 2016-2018, Karbo developers
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
#include "crypto/hash.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "WalletRpcServerErrorCodes.h"
#include "../CryptoNoteConfig.h"

namespace Tools {
namespace wallet_rpc {

using CryptoNote::ISerializer;

#define WALLET_RPC_STATUS_OK      "OK"
#define WALLET_RPC_STATUS_BUSY    "BUSY"

	/* Command: get_balance */
	struct COMMAND_RPC_GET_BALANCE
	{
		typedef CryptoNote::EMPTY_STRUCT request;
		struct response
		{
			uint64_t locked_amount;
			uint64_t available_balance;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(locked_amount)
				KV_MEMBER(available_balance)
			}
		};
	};

	/* Command: transfer */ 
	struct transfer_destination
	{
		uint64_t amount;
		std::string address;

		void serialize(ISerializer& s)
		{
			KV_MEMBER(amount)
			KV_MEMBER(address)
		}
	};

	struct COMMAND_RPC_TRANSFER
	{
		struct request
		{
			std::list<transfer_destination> destinations;
			uint64_t fee = CryptoNote::parameters::MINIMUM_FEE_V2;
			uint64_t mixin = 0;
			uint64_t unlock_time = 0;
			std::string payment_id;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(destinations)
				KV_MEMBER(fee)
				KV_MEMBER(mixin)
				KV_MEMBER(unlock_time)
				KV_MEMBER(payment_id)
			}
		};
		struct response
		{
			std::string tx_hash;
			std::string tx_key;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(tx_hash)
				KV_MEMBER(tx_key)
			}
		};
	};

	/* Command: store */
	struct COMMAND_RPC_STORE
	{
		typedef CryptoNote::EMPTY_STRUCT request;
		struct response
		{
			bool stored;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(stored)
			}
		};
	};

	/* Command: stop_wallet */
	struct COMMAND_RPC_STOP
	{
		typedef CryptoNote::EMPTY_STRUCT request;
		typedef CryptoNote::EMPTY_STRUCT response;
	};

	/* Command: get_payments */
	struct payment_details
	{
		std::string tx_hash;
		uint64_t amount;
		uint64_t block_height;
		uint64_t unlock_time;

		void serialize(ISerializer& s)
		{
			KV_MEMBER(tx_hash)
			KV_MEMBER(amount)
			KV_MEMBER(block_height)
			KV_MEMBER(unlock_time)
		}
	};

	struct COMMAND_RPC_GET_PAYMENTS
	{
		struct request
		{
			std::string payment_id;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(payment_id)
			}
		};
		struct response
		{
			std::list<payment_details> payments;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(payments)
			}
		};
	};

	/* Command: get_transfers */
	struct Transfer
	{
		uint64_t time;
		bool output;
		std::string transactionHash;
		uint64_t amount;
		uint64_t fee;
		std::string paymentId;
		std::string address;
		uint64_t blockIndex;
		uint64_t unlockTime;
		uint64_t confirmations;
		std::string txKey;

		void serialize(ISerializer& s)
		{
			KV_MEMBER(time)
			KV_MEMBER(output)
			KV_MEMBER(transactionHash)
			KV_MEMBER(amount)
			KV_MEMBER(fee)
			KV_MEMBER(paymentId)
			KV_MEMBER(address)
			KV_MEMBER(blockIndex)
			KV_MEMBER(unlockTime)
			KV_MEMBER(confirmations)
			KV_MEMBER(txKey)
		}
	};

	struct COMMAND_RPC_GET_TRANSFERS
	{
		typedef CryptoNote::EMPTY_STRUCT request;
		struct response
		{
			std::list<Transfer> transfers;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(transfers)
			}
		};
	};

	/* Command: get_transaction */
	struct COMMAND_RPC_GET_TRANSACTION
	{
		struct request
		{
			std::string tx_hash;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(tx_hash)
			}
		};
		struct response
		{
			Transfer transaction_details;
			std::list<transfer_destination> destinations;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(transaction_details)
				KV_MEMBER(destinations)
			}
		};
	};

	struct COMMAND_RPC_GET_HEIGHT
	{
		typedef CryptoNote::EMPTY_STRUCT request;
		struct response
		{
			uint64_t height;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(height)
			}
		};
	};

	/* Command: reset */
	struct COMMAND_RPC_RESET
	{
		typedef CryptoNote::EMPTY_STRUCT request;
		typedef CryptoNote::EMPTY_STRUCT response;
	}; 

	/* Command: query_key */
	struct COMMAND_RPC_QUERY_KEY
	{
		struct request
		{
			std::string key_type;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(key_type)
			}
		};
		struct response
		{
			std::string key;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(key)
			}
		};
	};

	/* Command: get_address */
	struct COMMAND_RPC_GET_ADDRESS
	{
		typedef CryptoNote::EMPTY_STRUCT request;
		struct response
		{
			std::string address;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(address)
			}
		};
	};

	/* Command: paymentid */
	struct COMMAND_RPC_GEN_PAYMENT_ID
	{
		typedef CryptoNote::EMPTY_STRUCT request;
		struct response
		{
			std::string payment_id;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(payment_id)
			}
		};
	};

	/* Command: get_tx_key */
	struct COMMAND_RPC_GET_TX_KEY
	{
		struct request
		{
			std::string tx_hash;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(tx_hash)
			}
		};
		struct response
		{
			std::string tx_key;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(tx_key)
			}
		};
	};

	struct COMMAND_RPC_SIGN_MESSAGE
	{
		struct request
		{
			std::string message;
 
			void serialize(ISerializer& s)
			{
				KV_MEMBER(message);
			}
		};

		struct response
		{
			std::string signature;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(signature);
			}
		};
	};

	struct COMMAND_RPC_VERIFY_MESSAGE
	{
		struct request
		{
			std::string message;
			std::string address;
			std::string signature;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(message);
				KV_MEMBER(address);
				KV_MEMBER(signature);
			}
		};

		struct response
		{
			bool good;
 
			void serialize(ISerializer& s)
			{
				KV_MEMBER(good);
			}
		};
	};

	struct COMMAND_RPC_CHANGE_PASSWORD
	{
		struct request
		{
			std::string old_password;
			std::string new_password;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(old_password);
				KV_MEMBER(new_password);
			}
		};

		struct response
		{
			bool password_changed;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(password_changed);
			}
		};
	};

	struct COMMAND_RPC_GET_OUTPUTS
    {
      typedef CryptoNote::EMPTY_STRUCT request;

      struct response
      {
        size_t unlocked_outputs_count;

        void serialize(ISerializer& s) {
          KV_MEMBER(unlocked_outputs_count)
        }
      };
    };

	struct COMMAND_RPC_GET_TX_PROOF
	{
		struct request
		{
			std::string tx_hash;
			std::string dest_address;
			std::string tx_key;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(tx_hash);
				KV_MEMBER(dest_address);
				KV_MEMBER(tx_key);
			}
		};

		struct response
		{
			std::string signature;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(signature);
			}
		};
	};

	struct COMMAND_RPC_GET_BALANCE_PROOF
	{
		struct request
		{
			uint64_t amount = 0;
			std::string message;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(amount);
				KV_MEMBER(message);
			}
		};

		struct response
		{
			std::string signature;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(signature);
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

	/* Fusion transactions */

	struct COMMAND_RPC_ESTIMATE_FUSION
	{
		struct request
		{
			uint64_t threshold;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(threshold)
			}
		};

		struct response
		{
			size_t fusion_ready_count;

			void serialize(ISerializer& s) {
				KV_MEMBER(fusion_ready_count)
			}
		};
	};

	struct COMMAND_RPC_SEND_FUSION
	{
		struct request
		{
			uint64_t mixin = 0;
			uint64_t threshold;
			uint64_t unlock_time = 0;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(mixin)
				KV_MEMBER(threshold)
				KV_MEMBER(unlock_time)
			}
		};
		struct response
		{
			std::string tx_hash;

			void serialize(ISerializer& s)
			{
				KV_MEMBER(tx_hash)
			}
		};
	};

}} //Tools::wallet_rpc
