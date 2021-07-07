// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2017, The Monero project
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

#pragma once

#include <list>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>

#include "P2pProtocolTypes.h"
#include "CryptoNoteConfig.h"

namespace CryptoNote {

class ISerializer;
/************************************************************************/
/*                                                                      */
/************************************************************************/
class PeerlistManager {
  struct by_time{};
  struct by_id{};
  struct by_addr{};

  typedef boost::multi_index_container<
    PeerlistEntry,
    boost::multi_index::indexed_by<
    // access by peerlist_entry::net_adress
    boost::multi_index::ordered_unique<boost::multi_index::tag<by_addr>, boost::multi_index::member<PeerlistEntry, NetworkAddress, &PeerlistEntry::adr> >,
    // sort by peerlist_entry::last_seen<
    boost::multi_index::ordered_non_unique<boost::multi_index::tag<by_time>, boost::multi_index::member<PeerlistEntry, uint64_t, &PeerlistEntry::last_seen> >
    >
  > peers_indexed;

  typedef boost::multi_index_container<
    AnchorPeerlistEntry,
    boost::multi_index::indexed_by<
    // access by anchor_peerlist_entry::net_adress
    boost::multi_index::ordered_unique<boost::multi_index::tag<by_addr>, boost::multi_index::member<AnchorPeerlistEntry, NetworkAddress, &AnchorPeerlistEntry::adr> >,
    // sort by anchor_peerlist_entry::first_seen
    boost::multi_index::ordered_non_unique<boost::multi_index::tag<by_time>, boost::multi_index::member<AnchorPeerlistEntry, int64_t, &AnchorPeerlistEntry::first_seen> >
    >
  > anchor_peers_indexed;

public:

  class Peerlist {
  public:
    Peerlist(peers_indexed& peers, size_t maxSize);
    size_t count() const;
    bool get(PeerlistEntry& entry, size_t index) const;
    void trim();

  private:
    peers_indexed& m_peers;
    const size_t m_maxSize;
  };

  PeerlistManager();

  bool init(bool allow_local_ip);
  size_t get_white_peers_count() const { return m_peers_white.size(); }
  size_t get_gray_peers_count() const { return m_peers_gray.size(); }
  bool merge_peerlist(const std::vector<PeerlistEntry>& outer_bs);
  bool get_peerlist_head(std::vector<PeerlistEntry>& bs_head, uint32_t depth = CryptoNote::P2P_DEFAULT_PEERS_IN_HANDSHAKE) const;
  bool get_peerlist_full(std::list<AnchorPeerlistEntry>& pl_anchor, std::vector<PeerlistEntry>& pl_gray, std::vector<PeerlistEntry>& pl_white) const;
  bool get_white_peer_by_index(PeerlistEntry& p, size_t i) const;
  bool get_gray_peer_by_index(PeerlistEntry& p, size_t i) const;
  bool remove_from_peer_gray(PeerlistEntry& p);
  bool append_with_peer_anchor(const AnchorPeerlistEntry& pr);
  bool append_with_peer_white(const PeerlistEntry& pr);
  bool append_with_peer_gray(const PeerlistEntry& pr);
  bool set_peer_just_seen(PeerIdType peer, uint32_t ip, uint32_t port);
  bool set_peer_just_seen(PeerIdType peer, const NetworkAddress& addr);
  bool set_peer_unreachable(const PeerlistEntry& pr);
  bool is_ip_allowed(uint32_t ip) const;
  void trim_white_peerlist();
  void trim_gray_peerlist();

  void serialize(ISerializer& s);

  Peerlist& getWhite();
  Peerlist& getGray();

  bool get_and_empty_anchor_peerlist(std::vector<AnchorPeerlistEntry>& apl);
  bool remove_from_peer_anchor(const NetworkAddress& addr);

private:
  std::string m_config_folder;
  bool m_allow_local_ip;
  peers_indexed m_peers_gray;
  peers_indexed m_peers_white;
  anchor_peers_indexed m_peers_anchor;
  Peerlist m_whitePeerlist;
  Peerlist m_grayPeerlist;
};

}
