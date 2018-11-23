#pragma once
namespace ps
{
	//forward declarations
	template<template<typename ...> class MapT = std::unordered_map>
	class PropertyContainerBase;
	using PropertyContainer = PropertyContainerBase<>;

	template<typename T>
	class ProxyProperty;

	class PropertyBase;
	template<typename T>
	class Property;
}
