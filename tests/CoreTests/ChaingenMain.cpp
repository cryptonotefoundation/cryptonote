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

#include "Chaingen.h"

#include "Common/CommandLine.h"

#include "BlockReward.h"
#include "BlockValidation.h"
#include "ChainSplit1.h"
#include "ChainSwitch1.h"
#include "Chaingen001.h"
#include "DoubleSpend.h"
#include "IntegerOverflow.h"
#include "RingSignature.h"
#include "TransactionTests.h"
#include "TransactionValidation.h"
#include "Upgrade.h"
#include "RandomOuts.h"

namespace po = boost::program_options;

namespace {
const command_line::arg_descriptor<std::string> arg_test_data_path = {"test_data_path", "", ""};
const command_line::arg_descriptor<bool> arg_generate_test_data = {"generate_test_data", ""};
const command_line::arg_descriptor<bool> arg_play_test_data = {"play_test_data", ""};
const command_line::arg_descriptor<bool> arg_generate_and_play_test_data = {"generate_and_play_test_data", ""};
const command_line::arg_descriptor<bool> arg_test_transactions = {"test_transactions", ""};
}

int main(int argc, char* argv[]) {
  try {

    po::options_description desc_options("Allowed options");
    command_line::add_arg(desc_options, command_line::arg_help);
    command_line::add_arg(desc_options, arg_test_data_path);
    command_line::add_arg(desc_options, arg_generate_test_data);
    command_line::add_arg(desc_options, arg_play_test_data);
    command_line::add_arg(desc_options, arg_generate_and_play_test_data);
    command_line::add_arg(desc_options, arg_test_transactions);

    po::variables_map vm;
    bool r = command_line::handle_error_helper(desc_options, [&]() {
      po::store(po::parse_command_line(argc, argv, desc_options), vm);
      po::notify(vm);
      return true;
    });
    if (!r)
      return 1;

    if (command_line::get_arg(vm, command_line::arg_help)) {
      std::cout << desc_options << std::endl;
      return 0;
    }

    size_t tests_count = 0;
    std::vector<std::string> failed_tests;
    std::string tests_folder = command_line::get_arg(vm, arg_test_data_path);
    if (command_line::get_arg(vm, arg_generate_test_data)) {
      GENERATE("chain001.dat", gen_simple_chain_001);
    } else if (command_line::get_arg(vm, arg_play_test_data)) {
      PLAY("chain001.dat", gen_simple_chain_001);
    } else if (command_line::get_arg(vm, arg_generate_and_play_test_data)) {
#define GENERATE_AND_PLAY_EX_2VER(TestCase)                                                                            \
  GENERATE_AND_PLAY_EX(TestCase(CryptoNote::BLOCK_MAJOR_VERSION_1))                                                    \
  GENERATE_AND_PLAY_EX(TestCase(CryptoNote::BLOCK_MAJOR_VERSION_2))


      GENERATE_AND_PLAY(GetRandomOutputs);
      GENERATE_AND_PLAY(gen_chain_switch_1);
      GENERATE_AND_PLAY(gen_block_reward);
      GENERATE_AND_PLAY(gen_ring_signature_1);
      GENERATE_AND_PLAY(gen_ring_signature_2);
      GENERATE_AND_PLAY_EX_2VER(TestBlockCumulativeSizeExceedsLimit);
      // Double spend
      GENERATE_AND_PLAY(gen_double_spend_in_the_same_block<false>);
      GENERATE_AND_PLAY(gen_double_spend_in_tx<false>);
      GENERATE_AND_PLAY(gen_double_spend_in_tx<true>);
      GENERATE_AND_PLAY(gen_double_spend_in_the_same_block<false>);
      GENERATE_AND_PLAY(gen_double_spend_in_different_blocks<false>);
      //GENERATE_AND_PLAY(gen_double_spend_in_different_blocks<true>);
      GENERATE_AND_PLAY(gen_double_spend_in_different_chains);
      GENERATE_AND_PLAY(gen_double_spend_in_alt_chain_in_the_same_block<false>);
      //GENERATE_AND_PLAY(gen_double_spend_in_alt_chain_in_the_same_block<true>);
      GENERATE_AND_PLAY(gen_double_spend_in_alt_chain_in_different_blocks<false>);
      //GENERATE_AND_PLAY(gen_double_spend_in_alt_chain_in_different_blocks<true>);

      GENERATE_AND_PLAY(gen_simple_chain_split_1);
      GENERATE_AND_PLAY(gen_simple_chain_001);
      GENERATE_AND_PLAY(one_block);
      // GENERATE_AND_PLAY(gen_ring_signature_big); // Takes up to XXX hours (if CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW ==
      // 10)

      //// BlockTemplate verification tests
      GENERATE_AND_PLAY_EX_2VER(TestBlockMajorVersionAccepted);
      GENERATE_AND_PLAY_EX(
          TestBlockMajorVersionRejected(CryptoNote::BLOCK_MAJOR_VERSION_1, CryptoNote::BLOCK_MAJOR_VERSION_2));
      GENERATE_AND_PLAY_EX(
          TestBlockMajorVersionRejected(CryptoNote::BLOCK_MAJOR_VERSION_2, CryptoNote::BLOCK_MAJOR_VERSION_1));
      GENERATE_AND_PLAY_EX(
          TestBlockMajorVersionRejected(CryptoNote::BLOCK_MAJOR_VERSION_2, CryptoNote::BLOCK_MAJOR_VERSION_2 + 1));
      GENERATE_AND_PLAY_EX_2VER(TestBlockBigMinorVersion);
      GENERATE_AND_PLAY_EX_2VER(gen_block_ts_not_checked);
      GENERATE_AND_PLAY_EX_2VER(gen_block_ts_in_future_rejected);
      GENERATE_AND_PLAY_EX_2VER(gen_block_ts_in_future_accepted);
      GENERATE_AND_PLAY_EX_2VER(gen_block_ts_in_past);
      GENERATE_AND_PLAY_EX_2VER(gen_block_invalid_prev_id);
      GENERATE_AND_PLAY_EX_2VER(gen_block_invalid_nonce);
      GENERATE_AND_PLAY_EX_2VER(gen_block_no_miner_tx);
      GENERATE_AND_PLAY_EX_2VER(gen_block_unlock_time_is_low);
      GENERATE_AND_PLAY_EX_2VER(gen_block_unlock_time_is_high);
      GENERATE_AND_PLAY_EX_2VER(gen_block_unlock_time_is_timestamp_in_past);
      GENERATE_AND_PLAY_EX_2VER(gen_block_unlock_time_is_timestamp_in_future);
      GENERATE_AND_PLAY_EX_2VER(gen_block_height_is_low);
      GENERATE_AND_PLAY_EX_2VER(gen_block_height_is_high);
      GENERATE_AND_PLAY_EX_2VER(gen_block_miner_tx_has_2_tx_gen_in);
      GENERATE_AND_PLAY_EX_2VER(gen_block_miner_tx_has_2_in);
      GENERATE_AND_PLAY_EX_2VER(gen_block_miner_tx_with_txin_to_key);
      GENERATE_AND_PLAY_EX_2VER(gen_block_miner_tx_out_is_small);
      GENERATE_AND_PLAY_EX_2VER(gen_block_miner_tx_out_is_big);
      GENERATE_AND_PLAY_EX_2VER(gen_block_miner_tx_has_no_out);
      GENERATE_AND_PLAY_EX_2VER(gen_block_miner_tx_has_out_to_alice);
      GENERATE_AND_PLAY_EX_2VER(gen_block_has_invalid_tx);
      GENERATE_AND_PLAY_EX_2VER(gen_block_is_too_big);
      // GENERATE_AND_PLAY_EX_2VER(gen_block_invalid_binary_format); // Takes up to 30 minutes, if
      // CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW == 10

      GENERATE_AND_PLAY(TestMaxSizeOfParentBlock);
      GENERATE_AND_PLAY(TestBigParentBlock);
      GENERATE_AND_PLAY(TestBlock2ExtraEmpty);
      GENERATE_AND_PLAY(TestBlock2ExtraWithoutMMTag);
      GENERATE_AND_PLAY(TestBlock2ExtraWithGarbage);

      // Transaction verification tests
      GENERATE_AND_PLAY(gen_tx_big_version);
      GENERATE_AND_PLAY(gen_tx_unlock_time);
      GENERATE_AND_PLAY(gen_tx_no_inputs_no_outputs);
      GENERATE_AND_PLAY(gen_tx_no_inputs_has_outputs);
      GENERATE_AND_PLAY(gen_tx_has_inputs_no_outputs);
      GENERATE_AND_PLAY(gen_tx_invalid_input_amount);
      GENERATE_AND_PLAY(gen_tx_in_to_key_wo_key_offsets);
      GENERATE_AND_PLAY(gen_tx_sender_key_offest_not_exist);
      GENERATE_AND_PLAY(gen_tx_key_offest_points_to_foreign_key);
      GENERATE_AND_PLAY(gen_tx_mixed_key_offest_not_exist);
      GENERATE_AND_PLAY(gen_tx_key_image_not_derive_from_tx_key);
      GENERATE_AND_PLAY(gen_tx_key_image_is_invalid);
      GENERATE_AND_PLAY(gen_tx_check_input_unlock_time);
      GENERATE_AND_PLAY(gen_tx_txout_to_key_has_invalid_key);
      GENERATE_AND_PLAY(gen_tx_output_with_zero_amount);
      GENERATE_AND_PLAY(gen_tx_signatures_are_invalid);
      GENERATE_AND_PLAY(GenerateTransactionWithZeroFee);

      // multisignature output
      GENERATE_AND_PLAY_EX(MultiSigTx_OutputSignatures(1, 1, true));
      GENERATE_AND_PLAY_EX(MultiSigTx_OutputSignatures(2, 2, true));
      GENERATE_AND_PLAY_EX(MultiSigTx_OutputSignatures(3, 2, true));
      GENERATE_AND_PLAY_EX(MultiSigTx_OutputSignatures(0, 0, true));
      GENERATE_AND_PLAY_EX(MultiSigTx_OutputSignatures(1, 0, true));
      GENERATE_AND_PLAY_EX(MultiSigTx_OutputSignatures(0, 1, false));
      GENERATE_AND_PLAY_EX(MultiSigTx_OutputSignatures(1, 2, false));
      GENERATE_AND_PLAY_EX(MultiSigTx_OutputSignatures(2, 3, false));
      GENERATE_AND_PLAY_EX(MultiSigTx_InvalidOutputSignature());

      // multisignature input
      GENERATE_AND_PLAY_EX(MultiSigTx_Input(1, 1, 1, true));
      GENERATE_AND_PLAY_EX(MultiSigTx_Input(2, 1, 1, true));
      GENERATE_AND_PLAY_EX(MultiSigTx_Input(3, 2, 2, true));
      GENERATE_AND_PLAY_EX(MultiSigTx_Input(1, 1, 0, false));
      GENERATE_AND_PLAY_EX(MultiSigTx_Input(2, 2, 1, false));
      GENERATE_AND_PLAY_EX(MultiSigTx_Input(3, 2, 1, false));
      GENERATE_AND_PLAY_EX(MultiSigTx_BadInputSignature());

      GENERATE_AND_PLAY_EX(MultiSigTx_DoubleSpendInTx(false));
      //GENERATE_AND_PLAY_EX(MultiSigTx_DoubleSpendInTx(true));
      GENERATE_AND_PLAY_EX(MultiSigTx_DoubleSpendSameBlock(false));
      //GENERATE_AND_PLAY_EX(MultiSigTx_DoubleSpendSameBlock(true));
      GENERATE_AND_PLAY_EX(MultiSigTx_DoubleSpendDifferentBlocks(false));
      //GENERATE_AND_PLAY_EX(MultiSigTx_DoubleSpendDifferentBlocks(true));
      GENERATE_AND_PLAY_EX(MultiSigTx_DoubleSpendAltChainSameBlock(false));
      //GENERATE_AND_PLAY_EX(MultiSigTx_DoubleSpendAltChainSameBlock(true));
      GENERATE_AND_PLAY_EX(MultiSigTx_DoubleSpendAltChainDifferentBlocks(false));
      //GENERATE_AND_PLAY_EX(MultiSigTx_DoubleSpendAltChainDifferentBlocks(true));

      GENERATE_AND_PLAY(gen_uint_overflow_1);
      GENERATE_AND_PLAY(gen_uint_overflow_2);

      //GENERATE_AND_PLAY(gen_upgrade);

      std::cout << (failed_tests.empty() ? concolor::green : concolor::magenta);
      std::cout << "\nREPORT:\n";
      std::cout << "  Test run: " << tests_count << '\n';
      std::cout << "  Failures: " << failed_tests.size() << '\n';
      if (!failed_tests.empty()) {
        std::cout << "FAILED TESTS:\n";
        BOOST_FOREACH (auto test_name, failed_tests) { std::cout << "  " << test_name << '\n'; }
      }
      std::cout << concolor::normal << std::endl;
    } else if (command_line::get_arg(vm, arg_test_transactions)) {
      CALL_TEST("TRANSACTIONS TESTS", test_transactions);
    } else {
      std::cout << concolor::magenta << "Wrong arguments" << concolor::normal << std::endl;
      std::cout << desc_options << std::endl;
      return 2;
    }

    return failed_tests.empty() ? 0 : 1;

  } catch (std::exception& e) {
    std::cout << "Exception in main(): " << e.what() << std::endl;
  }
}
