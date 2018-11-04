#include <gtest/gtest.h>
#include <cppproperties/PropertyContainer.h>
pd::PropertyDescriptor<int> IntPD(0);
pd::PropertyDescriptor<std::string> StringPD("Empty");
pd::PropertyDescriptor<int*> IntPtrPD(nullptr);


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

const pd::PropertyDescriptor<std::string> StringResultPD("");
TEST(CppPropertiesTest, TestSimpleProxyProperty)
{	
	pd::PropertyContainer container;
	auto intStringLambda = [](int i, std::string s)
	{ 
		return s + ": " + std::to_string(i);
	};
	auto proxyP = pd::make_proxy_property(intStringLambda, IntPD, StringPD);
	container.setProperty(StringResultPD, std::move(proxyP));
	ASSERT_TRUE(container.getProperty(StringResultPD) == intStringLambda(IntPD.getDefaultValue(), StringPD.getDefaultValue()));
	container.setProperty(IntPD, 20);
	ASSERT_TRUE(container.getProperty(StringResultPD) == intStringLambda(20, StringPD.getDefaultValue()));
	container.setProperty(StringPD, "something");
	ASSERT_TRUE(container.getProperty(StringResultPD) == intStringLambda(20, "something"));
}

TEST(CppPropertiesTest, TestCoreFunctionality)
{
	pd::PropertyContainer container;
	container.setProperty(IntPD, 1);

	auto val = container.getProperty(IntPD);
	ASSERT_TRUE(val == 1);
	container.changeProperty(IntPD, 10);

	auto valNew = container.getProperty(IntPD);
	ASSERT_TRUE(valNew == 10);
	int i = 11;
	container.setProperty(IntPtrPD, &i);
	auto&& intPtr = container.getProperty(IntPtrPD);
	ASSERT_TRUE(intPtr == &i && *intPtr == i);
	auto newPP = std::make_unique<IntPP>();
	auto newPPPtr = newPP.get();
	container.setProperty(IntPD, std::move(newPP));
	ASSERT_TRUE(container.getProperty(IntPD) == 42);

	ASSERT_TRUE(container.getProxyProperty(IntPD) == newPPPtr);
}

TEST(CppPropertiesTest, TestHierarchies)
{
	pd::PropertyContainer rootContainer;
	rootContainer.setProperty(StringPD, "Am I propagated to all children?");
	auto rootContainerPtr = &rootContainer;
	auto containerA = rootContainer.addChildContainer(std::make_unique<pd::PropertyContainer>());
	auto containerA1 = containerA->addChildContainer(std::make_unique<pd::PropertyContainer>());
	auto containerA2 = containerA->addChildContainer(std::make_unique<pd::PropertyContainer>());
	auto containerA2A = containerA2->addChildContainer(std::make_unique<pd::PropertyContainer>());
	auto containerA2B = containerA2->addChildContainer(std::make_unique<pd::PropertyContainer>());
	auto containerB = rootContainer.addChildContainer(std::make_unique<pd::PropertyContainer>());

	for (auto& container : { containerA , containerA1, containerA2, containerA2A, containerA2B, containerB })
		ASSERT_TRUE(container->getProperty(StringPD) == "Am I propagated to all children?");

	containerA2->setProperty(IntPD, 13);
	int valueA2 = containerA2B->getProperty(IntPD);
	ASSERT_TRUE(valueA2 == 13);
	ASSERT_TRUE(!rootContainer.hasProperty(IntPD) && rootContainer.getProperty(IntPD) == IntPD.getDefaultValue());
	rootContainer.setProperty(IntPD, 42);
	int valueA = containerA->getProperty(IntPD);
	valueA2 = containerA2->getProperty(IntPD);
	ASSERT_TRUE(valueA == 42 && valueA2 == 13);

	containerA2B->changeProperty(IntPD, 45);
	ASSERT_TRUE(containerA2A->getProperty(IntPD) == 45 && rootContainer.getProperty(IntPD) == 42);

	int oldA2value = containerA2->getProperty(IntPD);
	containerA2->removeProperty(IntPD);

	int newA2Value = containerA2->getProperty(IntPD);
	ASSERT_TRUE(newA2Value == 42);
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
	auto containerA = rootContainer.addChildContainer(std::make_unique<pd::PropertyContainer>());
	auto containerA1 = containerA->addChildContainer(std::make_unique<pd::PropertyContainer>());
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
