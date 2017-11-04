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
#include "Chaingen.h"

struct get_tx_validation_base : public test_chain_unit_base {
  get_tx_validation_base() : m_invalid_tx_index(0), m_invalid_block_index(0) {
    REGISTER_CALLBACK_METHOD(get_tx_validation_base, mark_invalid_tx);
    REGISTER_CALLBACK_METHOD(get_tx_validation_base, mark_invalid_block);
  }

  bool check_tx_verification_context(bool tve, bool tx_added, size_t event_idx, const CryptoNote::Transaction& /*tx*/) {
    return m_invalid_tx_index == event_idx ? !tve && !tx_added : tve && tx_added;
  }

  bool check_block_verification_context(std::error_code bve, size_t event_idx,
                                        const CryptoNote::BlockTemplate& /*block*/) {
    std::cout << "message: " << bve.message() << std::endl;
    return m_invalid_block_index == event_idx ? blockWasNotAdded(bve) : blockWasAdded(bve);
  }

  bool check_block_verification_context(std::error_code bve, size_t event_idx,
                                        const CryptoNote::RawBlock& /*block*/) {
    return check_block_verification_context(bve, event_idx, CryptoNote::BlockTemplate{});
  }

  bool mark_invalid_block(CryptoNote::Core& /*c*/, size_t ev_index, const std::vector<test_event_entry>& /*events*/) {
    m_invalid_block_index = ev_index + 1;
    return true;
  }

  bool mark_invalid_tx(CryptoNote::Core& /*c*/, size_t ev_index, const std::vector<test_event_entry>& /*events*/) {
    m_invalid_tx_index = ev_index + 1;
    return true;
  }

private:
  size_t m_invalid_tx_index;
  size_t m_invalid_block_index;
};

struct gen_tx_big_version : public get_tx_validation_base {
  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_tx_unlock_time : public get_tx_validation_base {
  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_tx_no_inputs_no_outputs : public get_tx_validation_base {
  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_tx_no_inputs_has_outputs : public get_tx_validation_base {
  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_tx_has_inputs_no_outputs : public get_tx_validation_base {
  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_tx_invalid_input_amount : public get_tx_validation_base {
  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_tx_in_to_key_wo_key_offsets : public get_tx_validation_base {
  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_tx_key_offest_points_to_foreign_key : public get_tx_validation_base {
  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_tx_sender_key_offest_not_exist : public get_tx_validation_base {
  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_tx_mixed_key_offest_not_exist : public get_tx_validation_base {
  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_tx_key_image_not_derive_from_tx_key : public get_tx_validation_base {
  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_tx_key_image_is_invalid : public get_tx_validation_base {
  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_tx_check_input_unlock_time : public get_tx_validation_base {
  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_tx_txout_to_key_has_invalid_key : public get_tx_validation_base {
  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_tx_output_with_zero_amount : public get_tx_validation_base {
  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_tx_signatures_are_invalid : public get_tx_validation_base {
  bool generate(std::vector<test_event_entry>& events) const;
};

struct GenerateTransactionWithZeroFee : public get_tx_validation_base
{
  bool generate(std::vector<test_event_entry>& events) const;
};

// MultiSignature

class TestGenerator;

struct MultiSigTx_OutputSignatures : public get_tx_validation_base {
  MultiSigTx_OutputSignatures(size_t givenKeys, uint32_t requiredSignatures, bool shouldSucceed);

  bool generate(std::vector<test_event_entry>& events) const;
  bool generate(TestGenerator& generator) const;

  const size_t m_givenKeys;
  const uint32_t m_requiredSignatures;
  const bool m_shouldSucceed;
  std::vector<CryptoNote::AccountBase> m_outputAccounts;
};

struct MultiSigTx_InvalidOutputSignature : public get_tx_validation_base {
  bool generate(std::vector<test_event_entry>& events) const;
};

struct MultiSigTx_Input : public MultiSigTx_OutputSignatures {
  MultiSigTx_Input(size_t givenKeys, uint32_t requiredSignatures, uint32_t givenSignatures, bool shouldSucceed);
  bool generate(std::vector<test_event_entry>& events) const;

  const bool m_inputShouldSucceed;
  const uint32_t m_givenSignatures;
};

struct MultiSigTx_BadInputSignature : public MultiSigTx_OutputSignatures {
  MultiSigTx_BadInputSignature();
  bool generate(std::vector<test_event_entry>& events) const;
};
