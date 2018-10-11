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
#include <typeinfo>
#include <typeindex>
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
	template<template<typename ...> class MapT = std::unordered_map>
	class PropertyContainerBase;

	using PropertyContainer = PropertyContainerBase<>;

	//forward declarations
	template<typename T>
	class ProxyProperty;

	class ProxyPropertyBase;

	class ChangeManager;

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
	
	template<template<typename ...> class MapT>
	class PropertyContainerBase
	{
	public:
		PropertyContainerBase() = default;
		//enable move constructors
		PropertyContainerBase& operator=(PropertyContainerBase&&) = default;
		PropertyContainerBase(PropertyContainerBase&&) = default;
		//the copy assignment operator is deleted, since it's not a use case to copy by assignment
		PropertyContainerBase& operator=(const PropertyContainerBase&) = delete;
		//we need the copy constructor to be able to easily implement a clone method
		//since we use unique_ptr we have to implement it ourselves
		PropertyContainerBase(const PropertyContainerBase& other)
			: m_toContainer(other.m_toContainer)
			, m_toTypedSignal(other.m_toTypedSignal)
			, m_propertyData(other.m_propertyData)
		{
			m_children.resize(other.m_children.size());
			for (const auto& child : other.m_children)
				m_children.emplace_back(std::make_unique<PropertyContainerBase>(*child));
		}

		//we only need the virtual destructor, because I didn't want to intrduce a special case for
		//owned proxy properties, an option could be to try a variant for this
		//or simple use a second vector to store proxy properties
		virtual ~PropertyContainerBase() = default;
		friend class ChangeManager;

		//getProperty returns the value for the provided PD
		//if the property is not set, the default value will be returned
		//the alternative would be to return an optional<T>, but that would 
		//only cater the non normal use case and the optional can also be
		//used by the caller
		template<typename T>
		T getProperty(const PropertyDescriptor<T>& pd) const
		{
			auto containerIt = m_toContainer.find(&pd);
			//check if a property has never been set -> return the default value
			if (containerIt == end(m_toContainer))
			{
				assert(false);
				return pd.getDefaultValue();
			}
				
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
		//you can either connect to a function without arguments or one that is callable
		//by the type of the property
		template<typename T, typename FuncT>
		void connect(const PropertyDescriptor<T>& pd, FuncT&& func)
		{
			using PMF = PMF_traits<FuncT>;
			//get / construct the signal if needed
			auto& anySignal = m_toTypedSignal[&pd];
			if (!anySignal.has_value())
				anySignal.emplace<Signal<T>>();

			Signal<T>& typedSignal = std::any_cast<Signal<T>&>(anySignal);
			//case 1: function object callable with argument of type T
			if constexpr (std::is_invocable_v<FuncT, T>)
			{
				typedSignal.connect(std::forward<FuncT>(func));
			}
			//case 2: pointer of member function with argument of type T
			else if constexpr (std::is_invocable_v<typename PMF::member_type, T>)
			{
				typedSignal.connect(static_cast<typename PMF::class_type*>(this), std::forward<FuncT>(func));
			}
			//case 3: pointer of member function with no argument
			//this should be used if we want multiple properties to trigger the same method
			//e.g. we have to recalulate something when any of the properties the result depends on changes
			else if constexpr (std::is_function_v<typename PMF::member_type>)
			{
				typedSignal.connect(std::type_index(typeid(FuncT)), [inst = static_cast<typename PMF::class_type*>(this), f = std::forward<FuncT>(func)](const T&){ (inst->*f)(); });
			}
			//case 4: callable function object with no argument
			else if constexpr (std::is_invocable_v<FuncT>)
			{
				typedSignal.connect([f = std::forward<FuncT>(func)](const T&) { f();});
			}
			else
			{
				static_assert(false,  "Argument is not a valid callable functor. "
									  "Argument should be convertible to std::function<void()> or std::function<void(T)>");
			}
			
			auto containerIt = m_toContainer.find(&pd);
			if(containerIt != end(m_toContainer))
				containerIt->second->addSignal(pd, &anySignal);
		}

		//for now we can only disconnect all the functions connect attached to a certain property
		template<typename T>
		void disconnect(const PropertyDescriptor<T>& pd)
		{
			if (auto& typedSignal = m_toTypedSignal[&pd]; typedSignals.has_value())
				std::any_cast<Signal<T>&>(typedSignal).disconnect();
		}
		//here can can connect a property to a variable
		//be aware this can crash if the provided variable goes out of scope
		//so it should only be used for member variables
		template<typename T>
		void connectToVar(const PropertyDescriptor<T>& pd, T& memberVariable)
		{
			connect(pd, [&memberVariable](const T& newValue)
			{
				memberVariable = newValue;
			});
		}

		//use this to build the property container tree structure
		PropertyContainerBase* addChildContainer(std::unique_ptr<PropertyContainerBase> propertyContainer)
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
					for (auto& subject : propertyData.connectedSignals)
						std::any_cast<Signal<T>&>(*subject).emit(std::any_cast<T&>(propertyData.value));
				}
			}
			//in this case we store a proxy property that can return a value of the given type
			else if constexpr(std::is_base_of_v<ProxyProperty<T>, U::element_type>)
			{
				propertyData.proxyProperty = value.get();
				addChildContainer(std::move(value));
				const auto newValue = static_cast<ProxyProperty<T>*>(propertyData.proxyProperty)->get();
				for (auto& subject : propertyData.connectedSignals)
					std::any_cast<Signal<T>&>(*subject).emit(newValue);
			}
			else
				static_assert(false, "The type T of the property descriptor doesn't match the type of the passed value.");
		}

		template<typename T>
		void updateAllSignals(const PropertyDescriptor<T>& pd)
		{
			auto& propertyData = m_propertyData[&pd];
			propertyData.connectedSignals.clear();
			getAllSignals(pd, propertyData.connectedSignals);
		}


		template<typename T>
		void addSignal(const PropertyDescriptor<T>& pd, std::any* signal)
		{
			auto& propertyData = m_propertyData[&pd];
			//TODO: check if this is needed or can be avoided
			if(std::find(begin(propertyData.connectedSignals), end(propertyData.connectedSignals), signal) == end(propertyData.connectedSignals))
				propertyData.connectedSignals.emplace_back(signal);
		}

		template<typename T>
		void addSignals(const PropertyDescriptor<T>& pd, std::vector<std::any*>& connectedSignals)
		{
			auto& propertyData = m_propertyData[&pd];
			std::copy(begin(connectedSignals), end(connectedSignals), std::back_inserter(propertyData.connectedSignals));
		}

		template<typename T>
		void getAllSignals(const PropertyDescriptor<T>& pd, std::vector<std::any*>& connectedSignals)
		{
			//add own subject
			if (auto subjectIt = m_toTypedSignal.find(&pd); subjectIt != end(m_toTypedSignal))
				connectedSignals.push_back(&(subjectIt->second));

			//add the signals of all children unless they own property data themselves
			for (auto& children : m_children)
				if (!children->ownsPropertyDataInternal(pd))
					children->getAllSignals(pd, connectedSignals);
		}

		void setParentContainerForProperty(const PropertyDescriptorBase& pd, PropertyContainerBase* container)
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
			auto oldSubjects = propertyData.connectedSignals;

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
			if (newContainer)
			{
				auto newValue = newContainer->getProperty(pd);
				if (newValue != oldValue)
				{
					for (auto* subject : oldSubjects)
						std::any_cast<Signal<T>&>(*subject).emit(newValue);
				}
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

		PropertyContainerBase* getOwningPropertyContainer(const PropertyDescriptorBase& pd)
		{
			//check if we own the property ourselves
			if (ownsPropertyDataInternal(pd))
				return this;
			//check if we know any container that owns the given PD
			auto containerIt = m_toContainer.find(&pd);
			return containerIt != end(m_toContainer) ? (containerIt->second) : nullptr;
		}

		void setParent(PropertyContainerBase* container)
		{
			m_parent = container;
		}

		using KeyT = const PropertyDescriptorBase*;
		//contains owned ProxyProperties as well as other owned children
		std::vector<std::unique_ptr<PropertyContainerBase>> m_children;
		PropertyContainerBase* m_parent = nullptr;

		MapT<KeyT, PropertyContainerBase*> m_toContainer;
		//not sure if using std::any is the correct choice here
		//I could also write a virtual base class for the Signal
		MapT<KeyT, std::any> m_toTypedSignal;
		struct PropertyData
		{
			std::any value;
			ProxyPropertyBase* proxyProperty = nullptr;
			std::any signal;
			std::vector<std::any*> connectedSignals;
			bool dirtyFlag = false;
		};
		MapT<KeyT, PropertyData> m_propertyData;
	};

	//###########################################################################
	//#
	//#                        ProxyProperty Definition                             
	//#
	//############################################################################

	class ProxyPropertyBase : public PropertyContainer
	{
	public:
		virtual ~ProxyPropertyBase() = default;
		virtual std::unique_ptr<ProxyPropertyBase> clone() = 0;
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
		ConvertingProxyProperty(const ConvertingProxyProperty&) = default;
		ConvertingProxyProperty(ConvertingProxyProperty&&) = default;

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

		virtual std::unique_ptr<ProxyPropertyBase> clone() override
		{
			return std::make_unique<ConvertingProxyProperty>(*this);
		}
	protected:
		FuncT m_func;
		std::tuple<const PropertDescriptors*...> m_pds;
	};

	//###########################################################################
	//#
	//#                        Signal                            
	//#
	//############################################################################


	//rather simple signal implementation that can handle as Functor and class ptr + PMF 
	//the only exception is that it will not add the same PMF twice
	template <typename... Args>
	class Signal 
	{

	public:
		Signal() = default;

		//// connects a member function to this Signal
		template <typename T, typename FuncT>
		void connect(T *inst, FuncT&& func) 
		{
			using PMF = PMF_traits<FuncT>;
			static_assert(std::is_same_v<T, std::remove_const_t<typename PMF::class_type>>, "Member func ptr type has to match instance type.");
			m_slots.try_emplace(std::type_index(typeid(FuncT)), [inst, func](Args... args) {
				(inst->*func)(args...);
			});
		}

		// connects a std::function to the signal
		template<typename FuncT>
		void connect(FuncT&& slot) 
		{
			m_slots.try_emplace(std::type_index(typeid(FuncT)), std::forward<FuncT>(slot));
		}

		//// connects a function to this Signal, but provides the type index upfront
		//// this  is needed when we use a lambda to wrap a member function ptr 
		template<typename FuncT>
		void connect(std::type_index idx, FuncT&& slot)
		{
			m_slots.try_emplace(idx, std::forward<FuncT>(slot));
		}

		// disconnects all previously connected functions
		void disconnect() 
		{
			m_slots.clear();
		}

		// calls all connected functions
		void emit(Args... p) const 
		{
			for (auto& [typeID, slot] : m_slots)
				slot(p...);
		}

	private:
		std::unordered_map<std::type_index, std::function<void(Args...)>> m_slots;
	};

	//the change manager handles an update step, which looks like:
	//1. collect all signals belonging to properties that changed since the last update
	//2. remove duplicates of calls to class member functions
	//3. reset the dirty flags of all the properties that changed
	//4. emit the function calls of all the signals
	//TODO: add a pre and post update step
	//TODO: add implementation

	class ChangeManager
	{
	public:
		ChangeManager(const PropertyContainer& container)
		{

		}

	};
}
