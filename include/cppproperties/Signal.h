#pragma once
#include <unordered_map>
#include <unordered_set>
#include <type_traits>
#include <typeindex>
#include <functional>

namespace ps
{
	//###########################################################################
	//#
	//#                        Signal                            
	//#
	//############################################################################


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
		using member_type = U;
		using class_type = T;
	};

	//specialized signal implementation suited to our needs
	//it handles class ptr + PMF (with or without argument)
	//it handles callable funtors (with or without argument)
	//the underlying storage uses std::function<void()>
	//and a specialized lambda funtion to abstract the different cases
	//another requirement is that you can't add the same PMF twice
	template<typename... Args>
	class Signal
	{
	protected:
		std::unordered_map<size_t, std::function<void(Args...)>> m_slots;
		size_t m_index = 0;
	public:
		Signal() = default;

		bool empty() const noexcept
		{
			return m_slots.empty();
		}

		template<typename FuncT>
		size_t connect(FuncT&& func) noexcept
		{
			if constexpr (std::is_invocable_v<FuncT, void>)
			{
				m_slots.try_emplace(m_index, [func = std::forward<FuncT>(func)](Args...)
				{
					func();
				});
			}
			else
			{
				m_slots.try_emplace(m_index, std::forward<FuncT>(func));
			}

			return m_index++;
		}

		// disconnects all previously connected functions
		void disconnect()
		{
			m_slots.clear();
		}
		//disconnects the function with the given type index
		void disconnect(size_t idx)
		{
			m_slots.erase(idx);
		}

		// calls all connected functions
		void emit(Args... args) const
		{
			for (auto& [typeID, slot] : m_slots)
				slot(args...);
		}
	};

	class Signal_PMF : public Signal<void*, const void*>
	{
	public:
		//// connects a member function to this signal
		template <typename T, typename pmfT>
		size_t connectPMF(pmfT&& func) noexcept
		{
			using PMF = PMF_traits<pmfT>;
			size_t hashVal = std::hash<std::type_index>{}(std::type_index(typeid(pmfT)));
			if constexpr (std::is_same_v<T, void>)
			{
				m_slots.try_emplace(hashVal, [func](void* inst, const void*)
					{
						(static_cast<typename PMF::class_type*>(inst)->*func)();
					});
			}
			else
			{
				m_slots.try_emplace(hashVal, [func](void* inst, const void* valPtr)
					{
						(static_cast<typename PMF::class_type*>(inst)->*func)(*static_cast<const T*>(valPtr));
					});
			}

			return hashVal;
		}

        void emitUnique(void* inst, const void* value, std::unordered_set<size_t>& alreadyInvoked)
        {
            for (auto& [typeID, slot] : m_slots)
                if (auto it = alreadyInvoked.find(typeID); it == alreadyInvoked.end())
                {
                    slot(inst, value);
                    alreadyInvoked.emplace_hint(it, typeID);
                }
        }
	};
}
