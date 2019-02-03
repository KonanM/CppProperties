#pragma once

#include "PropertySystem_forward.h"
#include "PropertyContainer.h"
#include "PropertyBase.h"

#include <any>
#include <typeindex>

namespace ps
{
	template <typename T>
	class Property : public PropertyBase
	{
	public:
		using value_type = T;

		Property() = default;
		Property(const Property<T>& that) = default;
		Property(Property<T>&& that) = default;

		explicit Property(const T& val)
			: m_value(val) {}

		// assigns a new value to this Property
		Property<T>& operator=(const T& rhs)
		{
			set(rhs);
			return *this;
		}

		Property(PropertyContainer* container)
			: PropertyBase(container) {}

		// connect to a signal which is fired when the internal value
		// has been changed. The new value is passed as parameter.
		template<typename U>
		std::type_index operator+=(U&& func)
		{
			return m_signal.connect<T>(std::forward<U>(func));
		}
		//disconnect a signal - the type_index can be used when using lambdas
		void operator-=(std::type_index index)
		{
			m_signal.disconnect(index);
		}
		//disconnect a signal by the same func that was used to connect it
		template<typename FuncT>
		void operator-=(FuncT&& func)
		{
			m_signal.disconnect(std::type_index(typeid(FuncT)));
		}

		// sets the Property to a new value.
		// on_change() will be emitted.
		template<typename U>
		void set(U&& value)
		{
			if (value != m_value) {
				m_value = std::forward<U>(value);
				propertyChanged();
			}
		}

		// returns the internal value
		virtual const T& get() const noexcept
		{ 
			return m_value;
		}
		//the get as any method is needed for the type erased signal implmentation
		virtual std::any getAsAny() const noexcept final override
		{
			return std::make_any<T>(get());
		}

		// if there are any Properties connected to this Property,
		// they won't be notified of any further changes
		void disconnectSignals() 
		{
			m_callerSignal->disconnect();
		}

		void propertyChanged() override
		{
			if (m_parent)
				m_parent->setDirty(*this);
			if(!m_signal.empty())
				m_signal.emit(std::make_any<T>(m_value));
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

	private:
		Signal m_signal;
		T m_value;
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