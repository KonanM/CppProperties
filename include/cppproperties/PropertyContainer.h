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

#include "PropertyDescriptor.h"

#include <any>
#include <type_traits>
#include <utility>
#include <tuple>
#include <stddef.h>
#include <memory>
#include <vector>
#include <functional>
#include <map>
#include <unordered_map>

//TODO: connect signals arguments correctly

namespace pd
{
	//forward declarations
	template<typename T>
	class ProxyProperty;
	class ProxyPropertyBase;
	template <typename... Args>
	class Signal;
	//###########################################################################
	//#
	//#                        PropertyContainer                               
	//#
	//############################################################################

	template<typename>
	struct PMF_traits {
		using member_type = void;
		using class_type = void;
	};

	template<class T, class U>
	struct PMF_traits<U T::*> {
		using member_type = typename U;
		using class_type = typename T;
	};
	
	template<template<typename ...> class SignalT = Signal, template<typename ...> class MapT = std::unordered_map>
	class PropertyContainer
	{
	public:
		PropertyContainer() = default;

		//we only need the virtual destructor, because I didn't want to intrduce a special case for
		//owned proxy properties, an option could be to try a variant for this
		//or simple use a second vector to store proxy properties
		virtual ~PropertyContainer() = default;

		//getProperty returns the value for the provided PD
		template<typename T>
		T getProperty(const PropertyDescriptor<T>& pd) const
		{
			auto containerIt = m_toContainer.find(&pd);
			//check if a property has never been set -> return the default value
			if (containerIt == end(m_toContainer))
				return pd.getDefaultValue();
			//first find out in which container the property is stored, then get the value from
			//that container
			auto& container = *containerIt->second;
			return container.getPropertyInternal(pd);
		}
		//this function should only be ever needed very rarely
		//you should not need to interact with a proxy property directly
		template<typename T>
		ProxyProperty<T>* getProxyProperty(const PropertyDescriptor<T>& pd) const
		{
			auto containerIt = m_toContainer.find(&pd);
			//property has never been set -> return nullptr
			if (containerIt == end(m_toContainer))
				return nullptr;
			auto& container = *containerIt->second;
			return container.getProxyPropertyInternal(pd);
		}
		//          *
		//		  /   \
		//		 set   *
		//		/   \
		//	  ...   ...
		//	   visHere    
		//only at levels below where we called setProperty, the value of the property will be visible
		template<typename T, typename U>
		void setProperty(const PropertyDescriptor<T>& pd, U && value)
		{
			//set this as new parent container for the given PD
			setParentContainerForProperty(pd, this);
			//for the case that the same property has been set at a lower (parent) level
			//we have to update the existing subjects before changning the property
			//otherwise the wrong subjects will be updated
			updateAllSignals(pd);

			auto parentContainer = m_parent ? m_parent->getOwningPropertyContainer(pd) : nullptr;
			if (parentContainer)
				parentContainer->updateAllSignals(pd);
			changePropertyInternal(pd, std::forward<U>(value));
		}

		//this is the removal counterpart of the setProperty interface
		//be aware that this only removes the property if it's set at the current level
		//if it's removed from the current level it might be that the property is still visible
		//because it was set at a parent level before
		template<typename T>
		void removeProperty(const PropertyDescriptor<T>& pd)
		{
			auto containerIT = m_toContainer.find(&pd);
			if (containerIT == end(m_toContainer))
				return;
			return containerIT->second->removePropertyInternal(pd);
		}

		//interface to check if a property has been set
		//if getting the default value, when no property is set is not intended use case
		//use this function to check for it first
		template<typename T>
		bool hasProperty(const PropertyDescriptor<T>& pd) const
		{
			return m_toContainer.find(&pd) != end(m_toContainer);
		}

