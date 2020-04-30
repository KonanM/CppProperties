#include <gtest/gtest.h>
#include <cppproperties/PropertyContainer.h>
#include <cppproperties/ProxyProperty.h>

//###########################################################################
//#
//#                    PropertyContainer Tests       
//#							Proxy Properties
//#
//###########################################################################

namespace 
{
	ps::PropertyDescriptor<int> IntPD(0);
	ps::PropertyDescriptor<std::string> StringPD("Empty");
}


class SimpleIntPP : public ps::ProxyProperty<int>
{
public:
	SimpleIntPP()
	{
		set(42);
	}
};


TEST(PropertyContainerTest, testProxyProperty_setAndGetProperty_newValue)
{
	ps::PropertyContainer root;
	root.setProperty(IntPD, std::make_unique<SimpleIntPP>());

	ASSERT_EQ(root.getProperty(IntPD), 42);
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

TEST(PropertyContainerTest, copyConstructor_testProxyProperty)
{
	auto root = std::make_unique<ps::PropertyContainer> ();
	root->setProperty(IntPD, std::make_unique<SimpleIntPP>());
	root->setProperty(StringPD, "Am I copied?");
	
	ps::PropertyContainer copy(*root);
	//delete the orignal container
	root = nullptr;
	copy.emit();
	ASSERT_TRUE(copy.getProperty(IntPD) ==  42);
	ASSERT_TRUE(copy.getProperty(StringPD) == "Am I copied?");
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
	auto findString = [matchString = "Hello"](const std::string & source)
	{
		return source.find(matchString) != std::string::npos;
	};
	auto proxyP = ps::make_proxy_property(findString, StringPD);
	root.setProperty(StringContainsHelloPD, std::move(proxyP));
	ASSERT_FALSE(root.getProperty(StringContainsHelloPD).get());

	root.setProperty(StringPD, "Hello World!");
	root.emit();
	ASSERT_TRUE(root.getProperty(StringContainsHelloPD));
}

TEST(CppPropertiesTest, makeProxyProperty_copyContainer_copiedCorrectly)
{
	ps::PropertyContainer root;
	auto findString = [matchString = "Hello"](const std::string& source)
	{
		return source.find(matchString) != std::string::npos;
	};
	auto proxyP = ps::make_proxy_property(findString, StringPD);
	root.setProperty(StringContainsHelloPD, std::move(proxyP));
	ps::PropertyContainer copy(root);
	copy.setProperty(StringPD, "Hello World!");
	copy.emit();
	auto* ppPtr = copy.getProxyProperty(StringContainsHelloPD);
	ASSERT_TRUE(copy.getProperty(StringContainsHelloPD));
	ASSERT_TRUE(ppPtr->get());
}


