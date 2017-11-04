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

#include <boost/serialization/split_free.hpp>
#include <unordered_map>
#include <unordered_set>

#include "google/sparse_hash_set"
#include "google/sparse_hash_map"

namespace boost
{
  namespace serialization
  {
    template <class Archive, class h_key, class hval, class hfunc>
    inline void save(Archive &a, const std::unordered_map<h_key, hval, hfunc> &x, const boost::serialization::version_type ver)
    {
      size_t s = x.size();
      a << s;
      BOOST_FOREACH(auto& v, x)
      {
        a << v.first;
        a << v.second;
      }
    }

    template <class Archive, class h_key, class hval, class hfunc>
    inline void load(Archive &a, std::unordered_map<h_key, hval, hfunc> &x, const boost::serialization::version_type ver)
    {
      x.clear();
      size_t s = 0;
      a >> s;
      x.reserve(s);
      for(size_t i = 0; i != s; i++)
      {
        h_key k;
        hval v;
        a >> k;
        a >> v;
        x.insert(std::pair<h_key, hval>(k, v));
      }
    }


    template <class Archive, class h_key, class hval>
    inline void save(Archive &a, const std::unordered_multimap<h_key, hval> &x, const boost::serialization::version_type ver)
    {
      size_t s = x.size();
      a << s;
      BOOST_FOREACH(auto& v, x)
      {
        a << v.first;
        a << v.second;
      }
    }

    template <class Archive, class h_key, class hval>
    inline void load(Archive &a, std::unordered_multimap<h_key, hval> &x, const boost::serialization::version_type ver)
    {
      x.clear();
      size_t s = 0;
      a >> s;
      x.reserve(s);
      for (size_t i = 0; i != s; i++)
      {
        h_key k;
        hval v;
        a >> k;
        a >> v;
        x.emplace(k, v);
      }
    }


    template <class Archive, class hval>
    inline void save(Archive &a, const std::unordered_set<hval> &x, const boost::serialization::version_type ver)
    {
      size_t s = x.size();
      a << s;
      BOOST_FOREACH(auto& v, x)
      {
        a << v;
      }
    }

    template <class Archive, class hval>
    inline void load(Archive &a, std::unordered_set<hval> &x, const boost::serialization::version_type ver)
    {
      x.clear();
      size_t s = 0;
      a >> s;
      x.reserve(s);
      for (size_t i = 0; i != s; i++)
      {
        hval v;
        a >> v;
        x.insert(v);
      }
    }

    template <class Archive, class hval>
    inline void save(Archive &a, const ::google::sparse_hash_set<hval> &x, const boost::serialization::version_type ver)
    {
      size_t s = x.size();
      a << s;
      BOOST_FOREACH(auto& v, x)
      {
        a << v;
      }
    }

    template <class Archive, class hval>
    inline void load(Archive &a, ::google::sparse_hash_set<hval> &x, const boost::serialization::version_type ver)
    {
      x.clear();
      size_t s = 0;
      a >> s;
      x.resize(s);
      for(size_t i = 0; i != s; i++)
      {
        hval v;
        a >> v;
        x.insert(v);
      }
    }

    template <class Archive, class hval>
    inline void serialize(Archive &a, ::google::sparse_hash_set<hval> &x, const boost::serialization::version_type ver)
    {
      split_free(a, x, ver);
    }

    template <class Archive, class h_key, class hval>
    inline void save(Archive &a, const ::google::sparse_hash_map<h_key, hval> &x, const boost::serialization::version_type ver)
    {
      size_t s = x.size();
      a << s;
      BOOST_FOREACH(auto& v, x)
      {
        a << v.first;
        a << v.second;
      }
    }

    template <class Archive, class h_key, class hval>
    inline void load(Archive &a, ::google::sparse_hash_map<h_key, hval> &x, const boost::serialization::version_type ver)
    {
      x.clear();
      size_t s = 0;
      a >> s;
      x.resize(s);
      for(size_t i = 0; i != s; i++)
      {
        h_key k;
        hval v;
        a >> k;
        a >> v;
        x.insert(std::pair<h_key, hval>(k, v));
      }
    }

    template <class Archive, class h_key, class hval>
    inline void serialize(Archive &a, ::google::sparse_hash_map<h_key, hval> &x, const boost::serialization::version_type ver)
    {
      split_free(a, x, ver);
    }

    template <class Archive, class h_key, class hval, class hfunc>
    inline void serialize(Archive &a, std::unordered_map<h_key, hval, hfunc> &x, const boost::serialization::version_type ver)
    {
      split_free(a, x, ver);
    }

    template <class Archive, class h_key, class hval>
    inline void serialize(Archive &a, std::unordered_multimap<h_key, hval> &x, const boost::serialization::version_type ver)
    {
      split_free(a, x, ver);
    }

    template <class Archive, class hval>
    inline void serialize(Archive &a, std::unordered_set<hval> &x, const boost::serialization::version_type ver)
    {
      split_free(a, x, ver);
    }
  }
}
