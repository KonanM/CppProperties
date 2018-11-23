# PropertySystem 
This library is basically inspired by the [Qt property system](http://doc.qt.io/qt-5/properties.html) and provides a very similar set of features without relying on something like a (meta) object system.
The implementation is a header only modern C++17 library, which does not rely on macros, but on template metaprogramming.
I found a few similar libraries, especially in [C#](https://www.codeproject.com/Articles/450344/A-Simple-Csharp-Property-System), and also some in [C++](http://www.academia.edu/401854/A_Generic_Data_Structure_for_An_Architectural_Design_Application), but most C++ implementation relied on macros or didn't offer a functionality to connect a callback when a property changes.

## Design Goals

* Can store arbitrary values (with std::any as underyling storage) and provides a type safe way for accessing them.
* Provides an easy to use interface to connect properties to arbitrary callbacks 
* Can be used in a property hierarchy, which works similar to css. All properties from a parent are visible for it's children unless the child provides the property itself
* Registering a property for the first time is the only expensive operation. Changing a property is quite cheap (two lookups in an unordered map) and triggering the property changed callbacks is linear with regards to changed properties.

## Examples