		//this will change the current property at the level where it was set
		//if the property wasn't set nothing happens
		template<typename T, typename U>
		void changeProperty(const PropertyDescriptor<T>& pd, U&& value)
		{
			//find the correct container where we have to change the property
			if (auto containerIT = m_toContainer.find(&pd); containerIT != end(m_toContainer))
				return containerIT->second->changePropertyInternal(pd, std::forward<U>(value));
		}
		//we can connect a property to a function/lambda (or member function ptr)
		//whenever the property changes the function will get called
		//you can either connect to function without argument or one that is callable
		//by the type of the property
		template<typename T, typename FuncT>
		void connect(const PropertyDescriptor<T>& pd, FuncT&& func)
		{
			using PMF = PMF_traits<FuncT>;
			//case 1: function object callable with argument of type T
			if constexpr (std::is_invocable_v<FuncT, T>)
			{
				auto& typedSignal = m_toTypedSignal[&pd];
				if (!typedSignal.has_value())
					typedSignal.emplace<SignalT<T>>();
				std::any_cast<SignalT<T>&>(typedSignal).connect(std::forward<FuncT>(func));
			}
			//case 2: pointer of member function with argument of type T
			else if constexpr (std::is_invocable_v<typename PMF::member_type, T>)
			{
				auto& typedSignal = m_toTypedSignal[&pd];
				if (!typedSignal.has_value())
					typedSignal.emplace<SignalT<T>>();
				std::any_cast<SignalT<T>&>(typedSignal).connect(static_cast<typename PMF::class_type*>(this), std::forward<FuncT>(func));
			}
			//case 3: pointer of member function with no argument
			else if constexpr (std::is_function_v<typename PMF::member_type>)
			{
				m_toVoidSignal[&pd].connect(static_cast<typename PMF::class_type*>(this), std::forward<FuncT>(func));
			}
			//case 4: callable function object with no argument
			else
			{
				m_toVoidSignal[&pd].connect(std::forward<FuncT>(func));
			}
			
		}

		//for now we can only disconnect all the functions connect attached to a certain property
		template<typename T>
		void disconnect(const PropertyDescriptor<T>& pd)
		{
			m_toVoidSignal[&pd].disconnect();
			if (auto& typedSignal = m_toTypedSignal[&pd]; typedSignals.has_value())
				std::any_cast<SignalT<T>&>(typedSignal).disconnect();

		}

		template<typename T>
		void connectToMember(const PropertyDescriptor<T>& pd, T& memberVariable)
		{
			connect(pd, [&memberVariable](auto& newValue)
			{
				memberVariable = newValue;
			});
		}

		PropertyContainer* addChildContainer(std::unique_ptr<PropertyContainer> propertyContainer)
		{
			auto propertyContainerPtr = propertyContainer.get();
			propertyContainer->setParent(this);
			//here we propagate all the pd's that we own
			for (auto& propertyData : m_propertyData)
			{
				if (auto& pd = *propertyData.first; !propertyContainer->ownsPropertyDataInternal(pd))
					propertyContainer->setParentContainerForProperty(pd, this);
			}
			//here we propagate all the pd's that we don't own
			for (auto&[pd, container] : m_toContainer)
			{
				if (!propertyContainer->ownsPropertyDataInternal(*pd))
					propertyContainer->setParentContainerForProperty(*pd, container);
			}

			m_children.emplace_back(std::move(propertyContainer));
			return propertyContainerPtr;
		}

	protected:
		template<typename T>
		T getPropertyInternal(const PropertyDescriptor<T>& pd) const
		{
			if (auto valueIt = m_propertyData.find(&pd); valueIt != end(m_propertyData))
			{
				auto& propertyData = valueIt->second;
				if (propertyData.proxyProperty)
					return static_cast<ProxyProperty<T>*>(propertyData.proxyProperty)->get();
				return std::any_cast<T>(propertyData.value);
			}
			assert(false);
			return pd.getDefaultValue();
		}

		template<typename T>
		ProxyProperty<T>* getProxyPropertyInternal(const PropertyDescriptor<T>& pd) const
		{
			auto valueIt = m_propertyData.find(&pd);
			if (valueIt == end(m_propertyData))
				return nullptr;
			auto& propertyData = valueIt->second;
			if (!propertyData.proxyProperty)
				return nullptr;
			return static_cast<ProxyProperty<T>*>(propertyData.proxyProperty);
		}

		template<typename T, typename U>
		void changePropertyInternal(const PropertyDescriptor<T>& pd, U && value)
		{
			auto& propertyData = m_propertyData[&pd];
			//this is the normal case where we store a value
			if constexpr(std::is_convertible_v<std::decay_t<U>, T>)
			{
				if (!propertyData.value.has_value() || std::any_cast<T>(propertyData.value) != value)
				{
					propertyData.value = std::make_any<T>(std::forward<U>(value));
					propertyData.proxyProperty = nullptr;
					//invoke all observers
					for (auto& subject : propertyData.signals)
						subject->emit();
				}
			}
			//in this case we store a proxy property that can return a value of the given type
			else if constexpr(std::is_base_of_v<ProxyProperty<T>, U::element_type>)
			{
				propertyData.proxyProperty = value.get();
				addChildContainer(std::move(value));
				for (auto& subject : propertyData.signals)
					subject->emit();
			}
			else
				static_assert(false, "The type T of the property descriptor doesn't match the type of the passed value.");
		}

