#include <gtest/gtest.h>
#include <cppproperties/PropertyContainer.h>
pd::PropertyDescriptor<int> IntPD(0);
pd::PropertyDescriptor<std::string> StringPD("Empty");
pd::PropertyDescriptor<int*> IntPtrPD(nullptr);

//###########################################################################
//#
//#                    PropertyContainer Tests     
//#                      set, change, get
//#
//###########################################################################

TEST(PropertyContainerTest, getProperty_emptyContainer_getDefaultValue)
{
	pd::PropertyContainer root;

	ASSERT_FALSE(root.hasProperty(IntPD));
	ASSERT_TRUE(root.getProperty(IntPD) == IntPD.getDefaultValue());
}

TEST(PropertyContainerTest, changeProperty_emptyContainer_noPropertyExists)
{
	pd::PropertyContainer root;
	//changing a property that hasn't been set has no effect
	root.changeProperty(IntPD, 355235);
	ASSERT_FALSE(root.hasProperty(IntPD));
}

TEST(PropertyContainerTest, changeProperty_emptyContainer_getDefaultValue)
{
	pd::PropertyContainer root;
	//changing a property that hasn't been set has no effect
	root.changeProperty(IntPD, 32535);
	ASSERT_TRUE(root.getProperty(IntPD) == IntPD.getDefaultValue());
}

TEST(PropertyContainerTest, setAndGetProperty_emptyContainer_newValue)
{
	pd::PropertyContainer root;
	root.setProperty(IntPD, 2);
	
	ASSERT_TRUE(root.hasProperty(IntPD));
	ASSERT_TRUE(root.getProperty(IntPD) == 2);
}

TEST(PropertyContainerTest, setAndGetProperty_propertyExists_newValue)
{
	pd::PropertyContainer root;
	
	root.setProperty(IntPD, 2);
	root.setProperty(IntPD, 42);
	
	ASSERT_TRUE(root.hasProperty(IntPD));
	ASSERT_TRUE(root.getProperty(IntPD) == 42);
}

TEST(PropertyContainerTest, changeProperty_propertyExists_newValue)
{
	pd::PropertyContainer root;
	root.setProperty(StringPD, "Hey there. Hello World.");

	root.changeProperty(StringPD, "Wuhu I'm here!");

	ASSERT_TRUE(root.getProperty(StringPD) == "Wuhu I'm here!");
}

TEST(PropertyContainerTest, removeProperty_containerEmpty_noCrash)
{
	pd::PropertyContainer root;
	
	root.removeProperty(IntPD);

	ASSERT_FALSE(root.hasProperty(IntPD));
}

TEST(PropertyContainerTest, removeProperty_propertyExists_defaultValue)
{
	pd::PropertyContainer root;
	root.setProperty(IntPD, 3663);
	
	root.removeProperty(IntPD);
	
	ASSERT_FALSE(root.hasProperty(IntPD));
	ASSERT_TRUE(root.getProperty(IntPD) == IntPD.getDefaultValue());
}

const auto propertyListString = { pd::PropertyDescriptor<std::string>(""), pd::PropertyDescriptor<std::string>("secondInLine\n"), pd::PropertyDescriptor<std::string>("lastOne...") };
const auto propertyListInt = { pd::PropertyDescriptor<int>(0), pd::PropertyDescriptor<int>(54), pd::PropertyDescriptor<int>(17), pd::PropertyDescriptor<int>(43356) };


