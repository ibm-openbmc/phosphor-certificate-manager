#pragma once
#include "logger.hpp"
#include "sdbus_calls_runner.hpp"

#include <openssl/evp.h>

#include <boost/process.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <vector>
static constexpr auto acfdirectory = "/tmp/acf";
static constexpr auto dreportexe = "/usr/bin/dreport";
namespace bp = boost::process;
namespace scrrunner
/**
 * @brief ScriptRunner is a utility struct for managing the execution of shell
 * scripts, capturing their output, and optionally triggering dump operations.
 *
 * This struct provides asynchronous execution of scripts using Boost.Process
 * and Boost.Asio, manages script output, and supports cancellation and cleanup
 * of running scripts. It also supports hashing scripts for identification and
 * caching purposes.
 *
 * Features:
 * - Asynchronous script execution with output/error capture.
 * - Optional dump operation after script execution.
 * - Script file and output file management.
 * - Script cancellation and cleanup.
 * - SHA256 hashing of script content for unique identification.
 *
 * Usage:
 * - Use run_script() to start a script asynchronously.
 * - Use cancel_script() to terminate a running script.
 * - Script output is written to a file and can be processed as needed.
 *
 * Thread Safety:
 * - Not thread-safe. Intended for use within a single-threaded io_context.
 *
 * Dependencies:
 * - Boost.Process
 * - Boost.Asio (net)
 * - OpenSSL EVP API (for hashing)
 * - C++17 or later (for std::optional, std::filesystem, etc.)
 *
 * Example:
 * @code
 *   ScriptRunner runner(io_context, dbus_connection);
 *   runner.run_script("id123", "#!/bin/bash\necho Hello", false, callback);
 * @endcode
 */
{
struct ScriptRunner
{
    std::map<std::string, std::unique_ptr<sdbusplus::bus::match::match>>
        dumpProgressMatches;
    using Callback =
        std::function<void(boost::system::error_code, std::string)>;
    static std::optional<std::string> makeHash(const std::string& script)
    {
        // Create a SHA256 hash of the script string using EVP API
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hash_len;
        EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
        if (mdctx == nullptr)
        {
            LOG_ERROR("Failed to create EVP_MD_CTX");
            return std::nullopt;
        }
        if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1 ||
            EVP_DigestUpdate(mdctx, script.c_str(), script.size()) != 1 ||
            EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1)
        {
            LOG_ERROR("Failed to compute SHA256 hash");
            EVP_MD_CTX_free(mdctx);
            return std::nullopt;
        }
        EVP_MD_CTX_free(mdctx);

        // Convert the hash to a hexadecimal string
        std::string hash_str;
        for (unsigned int i = 0; i < hash_len; ++i)
        {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02x", hash[i]);
            hash_str += buf;
        }
        // Restrict the hash length to 16 characters
        constexpr size_t maxHashLen = 16;
        if (hash_str.length() > maxHashLen)
        {
            hash_str.resize(maxHashLen);
        }
        return hash_str;
    }
    std::string scriptDir(std::string id)
    {
        std::string dir = std::format("{}/{}", acfdirectory, id);
        if (!std::filesystem::exists(dir))
        {
            std::filesystem::create_directories(dir);
        }
        return dir;
    }
    std::string scriptFileName(std::string id)
    {
        return std::format("{}/{}.sh", scriptDir(id), id);
    }
    std::string scriptOutputFileName(std::string id)
    {
        return std::format("{}/{}.out", scriptDir(id), id);
    }
    net::awaitable<boost::system::error_code> writeResult(bp::async_pipe& ap,
                                                          std::ostream& os)
    {
        std::vector<char> buf(4096);
        boost::system::error_code ec{};
        while (!ec)
        {
            auto size = co_await net::async_read(
                ap, net::buffer(buf),
                net::redirect_error(net::use_awaitable, ec));
            if (ec && ec != net::error::eof)
            {
                LOG_INFO("Error: {}", ec.message());
                break;
            }
            os.write(buf.data(), size);
        }
        co_return (ec == net::error::eof ? boost::system::error_code{} : ec);
    }
    net::awaitable<boost::system::error_code> writeResult(
        bp::async_pipe& out, bp::async_pipe& err, std::ostream& ofs)
    {
        auto ec = co_await writeResult(out, ofs);
        if (ec)
        {
            LOG_ERROR("{}", ec.message());
            co_return ec;
        }
        ec = co_await writeResult(err, ofs);
        if (ec)
        {
            LOG_ERROR("{}", ec.message());
            co_return ec;
        }
        co_return boost::system::error_code{};
    }
    void monitorDumpProgress(const std::string& id, const std::string& dumpId)
    {
        std::string matchRule = sdbusplus::bus::match::rules::propertiesChanged(
            "/xyz/openbmc_project/dump/bmc/entry/" + dumpId,
            "xyz.openbmc_project.Common.Progress");
        auto propcallback = [this, id,
                             dumpId](sdbusplus::message::message& msg) {
            std::string interfaceName;
            std::map<std::string, std::variant<std::string>> changedProperties;
            std::vector<std::string> invalidatedProperties;

            msg.read(interfaceName, changedProperties, invalidatedProperties);

            LOG_INFO("Properties changed on interface: {}", interfaceName);

            changedProperties | std::ranges::views::filter([](const auto& p) {
                return p.first == "Status";
            });
            for (const auto& [prop, value] : changedProperties)
            {
                LOG_DEBUG("Dump {} status changed: {}", id,
                          std::get<std::string>(value));
                if (std::get<std::string>(value) ==
                    "xyz.openbmc_project.Common.Progress.OperationStatus.Completed")
                {
                    std::filesystem::remove_all(scriptDir(id));
                    dumpProgressMatches.erase(dumpId);
                }
            }
        };
        dumpProgressMatches.emplace(
            dumpId, std::make_unique<sdbusplus::bus::match::match>(
                        *conn, matchRule.c_str(), std::move(propcallback)));
    }
    net::awaitable<void> startDump(const std::string& id)
    {
        using paramtype = std::vector<
            std::pair<std::string, std::variant<std::string, uint64_t>>>;
        std::string dumpId;
        while (true)
        {
            auto [ec, path] = co_await awaitable_dbus_method_call<
                sdbusplus::message::object_path>(
                *conn, "xyz.openbmc_project.Dump.Manager",
                "/xyz/openbmc_project/dump/bmc",
                "xyz.openbmc_project.Dump.Create", "CreateDump", paramtype());

            if (!ec)
            {
                const std::string dumpPath = path.parent_path().str;
                dumpId = path.filename();
                LOG_DEBUG("Dump created for {} at path: {}, id: {}", id,
                          dumpPath, dumpId);
                break;
            }
            LOG_ERROR("Error creating dump: {}", ec.message());
            co_await net::steady_timer(io_context, std::chrono::seconds(20))
                .async_wait(net::use_awaitable);
        }

        monitorDumpProgress(id, dumpId);
        // if (!std::filesystem::exists(dreportexe))
        // {
        //     LOG_ERROR("dreport not found");
        //     co_return;
        // }
        // bp::async_pipe ap(io_context);
        // bp::async_pipe ep(io_context);
        // LOG_DEBUG("Started Dump for {}", id);
        // bp::child dump(dreportexe, "-n", id, bp::start_dir = scriptDir(id),
        //                bp::std_out > ap, bp::std_err > ep);
        // if (dump.running())
        // {
        //     co_await writeResult(ap, ep, std::cout);
        //     co_return;
        // }
        // LOG_ERROR("Faild to start dump for {}", id);
    }
    /**
     * @brief Executes a script asynchronously using bash and handles its
     * output.
     *
     * This coroutine launches a child process to execute the specified script
     * file, captures its standard output and error streams, writes the output
     * to a file, and optionally triggers a dump operation if required. Upon
     * completion or error, the provided callback is invoked with the result.
     *
     * @param filename The path to the script file to execute.
     * @param hash A unique identifier (hash) for the script execution context.
     * @param dumpNeeded Indicates whether a dump operation should be performed
     * after execution.
     * @param callback A callback function to be called upon completion or
     * error.
     *
     * @return net::awaitable<void> Awaitable coroutine handle.
     */
    net::awaitable<void> execute(const std::string& filename,
                                 const std::string& hash, bool dumpNeeded,
                                 Callback callback)
    {
        try
        {
            bp::async_pipe ap(io_context);
            bp::async_pipe ep(io_context);

            boost::system::error_code ec;
            bp::child c("/usr/bin/bash", filename,
                        bp::start_dir = scriptDir(hash), bp::std_out > ap,
                        bp::std_err > ep);
            if (!c.running())
            {
                LOG_ERROR("Failed to start child process");
                callback(boost::asio::error::operation_aborted, hash);
                co_return;
            }
            script_cache.emplace(hash,
                                 ScriptEntry{std::ref(c), std::move(callback)});

            std::ofstream ofs(scriptOutputFileName(hash));
            co_await writeResult(ap, ep, ofs);
            if (c.exit_code() != 0)
            {
                LOG_DEBUG("Script execution failed with exit code: {}",
                          c.exit_code());
                ofs << "Script execution failed with exit code: "
                    << c.exit_code() << std::endl;
            }
            ofs.close();
            if (dumpNeeded)
            {
                co_await startDump(hash);
            }
            else
            {
                LOG_DEBUG("Dump not needed for script {}", hash);
                // Remove the script directory and its contents
                std::filesystem::remove_all(scriptDir(hash));
            }
            invokeCallback(boost::system::error_code{}, hash);
            remove(hash);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Exception: {}", e.what());
            callback(boost::asio::error::operation_aborted, hash);
        }
    }
    void invokeCallback(boost::system::error_code ec, const std::string& id)
    {
        auto it = script_cache.find(id);
        if (it == script_cache.end())
        {
            return;
        }
        it->second.callback(ec, id);
    }
    void remove(const std::string& id)
    {
        script_cache.erase(id);
    }
    bool run_script(const std::string& id, const std::string& script,
                    bool dumpNeeded, Callback callback)
    {
        auto filename = scriptFileName(id);
        // Write the script to a file
        std::ofstream script_file(filename);
        if (!script_file)
        {
            LOG_ERROR("Failed to create script file: {}", filename);
            return false;
        }
        script_file << script;
        script_file.close();

        net::co_spawn(
            io_context,
            [this, filename, id, dumpNeeded,
             callback = std::move(callback)]() mutable -> net::awaitable<void> {
                co_await execute(filename, id, dumpNeeded, std::move(callback));
            },
            net::detached);
        return true;
    }
    /**
     * @brief Cancels a running script identified by the given ID.
     *
     * This function searches for the script in the script cache using the
     * provided ID. If found, it terminates the associated child process,
     * invokes the registered callback with a default error code and the script
     * ID, removes the script from the cache, and returns true. If the script is
     * not found, it returns false.
     *
     * @param id The unique identifier of the script to cancel.
     * @return true if the script was found and cancelled; false otherwise.
     */
    bool cancel_script(const std::string& id)
    {
        auto it = script_cache.find(id);
        if (it == script_cache.end())
        {
            return false;
        }
        LOG_DEBUG("Cancelling Script {} ", id);
        it->second.child.get().terminate();
        it->second.callback(boost::system::error_code{}, id);
        remove(id);
        return true;
    }
    ScriptRunner(net::io_context& io_context,
                 std::shared_ptr<sdbusplus::asio::connection> conn) :
        io_context(io_context), conn(conn)
    {}
    ~ScriptRunner()
    {
        while (!script_cache.empty())
        {
            auto p = *script_cache.begin();
            p.second.child.get().terminate();
            remove(p.first);
        }
    }
    net::io_context& io_context;
    std::shared_ptr<sdbusplus::asio::connection> conn;
    struct ScriptEntry
    {
        std::reference_wrapper<bp::child> child;
        std::function<void(boost::system::error_code, std::string)> callback;
    };
    std::map<std::string, ScriptEntry> script_cache;
};
} // namespace scrrunner
