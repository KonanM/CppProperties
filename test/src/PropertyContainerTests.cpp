#include <gtest/gtest.h>
#include <cppproperties/PropertyContainer.h>

namespace
{
	ps::PropertyDescriptor<int> IntPD(0);
	ps::PropertyDescriptor<std::string> StringPD("Empty");
}

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
	ASSERT_EQ(root.getProperty(IntPD), 2);
}

TEST(PropertyContainerTest, setAndGetProperty_propertyExists_newValue)
{
	ps::PropertyContainer root;

	root.setProperty(IntPD, 2);
	root.setProperty(IntPD, 42);

	ASSERT_TRUE(root.hasProperty(IntPD));
	ASSERT_EQ(root.getProperty(IntPD), 42);
}

TEST(PropertyContainerTest, changeProperty_propertyExists_newValue)
{
	ps::PropertyContainer root;
	root.setProperty(StringPD, "Hey there. Hello World.");

	root.changeProperty(StringPD, "Wuhu I'm here!");

	ASSERT_EQ(root.getProperty(StringPD), "Wuhu I'm here!");
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
		root.setProperty(pd, pd.getDefaultValue() * 2);
	for (auto& pd : propertyListString)
		root.setProperty(pd, pd.getDefaultValue().get() + newString);

	for (auto& pd : propertyListString)
		ASSERT_TRUE(root.getProperty(pd) == pd.getDefaultValue().get() + newString);
	for (auto& pd : propertyListInt)
		ASSERT_TRUE(root.getProperty(pd) == pd.getDefaultValue().get() * 2);
}