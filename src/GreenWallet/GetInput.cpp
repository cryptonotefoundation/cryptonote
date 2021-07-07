// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2019, The Karbo Developers
// 
// Please see the included LICENSE file for more information.

///////////////////////////////
#include <GreenWallet/GetInput.h>
///////////////////////////////

#include <boost/algorithm/string.hpp>

#include "linenoise.hpp"

#include <GreenWallet/Sync.h>

/* Note: this is not portable, it only works with terminals that support ANSI
   codes (e.g., not Windows) - however! due to the way linenoise-cpp works,
   it will actually convert these codes for us to the windows equivalent. <3 */
std::string yellowANSIMsg(std::string msg)
{
    const std::string CYELLOW = "\033[1;33m";
    const std::string RESET = "\033[0m";
    return CYELLOW + msg + RESET;
}

std::string getPrompt(std::shared_ptr<WalletInfo> walletInfo)
{
    const int promptLength = 20;
    const std::string extension = ".wallet";

    std::string walletName = walletInfo->walletFileName;

    /* Filename ends in .wallet, remove extension */
    if (std::equal(extension.rbegin(), extension.rend(), 
                   walletInfo->walletFileName.rbegin()))
    {
        const size_t extPos = walletInfo->walletFileName.find_last_of('.');

        walletName = walletInfo->walletFileName.substr(0, extPos);
    }

    const std::string shortFileName = walletName.substr(0, promptLength - 9);

    const std::string addrStart = walletInfo->walletAddress.substr(0, 6);

    return "[" + addrStart + " " + shortFileName + "]: ";
}

template<typename T>
std::string getInputAndWorkInBackground(const std::vector<T> &availableCommands,
                                        std::string prompt,
                                        bool backgroundRefresh,
                                        std::shared_ptr<WalletInfo> walletInfo)
{
    /* If we are in the main program, we need to check for transactions in
       the background. Unfortunately, we have to do this on the main thread,
       so the best way to do it, is to check whilst waiting for an input on
       another thread. */
    if (backgroundRefresh)
    {
        auto lastUpdated = std::chrono::system_clock::now();

        std::future<std::string> inputGetter = std::async(std::launch::async, 
        [&availableCommands, &prompt]
        {
            return getInput(availableCommands, prompt);
        });


        while (true)
        {
            /* Check if the user has inputted something yet
               (Wait for zero seconds to instantly return) */
            std::future_status status = inputGetter
                                       .wait_for(std::chrono::seconds(0));

            /* User has inputted, get what they inputted and return it */
            if (status == std::future_status::ready)
            {
                return inputGetter.get();
            }

            const auto currentTime = std::chrono::system_clock::now();

            /* Otherwise check if we need to update the wallet cache */
            if ((currentTime - lastUpdated) > std::chrono::seconds(5))
            {
                lastUpdated = currentTime;
                checkForNewTransactions(walletInfo);
            }

            /* Sleep for enough for it to not be noticeable when the user
               enters something, but enough that we're not starving the CPU */
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    else
    {
        return getInput(availableCommands, prompt);
    }
}

template<typename T>
std::string getInput(const std::vector<T> &availableCommands,
                     std::string prompt)
{
    linenoise::SetCompletionCallback(
    [availableCommands](const char *input, std::vector<std::string> &completions)
    {
        /* Convert to std::string */
        std::string c = input;

        for (const auto &command : availableCommands)
        {
            /* Does command begin with input? */
            if (command.commandName.compare(0, c.length(), c) == 0)
            {
                completions.push_back(command.commandName);
            }
        }
    });

    /* Linenoise is printing this out, so we can't write colours to the stream
       like we normally would - have to include the escape characters directly
       in the string. Obviously this is not platform dependent - but linenoise
       doesn't work on windows, so it's fine. */
    std::string promptMsg = yellowANSIMsg(prompt);

    /* 256 max commands in the wallet command history */
    linenoise::SetHistoryMaxLen(256);

    /* The inputted command */
    std::string command;

    bool quit = linenoise::Readline(promptMsg.c_str(), command);
	
    /* Remove any whitespace */
    boost::algorithm::trim(command);

    if (command != "")
    {
        linenoise::AddHistory(command.c_str());
    }

    /* Ctrl-C, Ctrl-D, etc */
    if (quit)
    {
        return "exit";
    }

    return command;
}

/* Template instantations that we are going to use - this allows us to have
   the template implementation in the .cpp file. */
template
std::string getInput(const std::vector<Command> &availableCommands,
                     std::string prompt);

template
std::string getInput(const std::vector<AdvancedCommand> &availableCommands,
                     std::string prompt);

template
std::string getInputAndWorkInBackground(const std::vector<Command>
                                        &availableCommands,
                                        std::string prompt,
                                        bool backgroundRefresh,
                                        std::shared_ptr<WalletInfo>
                                        walletInfo);
template
std::string getInputAndWorkInBackground(const std::vector<AdvancedCommand>
                                        &availableCommands,
                                        std::string prompt,
                                        bool backgroundRefresh,
                                        std::shared_ptr<WalletInfo>
                                        walletInfo);
