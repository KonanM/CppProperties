# PropertySystem 
This library is basically inspired by the [Qt property system](http://doc.qt.io/qt-5/properties.html) and provides a very similar set of features, but also adds some functionality on top by providing property hierarchies.
The implementation is a header only modern C++17 library, which does not rely on macros or reflection, but on template metaprogramming.

## Design Goals

* Provides an easy to use interface to connect properties to arbitrary callbacks. 
* Can store arbitrary values in a property container and provides a type safe way for accessing them (by using typed property descriptors).
* Property containers be used in a hierarchical manner, which works similar to css. All properties from a parent are visible for it's children unless the child provides the property itself.
* Registering a property is the only operation that needs to traverse a property hierachy. Changing a property is quite cheap (two lookups in an unordered map) and triggering the property changed callbacks is linear with regards to changed properties.

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
A property container can be used to store multiple properties of any type. A type save way of accessing them is achieved by using typed property descriptors, which wrap an identifier along with a default value and a type.
```cpp
ps::PropertyDescriptor<int> IntPD(42, "SliderValue");

ps::PropertyContainer propertyContainer;

int defaultValue = propertyContainer.getValue(IntPD); //property hasn't been set, the default value is returned

auto disconnectIdx = propertyContainer.connect(IntPD, [](){ std::cout << "IntPD got changed"; });
//use setProperty to set a new property
propertyContainer.setProperty(1);
//use changeProperty to change a property after it has been set as it is more efficient
propertyContainer.changeProperty(2);

propertyContainer.removeProperty(IntPD);

```

### PropertyContainer Hierarchies
This feature has actually inspired the whole library, if you don't need this I would actually rather recommend something like [https://zajo.github.io/boost-synapse/](boost synapse). I have seen a property hierarchy in action once in a multi million LOC C++ codebase where it was one of the basic pillars of the software architecture, quite similar to [QObject from Qt](http://doc.qt.io/qt-5/qobject.html).
It probably makes sense to derive from ps::PropertyContainer if you need to use a class within a property hierarchy (it's not a requirement though).

**Why would I want to use this feature?**

* Makes all classes that derive from ps::PropertyContainer easily extensible by new properties.
* Gives you the decorator pattern for free, which for example makes factories very customiziable.
* All properties from parent containers can be queried, making things like injecting certain properties super easy. You don't have to pass properties along the property hierarchy, to be able to access a property somewhere down the hierarchy. 
* It's very easy to provide custom implementations for certain properties to make things easily testable.


```cpp


```

## FAQ - Frequently asked questions

**Aren't there any similar libraries out there?**  
Besides Qt I'm not aware of anything with a simlar feature set. There are some libraries for [C#](https://www.codeproject.com/Articles/450344/A-Simple-Csharp-Property-System), and also some in [C++](http://www.academia.edu/401854/A_Generic_Data_Structure_for_An_Architectural_Design_Application), but most C++ implementation relied on macros or didn't offer a functionality to connect a callback of any type when a property changes.

**Is the C++ 17 requirement really needed?**

The C++17 standard is needed for a std::any based type erasure and the implementation relies on if constexpr in quite a few places.

