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

#include "PeerListManager.h"

#include <time.h>
#include <boost/foreach.hpp>
#include <System/Ipv4Address.h>

#include "Serialization/SerializationOverloads.h"

using namespace CryptoNote;

namespace CryptoNote {
  template <typename T, typename Indexes>
  bool serialize(boost::multi_index_container<T, Indexes>& value, Common::StringView name, ISerializer& s) {
    if (s.type() == ISerializer::INPUT) {
      readSequence<T>(std::inserter(value, value.end()), name, s);
    } else {
      writeSequence<T>(value.begin(), value.end(), name, s);
    }

    return true;
  }

  void serialize(NetworkAddress& na, ISerializer& s) {
    s(na.ip, "ip");
    s(na.port, "port");
  }

  void serialize(PeerlistEntry& pe, ISerializer& s) {
    s(pe.adr, "adr");
    s(pe.id, "id");
    s(pe.last_seen, "last_seen");
  }

}

PeerlistManager::Peerlist::Peerlist(peers_indexed& peers, size_t maxSize) :
  m_peers(peers), m_maxSize(maxSize) {
}

void PeerlistManager::serialize(ISerializer& s) {
  const uint8_t currentVersion = 1;
  uint8_t version = currentVersion;

  s(version, "version");

  if (version != currentVersion) {
    return;
  }

  s(m_peers_white, "whitelist");
  s(m_peers_gray, "graylist");
}

size_t PeerlistManager::Peerlist::count() const {
  return m_peers.size();
}

bool PeerlistManager::Peerlist::get(PeerlistEntry& entry, size_t i) const {
  if (i >= m_peers.size())
    return false;

  peers_indexed::index<by_time>::type& by_time_index = m_peers.get<by_time>();

  auto it = by_time_index.rbegin();
  std::advance(it, i);
  entry = *it;

  return true;
}

void PeerlistManager::Peerlist::trim() {
  peers_indexed::index<by_time>::type& sorted_index = m_peers.get<by_time>();
  while (m_peers.size() > m_maxSize) {
    sorted_index.erase(sorted_index.begin());
  }
}

PeerlistManager::PeerlistManager() : 
  m_whitePeerlist(m_peers_white, CryptoNote::P2P_LOCAL_WHITE_PEERLIST_LIMIT),
  m_grayPeerlist(m_peers_gray, CryptoNote::P2P_LOCAL_GRAY_PEERLIST_LIMIT) {}

//--------------------------------------------------------------------------------------------------
bool PeerlistManager::init(bool allow_local_ip)
{
  m_allow_local_ip = allow_local_ip;
  return true;
}

//--------------------------------------------------------------------------------------------------
void PeerlistManager::trim_white_peerlist() {
  m_whitePeerlist.trim();
}
//--------------------------------------------------------------------------------------------------
void PeerlistManager::trim_gray_peerlist() {
  m_grayPeerlist.trim();
}

//--------------------------------------------------------------------------------------------------
bool PeerlistManager::merge_peerlist(const std::list<PeerlistEntry>& outer_bs)
{ 
  for(const PeerlistEntry& be : outer_bs) {
    append_with_peer_gray(be);
  }

  // delete extra elements
  trim_gray_peerlist();
  return true;
}
//--------------------------------------------------------------------------------------------------

bool PeerlistManager::get_white_peer_by_index(PeerlistEntry& p, size_t i) const {
  return m_whitePeerlist.get(p, i);
}

//--------------------------------------------------------------------------------------------------

bool PeerlistManager::get_gray_peer_by_index(PeerlistEntry& p, size_t i) const {
  return m_grayPeerlist.get(p, i);
}

//--------------------------------------------------------------------------------------------------

bool PeerlistManager::is_ip_allowed(uint32_t ip) const
{
  System::Ipv4Address addr(networkToHost(ip));

  //never allow loopback ip
  if (addr.isLoopback()) {
    return false;
  }

  if (!m_allow_local_ip && addr.isPrivate()) {
    return false;
  }

  return true;
}
//--------------------------------------------------------------------------------------------------

