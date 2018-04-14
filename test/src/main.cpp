#include <gtest/gtest.h>
#include <cppproperties/PropertyContainer.h>
pd::PropertyDescriptor<int> IntPD(0);
pd::PropertyDescriptor<std::string> StringPD("Empty");
pd::PropertyDescriptor<int*> IntPtrPD(nullptr);

class IntPP : public pd::ProxyProperty<int>
{
public:
	int get() override
	{
		return 42;
	}
	IntPP* clone() override
	{
		return new IntPP();
	}
};

const pd::PropertyDescriptor<int> IntPPPD(0);
TEST(CppPropertiesTest, TestSimpleProxyProperty)
{
	
	pd::PropertyContainer<> container;
	auto intStringLambda = [](int i, std::string s)
	{ return s + ": " + std::to_string(i);
	};
	auto proxyP = pd::makeProxyProperty(intStringLambda, IntPD, StringPD);
	container.setProperty(IntPPPD, std::move(proxyP));
	ASSERT_TRUE(container.getProperty(IntPPPD) == IntPPPD.getDefaultValue());
	container.setProperty(IntPD, 20);
	ASSERT_TRUE(container.getProperty(IntPPPD) == 20);
	container.setProperty(StringPD, "something");
	ASSERT_TRUE(container.getProperty(IntPPPD) == 20);
}

TEST(CppPropertiesTest, TestCoreFunctionality)
{
	pd::PropertyContainer<> container;
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
	pd::PropertyContainer<> rootContainer;
	rootContainer.setProperty(StringPD, "Am I propagated to all children?");
	auto rootContainerPtr = &rootContainer;
	auto containerA = rootContainer.addChildContainer(std::make_unique<pd::PropertyContainer<>>());
	auto containerA1 = containerA->addChildContainer(std::make_unique<pd::PropertyContainer<>>());
	auto containerA2 = containerA->addChildContainer(std::make_unique<pd::PropertyContainer<>>());
	auto containerA2A = containerA2->addChildContainer(std::make_unique<pd::PropertyContainer<>>());
	auto containerA2B = containerA2->addChildContainer(std::make_unique<pd::PropertyContainer<>>());
	auto containerB = rootContainer.addChildContainer(std::make_unique<pd::PropertyContainer<>>());

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

TEST(CppPropertiesTest, TestSubjectObserver)
{
	pd::PropertyContainer<> rootContainer;
	int observerFuncCalledCount = 0;
	auto observerFunc = [&observerFuncCalledCount]() { observerFuncCalledCount++; };
	rootContainer.observeProperty(IntPD, observerFunc);
	rootContainer.setProperty(IntPD, 5);
	ASSERT_TRUE(observerFuncCalledCount == 1);
	rootContainer.changeProperty(IntPD, 6);
	ASSERT_TRUE(observerFuncCalledCount == 2);
	auto containerA = rootContainer.addChildContainer(std::make_unique<pd::PropertyContainer<>>());
	auto containerA1 = containerA->addChildContainer(std::make_unique<pd::PropertyContainer<>>());
	containerA1->observeProperty(IntPD, observerFunc);
	containerA1->setProperty(IntPD, 10);
	containerA1->removeProperty(IntPD);
	ASSERT_TRUE(observerFuncCalledCount == 4);

	rootContainer.changeProperty(IntPD, IntPD.getDefaultValue());
	ASSERT_TRUE(observerFuncCalledCount == 6);
	//the value doesn't change, so we will not trigger an update
	rootContainer.removeProperty(IntPD);
	ASSERT_TRUE(observerFuncCalledCount == 6);
}



int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
