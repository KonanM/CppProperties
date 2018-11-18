#include <gtest/gtest.h>
#include <cppproperties/PropertyContainer.h>
ps::PropertyDescriptor<int> IntPD(0);
ps::PropertyDescriptor<std::string> StringPD("Empty");
ps::PropertyDescriptor<int*> IntPtrPD(nullptr);

//###########################################################################
//#
//#                    PropertyContainer Tests     
//#                      set, change, get
//#
//###########################################################################

TEST(PropertyContainerTest, getProperty_emptyContainer_getDefaultValue)
{
	ps::PropertyContainer root;

	ASSERT_FALSE(root.hasProperty(IntPD));
	ASSERT_TRUE(root.getProperty(IntPD) == IntPD.getDefaultValue());
}

TEST(PropertyContainerTest, changeProperty_emptyContainer_noPropertyExists)
{
	ps::PropertyContainer root;
	//changing a property that hasn't been set has no effect
	root.changeProperty(IntPD, 355235);
	ASSERT_FALSE(root.hasProperty(IntPD));
}

TEST(PropertyContainerTest, changeProperty_emptyContainer_getDefaultValue)
{
	ps::PropertyContainer root;
	//changing a property that hasn't been set has no effect
	root.changeProperty(IntPD, 32535);
	ASSERT_TRUE(root.getProperty(IntPD) == IntPD.getDefaultValue());
}

TEST(PropertyContainerTest, setAndGetProperty_emptyContainer_newValue)
{
	ps::PropertyContainer root;
	root.setProperty(IntPD, 2);
	
	ASSERT_TRUE(root.hasProperty(IntPD));
	ASSERT_TRUE(root.getProperty(IntPD) == 2);
}

TEST(PropertyContainerTest, setAndGetProperty_propertyExists_newValue)
{
	ps::PropertyContainer root;
	
	root.setProperty(IntPD, 2);
	root.setProperty(IntPD, 42);
	
	ASSERT_TRUE(root.hasProperty(IntPD));
	ASSERT_TRUE(root.getProperty(IntPD) == 42);
}

TEST(PropertyContainerTest, changeProperty_propertyExists_newValue)
{
	ps::PropertyContainer root;
	root.setProperty(StringPD, "Hey there. Hello World.");

	root.changeProperty(StringPD, "Wuhu I'm here!");

	ASSERT_TRUE(root.getProperty(StringPD) == "Wuhu I'm here!");
}

TEST(PropertyContainerTest, removeProperty_containerEmpty_noCrash)
{
	ps::PropertyContainer root;
	
	root.removeProperty(IntPD);

	ASSERT_FALSE(root.hasProperty(IntPD));
}

TEST(PropertyContainerTest, removeProperty_propertyExists_defaultValue)
{
	ps::PropertyContainer root;
	root.setProperty(IntPD, 3663);
	
	root.removeProperty(IntPD);
	
	ASSERT_FALSE(root.hasProperty(IntPD));
	ASSERT_TRUE(root.getProperty(IntPD) == IntPD.getDefaultValue());
}

const auto propertyListString = { ps::PropertyDescriptor<std::string>(""), ps::PropertyDescriptor<std::string>("secondInLine\n"), ps::PropertyDescriptor<std::string>("lastOne...") };
const auto propertyListInt = { ps::PropertyDescriptor<int>(0), ps::PropertyDescriptor<int>(54), ps::PropertyDescriptor<int>(17), ps::PropertyDescriptor<int>(43356) };


TEST(PropertyContainerTest, setMultipleProperties_getProperty_newValue)
{
	ps::PropertyContainer root;
	std::string newString = ". Let's append something new.";

	for (auto& pd : propertyListInt)
		root.setProperty(pd, pd.getDefaultValue() * 2 );
	for (auto& pd : propertyListString)
		root.setProperty(pd, pd.getDefaultValue() + newString);

	for (auto& pd : propertyListString)
		ASSERT_TRUE(root.getProperty(pd) == pd.getDefaultValue() + newString);
	for (auto& pd : propertyListInt)
		ASSERT_TRUE(root.getProperty(pd) == pd.getDefaultValue() * 2);
}

//###########################################################################
//#
//#                    PropertyContainer Tests       
//#							Proxy Properties
//#
//###########################################################################

