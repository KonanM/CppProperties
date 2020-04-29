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
	protected:
		struct PropertyData
		{
			//the property doesn't have to be set, there could only be observers to this property 
			//using a shared_ptr is intentional, since a property can also be a 
			//proxy property which has internal shared ownership with the m_children
			std::shared_ptr<PropertyBase> m_property{};
			//we need a way to signal all connected signals the default value in case a property is removed
			std::vector<Signal_PMF*> m_connectedSignals{};
			//the signal can be connected to for a delayed notification when this property has changed
			Signal_PMF m_signal{};
			//using a bool to indicate if this property is a proxy property saves a dynamic cast
			bool m_isProxyProperty = false;
			//we have to cache if a property has changed after it was last emitted
			bool m_propertyChanged = false;
			//currently this is my solution for decoupling the property from the property container
			//if we want the property data to be copyable we need to be able to copy them in a type erased way
			void(*m_copyTypeErased)(std::shared_ptr<PropertyBase>, std::shared_ptr<PropertyContainer>, PropertyContainer *, const PropertyDescriptorBase *) = nullptr;
			//we need to store a pointer to the type erased value of the property, 
			//this will be cast to the correct type when needed
			const void* m_valuePtr = nullptr;

			PropertyData() = default;
			PropertyData(PropertyData&&) = default;
			PropertyData& operator=(PropertyData&&) = default;
			//it's safer to completely disable the copy constructor and copy via m_copyTypeErased
			PropertyData(const PropertyData& other) = delete;

			template<typename T, typename PP = ProxyProperty<T>>
			void init(std::shared_ptr<Property<T>> propertyPtr, PropertyContainer* parentPtr, const PropertyDescriptorBase* pd)
			{
                m_valuePtr = &propertyPtr->get();
                (*propertyPtr).connect([propertyDataPtr = this, parentPtr](const T&) { parentPtr->setDirty(*propertyDataPtr); });
				//for each property we have to store how it can be copied
				m_copyTypeErased = +[](std::shared_ptr<PropertyBase> property, std::shared_ptr<PropertyContainer> proxyProperty, PropertyContainer* parentPtr, const PropertyDescriptorBase* pd) {
					if (property)
					{
						if constexpr (std::is_copy_constructible_v<T>)
							parentPtr->setProperty(static_cast<const PropertyDescriptor<T>&>(*pd), std::static_pointer_cast<Property<T>>(property)->get());
						else //we can't handle arbitrary non copyable properties, but we can handle std::shared_ptr and std::unique_ptr
							parentPtr->setProperty(static_cast<const PropertyDescriptor<T>&>(*pd), std::make_unique<typename T::element_type>(*std::static_pointer_cast<Property<T>>(property)->get()));
					}
					else if (proxyProperty)
						parentPtr->setProperty(static_cast<const PropertyDescriptor<T>&>(*pd), std::static_pointer_cast<PP>(proxyProperty));
					else
						assert(false);
				};
                m_property = std::move(propertyPtr);
			}
		};
		using KeyT = const PropertyDescriptorBase*;
		
		
		MapT<KeyT, PropertyData> m_propertyData;
		//if a container has children we need to lookup on which container a certain property is set
		MapT<KeyT, PropertyContainerBase*> m_toContainer;
		//whenever a property is changed it will be added to this vector
		//see also PropertyData->init and setDirty
		std::vector<PropertyData*> m_changedProperties;
		std::vector<std::unique_ptr<PropertyData>> m_removedProperties;

		//contains owned ProxyProperties as well as other owned children
		std::vector<std::shared_ptr<PropertyContainerBase>> m_children;
		PropertyContainerBase* m_parent = nullptr;
		//if the key has been set then we know that this container is actually a proxy property stored via this key in another container
		KeyT m_key = nullptr;


	public:
		
		PropertyContainerBase() = default;
		//enable move constructors
		PropertyContainerBase& operator=(PropertyContainerBase&&) = default;
		PropertyContainerBase(PropertyContainerBase&&) = default;
		//I don't think it makes sense to implement a copy constructor for now
		PropertyContainerBase& operator=(const PropertyContainerBase&) = delete;
		PropertyContainerBase(const PropertyContainerBase& other)
			: m_toContainer(other.m_toContainer)
		{
			
			for (auto& [pd, propertyData] : other.m_propertyData)
			{
				if (!propertyData.m_isProxyProperty)
				{
					propertyData.m_copyTypeErased(propertyData.m_property, nullptr, this, pd);
				}
			}
			m_children.reserve(other.m_children.size());
			for (const auto& child : other.m_children)
			{
				if (child->m_key /*isProxyProperty*/)
				{
					//here the child is a proxyproperty, that has to be copied type erased
					auto& propertyData = other.m_propertyData.at(child->m_key);
					propertyData.m_copyTypeErased(nullptr, child, this, child->m_key);
				}
				else
				{
					//TODO: add functionality to copy also child containers type erased...
					//I guess I really need a polymorphic_value
					//here we have a child container that we can copy directly
					addChildContainerInternal(std::make_shared<PropertyContainer>(*child));
				}
			}
		}
		virtual ~PropertyContainerBase() = default;

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
			//construct the property here to mark that we have the ownership of this property
			getOrConstructPropertyInternal(pd);
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
		[[maybe_unused]] size_t connect(const PropertyDescriptor<T>& pd, FuncT&& func)
		{
			using PMF = PMF_traits<FuncT>;
			//get / construct the signal if needed
			auto& signal = m_propertyData[&pd].m_signal;

			auto containerIt = m_toContainer.find(&pd);
			if (containerIt != end(m_toContainer))
				containerIt->second->addSignal(pd, &signal);

			//case 1: function object callable with argument of type T
			if constexpr (std::is_invocable_v<FuncT, T>)
			{
				return signal.connect([func](void*, const void* valuePtr) { func(*static_cast<const T*>(valuePtr)); });
			}
			//case 2: pointer of member function with argument of type T
			else if constexpr (std::is_invocable_v<typename PMF::member_type, T>)
			{
				return signal.template connectPMF<T>(std::forward<FuncT>(func));
			}
			//the last two should be used if we want multiple properties to trigger the same method
			//e.g. we have to recalulate something when any of the properties the result depends on changes
			//case 3: callable functor with no argument
			else if constexpr (std::is_function_v<typename PMF::member_type>)
			{
				return signal.template connectPMF<void>(std::forward<FuncT>(func));
			}
			//case 4: pointer of member function with no argument
			else if constexpr (std::is_invocable_v<FuncT>)
			{
				return signal.connect([func](void*, const void*) { func(); });
			}
			else
			{
				static_assert(std::is_same_v<T, void>, "Argument is not a valid callable functor. "
					"Argument should be convertible to std::function<void()> or std::function<void(T)>");
			}
		}

		//disconnect all the functions connect attached to a certain property
		template<typename T>
		void disconnect(const PropertyDescriptor<T>& pd)
		{
			m_propertyData[&pd].m_signal.disconnect();
		}
		//disconnect the function with the given type index
		template<typename T>
		void disconnect(const PropertyDescriptor<T>& pd, size_t idx)
		{
			m_propertyData[&pd].m_signal.disconnect(idx);
		}
		//here can can connect a property to a variable
		//be aware this can crash if the provided variable goes out of scope
		//so it should only be used for member variables
		template<typename T>
		size_t connectToVar(const PropertyDescriptor<T>& pd, T& memberVariable)
		{
			return connect<T>(pd, [&memberVariable](const T& newValue)
			{
				memberVariable = newValue;
			});
		}
		template<typename ContainerT, typename... Args>
		[[maybe_unused]] ContainerT& addChildContainer(Args&& ...args)
		{
			return *static_cast<ContainerT*>(addChildContainerInternal(std::make_shared<ContainerT>(std::forward<Args>(args)...)).get());
		}

		//use this to build the property container tree structure
		[[maybe_unused]] std::shared_ptr<PropertyContainerBase>& addChildContainer(std::unique_ptr<PropertyContainerBase> propertyContainer)
		{
			return addChildContainerInternal(std::move(propertyContainer));
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
			for (auto& removedProperty : m_removedProperties)
			{
				for (auto& dirtySignal : removedProperty->m_connectedSignals)
				{
					dirtySignal->emit(this, removedProperty->m_valuePtr);
				}
			}
			m_removedProperties.clear();
			for (auto& child : m_children)
				child->emit(ignoreDuplicateCalls);
		}
		// [] begin/end/size is to make the container more stl compatible
		//I think it's most reasonable to use the children as basis for the iterator / size
		const std::shared_ptr<PropertyContainerBase>& operator [](size_t idx) const
		{
			return m_children[idx];
		}
		
		size_t size() const noexcept
		{
			return m_children.size();
		}

		typename std::vector<std::shared_ptr<PropertyContainerBase>>::const_iterator cbegin() const noexcept
		{
			return m_children.cbegin();
		}

		typename std::vector<std::shared_ptr<PropertyContainerBase>>::const_iterator cend() const noexcept
		{
			return m_children.cend();
		}
		

	protected:

		[[maybe_unused]] std::shared_ptr<PropertyContainerBase>& addChildContainerInternal(std::shared_ptr<PropertyContainerBase> propertyContainer)
		{
			propertyContainer->setParent(this);
			//here we propagate all the pd's that we own
			for (auto& propertyData : m_propertyData)
			{
				if (auto& pd = *propertyData.first; !propertyContainer->ownsPropertyDataInternal(pd))
					propertyContainer->setParentContainerForProperty(pd, this);
			}
			//here we propagate all the pd's that we don't own
			for (auto& [pd, container] : m_toContainer)
			{
				if (!propertyContainer->ownsPropertyDataInternal(*pd))
					propertyContainer->setParentContainerForProperty(*pd, container);
			}

			return m_children.emplace_back(std::move(propertyContainer));
		}
		void emitEliminateDuplicates()
		{
			std::unordered_set<size_t> alreadyInvokedSlots;
			//here we make a local copy, because changing a property could result in the change of another property
			auto changedProperties = m_changedProperties;
			m_changedProperties.clear();
			for (auto* dirtyProperty : changedProperties)
			{
				const void* newValue = dirtyProperty->m_valuePtr;
				for (auto& dirtySignal : dirtyProperty->m_connectedSignals)
				{
					dirtySignal->emitUnique(this, newValue, alreadyInvokedSlots);
				}
			}
		}
		void emitWithDuplicates()
		{
			//here we make a local copy, because changing a property could result in the change of another property
			auto changedProperties = m_changedProperties;
			m_changedProperties.clear();
			for (auto* dirtyProperty : changedProperties)
			{
				const void* newValue = dirtyProperty->m_valuePtr;
				for (auto& dirtySignal : dirtyProperty->m_connectedSignals)
					dirtySignal->emit(this, newValue);
			}
		}
		template<typename T>
		Property<T>* getPropertyInternal(const PropertyDescriptor<T>& pd) const
		{
			if (auto valueIt = m_propertyData.find(&pd); valueIt != end(m_propertyData))
			{
				auto& propertyData = valueIt->second.m_property;
				return static_cast<Property<T>*>(propertyData.get());
			}
			return nullptr;
		}

		template<typename T>
		Property<T>& getOrConstructPropertyInternal(const PropertyDescriptor<T>& pd)
		{
			auto& propertyData = m_propertyData[&pd];;
			if (!propertyData.m_property)
			{
				propertyData.init(std::make_shared<Property<T>>(), this, &pd);
			}
			return static_cast<Property<T>&>(*propertyData.m_property);
		}

		template<typename T>
		ProxyProperty<T>* getProxyPropertyInternal(const PropertyDescriptor<T>& pd) const
		{
			auto& propertyData = m_propertyData[&pd];
			if (!propertyData.m_property || !propertyData.m_isProxyProperty)
				return nullptr;
			return static_cast<ProxyProperty<T>*>(propertyData.m_property);
		}

		template<typename T, typename U>
		void changePropertyInternal(const PropertyDescriptor<T>& pd, U && value)
		{
			//this is the normal case where we store a value
			if constexpr (std::is_convertible_v<std::decay_t<U>, T>)
			{
				Property<T>& property = getOrConstructPropertyInternal(pd);
				if (auto& propertydata = m_propertyData[&pd]; !propertydata.m_isProxyProperty)
				{
					property.set(std::forward<U>(value));
				}
				else
				{
					//this will most likely be a mistake
					//the property is a proxy property, but we call change property, which will
					//turn the proxy property into a normal property
					//if this is really intended call remove property + setProperty instead
					assert(false);
					removeProxyProperty(static_cast<ProxyProperty<T>*>(&property));
					auto& propertySharedPtr = m_propertyData[&pd].m_property;
					propertySharedPtr.reset();
					auto& newProperty = getOrConstructPropertyInternal(pd);
					newProperty.set(std::forward<U>(value));
				}
			}
			//we got a proxy property unique_ptr
			//in this case we store a proxy property that can return a value of the given type
			else if constexpr (std::is_base_of_v<ProxyProperty<T>, typename U::element_type>)
			{
				auto& propertyData = m_propertyData[&pd];
				auto proxyProperty = std::static_pointer_cast<ProxyProperty<T>>(addChildContainerInternal(std::move(value)));
				propertyData.template init<T, typename U::element_type>(std::static_pointer_cast<Property<T>>(proxyProperty), this, &pd);
				propertyData.m_isProxyProperty = true;
				static_cast<PropertyContainerBase&>(*proxyProperty).m_key = &pd;
				const T& newValue = proxyProperty->get();
				if (pd.getDefaultValue() != newValue)
				{
					setDirty(propertyData);
				}
			}
			else
				static_assert(std::is_same_v<T, void>, "The type T of the property descriptor doesn't match the type of the passed value.");
		}

		void removeProxyProperty(const PropertyContainerBase* proxyProperty)
		{
			auto ppIt = std::find_if(begin(m_children), end(m_children), [proxyProperty](const std::shared_ptr<PropertyContainerBase>& childPtr)
			{
				return childPtr.get() == proxyProperty;
			});
			if (ppIt != end(m_children))
				m_children.erase(ppIt);
		}

		template<typename T>
		void updateAllSignals(const PropertyDescriptor<T>& pd)
		{
			auto& propertyData = m_propertyData[&pd];
			propertyData.m_connectedSignals.clear();
			getAllSignals(pd, propertyData.m_connectedSignals);
		}

		void addSignal(const PropertyDescriptorBase& pd, Signal_PMF* signal)
		{
			auto& propertyData = m_propertyData[&pd];
			//TODO: check if this can be optimized
			if (std::find(begin(propertyData.m_connectedSignals), end(propertyData.m_connectedSignals), signal) == end(propertyData.m_connectedSignals))
				propertyData.m_connectedSignals.emplace_back(signal);
		}

		void addSignals(const PropertyDescriptorBase& pd, std::vector<Signal_PMF*>& connectedSignals)
		{
			auto& propertyData = m_propertyData[&pd];
			std::copy(begin(connectedSignals), end(connectedSignals), std::back_inserter(propertyData.m_connectedSignals));
		}

		void getAllSignals(const PropertyDescriptorBase& pd, std::vector<Signal_PMF*>& connectedSignals)
		{
			//add own subject
			if (auto propertyDataIt = m_propertyData.find(&pd); propertyDataIt != end(m_propertyData))
				connectedSignals.push_back(&(propertyDataIt->second.m_signal));

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
			if (!propertyData.m_property)
				return;
			//copy the old signal ptr
			auto oldSignals = propertyData.m_connectedSignals;
			auto* propertyPtr = propertyData.m_property.get();
			auto oldValue = static_cast<Property<T>*>(propertyPtr)->get();
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
                auto& removedProperty = m_removedProperties.emplace_back(std::make_unique<PropertyData>());
                removedProperty->m_property = std::make_shared<Property<T>>(pd.getDefaultValue());
                removedProperty->m_valuePtr = &(std::static_pointer_cast<Property<T>>(removedProperty->m_property)->get());
                removedProperty->m_connectedSignals = std::move(oldSignals);
			}
			//now we remove the property data
			if (propertyData.m_isProxyProperty)
				removeProxyProperty(static_cast<ProxyProperty<T>*>(propertyPtr));
			
			propertyData.m_property = nullptr;
		}

		template<typename T>
		void observePropertyInternal(const PropertyDescriptor<T>& pd, std::vector<std::function<void()>>* observers)
		{
			auto& propertyData = m_propertyData[&pd];
			propertyData.push_back(observers);
		}

		bool ownsPropertyDataInternal(const PropertyDescriptorBase& pd) const noexcept
		{
			auto it = m_propertyData.find(&pd);
			return  it != end(m_propertyData) && it->second.m_property;
		}

		template<typename T>
		void touchPropertyInternal(const PropertyDescriptor<T>& pd)
		{
			auto& propertyData = m_propertyData[&pd];
			setDirty(propertyData);
		}

		void setDirty(PropertyData& propertyData)
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
	};
		


//###########################################################################
//#
//#                        ProxyProperty Definition                             
//#
//###########################################################################

//proxy property is simply a an abstration of a property, instead of using a value
//you can use a class to calculate that value
	template<typename T>
	class ProxyProperty : public PropertyContainer, public Property<T>
	{
	public:
		using PropertyContainer::connect;
		using PropertyContainer::disconnect;
		using Property<T>::connect;
		using Property<T>::disconnect;
	};
}
