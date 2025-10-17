//
// Created by ianpo on 16/10/2025.
//

#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <utility>

namespace MVT
{
    template <typename T, typename Err>
    class Expected
    {
    public:
        inline static Expected expected(const T& value)
        {
            return Expected<T, Err>(value);
        }

        template <typename... Args>
        inline static Expected expected(T&& value)
        {
            return Expected<T, Err>{std::move(value)};
        }

        inline static Expected unexpected(const Err& error)
        {
            return Expected<T, Err>(error);
        }

        inline static Expected unexpected(Err&& error)
        {
            return Expected<T, Err>(std::move(error));
        }

    public:
        Expected(const T& value) : data(value)
        {
        }

        Expected(T&& value) : data(std::move(value))
        {
        }

        Expected(const Err& err) : data(err)
        {
            new(data.data()) Err(err);
        }

        Expected(Err&& err) : data(std::move(err))
        {
        }

        Expected(const Expected& cpy) : data(cpy.data)
        {
        }

        Expected& operator=(const Expected& cpy)
        {
            data = cpy.data;

            return *this;
        }

        Expected(Expected&& o) noexcept : data(std::move(o.data))
        {
        }

        Expected& operator=(Expected&& o) noexcept
        {
            swap(o);
            return *this;
        }

        ~Expected() = default;

        void swap(Expected& other) noexcept
        {
            std::swap(data, other.data);
        }

    private:
        void* ptr()
        {
            return data.data();
        }

        T* val_ptr()
        {
            return reinterpret_cast<T*>(data.data());
        }

        Err* err_ptr()
        {
            return reinterpret_cast<Err*>(data.data());
        }

    public:
        T& value()
        {
            assert(std::holds_alternative<T>(data));
            return std::get<1>(data);
        }

        const T& value() const
        {
            assert(std::holds_alternative<T>(data));
            return std::get<1>(data);
        }

        T* value_ptr()
        {
            assert(std::holds_alternative<T>(data));
            return &std::get<1>(data);
        }

        const T* value_ptr() const
        {
            assert(std::holds_alternative<T>(data));
            return &std::get<1>(data);
        }

        T value_or(T&& default_value)
        {
            if (std::holds_alternative<T>(data))
            {
                return std::get<1>(data);
            }
            else
            {
                return default_value;
            }
        }

        const T& value_or(const T& default_value) const
        {
            if (std::holds_alternative<T>(data))
            {
                return std::get<1>(data);
            }
            else
            {
                return default_value;
            }
        }

        Err& error()
        {
            assert(std::holds_alternative<Err>(data));
            return std::get<2>(data);
        }

        const Err& error() const
        {
            assert(std::holds_alternative<Err>(data));
            return std::get<2>(data);
        }

        Err* error_ptr()
        {
            assert(std::holds_alternative<Err>(data));
            return &std::get<2>(data);
        }

        const Err* error_ptr() const
        {
            assert(std::holds_alternative<Err>(data));
            return &std::get<2>(data);
        }

        Err error_or(T&& default_error)
        {
            if (std::holds_alternative<Err>(data))
            {
                return std::get<2>(data);
            }
            else
            {
                return default_error;
            }
        }

        const Err& error_or(const T& default_error) const
        {
            if (std::holds_alternative<Err>(data))
            {
                return std::get<2>(data);
            }
            else
            {
                return default_error;
            }
        }

        [[nodiscard]]
        bool has_value() const
        {
            return std::holds_alternative<T>(data);
        }

        [[nodiscard]]
        bool has_error() const
        {
            return std::holds_alternative<Err>(data);
        }

        template <typename F>
        auto& and_then(F&& f)
        {
            if (std::holds_alternative<T>(data))
            {
                f(std::get<1>(data));
            }
            return *this;
        }

        template <typename F>
        auto& or_else(F&& f)
        {
            if (std::holds_alternative<Err>(data))
            {
                f(std::get<2>(data));
            }
            return *this;
        }

    public:
        T* operator ->()
        {
            return value_ptr();
        }

        const T* operator ->() const
        {
            return value_ptr();
        }

        T& operator *()
        {
            return value();
        }

        const T& operator *() const
        {
            return value();
        }

    private:
        std::variant<std::monostate, T, Err> data;
    };
}
