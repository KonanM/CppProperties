#pragma once
#include <utility>
#include <type_traits>
#include "PropertyDescriptorBase.h"
#include "Property.h"
namespace ps
{
	//###########################################################################
	//#
	//#                        PropertyDescriptor                               
	//#
	//############################################################################

	template<typename T>
	class PropertyDescriptor : public PropertyDescriptorBase
	{
	public:
		using value_type = T;
		PropertyDescriptor(T&& defaultValue, std::string identifier = "")
			: PropertyDescriptorBase(std::move(identifier))
			, m_defaultValue(Property<T>(std::forward<T>(defaultValue)))
		{
			if constexpr (std::is_reference_v<T>)
				static_assert("Please use std::reference wrapper for using reference semantics.\n"
					"The underlying std::any storage doesn't support references");
		}

		PropertyDescriptor(const PropertyDescriptor&) = delete;
		PropertyDescriptor operator = (const PropertyDescriptor&) = delete;
		PropertyDescriptor(PropertyDescriptor&&) = delete;
		PropertyDescriptor operator=(PropertyDescriptor&&) = delete;

		const Property<T>& getDefaultValue() const
		{
			return m_defaultValue;
		}
	private:
		const Property<T> m_defaultValue;
	};
}