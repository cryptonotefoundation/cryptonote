// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2019, The Karbo Developers
// 
// Please see the included LICENSE file for more information.

/////////////////////////////////////
#include <GreenWallet/ParseArguments.h>
/////////////////////////////////////

#include "CryptoNoteConfig.h"

#include <iomanip>
#include <initializer_list>
#include <iostream>

#include <boost/filesystem.hpp>

#include "version.h"

#include <Common/UrlTools.h>
#include <GreenWallet/WalletConfig.h>

bool validateCertPath(std::string &path) {
  bool res = false;
  boost::system::error_code ec;
  boost::filesystem::path data_dir_path(boost::filesystem::current_path());
  boost::filesystem::path cert_file_path(path);
  if (!cert_file_path.has_parent_path()) cert_file_path = data_dir_path / cert_file_path;
  if (boost::filesystem::exists(cert_file_path, ec)) {
    path = boost::filesystem::canonical(cert_file_path).string();
    res = true;
  } else {
    path.clear();
    res = false;
  }
  return res;
}

/* Thanks to https://stackoverflow.com/users/85381/iain for this small command
   line parsing snippet! https://stackoverflow.com/a/868894/8737306 */
char* getCmdOption(char ** begin, char ** end, const std::string & option)
{
    auto it = std::find(begin, end, option);

    if (it != end && ++it != end)
    {
        return *it;
    }

    return 0;
}

bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}

Config parseArguments(int argc, char **argv)
{
    Config config;

    if (cmdOptionExists(argv, argv+argc, "-h")
     || cmdOptionExists(argv, argv+argc, "--help"))
    {
        helpMessage();
        config.exit = true;
        return config;
    }

    if (cmdOptionExists(argv, argv+argc, "-v")
     || cmdOptionExists(argv, argv+argc, "--version"))
    {
        std::cout << getVersion() << std::endl;
        config.exit = true;
        return config;
    }

    if (cmdOptionExists(argv, argv+argc, "--wallet-file"))
    {
        char *wallet = getCmdOption(argv, argv+argc, "--wallet-file");

        if (!wallet)
        {
            std::cout << "--wallet-file was specified, but no wallet file "
                      << "was given!" << std::endl;

            helpMessage();
            config.exit = true;
            return config;
        }

        config.walletFile = std::string(wallet);
        config.walletGiven = true;
    }

    if (cmdOptionExists(argv, argv+argc, "--password"))
    {
        char *password = getCmdOption(argv, argv+argc, "--password");

        if (!password)
        {
            std::cout << "--password was specified, but no password was "
                      << "given!" << std::endl;

            helpMessage();
            config.exit = true;
            return config;
        }

        config.walletPass = std::string(password);
        config.passGiven = true;
    }

    if (cmdOptionExists(argv, argv+argc, "--remote-daemon"))
    {
        char *url = getCmdOption(argv, argv + argc, "--remote-daemon");

        /* No url following --remote-daemon */
        if (!url)
        {
            std::cout << "--remote-daemon was specified, but no daemon was "
                      << "given!" << std::endl;

            helpMessage();

            config.exit = true;
        }
        else
        {
            std::string urlString(url);

            if (!Common::parseUrlAddress(urlString, config.host, config.port,
                                         config.path, config.ssl)) {

                std::cout << "Failed to parse daemon address!" << std::endl;
                config.exit = true;
            }

        }
    }

    if (cmdOptionExists(argv, argv+argc, "--daemon-cert"))
    {
        char *certPath = getCmdOption(argv, argv + argc, "--daemon-cert");

        if (!certPath)
        {
            std::cout << "--daemon-cert was specified, but no cert was "
                      << "given!" << std::endl;

            helpMessage();

            config.exit = true;
        }
        else
        {
            config.daemonCert = certPath;

            if (!validateCertPath(config.daemonCert)) {

                std::cout << "Custom cert file could not be found!" << std::endl;
            }

        }
    }

    if (cmdOptionExists(argv, argv+argc, "--daemon-no-verify"))
    {
      config.disableVerify = true;
    }

    return config;
}

std::string getVersion()
{
    return WalletConfig::coinName + " v" + PROJECT_VERSION + " "
         + WalletConfig::walletName;
}

std::vector<CLICommand> getCLICommands()
{
    std::vector<CLICommand> commands =
    {
        {"--help", "Display this help message and exit", "-h", true, false},

        {"--version", "Display the version information and exit", "-v", true,
         false},

        {"--remote-daemon <url>", "Connect to the remote daemon at <url>", "",
         false, true},

        {"--daemon-cert <path>", "Custom cert file at <path> for performing verification", "",
         false, true},

        {"--daemon-no-verify", "Disable verification procedure", "", false, false},

        {"--wallet-file <file>", "Open the wallet <file>", "", false, true},

        {"--password <pass>", "Use the password <pass> to open the wallet", "",
         false, true}
    };

    /* Pop em in alphabetical order */
    std::sort(commands.begin(), commands.end(), [](const CLICommand &lhs,
                                                   const CLICommand &rhs)
    {
        return lhs.name < rhs.name;
    });


    return commands;
}

void helpMessage()
{
    std::cout << getVersion() << std::endl;

    const auto commands = getCLICommands();

    std::cout << std::endl
              << WalletConfig::walletName;

    for (auto &command : commands)
    {
        if (command.hasArgument)
        {
            std::cout << " [" << command.name << "]";
        }
    }

    std::cout << std::endl << std::endl
              << "Commands: " << std::endl;

    for (auto &command : commands)
    {
        if (command.hasShortName)
        {
            std::cout << "  " << command.shortName << ", "
                      << std::left << std::setw(25) << command.name
                      << command.description << std::endl;
        }
        else
        {
            std::cout << "      " << std::left << std::setw(25) << command.name
                      << command.description << std::endl;
        }
    }
}
