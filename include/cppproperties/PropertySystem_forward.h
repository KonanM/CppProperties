#pragma once
#include <unordered_map>
namespace ps
{
	//forward declarations
	template<typename T, typename FuncT, typename ... PropertDescriptors>
	class ConvertingProxyProperty;

	template<template<typename ...> class MapT = std::unordered_map>
	class PropertyContainerBase;
	using PropertyContainer = PropertyContainerBase<>;

	class PropertyDescriptorBase;
	template<typename T>
	class PropertyDescriptor;

	template<typename T>
	class ProxyProperty;

	class PropertyBase;
	template<typename T>
	class Property;
	template<typename... Args>
	class Signal;
}
