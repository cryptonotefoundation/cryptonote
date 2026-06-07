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

#include "NetworkConfiguration.h"

#include <cstdlib>
#include <vector>

#include <boost/filesystem.hpp>

namespace Tests {

namespace {

#ifdef _WIN32
const char DAEMON_FILENAME[] = "karbowanecd.exe";
#else
const char DAEMON_FILENAME[] = "karbowanecd";
#endif

const char* BUILD_CONFIGS[] = { "", "Release", "Debug", "RelWithDebInfo", "MinSizeRel" };

bool isExistingFile(const boost::filesystem::path& path) {
  boost::system::error_code ec;
  return boost::filesystem::exists(path, ec) && !boost::filesystem::is_directory(path, ec);
}

bool isExistingDirectory(const boost::filesystem::path& path) {
  boost::system::error_code ec;
  return boost::filesystem::exists(path, ec) && boost::filesystem::is_directory(path, ec);
}

void addDaemonCandidates(std::vector<boost::filesystem::path>& candidates, const boost::filesystem::path& dir) {
  for (const char* config : BUILD_CONFIGS) {
    boost::filesystem::path candidateDir = dir;
    if (*config != '\0') {
      candidateDir /= config;
    }

    candidates.push_back(candidateDir / DAEMON_FILENAME);
  }
}

void addBuildLayoutCandidates(std::vector<boost::filesystem::path>& candidates, const boost::filesystem::path& base) {
  addDaemonCandidates(candidates, base);
  addDaemonCandidates(candidates, base / "src");
  addDaemonCandidates(candidates, base / "build" / "src");
}

}

std::string getTestDaemonFilename() {
  return DAEMON_FILENAME;
}

std::string resolveTestDaemonPath(const std::string& configuredPath) {
  std::vector<boost::filesystem::path> candidates;
  boost::filesystem::path configured = configuredPath.empty() ? boost::filesystem::path(DAEMON_FILENAME) : boost::filesystem::path(configuredPath);

  if (configured.is_absolute()) {
    if (isExistingDirectory(configured)) {
      addDaemonCandidates(candidates, configured);
    } else {
      candidates.push_back(configured);
    }
  } else {
    boost::filesystem::path current = boost::filesystem::current_path();
    boost::filesystem::path absoluteConfigured = boost::filesystem::absolute(configured, current);

    if (isExistingDirectory(absoluteConfigured)) {
      addDaemonCandidates(candidates, absoluteConfigured);
    } else {
      candidates.push_back(absoluteConfigured);
    }

    for (boost::filesystem::path base = current; !base.empty(); base = base.parent_path()) {
      addBuildLayoutCandidates(candidates, base);
      if (base == base.parent_path()) {
        break;
      }
    }
  }

  if (const char* envPath = std::getenv("KARBO_TEST_DAEMON")) {
    if (*envPath != '\0') {
      candidates.push_back(boost::filesystem::path(envPath));
    }
  }

  for (const auto& candidate : candidates) {
    if (isExistingFile(candidate)) {
      return boost::filesystem::absolute(candidate).string();
    }
  }

  return boost::filesystem::absolute(configured).string();
}

}
