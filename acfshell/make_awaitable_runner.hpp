/**
 * @file make_awaitable_runner.hpp
 * @brief Utilities for creating awaitable handlers and managing coroutine
 * results with Boost.Asio.
 *
 * This header provides helper types and functions to simplify the creation of
 * awaitable handlers using Boost.Asio's coroutine and awaitable features. It
 * allows for flexible handling of asynchronous operations, including automatic
 * error code management and tuple-based result passing.
 *
 * Namespace: scrrunner
 *
 * Functions and Types:
 * - mut_awaitable(): Returns a mutable reference to a static use_awaitable_t
 * instance for use in async operations.
 * - PrependEC<Types...>: Tuple type that prepends boost::system::error_code to
 * a list of types.
 * - ReturnTuple<RetTypes...>: Resolves to a tuple with error_code prepended if
 * not already present.
 * - AwaitableResult<Types...>: Alias for net::awaitable with the appropriate
 * ReturnTuple.
 * - PromiseType<Handler, Types...>: Helper struct to wrap a handler and provide
 * a setValues method for result passing.
 * - make_awaitable_handler<Ret..., HandlerFunc>: Factory function to create an
 * awaitable handler from a given function, automatically handling error code
 * and result tuple construction.
 *
 * Usage:
 * These utilities are intended for use in asynchronous code where results (and
 * optionally error codes) need to be passed through coroutines in a type-safe
 * and convenient manner.
 */
#pragma once
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
namespace net = boost::asio;

namespace scrrunner
{
inline net::use_awaitable_t<>& mut_awaitable()
{
    static net::use_awaitable_t<> myawitable;
    return myawitable;
}

template <typename... Types>
using PrependEC = std::tuple<boost::system::error_code, Types...>;
template <typename... RetTypes>
using ReturnTuple = std::conditional_t<
    std::is_same_v<boost::system::error_code,
                   std::tuple_element_t<0, std::tuple<RetTypes...>>>,
    std::tuple<RetTypes...>, PrependEC<RetTypes...>>;

template <typename... Types>
using AwaitableResult = net::awaitable<ReturnTuple<Types...>>;
template <typename Handler, typename... Types>
struct PromiseType
{
    Handler promise;
    void setValues(Types... values)
    {
        promise(ReturnTuple<Types...>{std::move(values)...});
    }
};

template <typename... Ret, typename HanlderFunc>
auto make_awaitable_handler(HanlderFunc&& h)
{
    return [h = std::move(h)]() -> AwaitableResult<Ret...> {
        co_return co_await net::async_initiate<
            net::use_awaitable_t<>, ReturnTuple<Ret...>(ReturnTuple<Ret...>)>(
            [h = std::move(h)](auto handler) {
                if constexpr (std::is_same_v<
                                  boost::system::error_code,
                                  std::tuple_element_t<0, std::tuple<Ret...>>>)
                {
                    PromiseType<decltype(handler), Ret...> promise{
                        std::move(handler)};
                    h(std::move(promise));
                }
                else
                {
                    PromiseType<decltype(handler), boost::system::error_code,
                                Ret...>
                        promise{std::move(handler)};
                    h(std::move(promise));
                }
            },
            mut_awaitable());
    };
}
} // namespace scrrunner
