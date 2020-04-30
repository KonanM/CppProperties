#pragma once

#include "PropertySystem_forward.h"
#include "PropertyContainer.h"

#include <unordered_map>
#include <type_traits>

namespace ps
{
	//###########################################################################
	//#
	//#              make_proxy_property and ConvertingProxyProperty                         
	//#
	//############################################################################

	//now here starts the fun / interesting part about ProxyProperties
	//you can use them so combine several properties into a new one, by providing a lambda
	//this can also act as a simple converter, e.g. perfoming a cast on an input
	//super simple example
	//auto intStringLambda = [](int i, std::string s)
	//{ return s + ": " + std::to_string(i);
	//};
	//container.setProperty(CombinedStrPD, pd::make_proxy_property(intStringLambda, IntPD, StringPD));

	template<typename FuncT, typename ... PropertDescriptors>
	auto make_proxy_property(FuncT&& func, const PropertDescriptors& ... pds)
	{
		using ResultT = typename std::invoke_result_t<FuncT, typename PropertDescriptors::value_type...>;
		return std::make_unique<ConvertingProxyProperty<ResultT, FuncT, PropertDescriptors...>>(std::forward<FuncT>(func), pds..., std::index_sequence_for<PropertDescriptors...>{});
	}

	template<typename T, typename FuncT, typename ... PropertDescriptors>
	class ConvertingProxyProperty : public ProxyProperty<T>
	{
	protected:
		FuncT m_func;
		std::tuple<typename PropertDescriptors::value_type...> m_values;
		std::tuple<const PropertDescriptors*...> m_pds;
	public:
		template<std::size_t... Is>
		ConvertingProxyProperty(FuncT&& funcT, const PropertDescriptors& ... pds, std::index_sequence<Is...>)
			: ProxyProperty<T>()
			, m_func(std::forward<FuncT>(funcT))
			, m_values(PropertyContainer::getProperty(pds)...)
			, m_pds(std::addressof(pds)...)
		{
			((void)PropertyContainer::connect(pds, [&](const typename PropertDescriptors::value_type& value) {
				std::get<Is>(m_values) = value;
				anyPropertyChanged();
			}), ...);

			anyPropertyChanged();
		}
		template<std::size_t... Is>
		ConvertingProxyProperty(FuncT&& funcT, const std::tuple<const PropertDescriptors *...>& pds, std::index_sequence<Is...> is)
			: ConvertingProxyProperty(funcT, *std::get<Is>(pds)..., is) {};
		//copying and moving is currently the same for ConvertingProxyProperty
		ConvertingProxyProperty(const ConvertingProxyProperty& that) : ConvertingProxyProperty(that.m_func, that.m_pds, std::index_sequence_for<PropertDescriptors...>{}) {};
		ConvertingProxyProperty(ConvertingProxyProperty&& that) : ConvertingProxyProperty(that.m_func, that.m_pds, std::index_sequence_for<PropertDescriptors...>{}) {};

	protected:
		void anyPropertyChanged()
		{
			Property<T>::set(std::apply(m_func, m_values));
		}
	};
}