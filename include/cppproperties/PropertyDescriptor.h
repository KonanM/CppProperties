#pragma once
#include <string>
#include <utility>
#include <type_traits>

namespace ps
{

	//###########################################################################
	//#
	//#                        PropertyContainer                               
	//#
	//############################################################################

	class PropertyDescriptorBase
	{
	public:
		template<typename T>
		PropertyDescriptorBase(T&& name = std::string())
			: m_name(std::forward<T>(name))
		{
		}

		PropertyDescriptorBase(const PropertyDescriptorBase&) = delete;
		PropertyDescriptorBase operator = (const PropertyDescriptorBase&) = delete;
		PropertyDescriptorBase(PropertyDescriptorBase&&) = delete;
		PropertyDescriptorBase operator=(PropertyDescriptorBase&&) = delete;

		const std::string& getName() const
		{
			return m_name;
		}

	protected:
		const std::string m_name;
	};
	//right now the primary lookup of a PD is via it's const ref
	//other implementation use a string type, but I think having to use a descriptor
	//directly makes everything a bit more comfortable and the lookup is faster
	template<typename T>
	class PropertyDescriptor : public PropertyDescriptorBase
	{
	public:
		using value_type = T;
		PropertyDescriptor(T&& defaultValue, std::string identifier = "")
			: PropertyDescriptorBase(std::move(identifier))
			, m_defaultValue(std::forward<T>(defaultValue))
		{
			if constexpr(std::is_reference_v<T>)
				static_assert("Please use std::reference wrapper for using reference semantics.\n"
							  "The underlying std::any storage doesn't support references");
		}

		PropertyDescriptor(const PropertyDescriptor&) = delete;
		PropertyDescriptor operator = (const PropertyDescriptor&) = delete;
		PropertyDescriptor(PropertyDescriptor&&) = delete;
		PropertyDescriptor operator=(PropertyDescriptor&&) = delete;

		const T& getDefaultValue() const
		{
			return m_defaultValue;
		}
	private:
		const T m_defaultValue;
	};
}