#include <gtest/gtest.h>
#include <cppproperties/PropertyContainer.h>
#include <cppproperties/ProxyProperty.h>
namespace
{
	ps::PropertyDescriptor<int> IntPD(0);
	ps::PropertyDescriptor<std::string> StringPD("Empty");
}

ps::PropertyDescriptor<int*> IntPtrPD(nullptr);

//###########################################################################
//#
//#                    Property Tests     
//#                   set, change, get
//#
//###########################################################################

TEST(PropertyTest, getAndSet_value42_get42)
{
	ps::Property<int> intP(42);
	ASSERT_EQ(intP, 42);
}

TEST(PropertyTest, onPropertyChanged_connectWithLambda_applyNewValue)
{
	ps::Property<int> intP(42);
	int newVal;

	//implementation of on property changed
	intP += [&newVal](int newValue) {newVal = newValue;};
	intP.set(3528);

	ASSERT_EQ(newVal, 3528);
}

TEST(PropertyTest, onPropertyChanged_disconnectLambda_oldValue)
{
	ps::Property<int> intP(42);
	int newVal = intP;

	//implementation of on property changed
	auto lambda = [&newVal](int newValue) {newVal = newValue;};
	auto idx = intP += lambda;
	intP -= idx;
	intP.set(3528);

	ASSERT_EQ(newVal, 42);
}

TEST(PropertyTest, onPropertyChanged_disconnectByTypeIndex_oldValue)
{
	ps::Property<int> intP(42);
	int newVal = intP;

	//implementation of on property changed
	auto disconnector = intP += [&newVal](int newValue) {newVal = newValue;};
	intP -= disconnector;
	intP.set(3528);

	ASSERT_EQ(newVal, 42);
}



