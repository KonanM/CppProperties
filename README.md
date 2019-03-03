# C++ PropertySystem 
The original idea for this library was basically inspired by the [Qt property system](http://doc.qt.io/qt-5/properties.html) and provides a very similar set of features, but also adds some functionality on top by providing property hierarchies.
The implementation is a header only modern C++17 library, which does not rely on macros or reflection, but on template metaprogramming.

There have been several discussions about [C# properties](https://docs.microsoft.com/en-us/dotnet/csharp/programming-guide/classes-and-structs/properties)/[__declspec(property)](https://docs.microsoft.com/en-us/cpp/cpp/property-cpp?view=vs-2017) in C++. Here an example from [reddit](https://www.reddit.com/r/cpp/comments/61m9r1/what_do_you_think_about_properties_in_the_c/) and I even found an [old paper](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2004/n1615.pdf) about them.
This library has just superficial similarities to those ideas.

In it's most base from properties from this library are a wrapper around a value and signal (that you can connect to, to get notified of changes). 
While a property itself can be useful, it actually doesn't really help you with writing extensible classes, since you would have to writer a getter / setter for each property anyways.
That's why this library provides some utilities on top of basic properties, that make them useful for large scale applications.

## Design Goals

* Properties are a wrappers around a value that provide an easy to use interface to connect them to arbitrary callbacks. 
* Property container can store arbitrary properties and provides a type safe way for accessing them (by using typed property descriptors).
* Property containers can be used in a hierarchical manner, which works similar to css. All properties from a parent are visible for it's children unless the child provides the property itself.
* Registering a property is the only operation that needs to traverse a property hierachy. Changing a property is quite cheap (two lookups in an unordered map) and triggering the property changed callbacks scales linear with regards to changed properties.

## Installation
### CMake
If you use CMake you can simply clone the repository and add it as a subdirectory of your project.

The library is provided as a header only interface library. 
```cmake
target_link_libraries(${TargetName} PRIVATE CppProperties::cppproperties)  
```
### Other
Alternatively copy the include/cppproperties folder to the include folder of your project an you should be good to go.
There is no need to link anything, since the library is header only.

## Examples

### Property
The most basic type is a property itself, which can be used to store a value and connect synchronous callbacks that get triggered when a property changes.
```cpp
ps::Property<std::string> stringProperty("defaultValue");
std::string localString;
auto setLocalString = [&localString](const std::string& newString){ localString = newString;};

//connect a callable to the property, which gets triggered when the property changes
stringProperty.connect(setLocalString);
//alternatively you can use the C# inspired syntax
stringProperty += setLocalString;


stringProperty.set("newValue");
//callback got invoked and local string now contains "newValue"

//disconnect can be based on the type or on the return value of connect or by type
stringProperty.disconnect(setLocalString);
```

### PropertyContainer
Similar to [QT's dynamic properties](https://doc.qt.io/qt-5/properties.html#dynamic-properties) there is a class called PropertyContainer which is - in it's most basic form - a wrapper around map<Key, Property>. 
The thing is we want to be able to store/access properties of any type. So we need some kind of type erasure e.g. map<Key, PropertyBase>. 
So how do we provide a type safe access to this kind of map? The answer is by using so called typed property descriptors PropertyDescriptor< T >, which wrap an identifier along with a type and a default value.
```cpp
ps::PropertyDescriptor<int> IntPD(42, "SliderValue");

ps::PropertyContainer propertyContainer;
//if the property has not been set, the default value is (of the PD) is returned
int defaultValue = propertyContainer.getValue(IntPD); //property hasn't been set, the default value is returned

//each container has it's own signal per PD which we can connect to
//the signals are emitted asnychronously
auto disconnectIdx = propertyContainer.connect(IntPD, [](){ std::cout << "Slider Value got changed"; });
//use setProperty to set a new property
propertyContainer.setProperty(1);
//use changeProperty to change a property after it has been set as it is more efficient
propertyContainer.changeProperty(2);

//emit the signals for all properties that have changed, this is usually don within an main application loop
propertyContainer.emit();

```
### Proxy Properties
A proxy property is basically a property as well as a property container. How is this useful you might ask?  
First of all it provides some great encasultion for more complex properties that are dependent on multiple other properties.
Just assume we have a requirement that we can switch the language at runtime and all the strings will update.

```cpp
//made up example how multi language support is implemented
std::string language = getLanguage("en-us" */default/*)
//done only at application startup
std::string myAppTitle = translate("MyApp_Title_ID", language);
```

Let me first show you how to implement a proxy property by hand.
```cpp

ps::PropertyDescriptor<std::string> TitlePD("MyApp");
ps::PropertyDescriptor<std::string> LanguagePD("en-us");

class TranslationProxyProperty : public ps::ProxyProperty< std::string >
{
public:
	TranslationProxyProperty(std::string ID) : m_ID(std::move(ID)){
		observeProperty(LanguagePD, &TranslationProxyProperty::onLanguageChanged);
	}
private:
	void onLanguageChanged(const std::string& language){
		set(translate(m_ID, language));
	}
	std::string m_ID;
};

//...
ps::PropertyContainer propertyContainer;
propertyContainer.setProperty(TitlePD, std::make_unique<TranslationProxyProperty>("MyApp_Title_ID"));

```
If you think that this is quite some boilerplate code, then you are indeed correct. This library provides a very convienient function to create such properties.
From my own experience ps::make_proxy_property covers most use cases, but for some more complex e.g. threaded calculations you have to implement the ProxyProperties yourself.
```cpp

auto make_translation_property(std::string ID){
	return ps::make_proxy_property([id = std::move(ID)](const auto& language){ return translate(id, language);}, LanguagePD);
}

ps::PropertyContainer propertyContainer;

propertyContainer.setProperty(TitlePD, make_translation_property("MyApp_Title_ID"));
```

Now lets have a look at another example, where you have to calculate a value dependent on multiple inputs and you have to recalulate this value when any of the input changes.  
Too not make this too complicated, let's assume an x and y value and we need to calculate the distance (make_proxy_property works with any number of input properties).

```cpp
ps::PropertyDescriptor<double> XCoordinatePD(0.), YCoordinatePD(0.), DistancePD(0.);

ps::PropertyContainer propertyContainer;

auto distanceFunc = [](double x, double y){ return std::sqrt(x*x + y*y); }

propertyContainer.setProperty(DistancePD, ps::make_proxy_property(distanceFunc, XCoordinatePD, YCoordinatePD));

```

### PropertyContainer Hierarchies
This feature has actually inspired the whole library, if you don't need this I would actually rather recommend something like [boost synapse](https://zajo.github.io/boost-synapse/). I have seen a property hierarchy in action once in a multi million LOC C++ codebase where it was one of the basic pillars of the software architecture, quite similar to [QObject from Qt](http://doc.qt.io/qt-5/qobject.html).
It probably makes sense to derive from ps::PropertyContainer if you need to use a class within a property hierarchy (it's not a requirement though).

**Why would I want to use this feature?**

* Makes all classes that derive from ps::PropertyContainer easily extensible by new properties.
* All properties from parent containers can be queried, making things like injecting certain properties super easy. You don't have to pass properties along the property hierarchy, to be able to access a property somewhere down the hierarchy. 
* It's very easy to provide custom implementations for certain properties to make things easily testable. 
* Makes it trivial to switch things like a logger - or similar custom classes - at runtime. 

**I need a more complex example**

Alright let's assume you are working a software with multiple 3D views that show a scene from different perspectives. For a new feature you need to keep track of the mouse position projected to your 3D view in the other views. 
Not only the mouse position could change, but also the coordinate system (e.g. you can move around with the keyboard, or switch perspectices).

![alt text](https://github.com/KonanM/CppProperties/images/LeftRightView.jpg "Property Hierarchy Example")

```cpp

PropertyDescriptor<geo::Matrix4D> ViewTransformationPD(geo::Matrix4D{});
PropertyDescriptor<geo::Point3D> MousePosInSceneCoordinatesPD(geo::Point3D{});
PropertyDescriptor<geo::Point2D> MousePosInWindowCoordinatesPD(geo::Point2D{});

class ViewModel : public ps::PropertyContainer(),...{
//just an example, could also be done in the constructor
void init()
{
	//every view model has it's own view transformation(which is dependent on multiple transformations)
	//for more complex properties it makes sense to encapsulate them into a ProxyProperty
	//a proxy property is added as a child (automatically) and might depend on multiple inputs and has a single output
	setProperty(ViewTransformationPD, std::make_unique<ViewTransformationProxyProperty>());
	setProperty(MousePosInWindowCoordinatesPD, geo::Point2D{});
}
void onMouseMove(int x, int y)
{
	changeProperty(MousePosInScreenPD, geo::Point2D{x, y});
}
...};

ViewModel mainWindowViewModel;
//if you set something at the main window / root level it will be visible to all children
//a child can both observe and change properties from a parent level
mainWindowViewModel.setProperty(MousePosInSceneCoordinatesPD, geo::Point3d{});

auto& lwvm = mainWindowViewModel.addChildContainter(std::make_unique<ViewModel>());

auto calculateMousePosInScene = [](const geo::Point2D& mousePos, const geo::Matrix4D& transformation) -> geo::Point3D {
	//this is very simplified and will be a few more lines in reality
	//the important part is that this lambda will get called every time when either the mouse or the transformation changes
	geo::Point3D pointInSceneCoordinates = transformation * geo::Point3D{mousePos.x, mousePos.y, 0.0};
	return projectPointOntoScene(pointInScreenCoordinates, transformation); 
};

lwvm.changeProperty(MousePosInSceneCoordinatesPD, ps::make_proxy_property(calculateMousePosInScene, MousePosInWindowCoordinatesPD, ViewTransformationPD));

auto& rwvm = mainWindowViewModel.addChildContainter(std::make_unique<ViewModel>());
rwvm.observeProperty(MousePosInSceneCoordinatesPD, [](){//do something in the other view});

```

## FAQ - Frequently asked questions

**Aren't there any similar libraries out there?**  
I'm aware of the QT property system and the [reactive extensions for C++](https://github.com/ReactiveX/RxCpp), but they are not quite the same. There are some libraries for [C#](https://www.codeproject.com/Articles/450344/A-Simple-Csharp-Property-System), and also some in [C++](http://www.academia.edu/401854/A_Generic_Data_Structure_for_An_Architectural_Design_Application), but most C++ implementation relied on macros or didn't offer a functionality to connect a callback of any type when a property changes.

**Is the C++ 17 requirement really needed?**

The C++17 standard is currently needed for a std::any based type erasure and the implementation relies on if constexpr in quite a few places.
That being said it should be possible to port it to C++ 14.

**Why is the library header only? Isn't that bad for compilation times?**

The descision for being header is not because of ease of distribution, but simply because most of the code is templated and a traditional library wouldn't bring any benefits.