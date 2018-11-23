#pragma once

#include "PropertySystem_forward.h"
#include "PropertyContainer.h"
#include <any>

namespace ps
{
	class PropertyBase
	{
	public:
		PropertyBase(PropertyContainer* parent)
			: m_parent(parent)
		{
		}
		PropertyBase() = default;
		template<template<typename ...> class MapT>
		friend class PropertyContainerBase;

		
		virtual ~PropertyBase() = default;
		virtual std::any getAsAny() const noexcept = 0;

		bool isProxyProperty() const noexcept
		{
			return m_isProxyProperty;
		}

		void setParentContainer(PropertyContainer* parent)
		{
			m_parent = parent;
		}

		std::vector<Signal*>& connectedSignals()
		{
			return m_connectedSignals;
		}

		virtual void propertyChanged() = 0;

	protected:
		PropertyContainer* m_parent = nullptr;
		std::vector<Signal*> m_connectedSignals;
		bool m_propertyChanged = false;
		bool m_isProxyProperty = false;
	};
}