TEST(PropertyContainerTest, setMultipleProperties_getProperty_newValue)
{
	pd::PropertyContainer root;
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

class SimpleIntPP : public pd::ProxyProperty<int>
{
public:
	int get() const final
	{
		return 42;
	}
};

TEST(PropertyContainerTest, testProxyProperty_setAndGetProperty_newValue)
{
	pd::PropertyContainer root;
	root.setProperty(IntPD, std::make_unique<SimpleIntPP>());

	ASSERT_TRUE(root.getProperty(IntPD) == 42);
}

TEST(PropertyContainerTest, testProxyProperty_removeProxyProperty_defaultValue)
{
	pd::PropertyContainer root;
	root.setProperty(IntPD, std::make_unique<SimpleIntPP>());
	root.removeProperty(IntPD);

	ASSERT_TRUE(root.getProperty(IntPD) == IntPD.getDefaultValue());
}

TEST(PropertyContainerTest, testProxyProperty_removeProxyProperty_containerEmpty)
{
	pd::PropertyContainer root;
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

const pd::PropertyDescriptor<std::string> StringResultPD("");
TEST(CppPropertiesTest, makeProxyProperty_defaultInput_defaultOut)
{
	pd::PropertyContainer root;

	auto intStringLambda = [](int i, std::string s)
	{
		return s + ": " + std::to_string(i);
	};
	auto proxyP = pd::make_proxy_property(intStringLambda, IntPD, StringPD);
	root.setProperty(StringResultPD, std::move(proxyP));

	ASSERT_TRUE(root.getProperty(StringResultPD) == intStringLambda(IntPD.getDefaultValue(), StringPD.getDefaultValue()));
}
const pd::PropertyDescriptor<bool> StringContainsHelloPD(false);
TEST(CppPropertiesTest, makeProxyProperty_matchString_findHello)
{
	pd::PropertyContainer root;
	auto findString = [matchString = "Hello"](const std::string& source)
	{
		return source.find(matchString) != std::string::npos;
	};
	auto proxyP = pd::make_proxy_property(findString, StringPD);
	root.setProperty(StringContainsHelloPD, std::move(proxyP));
	ASSERT_FALSE(root.getProperty(StringContainsHelloPD));
	
	root.setProperty(StringPD, "Hello World!");
	ASSERT_TRUE(root.getProperty(StringContainsHelloPD));
}


class IntPP : public pd::ProxyProperty<int>
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
	pd::PropertyContainer rootContainer;
	rootContainer.setProperty(StringPD, "Am I propagated to all children?");
	auto containerA = rootContainer.addChildContainer<pd::PropertyContainer>();
	auto containerA1 = containerA->addChildContainer<pd::PropertyContainer>();
	auto containerA2 = containerA->addChildContainer<pd::PropertyContainer>();
	auto containerA2A = containerA2->addChildContainer<pd::PropertyContainer>();
	auto containerA2B = containerA2->addChildContainer<pd::PropertyContainer>();
	auto containerB = rootContainer.addChildContainer<pd::PropertyContainer>();

	for (auto& container : { containerA , containerA1, containerA2, containerA2A, containerA2B, containerB })
		ASSERT_TRUE(container->getProperty(StringPD) == "Am I propagated to all children?");
}

TEST(CppPropertiesTest, TestHierarchies_setAfterRoot_notVisibleForParents)
{
	pd::PropertyContainer rootContainer;

	auto containerA = rootContainer.addChildContainer<pd::PropertyContainer>();
	auto containerA1 = containerA->addChildContainer<pd::PropertyContainer>();
	
	auto containerA2 = containerA->addChildContainer<pd::PropertyContainer>();
	containerA2->setProperty(StringPD, "Am I propagated to all children?");
	auto containerA2A = containerA2->addChildContainer<pd::PropertyContainer>();
	auto containerA2B = containerA2->addChildContainer<pd::PropertyContainer>();
	
	auto containerB = rootContainer.addChildContainer<pd::PropertyContainer>();

	for (auto& container : { containerA2, containerA2A, containerA2B })
		ASSERT_TRUE(container->getProperty(StringPD) == "Am I propagated to all children?");

	for (auto& container : { &rootContainer, containerA, containerA1, containerB })
		ASSERT_FALSE(container->getProperty(StringPD) == "Am I propagated to all children?");

}

TEST(CppPropertiesTest, TestHierarchies_setDifferentValues_onlyVisibleUntilSet)
{
	pd::PropertyContainer rootContainer;
	
	auto containerA = rootContainer.addChildContainer<pd::PropertyContainer>();
	auto containerA1 = containerA->addChildContainer<pd::PropertyContainer>();
	
	auto containerA2 = containerA->addChildContainer<pd::PropertyContainer>();
	auto containerA2A = containerA2->addChildContainer<pd::PropertyContainer>();
	auto containerA2B = containerA2->addChildContainer<pd::PropertyContainer>();
	auto containerB = rootContainer.addChildContainer<pd::PropertyContainer>();

	containerA2->setProperty(StringPD, "A2 String!");
	rootContainer.setProperty(StringPD, "Root String!");

	for (auto& container : { &rootContainer, containerA, containerA1, containerB})
		ASSERT_TRUE(container->getProperty(StringPD) == "Root String!");

	for (auto& container : { containerA2, containerA2A, containerA2B })
		ASSERT_TRUE(container->getProperty(StringPD) == "A2 String!");

}
struct TestStruct
{
	void func() { std::cout << "Hello World!"; };
};

TEST(CppPropertiesTest, TestSubjectObserver)
{
	pd::PropertyContainerBase rootContainer;
	int observerFuncCalledCount = 0;
	auto observerFunc = [&observerFuncCalledCount]() { observerFuncCalledCount++; };
	rootContainer.connect(IntPD, observerFunc);
	rootContainer.setProperty(IntPD, 5);
	rootContainer.emit();
	ASSERT_TRUE(observerFuncCalledCount == 1);
	rootContainer.changeProperty(IntPD, 6);
	rootContainer.emit();
	ASSERT_TRUE(observerFuncCalledCount == 2);
	auto containerA = rootContainer.addChildContainer<pd::PropertyContainer>();
	auto containerA1 = containerA->addChildContainer<pd::PropertyContainer>();
	containerA1->connect(IntPD, observerFunc);
	containerA1->setProperty(IntPD, 10);
	rootContainer.emit();
	containerA1->removeProperty(IntPD);
	rootContainer.emit();
	ASSERT_TRUE(observerFuncCalledCount == 4);
	//here we test if we get 2 updates with the emit false option
	rootContainer.changeProperty(IntPD, IntPD.getDefaultValue());
	rootContainer.emit(false);
	ASSERT_TRUE(observerFuncCalledCount == 6);
	//the value doesn't change, so we will not trigger an update
	rootContainer.removeProperty(IntPD);
	rootContainer.emit();
	ASSERT_TRUE(observerFuncCalledCount == 6);
}

TEST(CppPropertiesTest, TestPMFOnlyAddedOnce)
{
	IntPP rootContainer;
	rootContainer.setProperty(IntPD, 0);
	rootContainer.connect(IntPD, &IntPP::dirtyIntFunc);
	rootContainer.connect(IntPD, &IntPP::dirtyIntFunc);
	rootContainer.connect(IntPD, &IntPP::dirtyIntFunc);
	rootContainer.changeProperty(IntPD, 1);
	rootContainer.emit();
	ASSERT_TRUE(rootContainer.dirtyInt == 1);
	rootContainer.changeProperty(IntPD, 2);
	rootContainer.emit();
	ASSERT_TRUE(rootContainer.dirtyInt == 3);
}


int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
