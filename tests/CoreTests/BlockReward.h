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

struct gen_block_reward : public test_chain_unit_base {
  gen_block_reward();

  bool generate(std::vector<test_event_entry>& events) const;

  bool check_block_verification_context(std::error_code bve, size_t event_idx, const CryptoNote::BlockTemplate& blk);
  bool check_block_verification_context(std::error_code bve, size_t event_idx, const CryptoNote::RawBlock& /*blk*/);

  bool mark_invalid_block(CryptoNote::Core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool mark_checked_block(CryptoNote::Core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_block_rewards(CryptoNote::Core& c, size_t ev_index, const std::vector<test_event_entry>& events);

private:
  //uint64_t ts_start = 1338224400;
  size_t m_invalid_block_index;
  std::vector<size_t> m_checked_blocks_indices;
};
