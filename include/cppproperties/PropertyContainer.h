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
#include <unordered_set>

namespace pd
{
	template<template<typename ...> class MapT = std::unordered_map>
	class PropertyContainerBase;
	using PropertyContainer = PropertyContainerBase<>;

	//forward declarations
	template<typename T>
	class ProxyProperty;
	class ProxyPropertyBase;
	class Signal;

	//pointer to member function utilities

	template<typename>
	struct PMF_traits 
	{
		using member_type = void;
		using class_type = void;
	};

	template<class T, class U>
	struct PMF_traits<U T::*> 
	{
		using member_type = typename U;
		using class_type = typename T;
	};


	//###########################################################################
	//#
	//#                        PropertyContainer                               
	//#
	//############################################################################
	
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
		//or simply use a second vector to store proxy properties
		virtual ~PropertyContainerBase() = default;
		

		//getProperty returns the value for the provided PD
		//if the property is not set, the default value will be returned
		//the alternative would be to return an optional<T>, but that would 
		//only cater the excpetional use case and the optional can also be
		//used by the caller
		template<typename T>
		T getProperty(const PropertyDescriptor<T>& pd) const
		{
			auto containerIt = m_toContainer.find(&pd);
			//check if a property has never been set -> return the default value
			if (containerIt == end(m_toContainer))
			{
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

		//interface to trigger a property changed without changing the value
		template<typename T>
		void touchProperty(const PropertyDescriptor<T>& pd)
		{
			auto containerIt = m_toContainer.find(&pd);
			if (containerIt != end(m_toContainer))
				containerIt->second->touchPropertyInternal(pd);
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
			auto& signal = m_toTypedSignal[&pd];

			//case 1: function object callable with argument of type T
			if constexpr (std::is_invocable_v<FuncT, T>)
			{
				signal.connect<T>(std::forward<FuncT>(func));
			}
			//case 2: pointer of member function with argument of type T
			
			else if constexpr (std::is_invocable_v<typename PMF::member_type, T>)
			{
				signal.connect<T>(static_cast<typename PMF::class_type*>(this), std::forward<FuncT>(func));
			}
			//the last two should be used if we want multiple properties to trigger the same method
			//e.g. we have to recalulate something when any of the properties the result depends on changes
			//case 3: callable functor with no argument
			else if constexpr (std::is_function_v<typename PMF::member_type>)
			{
				signal.connect(static_cast<typename PMF::class_type*>(this), std::forward<FuncT>(func));
			}
			//case 4: pointer of member function with no argument

			else if constexpr (std::is_invocable_v<FuncT>)
			{
				signal.connect(std::forward<FuncT>(func));
			}
			else
			{
				static_assert(false,  "Argument is not a valid callable functor. "
									  "Argument should be convertible to std::function<void()> or std::function<void(T)>");
			}
			
			auto containerIt = m_toContainer.find(&pd);
			if(containerIt != end(m_toContainer))
				containerIt->second->addSignal(pd, &signal);
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
		template<typename ContainerT, typename... Args>
		[[maybe_unused]] PropertyContainerBase& addChildContainer(Args&& ...args)
		{
			return addChildContainer(std::make_unique<ContainerT>(std::forward<Args>(args)...));
		}

		//use this to build the property container tree structure
		[[maybe_unused]] PropertyContainerBase& addChildContainer(std::unique_ptr<PropertyContainerBase> propertyContainer)
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
			return *propertyContainerPtr;
		}
		//the emit step looks like:
		//1. collect all signals belonging to properties that changed since the last update
		//2. remove duplicates of calls to class member functions (optional)
		//3. reset the dirty flags of all the properties that changed
		//4. get the new value and make a copy
		//5. emit the function calls of all the signals
		//6. call emit on all children
		//TODO: add a pre and post update step
		void emit(bool ignoreDuplicateCalls = true)
		{
			for (auto* dirtyProperty : m_changedProperties)
			{
				dirtyProperty->isDirty = false;
			}
			//TODO add compile option, instead of runtime?
			std::unordered_map<std::type_index, const std::function<void()>&> slots;
			for (auto* dirtyProperty : m_changedProperties)
			{
				//the copy here is needed if we want to have stable values for all connected signals
				//this is because an emit might influence update value 
				//if that's not needed, we could simply pass the value directly to the emit
				std::any newValue = dirtyProperty->proxyProperty ? dirtyProperty->proxyProperty->getAsAny() : dirtyProperty->value;
				if (ignoreDuplicateCalls)
				{
					for (auto& dirtySignal : dirtyProperty->connectedSignals)
					{
						dirtySignal->merge(slots);
						dirtySignal->setEmitValue(newValue);
					}
						
					for (auto&[idx, slot] : slots)
						slot();
					slots.clear();
				}
				else
				{
					for (auto& dirtySignal : dirtyProperty->connectedSignals)
						dirtySignal->emit(newValue);
				}
			}
			//TODO: check if we need to support duplicate signal resolving for removed properties
			for (auto& removedProperty : m_removedProperies)
			{
				for (auto& dirtySignal : removedProperty.connectedSignals)
				{
					dirtySignal->emit(removedProperty.value);
				}
			}
				
			m_changedProperties.clear();
			m_removedProperies.clear();
			for (auto& child : m_children)
				child->emit(ignoreDuplicateCalls);
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
					if (propertyData.proxyProperty)
					{
						//this will most likely be a mistake
						//if this is really intended call remove property before
						assert(!propertyData.proxyProperty);
						removeProxyProperty(propertyData.proxyProperty);
						propertyData.proxyProperty = nullptr;
					}
						
					

					setDirty(propertyData);
				}
			}
			//in this case we store a proxy property that can return a value of the given type
			else if constexpr(std::is_base_of_v<ProxyProperty<T>, U::element_type>)
			{
				propertyData.proxyProperty = value.get();
				propertyData.proxyProperty->setDirtyCallback(DirtyCallback{ &propertyData });
				addChildContainer(std::move(value));
				const auto newValue = static_cast<ProxyProperty<T>*>(propertyData.proxyProperty)->get();
				if (newValue != pd.getDefaultValue())
				{
					setDirty(propertyData);
				}
			}
			else
				static_assert(false, "The type T of the property descriptor doesn't match the type of the passed value.");
		}

