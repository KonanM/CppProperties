#include <gtest/gtest.h>
#include <cppproperties/PropertyContainer.h>

//###########################################################################
//#
//#                    PropertyContainer Tests       
//#						 Container Hierarchy
//#
//###########################################################################

namespace
{
	ps::PropertyDescriptor<int> IntPD(0);
	ps::PropertyDescriptor<std::string> StringPD("Empty");

	class IntPP : public ps::ProxyProperty<int>
	{
	public:
		IntPP()
		{
			set(42);
		}
		void dirtyFunc()
		{
			isDirty = true;
		}
		void dirtyIntFunc(int i)
		{
			dirtyInt += i;
		}

		bool isDirty = false;
		int dirtyInt = 0;
	};
}




TEST(CppPropertiesTest, TestHierarchies_setAtRoot_visibleAtChildren)
{
	ps::PropertyContainer rootContainer;
	rootContainer.setProperty(StringPD, "Am I propagated to all children?");
	auto& containerA = rootContainer.addChildContainer<ps::PropertyContainer>();
	auto& containerA1 = containerA.addChildContainer<ps::PropertyContainer>();
	auto& containerA2 = containerA.addChildContainer<ps::PropertyContainer>();
	auto& containerA2A = containerA2.addChildContainer<ps::PropertyContainer>();
	auto& containerA2B = containerA2.addChildContainer<ps::PropertyContainer>();
	auto& containerB = rootContainer.addChildContainer<ps::PropertyContainer>();

	for (auto& container : { containerA , containerA1, containerA2, containerA2A, containerA2B, containerB })
		ASSERT_TRUE(container.getProperty(StringPD) == "Am I propagated to all children?");
}

TEST(CppPropertiesTest, TestHierarchies_setDifferentValues_onlyVisibleUntilSet)
{
	ps::PropertyContainer rootContainer;

	auto& containerA = rootContainer.addChildContainer<ps::PropertyContainer>();
	auto& containerA1 = containerA.addChildContainer<ps::PropertyContainer>();

	auto& containerA2 = containerA.addChildContainer<ps::PropertyContainer>();
	auto& containerA2A = containerA2.addChildContainer<ps::PropertyContainer>();
	auto& containerA2B = containerA2.addChildContainer<ps::PropertyContainer>();
	auto& containerB = rootContainer.addChildContainer<ps::PropertyContainer>();

	containerA2.setProperty(StringPD, "A2 String!");
	rootContainer.setProperty(StringPD, "Root String!");

	for (auto& container : { rootContainer, containerA, containerA1, containerB })
		ASSERT_TRUE(container.getProperty(StringPD) == "Root String!");

	for (auto& container : { containerA2, containerA2A, containerA2B })
		ASSERT_TRUE(container.getProperty(StringPD) == "A2 String!");

}

TEST(CppPropertiesTest, TestHierarchies_removeProperty_parentPropertyVisible)
{
	ps::PropertyContainer rootContainer;
	auto& containerA = rootContainer.addChildContainer<ps::PropertyContainer>();
	auto& containerA1 = containerA.addChildContainer<ps::PropertyContainer>();

	containerA.setProperty(StringPD, "Container A!");
	rootContainer.setProperty(StringPD, "Root String!");

	containerA.removeProperty(StringPD);

	ASSERT_TRUE(containerA1.getProperty(StringPD) == "Root String!");
}

TEST(CppPropertiesTest, TestHierarchies_changeParentProperty_NotVisibleForChildren)
{
	ps::PropertyContainer rootContainer;
	auto& containerA = rootContainer.addChildContainer<ps::PropertyContainer>();
	auto& containerA1 = containerA.addChildContainer<ps::PropertyContainer>();

	containerA.setProperty(StringPD, "Container A!");
	rootContainer.setProperty(StringPD, "Root String!");

	rootContainer.changeProperty(StringPD, "New Root Value");

	ASSERT_TRUE(containerA1.getProperty(StringPD) == "Container A!");
}

//###########################################################################
//#
//#                    PropertyContainer Tests       
//#					   Signals and connections
//#
//###########################################################################