class SimpleIntPP : public ps::ProxyProperty<int>
{
public:
	int get() const final
	{
		return 42;
	}
};

TEST(PropertyContainerTest, testProxyProperty_setAndGetProperty_newValue)
{
	ps::PropertyContainer root;
	root.setProperty(IntPD, std::make_unique<SimpleIntPP>());

	ASSERT_TRUE(root.getProperty(IntPD) == 42);
}

TEST(PropertyContainerTest, testProxyProperty_removeProxyProperty_defaultValue)
{
	ps::PropertyContainer root;
	root.setProperty(IntPD, std::make_unique<SimpleIntPP>());
	root.removeProperty(IntPD);

	ASSERT_TRUE(root.getProperty(IntPD) == IntPD.getDefaultValue());
}

TEST(PropertyContainerTest, testProxyProperty_removeProxyProperty_containerEmpty)
{
	ps::PropertyContainer root;
	root.setProperty(IntPD, std::make_unique<SimpleIntPP>());
	root.removeProperty(IntPD);

	ASSERT_FALSE(root.hasProperty(IntPD));
}

//###########################################################################
//#
//#                    PropertyContainer Tests       
//#						   Proxy Properties
//#						  make_proxy_property
//#
//###########################################################################

const ps::PropertyDescriptor<std::string> StringResultPD("");
TEST(CppPropertiesTest, makeProxyProperty_defaultInput_defaultOut)
{
	ps::PropertyContainer root;

	auto intStringLambda = [](int i, std::string s)
	{
		return s + ": " + std::to_string(i);
	};
	auto proxyP = ps::make_proxy_property(intStringLambda, IntPD, StringPD);
	root.setProperty(StringResultPD, std::move(proxyP));

	ASSERT_TRUE(root.getProperty(StringResultPD) == intStringLambda(IntPD.getDefaultValue(), StringPD.getDefaultValue()));
}
const ps::PropertyDescriptor<bool> StringContainsHelloPD(false);
TEST(CppPropertiesTest, makeProxyProperty_matchString_findHello)
{
	ps::PropertyContainer root;
	auto findString = [matchString = "Hello"](const std::string& source)
	{
		return source.find(matchString) != std::string::npos;
	};
	auto proxyP = ps::make_proxy_property(findString, StringPD);
	root.setProperty(StringContainsHelloPD, std::move(proxyP));
	ASSERT_FALSE(root.getProperty(StringContainsHelloPD));
	
	root.setProperty(StringPD, "Hello World!");
	ASSERT_TRUE(root.getProperty(StringContainsHelloPD));
}


class IntPP : public ps::ProxyProperty<int>
{
public:
	int get() const final
	{
		return 42;
	}
	void dirtyFunc()
	{
		isDirty = true;
	}
	void dirtyIntFunc(int i)
	{
		dirtyInt+=i;
	}
	std::unique_ptr<ProxyPropertyBase> clone() override
	{
		return std::make_unique<IntPP>(*this);
	}
	bool isDirty = false;
	int dirtyInt = 0;
};

//###########################################################################
//#
//#                    PropertyContainer Tests       
//#						 Container Hierarchy
//#
//###########################################################################

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
	
	ASSERT_TRUE(observerFuncCalledCount == 2);
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
	
	ASSERT_TRUE(observerFuncCalledCount == 2);
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

	ASSERT_TRUE(observerFuncCalledCount == 2);
	
	childContainer.removeProperty(IntPD);
	rootContainer.emit();

	ASSERT_TRUE(observerFuncCalledCount == 3);
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

	ASSERT_TRUE(rootContainer.dirtyInt == 1);
}

TEST(CppPropertiesTest, TestSignals_connectToVar_varTakesValue)
{
	IntPP rootContainer;
	rootContainer.setProperty(IntPD, 0);
	rootContainer.connectToVar(IntPD, rootContainer.dirtyInt);

	rootContainer.changeProperty(IntPD, 3234);
	rootContainer.emit();

	ASSERT_TRUE(rootContainer.dirtyInt == 3234);
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

	ASSERT_TRUE(localInt == 42);
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

	ASSERT_TRUE(rootContainer.dirtyInt != 3234);
	ASSERT_TRUE(localInt == 42);
}


int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
