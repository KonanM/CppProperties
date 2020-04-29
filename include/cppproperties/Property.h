#pragma once

#include "PropertySystem_forward.h"
#include "Signal.h"

namespace ps
{
	class PropertyBase
	{
	public:
		PropertyBase() = default;
		virtual ~PropertyBase() = default;
	};

	template <typename T>
	class Property : public PropertyBase
	{
	private:
		Signal<const T&> m_signal;
		T m_value{};
	public:
		using value_type = T;

		Property() = default;
		template<typename ValueT>
		explicit Property(ValueT&& val)
			: m_value(std::forward<ValueT>(val)) {}

		// assigns a new value to this Property
		Property<T>& operator=(const T& rhs)
		{
			set(rhs);
			return *this;
		}

		// connect to a signal which is fired when the internal value
		// has been changed. The new value is passed as parameter.
		template<typename U>
		size_t operator+=(U&& func)
		{
			return m_signal.connect(std::forward<U>(func));
		}
		template<typename U, typename = std::enable_if_t<std::is_invocable_v<U,T>>>
		size_t connect(U&& func)
		{
			return m_signal.connect(std::forward<U>(func));
		}
		//disconnect a signal - the type_index can be used when using lambdas
		void operator-=(size_t index)
		{
			m_signal.disconnect(index);
		}

		void disconnect(size_t index)
		{
			m_signal.disconnect(index);
		}

		// sets the Property to a new value.
		// on_change() will be emitted.
		template<typename U>
		void set(U&& value)
		{
			if (value != m_value) {
				m_value = std::forward<U>(value);
				m_signal.emit(m_value);
			}
		}

		// returns the internal value
		virtual const T& get() const noexcept
		{ 
			return m_value;
		}

		// if there are any Properties connected to this Property,
		// they won't be notified of any further changes
		void disconnectSignals() 
		{
			m_signal->disconnect();
		}

		// returns the value of this Property
		[[nodiscard]] operator T() const noexcept
		{
			return get();
		}

		[[nodiscard]] const T& operator()() const noexcept
		{
			return get();
		}
	};

	//comparision operator implementation
	template<typename T, typename U>
	bool operator==(const Property<T>& property, const U& value)
	{
		return property.get() == value;
	}
	template<typename T, typename U>
	bool operator!=(const Property<T>& property, const U& value)
	{
		return !(property == value);
	}
	template<typename T, typename U>
	bool operator<(const Property<T>& property, const U& value)
	{
		return property.get() < value;
	}
	template<typename T, typename U>
	bool operator>(const Property<T>& property, const U& value)
	{
		return property.get() < value;
	}
	template<typename T, typename U>
	bool operator<=(const Property<T>& property, const U& value)
	{
		return property.get() <= value;
	}
	template<typename T, typename U>
	bool operator>=(const Property<T>& property, const U& value)
	{
		return property.get() >= value;
	}
}