		void removeProxyProperty(const ProxyPropertyBase* proxyProperty)
		{
			auto ppIt = std::find_if(begin(m_children), end(m_children), [proxyProperty](const std::unique_ptr<PropertyContainerBase>& childPtr)
			{
				return childPtr.get() == proxyProperty;
			});
			if(ppIt != end(m_children))
				m_children.erase(ppIt);
		}

		template<typename T>
		void updateAllSignals(const PropertyDescriptor<T>& pd)
		{
			auto& propertyData = m_propertyData[&pd];
			propertyData.connectedSignals.clear();
			getAllSignals(pd, propertyData.connectedSignals);
		}


		template<typename T>
		void addSignal(const PropertyDescriptor<T>& pd, Signal* signal)
		{
			auto& propertyData = m_propertyData[&pd];
			//TODO: check if this can be optimized
			if(std::find(begin(propertyData.connectedSignals), end(propertyData.connectedSignals), signal) == end(propertyData.connectedSignals))
				propertyData.connectedSignals.emplace_back(signal);
		}

		template<typename T>
		void addSignals(const PropertyDescriptor<T>& pd, std::vector<Signal*>& connectedSignals)
		{
			auto& propertyData = m_propertyData[&pd];
			std::copy(begin(connectedSignals), end(connectedSignals), std::back_inserter(propertyData.connectedSignals));
		}

		template<typename T>
		void getAllSignals(const PropertyDescriptor<T>& pd, std::vector<Signal*>& connectedSignals)
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
			//copy the old signal ptr
			auto oldSignals = propertyData.connectedSignals;

			T oldValue = getProperty(pd);
			//we have to update all children and tell it which is the correct owning property container
			auto newContainer = m_parent ? m_parent->getOwningPropertyContainer(pd) : nullptr;
			setParentContainerForProperty(pd, newContainer);
			//we have to add all the signals from this level to the new owning container
			if (newContainer)
			{
				newContainer->addSignals(pd, oldSignals);
				
				auto newValue = newContainer->getProperty(pd);
				if (newValue != oldValue)
				{
					//TODO: this is not completely correct as we only need to trigger the old observers
					newContainer->touchPropertyInternal(pd);
				}
			}
			else if(!oldSignals.empty())
			{
				//there are still observers, but no new container
				//we need to signal the default value to the observers
				auto& removedProperty = m_removedProperies.emplace_back();
				removedProperty.value = pd.getDefaultValue();
				removedProperty.connectedSignals = std::move(oldSignals);
			}

			//now we remove the property data
			if (propertyData.proxyProperty)
				removeProxyProperty(propertyData.proxyProperty);
			m_propertyData.erase(&pd);
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

