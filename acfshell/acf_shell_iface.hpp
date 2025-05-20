#pragma once
#include "script_iface.hpp"
#include "script_runner.hpp"
#include "sdbus_calls_runner.hpp"

#include <memory>
#include <vector>
namespace scrrunner
/**
 * @struct AcfShellIface
 * @brief Manages the D-Bus interface for ACF shell script execution.
 *
 * The AcfShellIface structure encapsulates the logic for exposing and managing
 * script execution over D-Bus. It provides methods to query active scripts,
 * start new scripts, and cancel running scripts via D-Bus method calls.
 * The interface is registered under the bus name "xyz.openbmc_project.acfshell"
 * and object path "/xyz/openbmc_project/acfshell" with the interface name
 * "xyz.openbmc_project.TacfShell".
 *
 * Members:
 * - io_context: Reference to the Boost.Asio IO context for asynchronous
 * operations.
 * - scriptRunner: Reference to the ScriptRunner responsible for executing
 * scripts.
 * - conn: Shared pointer to the sdbusplus ASIO connection for D-Bus
 * communication.
 * - dbusServer: D-Bus object server for registering interfaces and methods.
 * - iface: Shared pointer to the D-Bus interface for the shell.
 * - busName: D-Bus bus name for the shell interface.
 * - objPath: D-Bus object path for the shell interface.
 * - interface: D-Bus interface name for the shell.
 * - scriptIfaces: Container for active ScriptIface instances.
 *
 * Key Methods:
 * - AcfShellIface(...): Constructor that initializes the D-Bus interface and
 * registers methods.
 * - addToActive(...): Adds and starts a script, managing its lifecycle.
 * - runScript(...): Runs a script and manages its timeout and storage.
 * - execute(...): Asynchronously executes a script via D-Bus.
 * - getScriptIface(...): Retrieves a ScriptIface by script ID.
 * - removeFromActive(...): Removes a script from the active list by ID.
 */
{
struct AcfShellIface
{
    net::io_context& io_context;
    ScriptRunner& scriptRunner;
    std::shared_ptr<sdbusplus::asio::connection> conn;
    sdbusplus::asio::object_server dbusServer;
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface;
    static constexpr std::string_view busName = "xyz.openbmc_project.acfshell";
    static constexpr std::string_view objPath = "/xyz/openbmc_project/acfshell";
    static constexpr std::string_view interface =
        "xyz.openbmc_project.TacfShell";
    std::vector<std::unique_ptr<ScriptIface>> scriptIfaces;
    /**
     * @brief Constructs an AcfShellIface object to manage D-Bus interface for
     * script execution.
     *
     * This constructor initializes the D-Bus interface for the ACF shell,
     * registers methods for querying active scripts, starting new scripts, and
     * cancelling running scripts.
     *
     * @param ioc Reference to the Boost.Asio IO context used for asynchronous
     * operations.
     * @param runner Reference to the ScriptRunner responsible for executing
     * scripts.
     * @param conn Shared pointer to the sdbusplus ASIO connection for D-Bus
     * communication.
     *
     * The following D-Bus methods are registered:
     * - "active": Returns a list of currently active script IDs.
     * - "start": Starts a new script with the given name, timeout, and
     * dumpNeeded flag.
     * - "cancel": Cancels the script with the specified ID.
     */
    AcfShellIface(net::io_context& ioc, ScriptRunner& runner,
                  std::shared_ptr<sdbusplus::asio::connection> conn) :
        io_context(ioc), scriptRunner(runner), conn(conn), dbusServer(conn)
    {
        conn->request_name(busName.data());
        iface = dbusServer.add_interface(objPath.data(), interface.data());
        // test generic properties

        iface->register_method("active", [this]() {
            std::vector<std::string> activeScripts;
            for (const auto& iface : scriptIfaces)
            {
                activeScripts.push_back(iface->data.id);
            }
            return activeScripts;
        });

        iface->register_method(
            "start", [this](const std::string& script, uint64_t timeout,
                            bool dumpNeeded) {
                ensureMaxActiveScripts();
                return addToActive(script, timeout, dumpNeeded);
            });
        iface->register_method("cancel", [this](const std::string& id) {
            auto iface = getScriptIface(id);
            if (iface)
            {
                return iface->cancel();
            }
            return false;
        });

        iface->initialize();
    }
    void ensureMaxActiveScripts()
    {
        // Ensure the number of active scripts does not exceed a certain limit
        constexpr size_t maxActiveScripts = 1;
        while (scriptIfaces.size() >= maxActiveScripts)
        {
            LOG_DEBUG(
                "Cancelling oldest script to maintain max active scripts");
            auto iface = std::move(scriptIfaces.front());
            scriptIfaces.erase(scriptIfaces.begin());
            iface->cancel();
        }
    }
    auto makeScriptId(const std::string& script)
    {
        // Prepend current time to the script before hashing
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        std::string scriptWithTime = std::to_string(now_time_t) + "_" + script;
        auto scriptId = ScriptRunner::makeHash(scriptWithTime);
        return scriptId;
    }
    /**
     * @brief Adds a script to the list of active scripts and starts its
     * execution.
     *
     * This function generates a unique hash for the provided script, logs the
     * attempt to start the script, and creates a new ScriptIface instance to
     * manage the script execution. The script is then started with the
     * specified timeout and dumpNeeded flag. If any error occurs during the
     * process, it is logged and the function returns false.
     *
     * @param script The script content to be executed.
     * @param timeout The timeout value (in minutes) for the script execution.
     * @param dumpNeeded Indicates whether a dump is needed after script
     * execution.
     * @return true if the script was successfully added and started, false
     * otherwise.
     */
    bool addToActive(const std::string& script, uint64_t timeout,
                     bool dumpNeeded)
    {
        auto scriptId = makeScriptId(script);

        LOG_DEBUG("Starting script: {}", scriptId.value());
        if (!scriptId)
        {
            LOG_ERROR("Failed to create script hash");
            return false;
        }
        try
        {
            auto iface = std::make_unique<ScriptIface>(
                io_context, scriptRunner,
                ScriptIface::Data{script, *scriptId, timeout, dumpNeeded},
                dbusServer);
            return runScript(std::move(iface));
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Failed to create script interface: {}", e.what());
            return false;
        }
    }
    /**
     * @brief Runs a script using the provided ScriptIface instance.
     *
     * This function attempts to start the script specified in the given
     * ScriptIface object by invoking the scriptRunner. If the script starts
     * successfully, it initiates a timeout for the script and stores the
     * ScriptIface instance for further management. If the script fails to
     * start, an error is logged and the function returns false.
     *
     * @param iface A unique pointer to a ScriptIface object containing script
     * data.
     * @return true if the script was started successfully, false otherwise.
     */
    bool runScript(std::unique_ptr<ScriptIface> iface)
    {
        bool success = scriptRunner.run_script(
            iface->data.id, iface->data.script, iface->data.dumpNeeded,
            std::bind_front(&AcfShellIface::removeFromActive, this));
        if (!success)
        {
            LOG_ERROR("Failed to start script");
            return false;
        }
        iface->startTimeout();
        scriptIfaces.push_back(std::move(iface));
        return success;
    }
    /**
     * @brief Executes a script asynchronously via a D-Bus method call.
     *
     * This function initiates the execution of the specified script by making
     * an asynchronous D-Bus call to the "start" method of the given interface.
     * It uses a default timeout of 30 seconds and passes the script, timeout,
     * and a boolean flag as arguments to the method call.
     *
     * @param script The script to be executed.
     * @return net::awaitable<void> An awaitable representing the asynchronous
     * operation.
     *
     * @note Logs an error message if the D-Bus method call fails.
     */
    net::awaitable<void> execute(const std::string& script)
    {
        uint64_t timeout = 30;
        auto [ec, value] = co_await awaitable_dbus_method_call<bool>(
            *conn, busName.data(), objPath.data(), interface.data(), "start",
            script, timeout, true);

        if (ec)
        {
            LOG_ERROR("Error starting script: {}", ec.message());
        }
    }
    ScriptIface* getScriptIface(std::string scriptId)
    {
        auto it = std::find_if(scriptIfaces.begin(), scriptIfaces.end(),
                               [scriptId](const auto& iface) {
                                   return iface->data.id == scriptId;
                               });
        if (it != scriptIfaces.end())
        {
            return it->get();
        }
        return nullptr;
    }
    /**
     * @brief Removes a script interface from the active list by its script ID.
     *
     * Searches for a script interface in the scriptIfaces container whose
     * data.id matches the provided scriptId. If found, removes it from the
     * container.
     *
     * @param unused Unused boost::system::error_code parameter.
     * @param scriptId The ID of the script interface to remove.
     * @return true if the script interface was found and removed; false
     * otherwise.
     */
    bool removeFromActive(boost::system::error_code /*unused*/,
                          std::string scriptId)
    {
        auto it = std::find_if(scriptIfaces.begin(), scriptIfaces.end(),
                               [scriptId](const auto& iface) {
                                   return iface->data.id == scriptId;
                               });
        if (it != scriptIfaces.end())
        {
            scriptIfaces.erase(it);
            return true;
        }
        return false;
    }
};
} // namespace scrrunner
