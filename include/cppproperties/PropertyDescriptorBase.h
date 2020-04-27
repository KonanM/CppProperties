#pragma once
#include <string>

namespace ps
{

	//###########################################################################
	//#
	//#                        PropertyDescriptorBase                               
	//#
	//############################################################################

	//right now the primary lookup of a PD is via it's const ref
	//other implementation use a string type, but I think having to use a descriptor
	//directly makes everything a bit more comfortable and the lookup is faster

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
}
