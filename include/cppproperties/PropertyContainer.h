#pragma once

#include "PropertyDescriptor.h"
#include "Property.h"
#include "Signal.h"
#include <type_traits>
#include <typeinfo>
#include <typeindex>
#include <utility>
#include <tuple>
#include <memory>
#include <vector>
#include <functional>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace ps
{
	//###########################################################################
	//#
	//#                        PropertyContainer                               
	//#
	//############################################################################

	template<template<typename ...> class MapT>
	class PropertyContainerBase
	{
	public:
		using KeyT = const PropertyDescriptorBase*;
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
			, m_toSignal(other.m_toSignal)
		{
			m_children.resize(other.m_children.size());
			//TODO: implement clone... where is my polymorphic_value...
			//for(auto& [pd, propertyPtr] : other.m_propertyData)
			//	m_propertyData[pd].reset()
			for (const auto& child : other.m_children)
				m_children.emplace_back(std::make_unique<PropertyContainerBase>(*child));
		}

		//we only need the virtual destructor, because I didn't want to intrduce a special case for
		//owned proxy properties, an option could be to try a variant for this
		//or simply use a second vector to store proxy properties
		virtual ~PropertyContainerBase() = default;

		template <typename T>
		friend class Property;

		//getProperty returns the value for the provided PD
		//if the property is not set, the default value will be returned
		//the alternative would be to return an optional<T>, but that would 
		//only cater the excpetional use case and the optional can also be
		//used by the caller
		template<typename T>
		[[nodiscard]] const Property<T>& getProperty(const PropertyDescriptor<T>& pd) const
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
			auto propertyData = container.getPropertyInternal(pd);
			return propertyData ? *propertyData : pd.getDefaultValue();
		}
		//convenience function for getProperty
		template<typename T>
		[[nodiscard]] const Property<T>& operator [] (const PropertyDescriptor<T>& pd) const
		{
			return getProperty(pd);
		}

		//this function should only be ever needed very rarely
		//you should not need to interact with a proxy property directly
		template<typename T>
		[[nodiscard]] ProxyProperty<T>* getProxyProperty(const PropertyDescriptor<T>& pd) const
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

			PropertyContainerBase* parentContainer = m_parent ? m_parent->getOwningPropertyContainer(pd) : nullptr;
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
			auto containerIt = m_toContainer.find(&pd);
			if (containerIt == end(m_toContainer))
				return;
			auto& container = *containerIt->second;
			container.removePropertyInternal(pd);
		}

		//interface to check if a property has been set
		//if getting the default value (when no property is set) is not intended use case
		//use hasProperty to check if the property has been set already
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
		//the returned type index can be used to disconnect the connected function
		template<typename T, typename FuncT>
		[[maybe_unused]] std::type_index connect(const PropertyDescriptor<T>& pd, FuncT&& func)
		{
			using PMF = PMF_traits<FuncT>;
			//get / construct the signal if needed
			auto& signal = m_toSignal[&pd];

			auto containerIt = m_toContainer.find(&pd);
			if (containerIt != end(m_toContainer))
				containerIt->second->addSignal(pd, &signal);

			//case 1: function object callable with argument of type T
			if constexpr (std::is_invocable_v<FuncT, T>)
			{
				return signal.connect<T>(std::forward<FuncT>(func));
			}
			//case 2: pointer of member function with argument of type T

			else if constexpr (std::is_invocable_v<typename PMF::member_type, T>)
			{
				return signal.connect<T>(static_cast<typename PMF::class_type*>(this), std::forward<FuncT>(func));
			}
			//the last two should be used if we want multiple properties to trigger the same method
			//e.g. we have to recalulate something when any of the properties the result depends on changes
			//case 3: callable functor with no argument
			else if constexpr (std::is_function_v<typename PMF::member_type>)
			{
				return signal.connect<void>(static_cast<typename PMF::class_type*>(this), std::forward<FuncT>(func));
			}
			//case 4: pointer of member function with no argument
			else if constexpr (std::is_invocable_v<FuncT>)
			{
				return signal.connect<void>(std::forward<FuncT>(func));
			}
			else
			{
				static_assert(false, "Argument is not a valid callable functor. "
					"Argument should be convertible to std::function<void()> or std::function<void(T)>");
			}
		}

		//disconnect all the functions connect attached to a certain property
		template<typename T>
		void disconnect(const PropertyDescriptor<T>& pd)
		{
			m_toSignal[&pd].disconnect();
		}
		//disconnect the function with the given type index
		template<typename T>
		void disconnect(const PropertyDescriptor<T>& pd, std::type_index idx)
		{
			m_toSignal[&pd].disconnect(idx);
		}
		//here can can connect a property to a variable
		//be aware this can crash if the provided variable goes out of scope
		//so it should only be used for member variables
		template<typename T>
		std::type_index connectToVar(const PropertyDescriptor<T>& pd, T& memberVariable)
		{
			return connect<T>(pd, [&memberVariable](const T& newValue)
			{
				memberVariable = newValue;
			});
		}
		template<typename ContainerT, typename... Args>
		[[maybe_unused]] ContainerT& addChildContainer(Args&& ...args)
		{
			return *static_cast<ContainerT*>(&addChildContainer(std::make_unique<ContainerT>(std::forward<Args>(args)...)));
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
		//2. reset the dirty flags of all the properties that changed
		//3. remove duplicates of calls to class member functions (optional)
		//4. get the new value
		//5. emit the function calls of all the signals
		//6. remove the signals that got removed
		//7. call emit on all children
		//TODO: add a pre and post update step?
		void emit(bool ignoreDuplicateCalls = true)
		{
			for (auto* dirtyProperty : m_changedProperties)
			{
				dirtyProperty->m_propertyChanged = false;
			}

			if (ignoreDuplicateCalls)
				emitEliminateDuplicates();
			else
				emitWithDuplicates();
			
			//TODO: check if we need to support duplicate signal resolving for removed properties
			for (auto& removedProperty : m_removedProperies)
			{
				for (auto& dirtySignal : removedProperty->m_connectedSignals)
				{
					dirtySignal->emit(removedProperty->getPointer());
				}
			}

			m_changedProperties.clear();
			m_removedProperies.clear();
			for (auto& child : m_children)
				child->emit(ignoreDuplicateCalls);
		}
		// [] begin/end/size is to make the container more stl compatible
		//I think it's most reasonable to use the children as basis for the iterator / size
		const std::unique_ptr<PropertyContainerBase>& operator [](size_t idx) const
		{
			return m_children[idx];
		}
		
		size_t size() const noexcept
		{
			return m_children.size();
		}

		typename std::vector<std::unique_ptr<PropertyContainerBase>>::const_iterator cbegin() const noexcept
		{
			return m_children.cbegin();
		}

		typename std::vector<std::unique_ptr<PropertyContainerBase>>::const_iterator cend() const noexcept
		{
			return m_children.cend();
		}
		

	protected:
		void emitEliminateDuplicates()
		{
			//TODO add compile option, instead of runtime?
			std::unordered_map<std::type_index, const std::function<void()>&> slots;
			for (auto* dirtyProperty : m_changedProperties)
			{
				const void* newValue = dirtyProperty->getPointer();
				for (auto& dirtySignal : dirtyProperty->m_connectedSignals)
				{
					dirtySignal->getSlots(slots);
					dirtySignal->setEmitValue(newValue);
				}
			}

			for (auto& [idx, slot] : slots)
				slot();
		}
		void emitWithDuplicates()
		{
			for (auto* dirtyProperty : m_changedProperties)
			{
				const void* newValue = dirtyProperty->getPointer();
				for (auto& dirtySignal : dirtyProperty->m_connectedSignals)
					dirtySignal->emit(newValue);
			}
		}
		template<typename T>
		Property<T>* getPropertyInternal(const PropertyDescriptor<T>& pd) const
		{
			if (auto valueIt = m_propertyData.find(&pd); valueIt != end(m_propertyData))
			{
				auto& propertyData = valueIt->second;
				return static_cast<Property<T>*>(propertyData.get());
			}
			return nullptr;
		}

		template<typename T>
		Property<T>& getOrConstructPropertyInternal(const PropertyDescriptor<T>& pd)
		{
			auto& propertyData = m_propertyData[&pd];
			if (!propertyData)
				propertyData.reset(new Property<T>(this));
				
			return *static_cast<Property<T>*>(propertyData.get());
		}

		template<typename T>
		ProxyProperty<T>* getProxyPropertyInternal(const PropertyDescriptor<T>& pd) const
		{
			auto propertyData = getPropertyInternal(pd);
			if (!propertyData || !propertyData->isProxyProperty())
				return nullptr;

			return static_cast<ProxyProperty<T>*>(propertyData);
		}

		template<typename T, typename U>
		void changePropertyInternal(const PropertyDescriptor<T>& pd, U && value)
		{
			//this is the normal case where we store a value
			if constexpr (std::is_convertible_v<std::decay_t<U>, T>)
			{
				auto& propertyData = getOrConstructPropertyInternal(pd);
				if (!propertyData.isProxyProperty())
				{
					propertyData.set(std::forward<U>(value));
				}
				else
				{
					//this will most likely be a mistake
					//the property is a proxy property, but we call change property, which will
					//turn the proxy property into a normal property
					//if this is really intended call remove property + setProperty instead
					assert(false);
					removeProxyProperty(static_cast<ProxyProperty<T>*>(&propertyData));
					auto& propertyUniquePtr = m_propertyData[&pd];
					auto newProperty = new Property<T>(this);
					propertyUniquePtr.reset(newProperty);
					newProperty->set(std::forward<U>(value));
				}
			}
			//we got a proxy property unique_ptr
			//in this case we store a proxy property that can return a value of the given type
			else if constexpr (std::is_base_of_v<ProxyProperty<T>, U::element_type>)
			{
				auto proxyProperty = static_cast<ProxyProperty<T>*>(&addChildContainer(std::move(value)));
				m_propertyData[&pd].reset(proxyProperty);
				const T& newValue = proxyProperty->get();
				if (pd.getDefaultValue() != newValue)
				{
					setDirty(*proxyProperty);
				}
			}
			else
				static_assert(false, "The type T of the property descriptor doesn't match the type of the passed value.");
		}

		void removeProxyProperty(const PropertyContainerBase* proxyProperty)
		{
			auto ppIt = std::find_if(begin(m_children), end(m_children), [proxyProperty](const std::unique_ptr<PropertyContainerBase>& childPtr)
			{
				return childPtr.get() == proxyProperty;
			});
			if (ppIt != end(m_children))
				m_children.erase(ppIt);
		}

		template<typename T>
		void updateAllSignals(const PropertyDescriptor<T>& pd)
		{
			Property<T>& propertyData = getOrConstructPropertyInternal(pd);
			propertyData.m_connectedSignals.clear();
			getAllSignals(pd, propertyData.m_connectedSignals);
		}


		template<typename T>
		void addSignal(const PropertyDescriptor<T>& pd, Signal* signal)
		{
			Property<T>& propertyData = getOrConstructPropertyInternal(pd);
			//TODO: check if this can be optimized
			if (std::find(begin(propertyData.m_connectedSignals), end(propertyData.m_connectedSignals), signal) == end(propertyData.m_connectedSignals))
				propertyData.m_connectedSignals.emplace_back(signal);
		}

		template<typename T>
		void addSignals(const PropertyDescriptor<T>& pd, std::vector<Signal*>& connectedSignals)
		{
			auto& propertyData = m_propertyData[&pd];
			std::copy(begin(connectedSignals), end(connectedSignals), std::back_inserter(propertyData->m_connectedSignals));
		}

		template<typename T>
		void getAllSignals(const PropertyDescriptor<T>& pd, std::vector<Signal*>& connectedSignals)
		{
			//add own subject
			if (auto subjectIt = m_toSignal.find(&pd); subjectIt != end(m_toSignal))
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
			Property<T>* propertyData = getPropertyInternal(pd);
			if (!propertyData)
				return;
			//copy the old signal ptr
			auto oldSignals = propertyData->m_connectedSignals;

			auto oldValue = propertyData->get();
			//we have to update all children and tell it which is the correct owning property container
			auto newContainer = m_parent ? m_parent->getOwningPropertyContainer(pd) : nullptr;
			setParentContainerForProperty(pd, newContainer);
			//we have to add all the signals from this level to the new owning container
			if (newContainer)
			{
				newContainer->addSignals(pd, oldSignals);

				Property<T>* newValue = newContainer->getPropertyInternal(pd);
				if (newValue->get() != oldValue)
				{
					//TODO: this is not completely correct as we only need to trigger the old observers
					newContainer->touchPropertyInternal(pd);
				}
			}
			else if (!oldSignals.empty())
			{
				//there are still observers, but no new container
				//we need to signal the default value to the observers
				auto& removedProperty = m_removedProperies.emplace_back(std::make_unique<Property<T>>(pd.getDefaultValue()));
				removedProperty->m_connectedSignals = std::move(oldSignals);
			}

			//now we remove the property data
			if (propertyData->isProxyProperty())
				removeProxyProperty(static_cast<ProxyProperty<T>*>(propertyData));
			m_propertyData.erase(&pd);
		}

		template<typename T>
		void observePropertyInternal(const PropertyDescriptor<T>& pd, std::vector<std::function<void()>>* observers)
		{
			auto& propertyData = m_propertyData[&pd];
			propertyData.push_back(observers);
		}

		bool ownsPropertyDataInternal(const PropertyDescriptorBase& pd) const noexcept
		{
			return m_propertyData.find(&pd) != end(m_propertyData);
		}

		template<typename T>
		void touchPropertyInternal(const PropertyDescriptor<T>& pd)
		{
			auto& propertyData = m_propertyData[&pd];
			setDirty(*propertyData);
		}
		void setDirty(PropertyBase& propertyData)
		{
			if (!propertyData.m_propertyChanged)
			{
				propertyData.m_propertyChanged = true;
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

		void setParent(PropertyContainerBase* container)
		{
			m_parent = container;
		}

		
		//contains owned ProxyProperties as well as other owned children
		std::vector<std::unique_ptr<PropertyContainerBase>> m_children;
		PropertyContainerBase* m_parent = nullptr;
		//needed for lookup on which container the property is set
		MapT<KeyT, PropertyContainerBase*> m_toContainer;
		//we have to store the signals outside of the properties, since the signals are per container and not per property
		//e.g. I can start an observation before a property is even set
		MapT<KeyT, Signal> m_toSignal;
		struct CustomDelete
		{
			void operator()(PropertyBase* propertyPtr)
			{
				if (!propertyPtr->isProxyProperty())
					delete propertyPtr;
			}
		};
		MapT<KeyT, std::unique_ptr<PropertyBase, CustomDelete>> m_propertyData;

		std::vector<PropertyBase*> m_changedProperties;
		std::vector<std::unique_ptr<PropertyBase>> m_removedProperies;
	};
		


	//###########################################################################
//#
//#                        ProxyProperty Definition                             
//#
//############################################################################

//proxy property is simply a an abstration of a property, instead of using a value
//you can use a class to calculate that value
	template<typename T>
	class ProxyProperty : public PropertyContainer, public Property<T>
	{
	public:
		ProxyProperty() 
		: PropertyContainer()
		, Property<T>()
		{
			m_isProxyProperty = true;
		}
	};
}
