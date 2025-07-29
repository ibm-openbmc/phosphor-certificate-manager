#pragma once
#include "script_runner.hpp"
#include "sdbus_calls_runner.hpp"

#include <format>
namespace scrrunner
{
/**
 * @brief Represents a D-Bus interface for managing script execution.
 *
 * The struct encapsulates the running instance of a script, which can be
 * cancelled and monitored through this interface. The instance of this
 * interface will be removed automatically once the script execution ends
 * normally, is cancelled, or times out.
 *
 * Usage:
 * - Instantiated with references to the IO context, a ScriptRunner, script
 * data, and an object server.
 * - Registers a "cancel" method on the D-Bus interface to allow external
 * cancellation.
 * - Manages a timeout for script execution, automatically cancelling the script
 * if the timeout elapses.
 *
 * Members:
 * - Data: Holds script-specific information such as the script content, ID,
 * timeout, and whether a dump is needed.
 * - scriptPath: Format string for the D-Bus object path.
 * - scriptInterface: Name of the D-Bus interface.
 * - busName: Name of the D-Bus bus.
 * - io_context: Reference to the Boost.Asio IO context.
 * - scriptRunner: Reference to the ScriptRunner responsible for script
 * execution.
 * - data: Script-specific data.
 * - objServer: Reference to the sdbusplus object server.
 * - dbusIface: Shared pointer to the D-Bus interface object.
 * - timer: Shared pointer to the steady timer used for timeouts.
 *
 * Methods:
 * - ScriptIface(...): Constructor. Sets up the D-Bus interface and registers
 * methods.
 * - ~ScriptIface(): Destructor. Cleans up the timer and D-Bus interface.
 * - cancel(): Cancels the running script via the ScriptRunner.
 * - startTimeout(): Starts the timeout timer for the script execution.
 */

struct ScriptIface
{
    struct Data
    {
        std::string script;
        std::string id;
        uint64_t timeout;
        bool dumpNeeded;
    };
    static constexpr auto scriptPath = "/xyz/openbmc_project/acfshell/{}";
    static constexpr auto scriptInterface = "xyz.openbmc_project.TacfScript";
    static constexpr std::string_view busName = "xyz.openbmc_project.acfshell";
    ScriptIface(net::io_context& ioc, ScriptRunner& scriptRunner,
                const Data& data, sdbusplus::asio::object_server& objServer) :
        io_context(ioc), scriptRunner(scriptRunner), data(data),
        objServer(objServer),
        timer(std::make_shared<boost::asio::steady_timer>(io_context))
    {
        std::string path = std::format(scriptPath, data.id);
        // Create the D-Bus object
        dbusIface = objServer.add_interface(path.data(), scriptInterface);

        // Register the cancel method
        dbusIface->register_method("cancel", [this]() { return cancel(); });
        dbusIface->initialize();
    }
    ~ScriptIface()
    {
        timer->cancel();
        objServer.remove_interface(dbusIface);
    }
    bool cancel()
    {
        bool success = scriptRunner.cancel_script(data.id);
        if (!success)
        {
            LOG_ERROR("Failed to cancel script");
            return false;
        }
        return success;
    }
    void startTimeout()
    {
        if (data.timeout == 0)
        {
            return;
        }
        timer->expires_after(std::chrono::seconds(data.timeout));
        timer->async_wait(
            [this, timer = timer](const boost::system::error_code& ec) {
                if (ec)
                {
                    return;
                }
                LOG_DEBUG("Script {} timed out {}", data.id, data.timeout);
                cancel();
            });
    }
    net::io_context& io_context;
    ScriptRunner& scriptRunner;
    Data data;
    sdbusplus::asio::object_server& objServer;
    std::shared_ptr<sdbusplus::asio::dbus_interface> dbusIface;
    std::shared_ptr<boost::asio::steady_timer> timer;
};
} // namespace scrrunner