		template<typename T>
		void touchPropertyInternal(const PropertyDescriptor<T>& pd)
		{
			auto& propertyData = m_propertyData[&pd];
			setDirty(propertyData);
		}
		struct Property;
		void setDirty(Property& propertyData)
		{
			if (!propertyData.isDirty)
			{
				propertyData.isDirty = true;
				m_changedProperties.emplace_back(&propertyData);
			}
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
		struct DirtyCallback;
		friend struct DirtyCallback;
		struct DirtyCallback
		{
			void operator()(PropertyContainer& container)
			{
				container.setDirty(*propertyData);
			}
			Property* propertyData = nullptr;
		};
		
		
		void setParent(PropertyContainerBase* container)
		{
			m_parent = container;
		}

		using KeyT = const PropertyDescriptorBase*;
		//contains owned ProxyProperties as well as other owned children
		std::vector<std::unique_ptr<PropertyContainerBase>> m_children;
		PropertyContainerBase* m_parent = nullptr;

		MapT<KeyT, PropertyContainerBase*> m_toContainer;
		//we could also store the signal directly in the property
		MapT<KeyT, Signal> m_toTypedSignal;
		struct Property
		{
			std::any value;
			//this flag is an optimization to indicate that a property has changed
			bool isDirty = false;
			//in theory we could also put the proxy property base ptr into the any,
			//or use a variant, but I think the minimal memory overhead is a price
			//I'm happy to pay (vs. compile time overhead of std::variant)
			ProxyPropertyBase* proxyProperty = nullptr;
			std::vector<Signal*> connectedSignals;
			
		};
		MapT<KeyT, Property> m_propertyData;
		std::vector<Property*> m_changedProperties;
		std::vector<Property> m_removedProperies;
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
		virtual std::unique_ptr<ProxyPropertyBase> clone() { return nullptr; };
		virtual std::any getAsAny() const = 0;

		//maybe there is a nicer way to achieve the same thing?
		//proxy properties have to indicate themselves if they got changed
		//so the parent sets the callback
		//I guess a std::funtion could serve as a more general solution?
		void proxyPropertyChanged()
		{
			if (m_dirtyCallBack.propertyData)
			{
				m_dirtyCallBack(*this);
			}
		}
		void setDirtyCallback(DirtyCallback&& dirtyCallback)
		{
			m_dirtyCallBack = std::move(dirtyCallback);
		}
	protected:

		DirtyCallback m_dirtyCallBack;
	};
	//proxy property is simply a an abstration of a property, instead of using a value
	//you can use a class to calculate that value
	template<typename T>
	class ProxyProperty : public ProxyPropertyBase
	{
	public:
		using type = T;
		virtual T get() const = 0;
		
		virtual std::any getAsAny() const final
		{
			return std::make_any<T>(get());
		}
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
			((void)connect(pds, &ProxyPropertyBase::proxyPropertyChanged), ...);
		}
		//implement contructor in the base classes later
		ConvertingProxyProperty(const ConvertingProxyProperty&) = default;
		ConvertingProxyProperty(ConvertingProxyProperty&&) = default;

		//ConvertingProxyProperty(const ConvertingProxyProperty&) = default;
		virtual T get() const final
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


	//specialized signal implementation suited to our needs
	//it handles class ptr + PMF (with or without argument)
	//it handles callable funtors (with or without argument)
	//the underlying storage uses std::function<void()>
	//and a specialized lambda funtion to abstract the different cases
	//another requirement is that you can't add the same PMF twice
	class Signal 
	{
	public:
		Signal() = default;

		//// connects a member function without argument to this 
		template <typename classT, typename pmfT>
		void connect(classT *inst, pmfT&& func)
		{
			using PMF = PMF_traits<pmfT>;
			static_assert(std::is_same_v<classT, std::remove_const_t<typename PMF::class_type>>, "Member func ptr type has to match instance type.");
			m_slots.try_emplace(std::type_index(typeid(pmfT)), [inst, func]()
			{
				(inst->*func)();
			});
		}

		//// connects a member function with an argument of type T to this signal
		template <typename T, typename classT, typename pmfT, typename = T /*reject void argument PMF*/>
		void connect(classT *inst, pmfT&& func)
		{
			using PMF = PMF_traits<pmfT>;
			static_assert(std::is_same_v<classT, std::remove_const_t<typename PMF::class_type>>, "Member func ptr type has to match instance type.");
			m_slots.try_emplace(std::type_index(typeid(pmfT)), [inst, func, &valPtrPtr = m_proptertyPtr]()
			{
				(inst->*func)(std::any_cast<T>(*valPtrPtr));
			});
		}


		// connects a callable function with an argument of type T to the signal
		template<typename T, typename FuncT>
		void connect(FuncT&& func)
		{
			m_slots.try_emplace(std::type_index(typeid(FuncT)), [func = std::forward<pmfT>(func), &valPtrPtr = m_proptertyPtr]()
			{
				func(std::any_cast<T>(*valPtrPtr));
			});
		}

		//// connects a callable function to this Signal
		template<typename FuncT>
		void connect(FuncT&& slot)
		{
			m_slots.try_emplace(std::type_index(typeid(FuncT)), std::forward<FuncT>(slot));
		}

		// disconnects all previously connected functions
		void disconnect() 
		{
			m_slots.clear();
		}

		// calls all connected functions
		void emit(std::any& value) const 
		{
			setEmitValue(value);
			for (auto& [typeID, slot] : m_slots)
				slot();
		}
		void setEmitValue(std::any& value) const
		{
			m_proptertyPtr = &value;
		}
		void merge(std::unordered_map<std::type_index, const std::function<void()>&>& slots) const 
		{
			for (auto& [typeIndex, func] : m_slots)
				slots.emplace(typeIndex, func);
		}

	protected:

		std::unordered_map<std::type_index, std::function<void()>> m_slots;
		//we need to keep a ptr to a std::any as member, since the lambdas stored inside
		//the slots reference it
		mutable std::any* m_proptertyPtr;
	};


}