		template<typename T>
		void updateAllSignals(const PropertyDescriptor<T>& pd)
		{
			auto& propertyData = m_propertyData[&pd];
			propertyData.signals.clear();
			getAllSignals(pd, propertyData.signals);
		}

		template<typename T>
		void addSignals(const PropertyDescriptor<T>& pd, std::vector<SignalT<>*>& signals)
		{
			auto& propertyData = m_propertyData[&pd];
			std::copy(begin(signals), end(signals), std::back_inserter(propertyData.signals));
		}

		template<typename T>
		void getAllSignals(const PropertyDescriptor<T>& pd, std::vector<SignalT<>*>& signals)
		{
			//add own subject
			if (auto subjectIt = m_toVoidSignal.find(&pd); subjectIt != end(m_toVoidSignal))
				signals.push_back(&(subjectIt->second));

			//add the signals of all children unless they own property data themselves
			for (auto& children : m_children)
				if (!children->ownsPropertyDataInternal(pd))
					children->getAllSignals(pd, signals);
		}

		void setParentContainerForProperty(const PropertyDescriptorBase& pd, PropertyContainer* container)
		{
			if (container)
				m_toContainer[&pd] = container;
			else
				m_toContainer.erase(&pd);

			for (auto& children : m_children)
			{
				if (!children->ownsPropertyDataInternal(pd))
					children->setParentContainerForProperty(pd, container);
			}
		}

		template<typename T>
		void removePropertyInternal(const PropertyDescriptor<T>& pd)
		{
			auto& propertyData = m_propertyData[&pd];
			//copy all the subject ptrs
			auto oldSubjects = propertyData.signals;

			T oldValue = getProperty(pd);
			//we have to update all children and tell it which is the correct owning property container
			auto newContainer = m_parent ? m_parent->getOwningPropertyContainer(pd) : nullptr;
			setParentContainerForProperty(pd, newContainer);
			//we also have to add all the signals from this level to the owning container
			if (newContainer)
				newContainer->addSignals(pd, oldSubjects);
			//now we remove the property data
			m_propertyData.erase(&pd);
			//notify all old signals (if necessary)
			const bool valueChanged = (newContainer ? oldValue != newContainer->getProperty(pd) : oldValue != pd.getDefaultValue());
			if (valueChanged)
			{
				for (auto& subject : oldSubjects)
					subject->emit();
			}
		}

		template<typename T>
		void observePropertyInternal(const PropertyDescriptor<T>& pd, std::vector<std::function<void()>>* observers)
		{
			auto& propertyData = m_propertyData[&pd];
			propertyData.push_back(observers);
		}

		bool ownsPropertyDataInternal(const PropertyDescriptorBase& pd) const
		{
			return m_propertyData.find(&pd) != end(m_propertyData);
		}

		PropertyContainer* getOwningPropertyContainer(const PropertyDescriptorBase& pd)
		{
			//check if we own the property ourselves
			if (ownsPropertyDataInternal(pd))
				return this;
			//check if we know any container that owns the given PD
			auto containerIt = m_toContainer.find(&pd);
			return containerIt != end(m_toContainer) ? (containerIt->second) : nullptr;
		}

		void setParent(PropertyContainer* container)
		{
			m_parent = container;
		}

		using KeyT = const PropertyDescriptorBase*;
		//contains owned ProxyProperties as well as other owned children
		std::vector<std::unique_ptr<PropertyContainer>> m_children;
		PropertyContainer* m_parent = nullptr;

		MapT<KeyT, PropertyContainer*> m_toContainer;
		MapT<KeyT, SignalT<>> m_toVoidSignal;
		//not sure if using std::any is the correct choice here
		//I could also write a virtual base class for the SignalT
		//and use a unique_ptr of that, but that seems worse
		MapT<KeyT, std::any> m_toTypedSignal;
		struct PropertyData
		{
			std::any value;
			ProxyPropertyBase* proxyProperty = nullptr;
			std::vector<SignalT<>*> signals;
			std::vector<std::any*> typedSignal;
		};
		MapT<KeyT, PropertyData> m_propertyData;
	};

