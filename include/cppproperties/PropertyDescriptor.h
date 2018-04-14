#pragma once

// The MIT License (MIT)
//
// Copyright (c) 2018, KMahdi
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include <string>
#include <utility>

namespace pd
{

	//###########################################################################
	//#
	//#                        PropertyContainer                               
	//#
	//############################################################################

	class PropertyDescriptorBase
	{
	};
	//right now the primary lookup of a PD is via it's const ref
	//other implementation use a string type, but I think having to use a descriptor
	//directly makes everything a bit more comfortable and the lookup is faster
	template<typename T>
	class PropertyDescriptor : public PropertyDescriptorBase
	{
	public:
		using type = T;
		PropertyDescriptor(T&& defaultValue, std::string identifier = "")
			: PropertyDescriptorBase()
			, m_defaultValue(std::forward<T>(defaultValue))
			, m_identifier(std::move(identifier))
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
		const std::string m_identifier;
	};

}