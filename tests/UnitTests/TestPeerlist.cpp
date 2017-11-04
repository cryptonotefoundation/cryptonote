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

#include "gtest/gtest.h"

#include "Common/Util.h"

#include "P2p/PeerListManager.h"
#include "P2p/PeerListManager.cpp"

using namespace CryptoNote;

#define MAKE_IP( a1, a2, a3, a4 )	(a1|(a2<<8)|(a3<<16)|(a4<<24))


TEST(peer_list, peer_list_general)
{
  CryptoNote::PeerlistManager plm;
  plm.init(false);
#define ADD_GRAY_NODE(ip_, port_, id_, last_seen_) {  PeerlistEntry ple; ple.last_seen=last_seen_;ple.adr.ip = ip_; ple.adr.port = port_; ple.id = id_;plm.append_with_peer_gray(ple);}  
#define ADD_WHITE_NODE(ip_, port_, id_, last_seen_) {  PeerlistEntry ple;ple.last_seen=last_seen_; ple.adr.ip = ip_; ple.adr.port = port_; ple.id = id_;plm.append_with_peer_white(ple);}  

#define PRINT_HEAD(step) {std::list<PeerlistEntry> bs_head; bool r = plm.get_peerlist_head(bs_head, 100);std::cout << "step " << step << ": " << bs_head.size() << std::endl;}

  ADD_GRAY_NODE(MAKE_IP(123,43,12,1), 8080, 121241, 34345);
  ADD_GRAY_NODE(MAKE_IP(123,43,12,2), 8080, 121241, 34345);
  ADD_GRAY_NODE(MAKE_IP(123,43,12,3), 8080, 121241, 34345);
  ADD_GRAY_NODE(MAKE_IP(123,43,12,4), 8080, 121241, 34345);
  ADD_GRAY_NODE(MAKE_IP(123,43,12,5), 8080, 121241, 34345);

  ADD_WHITE_NODE(MAKE_IP(123,43,12,1), 8080, 121241, 34345);
  ADD_WHITE_NODE(MAKE_IP(123,43,12,2), 8080, 121241, 34345);
  ADD_WHITE_NODE(MAKE_IP(123,43,12,3), 8080, 121241, 34345);
  ADD_WHITE_NODE(MAKE_IP(123,43,12,4), 8080, 121241, 34345);

  size_t gray_list_size = plm.get_gray_peers_count();
  ASSERT_EQ(gray_list_size, 1);

  std::list<PeerlistEntry> bs_head;
  bool r = plm.get_peerlist_head(bs_head, 100);
  std::cout << bs_head.size() << std::endl;
  ASSERT_TRUE(r);

  ASSERT_EQ(bs_head.size(), 4);


  ADD_GRAY_NODE(MAKE_IP(123,43,12,5), 8080, 121241, 34345);
  ASSERT_EQ(plm.get_gray_peers_count(), 1);
  ASSERT_EQ(plm.get_white_peers_count(), 4);
}


TEST(peer_list, merge_peer_lists)
{
  //([^ \t]*)\t([^ \t]*):([^ \t]*) \tlast_seen: d(\d+)\.h(\d+)\.m(\d+)\.s(\d+)\n
  //ADD_NODE_TO_PL("\2", \3, 0x\1, (1353346618 -(\4*60*60*24+\5*60*60+\6*60+\7 )));\n
  PeerlistManager plm;
  plm.init(false);
  std::list<PeerlistEntry> outer_bs;


}