	//###########################################################################
	//#
	//#                        PropertyContainer CTAD                             
	//#
	//############################################################################

	template<template<typename ...> class SignalT, template<typename ...> class MapT>
	PropertyContainer() -> PropertyContainer<SignalT, MapT>;

	//###########################################################################
	//#
	//#                        ProxyProperty Definition                             
	//#
	//############################################################################

	class ProxyPropertyBase : public PropertyContainer<>
	{
	public:
		virtual ProxyPropertyBase* clone() = 0;
		virtual ~ProxyPropertyBase() = default;
	};
	//proxy property is simply a an abstration of a property, instead of using a value
	//you can use a class to calculate that value
	template<typename T>
	class ProxyProperty : public ProxyPropertyBase
	{
	public:
		using type = T;
		virtual T get() const = 0;
	};

	//###########################################################################
	//#
	//#              makeProxyProperty and ConvertingProxyProperty                         
	//#
	//############################################################################

	//now here starts the fun / interesting part about ProxyProperties
	//you can use them so combine several properties into a new one, by providing a lambda
	//this can also act as a simple converter, e.g. perfoming a cast on an input
	//super simple example
	//auto intStringLambda = [](int i, std::string s)
	//{ return s + ": " + std::to_string(i);
	//};
	//container.setProperty(CombinedStrPD, pd::makeProxyProperty(intStringLambda, IntPD, StringPD));

	template<typename FuncT, typename ... PropertDescriptors>
	auto makeProxyProperty(FuncT&& func, const PropertDescriptors& ... pds)
	{
		using ResultT = typename std::invoke_result_t<FuncT, PropertDescriptors::type...>;
		return std::make_unique<ConvertingProxyProperty<ResultT, FuncT, PropertDescriptors...>>(std::forward<FuncT>(func), pds...);
	}

	template<typename T, typename FuncT, typename ... PropertDescriptors>
	class ConvertingProxyProperty : public ProxyProperty<T>
	{
	public:
		ConvertingProxyProperty(FuncT funcT, const PropertDescriptors& ... pds)
			: ProxyProperty<T>()
			, m_func(std::move(funcT))
			, m_pds(std::make_tuple(std::addressof(pds)...))
		{
		}
		//implement contructor in the base classes later
		//ConvertingProxyProperty(const ConvertingProxyProperty&) = default;
		//ConvertingProxyProperty(ConvertingProxyProperty&&) = default;

		//ConvertingProxyProperty(const ConvertingProxyProperty&) = default;
		virtual T get() const override
		{
			//all the property descriptors are packed in a tuple, therefore we need std::apply to unpack them
			return std::apply([&](const auto& pds...)
			{
				//now we call getProperty for every property descriptor and invoke the function with the result 
				return std::invoke(m_func, getProperty(*pds)...);
			}
			, m_pds);
		}

		virtual ConvertingProxyProperty* clone() override
		{
			return nullptr;
		}
	protected:
		FuncT m_func;
		std::tuple<const PropertDescriptors*...> m_pds;
	};

	//###########################################################################
	//#
	//#                        Subject                            
	//#
	//############################################################################


	//super simple signal implementation

	template <typename... Args>
	class Signal {

	public:

		Signal() = default;

		// connects a member function to this Signal
		template <typename T>
		void connect(T *inst, void (T::*func)(Args...)) {
			connect([inst, func](Args... args) {
				(inst->*func)(args...);
			});
		}

		// connects a const member function to this Signal
		template <typename T>
		void connect(T *inst, void (T::*func)(Args...) const) {
			connect([inst, func](Args... args) {
				(inst->*func)(args...);
			});
		}

		// connects a std::function to the signal. The returned
		// value can be used to disconnect the function again
		
		template<typename FuncT>
		void connect(FuncT&& slot) {
			m_slots.emplace_back(std::forward<FuncT>(slot));
		}

		// disconnects all previously connected functions
		void disconnect() {
			m_slots.clear();
		}

		// calls all connected functions
		void emit(Args... p) const {
			for (auto& slot : m_slots) {
				slot(p...);
			}
		}

	private:
		std::vector<std::function<void(Args...)>> m_slots;
	};
}