TEST(CppPropertiesTest, TestSignals_connectAndChangeProperty_lambdaCallBack)
{
	ps::PropertyContainer rootContainer;
	int observerFuncCalledCount = 0;
	auto observerFunc = [&observerFuncCalledCount]() { observerFuncCalledCount++; };

	rootContainer.connect(IntPD, observerFunc);
	rootContainer.setProperty(IntPD, 5);
	rootContainer.emit();
	rootContainer.changeProperty(IntPD, 6);
	rootContainer.emit();

	ASSERT_EQ(observerFuncCalledCount, 2);
}

TEST(CppPropertiesTest, TestSignals_connectToParentProperty_lambdaCallBack)
{
	ps::PropertyContainer rootContainer;
	auto& childContainer = rootContainer.addChildContainer<ps::PropertyContainer>();
	int observerFuncCalledCount = 0;
	auto observerFunc = [&observerFuncCalledCount]() { observerFuncCalledCount++; };

	childContainer.connect(IntPD, observerFunc);
	rootContainer.setProperty(IntPD, 5);
	rootContainer.emit();
	rootContainer.changeProperty(IntPD, 6);
	rootContainer.emit();

	ASSERT_EQ(observerFuncCalledCount, 2);
}

TEST(CppPropertiesTest, TestSignals_removeProperty_lambdaCallBack)
{
	ps::PropertyContainer rootContainer;
	auto& childContainer = rootContainer.addChildContainer<ps::PropertyContainer>();
	auto& childChildContainer = childContainer.addChildContainer<ps::PropertyContainer>();

	int observerFuncCalledCount = 0;
	auto observerFunc = [&observerFuncCalledCount]() { observerFuncCalledCount++; };
	childChildContainer.connect(IntPD, observerFunc);

	//first trigger
	rootContainer.setProperty(IntPD, 5);
	rootContainer.emit();

	//second trigger
	childContainer.setProperty(IntPD, 10);
	rootContainer.emit();

	//value doesn't change, since it's still set on the child container
	rootContainer.removeProperty(IntPD);
	rootContainer.emit();

	ASSERT_EQ(observerFuncCalledCount, 2);

	childContainer.removeProperty(IntPD);
	rootContainer.emit();

	ASSERT_EQ(observerFuncCalledCount, 3);
}

TEST(CppPropertiesTest, TestSignals_AddMultiplePMF_OnlyAddedOnce)
{
	IntPP rootContainer;
	rootContainer.setProperty(IntPD, 0);
	rootContainer.connect(IntPD, &IntPP::dirtyIntFunc);
	rootContainer.connect(IntPD, &IntPP::dirtyIntFunc);
	rootContainer.connect(IntPD, &IntPP::dirtyIntFunc);

	rootContainer.changeProperty(IntPD, 1);
	rootContainer.emit();

	ASSERT_EQ(rootContainer.dirtyInt, 1);
}

TEST(CppPropertiesTest, TestSignals_connectToVar_varTakesValue)
{
	IntPP rootContainer;
	rootContainer.setProperty(IntPD, 0);
	rootContainer.connectToVar(IntPD, rootContainer.dirtyInt);

	rootContainer.changeProperty(IntPD, 3234);
	rootContainer.emit();

	ASSERT_EQ(rootContainer.dirtyInt, 3234);
}

TEST(CppPropertiesTest, TestSignals_disconnectSingle_NoUpdateCall)
{
	ps::PropertyContainer rootContainer;
	rootContainer.setProperty(IntPD, 0);
	int localInt = 42;
	auto idx = rootContainer.connectToVar(IntPD, localInt);

	rootContainer.changeProperty(IntPD, 3234);
	rootContainer.disconnect(IntPD, idx);
	rootContainer.emit();

	ASSERT_EQ(localInt, 42);
}

TEST(CppPropertiesTest, TestSignals_disconnectAll_NoUpdateCall)
{
	IntPP rootContainer;
	rootContainer.setProperty(IntPD, 0);
	int localInt = 42;
	rootContainer.connectToVar(IntPD, localInt);
	rootContainer.connectToVar(IntPD, rootContainer.dirtyInt);

	rootContainer.changeProperty(IntPD, 3234);
	rootContainer.disconnect(IntPD);
	rootContainer.emit();

	ASSERT_NE(rootContainer.dirtyInt, 3234);
	ASSERT_EQ(localInt, 42);
}