bool PeerlistManager::get_peerlist_head(std::list<PeerlistEntry>& bs_head, uint32_t depth) const
{
  const peers_indexed::index<by_time>::type& by_time_index = m_peers_white.get<by_time>();
  uint32_t cnt = 0;

  BOOST_REVERSE_FOREACH(const peers_indexed::value_type& vl, by_time_index)
  {
    if (!vl.last_seen)
      continue;
    bs_head.push_back(vl);
    if (cnt++ > depth)
      break;
  }
  return true;
}
//--------------------------------------------------------------------------------------------------

bool PeerlistManager::get_peerlist_full(std::list<PeerlistEntry>& pl_gray, std::list<PeerlistEntry>& pl_white) const
{
  const peers_indexed::index<by_time>::type& by_time_index_gr = m_peers_gray.get<by_time>();
  const peers_indexed::index<by_time>::type& by_time_index_wt = m_peers_white.get<by_time>();

  std::copy(by_time_index_gr.rbegin(), by_time_index_gr.rend(), std::back_inserter(pl_gray));
  std::copy(by_time_index_wt.rbegin(), by_time_index_wt.rend(), std::back_inserter(pl_white));

  return true;
}
//--------------------------------------------------------------------------------------------------

bool PeerlistManager::set_peer_just_seen(PeerIdType peer, uint32_t ip, uint32_t port)
{
  NetworkAddress addr;
  addr.ip = ip;
  addr.port = port;
  return set_peer_just_seen(peer, addr);
}
//--------------------------------------------------------------------------------------------------

bool PeerlistManager::set_peer_just_seen(PeerIdType peer, const NetworkAddress& addr)
{
  try {
    //find in white list
    PeerlistEntry ple;
    ple.adr = addr;
    ple.id = peer;
    ple.last_seen = time(NULL);
    return append_with_peer_white(ple);
  } catch (std::exception&) {
  }

  return false;
}
//--------------------------------------------------------------------------------------------------

bool PeerlistManager::append_with_peer_white(const PeerlistEntry& ple)
{
  try {
    if (!is_ip_allowed(ple.adr.ip))
      return true;

    //find in white list
    auto by_addr_it_wt = m_peers_white.get<by_addr>().find(ple.adr);
    if (by_addr_it_wt == m_peers_white.get<by_addr>().end()) {
      //put new record into white list
      m_peers_white.insert(ple);
      trim_white_peerlist();
    } else {
      //update record in white list 
      m_peers_white.replace(by_addr_it_wt, ple);
    }
    //remove from gray list, if need
    auto by_addr_it_gr = m_peers_gray.get<by_addr>().find(ple.adr);
    if (by_addr_it_gr != m_peers_gray.get<by_addr>().end()) {
      m_peers_gray.erase(by_addr_it_gr);
    }
    return true;
  } catch (std::exception&) {
  }
  return false;
}
//--------------------------------------------------------------------------------------------------

bool PeerlistManager::append_with_peer_gray(const PeerlistEntry& ple)
{
  try {
    if (!is_ip_allowed(ple.adr.ip))
      return true;

    //find in white list
    auto by_addr_it_wt = m_peers_white.get<by_addr>().find(ple.adr);
    if (by_addr_it_wt != m_peers_white.get<by_addr>().end())
      return true;

    //update gray list
    auto by_addr_it_gr = m_peers_gray.get<by_addr>().find(ple.adr);
    if (by_addr_it_gr == m_peers_gray.get<by_addr>().end())
    {
      //put new record into white list
      m_peers_gray.insert(ple);
      trim_gray_peerlist();
    } else
    {
      //update record in white list 
      m_peers_gray.replace(by_addr_it_gr, ple);
    }
    return true;
  } catch (std::exception&) {
  }
  return false;
}
//--------------------------------------------------------------------------------------------------

PeerlistManager::Peerlist& PeerlistManager::getWhite() { 
  return m_whitePeerlist; 
}

PeerlistManager::Peerlist& PeerlistManager::getGray() { 
  return m_grayPeerlist; 
}
