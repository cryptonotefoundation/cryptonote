// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
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

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/random_access_index.hpp>

#include "crypto/hash.h"
#include <vector>

namespace CryptoNote
{
  class ISerializer;

  class BlockIndex {

  public:

    BlockIndex() : 
      m_index(m_container.get<1>()) {}

    void pop() {
      m_container.pop_back();
    }

    // returns true if new element was inserted, false if already exists
    bool push(const Crypto::Hash& h) {
      auto result = m_container.push_back(h);
      return result.second;
    }

    bool hasBlock(const Crypto::Hash& h) const {
      return m_index.find(h) != m_index.end();
    }

    bool getBlockHeight(const Crypto::Hash& h, uint32_t& height) const {
      auto hi = m_index.find(h);
      if (hi == m_index.end())
        return false;

      height = static_cast<uint32_t>(std::distance(m_container.begin(), m_container.project<0>(hi)));
      return true;
    }

    uint32_t size() const {
      return static_cast<uint32_t>(m_container.size());
    }

    void clear() {
      m_container.clear();
    }

    Crypto::Hash getBlockId(uint32_t height) const;
    std::vector<Crypto::Hash> getBlockIds(uint32_t startBlockIndex, uint32_t maxCount) const;
    bool findSupplement(const std::vector<Crypto::Hash>& ids, uint32_t& offset) const;
    std::vector<Crypto::Hash> buildSparseChain(const Crypto::Hash& startBlockId) const;
    Crypto::Hash getTailId() const;

    void serialize(ISerializer& s);

  private:

    typedef boost::multi_index_container <
      Crypto::Hash,
      boost::multi_index::indexed_by<
        boost::multi_index::random_access<>,
        boost::multi_index::hashed_unique<boost::multi_index::identity<Crypto::Hash>>
      >
    > ContainerT;

    ContainerT m_container;
    ContainerT::nth_index<1>::type& m_index;

  };
}
