//
// Created by ianpo on 16/10/2025.
//

#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <utility>

namespace MVT {
	template<typename T, typename Err>
	class Expected {
	public:
		inline static Expected expected(const T &value) {
			return Expected<T, Err>(value);
		}
		// inline static Expected expected(T &&value) {
		// 	return Expected<T, Err>{std::move(value)};
		// }

		inline static Expected unexpected(const Err &error) {
			return Expected<T, Err>(error);
		}
		// inline static Expected unexpected(Err &&error) {
		// 	return Expected<T, Err>(std::move(error));
		// }
	public:
		Expected(const T &value) : data(0), valueType(V_Expected) {
			new(data.data()) T(value);
		}

		Expected(T &&value) : data(0), valueType(V_Expected) {
			new(data.data()) T(std::move(value));
		}

		Expected(const Err &err) : data(0), valueType(V_Unexpected) {
			new(data.data()) Err(err);
		}

		Expected(Err &&err) : data(0), valueType(V_Unexpected) {
			new(data.data()) Err(std::move(err));
		}

		Expected(const Expected &cpy) : valueType(cpy.valueType) {
			switch (valueType) {
				case V_Expected:
				{
					new (ptr()) T(*cpy.val_ptr());
				}
					break;
				case V_Unexpected:
				{
					new (ptr()) Err(*cpy.err_ptr());
				}
					break;
				default:
					break;
			}
		}

		Expected& operator=(const Expected &cpy){
			if (valueType == cpy.valueType) {
				switch (valueType) {
					case V_Expected:
					{
						val_ptr()->T(*cpy.val_ptr());
					}
						break;
					case V_Unexpected:
					{
						err_ptr()->Err(*cpy.err_ptr());
					}
						break;
					default:
						break;
				}
			} else {
				switch (valueType) {
					case V_Expected:
					{
						val_ptr()->~T();
					}
						break;
					case V_Unexpected:
					{
						err_ptr()->~Err();
					}
						break;
					default:
						break;
				}

				valueType = cpy.valueType;

				switch (valueType) {
					case V_Expected:
					{
						new (ptr()) T(*valueType.val_ptr());
					}
						break;
					case V_Unexpected:
					{
						new (ptr()) Err(*valueType.err_ptr());
					}
						break;
					default:
						break;
				}
			}

			return *this;
		}

		Expected(Expected&& o) noexcept : data(0), valueType(V_Deleted) {
			swap(o);
		}

		Expected& operator=(Expected&& o) noexcept {
			swap(o);
			return *this;
		}

		~Expected() {
			switch (valueType) {
				case V_Expected:
					reinterpret_cast<T *>(data.data())->~T();
					break;
				case V_Unexpected:
					reinterpret_cast<Err *>(data.data())->~Err();
					break;
				default:
					break;
			}
			valueType = V_Deleted;
		}

		void swap(Expected &other) noexcept {
			if (valueType == other.valueType) {
				switch (valueType) {
					case V_Unexpected: {
						std::swap(error(), other.error());
					}
					break;
					case V_Expected: {
						std::swap(value(), other.value());
					}
					break;
					default:
						break;
				}
			}
			else {
				if (valueType == V_Unexpected && other.valueType == V_Expected) {
					T tmp{std::move(*other.val_ptr())};
					other.val_ptr()->~T();

					new (other.ptr()) Err(std::move(*err_ptr()));
					err_ptr()->~Err();

					new (ptr()) T(std::move(tmp));
				}
				else if (valueType == V_Expected && other.valueType == V_Unexpected) {
					Err tmp{std::move(*other.err_ptr())};
					other.err_ptr()->~Err();

					new (other.ptr()) T(std::move(*val_ptr()));
					val_ptr()->~T();

					new (ptr()) Err(std::move(tmp));
				}
				else if (valueType == V_Deleted && other.valueType == V_Expected) {
					new (ptr()) T(std::move(*other.val_ptr()));
					other.val_ptr()->~T();
				}
				else if (valueType == V_Deleted && other.valueType == V_Unexpected) {
					new (ptr()) Err(std::move(*other.err_ptr()));
					other.err_ptr()->~Err();
				}
				else if (valueType == V_Expected && other.valueType == V_Deleted) {
					new (other.ptr()) T(std::move(*val_ptr()));
					val_ptr()->~T();
				}
				else if (valueType == V_Unexpected && other.valueType == V_Deleted) {
					new (other.ptr()) Err(std::move(*err_ptr()));
					err_ptr()->~Err();
				}

				std::swap(valueType, other.valueType);
			}
		}

	private:

	void* ptr() {
		return data.data();
	}
	T* val_ptr() {
		return reinterpret_cast<T*>(data.data());
	}
	Err* err_ptr() {
		return reinterpret_cast<Err*>(data.data());
	}

	public:
		T &value() {
			assert(valueType == V_Expected);
			return std::move(*reinterpret_cast<T *>(data.data()));
		}

		const T &value() const {
			assert(valueType == V_Expected);
			return *reinterpret_cast<T *>(data.data());
		}

		T *value_ptr() {
			assert(valueType == V_Expected);
			return reinterpret_cast<T *>(data.data());
		}

		const T *value_ptr() const {
			assert(valueType == V_Expected);
			return reinterpret_cast<T *>(data.data());
		}

		T value_or(T &&default_value) {
			return valueType == V_Expected ? value() : std::move(default_value);
		}

		const T &value_or(const T &default_value) const {
			return valueType == V_Expected ? value() : default_value;
		}

		Err & error() {
			assert(valueType == V_Unexpected);
			return *reinterpret_cast<Err *>(data.data());
		}

		const Err &error() const {
			assert(valueType == V_Unexpected);
			return *reinterpret_cast<Err *>(data.data());
		}

		Err *error_ptr() {
			assert(valueType == V_Unexpected);
			return reinterpret_cast<Err *>(data.data());
		}

		const Err *error_ptr() const {
			assert(valueType == V_Unexpected);
			return reinterpret_cast<Err *>(data.data());
		}

		Err error_or(T &&default_error) {
			return valueType == V_Expected ? error() : std::move(default_error);
		}

		const Err &error_or(const T &default_error) const {
			return valueType == V_Expected ? error() : default_error;
		}

		template<typename F>
		auto &and_then(F &&f) {
			if (valueType == V_Expected) {
				f(value());
			}
			return *this;
		}

		template<typename F>
		auto &or_else(F &&f) {
			if (valueType == V_Unexpected) {
				f(error());
			}
			return *this;
		}

	public:
		T *operator ->() {
			return value_ptr();
		}

		const T *operator ->() const {
			return value_ptr();
		}

		T &operator *() {
			return value();
		}

		const T &operator *() const {
			return value();
		}

	private:
		enum ValueType : int8_t {
			V_Deleted = -1,
			V_Expected = 0,
			V_Unexpected = 1,
		};

		alignas(std::max(alignof(T), alignof(Err)))
		std::array<uint8_t, std::max(sizeof(T), sizeof(Err))> data;
		ValueType valueType;
	};
}
