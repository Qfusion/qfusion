#pragma once
#ifndef __ASBIND_H__
#define __ASBIND_H__

//=========================================================

/*
    asbind.h - Copyright Christian Holmberg 2011

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

//=========================================================

/*

API

Binding a c++ class or struct can be done like this :

    class MyClass { ... };

    ASBind::Class<MyClass> myclass = ASBind::MyClass<MyClass>( asEngine );

Or :
    ASBind::Class<MyClass> myclass = ASBind::CreateClass<MyClass>( asEngine [, name ]  );

Or you can get already defined class like this :

    ASBind::Class<MyClass> myclass = ASBind::GetClass<MyClass>( asEngine, name );

You can bind a class either as a reference, POD or class type. You can reference to Angelscript
documentation on these. POD type is a type registered as asOBJ_VALUE | asOBJ_POD.
Class types are registered as asOBJ_VALUE | asCLASS_APP_CDAK.
You can choose the type as the second template argument which defaults to ASBind::class_ref
(reference type). Other values here are ASBind::class_pod (POD) and ASBind::class_class (complex class)

    ASBind::Class<MyPOD, ABind::class_pod>( asEngine, name );

And this needs to be declared in global scope/namespace

ASBIND_TYPE( MyClass, ClassName );

Binding methods to class can be done with .method(). There's two versions available,
one is to bind actual methods and the other is used to bind global c-like "methods"

    myclass.method( &MyClass::Function, "funcname" );

    void MyClass_Function( MyClass *self );

    myclass.method( MyClass_Function, "funcname", true );

The last argument on the second version is true if 'this' is the first of function parameters,
false if its the last of them.
You also have .constmethod() to explicitly bind a 'const' method (actual const methods get the
const qualifier automatically)

    myclass.constmethod( &MyClass::cFunction, "cfuncname" );
    myclass.constmethod( MyClass_cFunction, "cfuncname" );

Binding members is done with .member().

    myclass.member( &MyClass:Variable, "var" );

======

Factory functions are binded through .factory. These are for reference types if you want to create
instances of this type in scripts.
You can declare multiple factories with different prototypes

    MyClass *MyClass_Factory()
    {
        return new MyClass();
    }

    myclass.factory( MyClass_Factory );

Reference types need to declare functions for adding and releasing references. This is done
here like this.

    myclass.refs( &MyClass::AddRef, &MyClass:Release );

    myclass.refs( MyClass_AddRef, MyClass_Release );

Here's a utility struct you can derive from if you declare your own new classes to bind
    class ASBind::RefWrapper
    {
        void AddRef();
        void Release();
        int refcount;
    }

=====

Constructors are declared using the template arguments (which have to have void-return type).
Types that are registered as class_class need constructor/destructors.
You can declare multiple constructors with different prototypes (as for every function)

    myclass.constructor<void(int,int)>();

Or global function version (last parameter is once again true if 'this' is first in the function
argument list)

    void MyClass_Constructor( MyClass *self )
    {
        new( self ) MyClass();
    }

    myclass.constructor( MyClass_Constructor, true );


Destructors are just callbacks to void( MyClass* )
You can use a helper here
    myclass.destructor( ASBind::CallDestructor<MyClass> );

Or declare your own
    void Myclass_Destructor( MyClass *self )
    {
        self->~MyClass();
    }

    myclass.destructor( MyClass_Destructor );

You can define cast operations too, once again either as real methods or global functions

    string MyClass::toString() { ... return string; }
    string MyClass_toString(MyClass *self) { ... return string; }

    myclass.cast( &MyClass::toString );
    myclass.cast( MyClass_toString );

Note that any custom type you use has to be declared as a type before using them in any of these
bindings.

=====

Global functions and variables are binded through the Global object :

    ASBind::Global global;

    global.func( MyGlobalFunction, "globfunc" );
    global.var( MyGlobalVariable, "globvar" );

=====

Enumerations are done through Enum object

    enum { ENUM0, ENUM1, ENUM2 .. };

    ASBind::Enum e( "EnumType" );
    e.add( "ENUM0", ENUM0 );

Enumerations also have operator() that does the same as .add

    e( "ENUM0", ENUM0 );

=====

All functions here return a reference to 'this' so you can cascade them. Example for enum :

    ASBind::Enum( "EnumType" )
        .add( "ENUM0", ENUM0 )
        .add( "ENUM1", ENUM1 )
        .add( "ENUM2", ENUM2 );

And you can use the operator() in enum too for some funkyness

    ASBind::Enum( "EnumType" )( "ENUM0", ENUM0 )( "ENUM1", ENUM1 );

Chaining can be used on Class, Global and Enum.

=============

Fetching and calling script functions.
Lets say you have a 'main' function in the script like this

    void main() { ... }

You create a function pointer object and fetch it from the script in 2 ways:
You can fetch FunctionPtr's directly with the script function pointer (if you know it)

    ASBind::FunctionPtr<void()> mainPtr = ASBind::CreateFunctionPtr( scriptFunc, mainPtr );

Or by name (note the additional module parameter, needed to resolve the name to id)

    ASBind::FunctionPtr<void()> mainPtr = ASBind::CreateFunctionPtr( "main", asModule, mainPtr );

Note that the FunctionPtr has a matching function prototype as a template argument
and the instance is also passed as a reference to ASBind::CreateFunctionPtr. This is only a dummy parameter
used for template deduction.

Now you can call your function pointer syntactically the same way as regular functions :

    mainPtr();

Only thing here is that you are accessing operator() of the FunctionPtr which does some handy stuff
to set parameters and whatnot. FunctionPtr can also return values normally, except for custom types
only pointers and references are supported as-of-now (same goes for passing arguments i guess?)

*/

//=========================================================

// some configuration variables
#ifndef ASBIND_THROW
	#include <stdexcept>
	#define ASBIND_THROW( a ) throw ASBind::Exception( a )
#endif

// #define __DEBUG_COUT_PRINT__
// #define __DEBUG_COM_PRINTF__

// Quake engine fixes
#ifdef min
	#undef min
#endif
#ifdef max
	#undef max
#endif

// some necessary includes
#include <string>
#include <sstream>
#ifdef __DEBUG_COUT_PRINT__
	#include <iostream>
#endif

namespace ASBind
{

//=========================================================

// ctassert.h http://ksvanhorn.com/Articles/ctassert.h.txt
template <bool t>
struct ctassert {
	enum { N = 1 - 2 * int(!t) };
	// 1 if t is true, -1 if t is false.
	static char A[N];
};

template <bool t>
char ctassert<t>::A[N];

// basic exception
typedef std::runtime_error Exception;

// asbind_typestring.h
// utilities to convert types to strings

#define ASBIND_TYPE( type, name ) \
	namespace ASBind {          \
	template<> \
	inline const char *typestr<type>() { return # name ; }   \
	}

#define ASBIND_ARRAY_TYPE( type, name ) \
	namespace ASBind {          \
	template<> \
	inline const char *typestr<type>() { return "array<" # name ">" ; }  \
	}

// throw ?
template<typename T>
const char * typestr() { ctassert<sizeof( T ) == 0>(); return "ERROR"; }

template<>
inline const char *typestr<signed int>() { return "int"; }
template<>
inline const char *typestr<unsigned int>() { return "uint"; }
template<>
inline const char *typestr<char>() { return "uint8"; }
template<>
inline const char *typestr<signed char>() { return "int8"; }
template<>
inline const char *typestr<unsigned char>() { return "uint8"; }
template<>
inline const char *typestr<signed short>() { return "int16"; }
template<>
inline const char *typestr<unsigned short>() { return "uint16"; }
template<>
inline const char *typestr<bool>() { return "bool"; }
template<>
inline const char *typestr<int64_t>() { return "int64"; }
template<>
inline const char *typestr<uint64_t>() { return "uint64"; }

template<>
inline const char *typestr<float>() { return "float"; }
template<>
inline const char *typestr<double>() { return "double"; }

template<>
inline const char *typestr<void>() { return "void"; }


// this here can be used to make sure we have a pointer in our hands
// (used for function/function-pointer types)
template<typename T>
struct __ptr {
	typedef T *type;
};

template<typename T>
struct __ptr<T*>{
	typedef T *type;
};

// inout specifiers
template<typename T>
struct __inout__ {};
template<typename T>
struct __in__ {};
template<typename T>
struct __out__ {};

// custom types NEED to define static property
//  (string) typestr
template<typename T>
struct TypeStringProxy {
	std::string operator()( const char *name = "" ) {
		std::ostringstream os;
		os << typestr<T>();
		if( name && strlen( name ) ) {
			os << " " << name;
		}
		return os.str();
	}
	std::string return_type( const char *name = "" ) {
		return operator()( name );
	}
};

template<typename T>
struct TypeStringProxy<T*>{
	std::string operator()( const char *name = "" ) {
		std::ostringstream os;
		os << typestr<T>() << "@";
		if( name && strlen( name ) ) {
			os << " " << name;
		}
		return os.str();
	}
	std::string return_type( const char *name = "" ) {
		return operator()( name );
	}
};

// FIXME: separate types and RETURN TYPES! return references dont have inout qualifiers!
template<typename T>
struct TypeStringProxy<T&>{
	std::string operator()( const char *name = "" ) {
		std::ostringstream os;
		os << typestr<T>() << "&inout"; // FIXME should flag inout/out somehow..
		if( name && strlen( name ) ) {
			os << " " << name;
		}
		return os.str();
	}
	// no inout qualifiers for returned references
	std::string return_type( const char *name = "" ) {
		std::ostringstream os;
		os << typestr<T>() << "&";  // FIXME should flag inout/out somehow..
		if( name && strlen( name ) ) {
			os << " " << name;
		}
		return os.str();
	}
};

template<typename T>
struct TypeStringProxy<T*&>{
	std::string operator()( const char *name = "" ) {
		std::ostringstream os;
		os << typestr<T>() << "@&inout";    // FIXME should flag inout/out somehow..
		if( name && strlen( name ) ) {
			os << " " << name;
		}
		return os.str();
	}
	// no inout qualifiers for returned references
	std::string return_type( const char *name = "" ) {
		std::ostringstream os;
		os << typestr<T>() << "@&"; // FIXME should flag inout/out somehow..
		if( name && strlen( name ) ) {
			os << " " << name;
		}
		return os.str();
	}
};

template<typename T>
struct TypeStringProxy<const T>{
	std::string operator()( const char *name = "" ) {
		std::ostringstream os;
		os << "const " << typestr<T>();
		if( name && strlen( name ) ) {
			os << " " << name;
		}
		return os.str();
	}
	std::string return_type( const char *name = "" ) {
		return operator()( name );
	}
};

template<typename T>
struct TypeStringProxy<const T*>{
	std::string operator()( const char *name = "" ) {
		std::ostringstream os;
		os << "const " << typestr<T>() << "@";
		if( name && strlen( name ) ) {
			os << " " << name;
		}
		return os.str();
	}
	std::string return_type( const char *name = "" ) {
		return operator()( name );
	}
};

// FIXME: separate types and RETURN TYPES! return references dont have inout qualifiers!
template<typename T>
struct TypeStringProxy<const T&>{
	std::string operator()( const char *name = "" ) {
		std::ostringstream os;
		os << "const " << typestr<T>() << "&in";
		if( name && strlen( name ) ) {
			os << " " << name;
		}
		return os.str();
	}
	// no inout qualifiers for returned references
	std::string return_type( const char *name = "" ) {
		std::ostringstream os;
		os << "const " << typestr<T>() << "&";
		if( name && strlen( name ) ) {
			os << " " << name;
		}
		return os.str();
	}
};

template<typename T>
struct TypeStringProxy<const T*&>{
	std::string operator()( const char *name = "" ) {
		std::ostringstream os;
		os << "const " << typestr<T>() << "@&in";
		if( name && strlen( name ) ) {
			os << " " << name;
		}
		return os.str();
	}
	// no inout qualifiers for returned references
	std::string return_type( const char *name = "" ) {
		std::ostringstream os;
		os << "const " << typestr<T>() << "@&";
		if( name && strlen( name ) ) {
			os << " " << name;
		}
		return os.str();
	}
};


//=================================================

// function string

template<typename R>
struct FunctionStringProxy {
	std::string operator()( const char *s ) {
		ctassert<sizeof( R ) == 0>();
		throw Exception( std::string( "FunctionStringProxy base called with " ) + s );
	}
};

template<typename R>
struct FunctionStringProxy<R ( * )()>{
	std::string operator()( const char *s  ) {
		std::ostringstream os;
		os << TypeStringProxy<R>().return_type() << " " << s << "()";
		return os.str();
	}
};

template<typename R, typename A1>
struct FunctionStringProxy<R ( * )( A1 )>{
	std::string operator()( const char *s ) {
		std::ostringstream os;
		os << TypeStringProxy<R>().return_type() << " " << s << " (" <<
			TypeStringProxy<A1>()() << ")";
		return os.str();
	}
};

template<typename R, typename A1, typename A2>
struct FunctionStringProxy<R ( * )( A1,A2 )>{
	std::string operator()( const char *s ) {
		std::ostringstream os;
		os << TypeStringProxy<R>().return_type() << " " << s << " (" <<
			TypeStringProxy<A1>()() << "," <<
			TypeStringProxy<A2>()() << ")";
		return os.str();
	}
};

template<typename R, typename A1, typename A2, typename A3>
struct FunctionStringProxy<R ( * )( A1,A2,A3 )>{
	std::string operator()( const char *s ) {
		std::ostringstream os;
		os << TypeStringProxy<R>().return_type() << " " << s << " (" <<
			TypeStringProxy<A1>()() << "," <<
			TypeStringProxy<A2>()() << "," <<
			TypeStringProxy<A3>()() << ")";
		return os.str();
	}
};

template<typename R, typename A1, typename A2, typename A3, typename A4>
struct FunctionStringProxy<R ( * )( A1,A2,A3,A4 )>{
	std::string operator()( const char *s ) {
		std::ostringstream os;
		os << TypeStringProxy<R>().return_type() << " " << s << " (" <<
			TypeStringProxy<A1>()() << "," <<
			TypeStringProxy<A2>()() << "," <<
			TypeStringProxy<A3>()() << "," <<
			TypeStringProxy<A4>()() << ")";
		return os.str();
	}
};

template<typename R, typename A1, typename A2, typename A3, typename A4, typename A5>
struct FunctionStringProxy<R ( * )( A1,A2,A3,A4,A5 )>{
	std::string operator()( const char *s ) {
		std::ostringstream os;
		os << TypeStringProxy<R>().return_type() << " " << s << " (" <<
			TypeStringProxy<A1>()() << "," <<
			TypeStringProxy<A2>()() << "," <<
			TypeStringProxy<A3>()() << "," <<
			TypeStringProxy<A4>()() << "," <<
			TypeStringProxy<A5>()() << ")";
		return os.str();
	}
};

template<typename R, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
struct FunctionStringProxy<R ( * )( A1,A2,A3,A4,A5,A6 )>{
	std::string operator()( const char *s ) {
		std::ostringstream os;
		os << TypeStringProxy<R>().return_type() << " " << s << " (" <<
			TypeStringProxy<A1>()() << "," <<
			TypeStringProxy<A2>()() << "," <<
			TypeStringProxy<A3>()() << "," <<
			TypeStringProxy<A4>()() << "," <<
			TypeStringProxy<A5>()() << "," <<
			TypeStringProxy<A6>()() << ")";
		return os.str();
	}
};

//========================================

// method string

template<typename T>
struct MethodStringProxy {
	std::string operator()( const char *s  ) {
		ctassert<sizeof( T ) == 0>();
		throw Exception( std::string( "MethodStringProxy: base class called in " ) + s );
	}
};

//==

template<typename T,typename R>
struct MethodStringProxy<R ( T::* )()>{
	std::string operator()( const char *s  ) {
		std::ostringstream os;
		// dont include T here
		os << TypeStringProxy<R>().return_type() << " " << s << "()";
		return os.str();
	}
};
template<typename T,typename R>
struct MethodStringProxy<R ( T::* )() const>{
	std::string operator()( const char *s  ) {
		std::ostringstream os;
		// dont include T here
		os << TypeStringProxy<R>().return_type() << " " << s << "() const";
		return os.str();
	}
};

//==

template<typename T,typename R, typename A1>
struct MethodStringProxy<R ( T::* )( A1 )>{
	std::string operator()( const char *s ) {
		std::ostringstream os;
		// dont include T here
		os << TypeStringProxy<R>().return_type() << " " << s << " (" <<
			TypeStringProxy<A1>()() << ")";
		return os.str();
	}
};
template<typename T,typename R, typename A1>
struct MethodStringProxy<R ( T::* )( A1 ) const>{
	std::string operator()( const char *s ) {
		std::ostringstream os;
		// dont include T here
		os << TypeStringProxy<R>().return_type() << " " << s << " (" <<
			TypeStringProxy<A1>()() << ") const";
		return os.str();
	}
};

//==

template<typename T,typename R, typename A1, typename A2>
struct MethodStringProxy<R ( T::* )( A1,A2 )>{
	std::string operator()( const char *s ) {
		std::ostringstream os;
		// dont include T here
		os << TypeStringProxy<R>().return_type() << " " << s << " (" <<
			TypeStringProxy<A1>()() << ", " <<
			TypeStringProxy<A2>()() << ")";
		return os.str();
	}
};
template<typename T,typename R, typename A1, typename A2>
struct MethodStringProxy<R ( T::* )( A1,A2 ) const>{
	std::string operator()( const char *s ) {
		std::ostringstream os;
		// dont include T here
		os << TypeStringProxy<R>().return_type() << " " << s << " (" <<
			TypeStringProxy<A1>()() << ", " <<
			TypeStringProxy<A2>()() << ") const";
		return os.str();
	}
};

//==

template<typename T,typename R, typename A1, typename A2, typename A3>
struct MethodStringProxy<R ( T::* )( A1,A2,A3 )>{
	std::string operator()( const char *s ) {
		std::ostringstream os;
		// dont include T here
		os << TypeStringProxy<R>().return_type() << " " << s << " (" <<
			TypeStringProxy<A1>()() << ", " <<
			TypeStringProxy<A2>()() << ", " <<
			TypeStringProxy<A3>()() << ")";
		return os.str();
	}
};
template<typename T,typename R, typename A1, typename A2, typename A3>
struct MethodStringProxy<R ( T::* )( A1,A2,A3 ) const>{
	std::string operator()( const char *s ) {
		std::ostringstream os;
		// dont include T here
		os << TypeStringProxy<R>().return_type() << " " << s << " (" <<
			TypeStringProxy<A1>()() << ", " <<
			TypeStringProxy<A2>()() << ", " <<
			TypeStringProxy<A3>()() << ") const";
		return os.str();
	}
};

//==

template<typename T,typename R, typename A1, typename A2, typename A3, typename A4>
struct MethodStringProxy<R ( T::* )( A1,A2,A3,A4 )>{
	std::string operator()( const char *s ) {
		std::ostringstream os;
		// dont include T here
		os << TypeStringProxy<R>().return_type() << " " << s << " (" <<
			TypeStringProxy<A1>()() << ", " <<
			TypeStringProxy<A2>()() << ", " <<
			TypeStringProxy<A3>()() << ", " <<
			TypeStringProxy<A4>()() << ")";
		return os.str();
	}
};
template<typename T,typename R, typename A1, typename A2, typename A3, typename A4>
struct MethodStringProxy<R ( T::* )( A1,A2,A3,A4 ) const>{
	std::string operator()( const char *s ) {
		std::ostringstream os;
		// dont include T here
		os << TypeStringProxy<R>().return_type() << " " << s << " (" <<
			TypeStringProxy<A1>()() << ", " <<
			TypeStringProxy<A2>()() << ", " <<
			TypeStringProxy<A3>()() << ", " <<
			TypeStringProxy<A4>()() << ") const";
		return os.str();
	}
};

//==

template<typename T,typename R, typename A1, typename A2, typename A3, typename A4, typename A5>
struct MethodStringProxy<R ( T::* )( A1,A2,A3,A4,A5 )>{
	std::string operator()( const char *s ) {
		std::ostringstream os;
		// dont include T here
		os << TypeStringProxy<R>().return_type() << " " << s << " (" <<
			TypeStringProxy<A1>()() << ", " <<
			TypeStringProxy<A2>()() << ", " <<
			TypeStringProxy<A3>()() << ", " <<
			TypeStringProxy<A4>()() << ", " <<
			TypeStringProxy<A5>()() << ")";
		return os.str();
	}
};
template<typename T,typename R, typename A1, typename A2, typename A3, typename A4, typename A5>
struct MethodStringProxy<R ( T::* )( A1,A2,A3,A4,A5 ) const>{
	std::string operator()( const char *s ) {
		std::ostringstream os;
		// dont include T here
		os << TypeStringProxy<R>().return_type() << " " << s << " (" <<
			TypeStringProxy<A1>()() << ", " <<
			TypeStringProxy<A2>()() << ", " <<
			TypeStringProxy<A3>()() << ", " <<
			TypeStringProxy<A4>()() << ", " <<
			TypeStringProxy<A5>()() << ") const";
		return os.str();
	}
};

//==

template<typename T,typename R, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
struct MethodStringProxy<R ( T::* )( A1,A2,A3,A4,A5,A6 )>{
	std::string operator()( const char *s ) {
		std::ostringstream os;
		// dont include T here
		os << TypeStringProxy<R>().return_type() << " " << s << " (" <<
			TypeStringProxy<A1>()() << ", " <<
			TypeStringProxy<A2>()() << ", " <<
			TypeStringProxy<A3>()() << ", " <<
			TypeStringProxy<A4>()() << ", " <<
			TypeStringProxy<A5>()() << ", " <<
			TypeStringProxy<A6>()() << ")";
		return os.str();
	}
};
template<typename T,typename R, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
struct MethodStringProxy<R ( T::* )( A1,A2,A3,A4,A5,A6 ) const>{
	std::string operator()( const char *s ) {
		std::ostringstream os;
		// dont include T here
		os << TypeStringProxy<R>().return_type() << " " << s << " (" <<
			TypeStringProxy<A1>()() << ", " <<
			TypeStringProxy<A2>()() << ", " <<
			TypeStringProxy<A3>()() << ", " <<
			TypeStringProxy<A4>()() << ", " <<
			TypeStringProxy<A5>()() << ", " <<
			TypeStringProxy<A6>()() << ") const";
		return os.str();
	}
};

//========================================

// actual function to convert type to string
template<typename T>
std::string TypeString( const char *name = "" ) {
	return TypeStringProxy<T>()( name );
}

// actual function to convert type to string
template<typename T>
std::string TypeString( T &t, const char *name = "" ) {
	return TypeStringProxy<T>()( name );
}

// actual function to convert the function type to string
template <typename F>
std::string FunctionString( const char *name = "" ) {
	return FunctionStringProxy< typename __ptr<F>::type >()( name );
}

// actual function to convert the function type to string
template <typename F>
std::string FunctionString( F f, const char *name = "" ) {
	return FunctionStringProxy< typename __ptr<F>::type >()( name );
}

// actual function to convert the method type to string
template<typename F>
std::string MethodString( const char *name = "" ) {
	return MethodStringProxy<F>()( name );
}

// actual function to convert the method type to string
template<typename F>
std::string MethodString( F f, const char *name = "" ) {
	return MethodStringProxy<F>()( name );
}

// actual function to convert the funcdef type to string
template <typename F>
std::string FuncdefString( const char *name = "" ) {
	return FunctionStringProxy< typename __ptr<F>::type >()( name );
}

// actual function to convert the funcdef type to string
template<typename F>
std::string FuncdefString( F f, const char *name = "" ) {
	return FunctionStringProxy< typename __ptr<F>::type >()( name );
}

//=========================================================

// asbind_stripthis.h
// utility to strip 'this' from either start or the end of argument list
// and return a null function pointer that has the 'correct' prototype for a member

template<typename F>
struct StripThisProxy {};

template<typename R, typename A1>
struct StripThisProxy<R ( * )( A1 )>{
	typedef R (*func_in)( A1 );
	typedef R (*func_of)(); // obj-first
	typedef R (*func_ol)(); // obj-last

	func_of objfirst( func_in f ) { return (func_of)0; }
	func_ol objlast( func_in f ) { return (func_ol)0; }
};

template<typename R, typename A1, typename A2>
struct StripThisProxy<R ( * )( A1,A2 )>{
	typedef R (*func_in)( A1,A2 );
	typedef R (*func_of)( A2 );   // obj-first
	typedef R (*func_ol)( A1 );   // obj-last

	func_of objfirst( func_in f ) { return (func_of)0; }
	func_ol objlast( func_in f ) { return (func_ol)0; }
};

template<typename R, typename A1, typename A2, typename A3>
struct StripThisProxy<R ( * )( A1,A2,A3 )>{
	typedef R (*func_in)( A1,A2,A3 );
	typedef R (*func_of)( A2,A3 );    // obj-first
	typedef R (*func_ol)( A1,A2 );    // obj-last

	func_of objfirst( func_in f ) { return (func_of)0; }
	func_ol objlast( func_in f ) { return (func_ol)0; }
};

template<typename R, typename A1, typename A2, typename A3, typename A4>
struct StripThisProxy<R ( * )( A1,A2,A3,A4 )>{
	typedef R (*func_in)( A1,A2,A3,A4 );
	typedef R (*func_of)( A2,A3,A4 ); // obj-first
	typedef R (*func_ol)( A1,A2,A3 ); // obj-last

	func_of objfirst( func_in f ) { return (func_of)0; }
	func_ol objlast( func_in f ) { return (func_ol)0; }
};

template<typename R, typename A1, typename A2, typename A3, typename A4, typename A5>
struct StripThisProxy<R ( * )( A1,A2,A3,A4,A5 )>{
	typedef R (*func_in)( A1,A2,A3,A4,A5 );
	typedef R (*func_of)( A2,A3,A4,A5 );  // obj-first
	typedef R (*func_ol)( A1,A2,A3,A4 );  // obj-last

	func_of objfirst( func_in f ) { return (func_of)0; }
	func_ol objlast( func_in f ) { return (func_ol)0; }
};

template<typename R, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
struct StripThisProxy<R ( * )( A1,A2,A3,A4,A5,A6 )>{
	typedef R (*func_in)( A1,A2,A3,A4,A5,A6 );
	typedef R (*func_of)( A2,A3,A4,A5,A6 );   // obj-first
	typedef R (*func_ol)( A1,A2,A3,A4,A5 );   // obj-last

	func_of objfirst( func_in f ) { return (func_of)0; }
	func_ol objlast( func_in f ) { return (func_ol)0; }
};

template<typename R, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
struct StripThisProxy<R ( * )( A1,A2,A3,A4,A5,A6,A7 )>{
	typedef R (*func_in)( A1,A2,A3,A4,A5,A6,A7 );
	typedef R (*func_of)( A2,A3,A4,A5,A6,A7 );    // obj-first
	typedef R (*func_ol)( A1,A2,A3,A4,A5,A6 );    // obj-last

	func_of objfirst( func_in f ) { return (func_of)0; }
	func_ol objlast( func_in f ) { return (func_ol)0; }
};

template<typename F>
typename StripThisProxy<typename __ptr<F>::type>::func_of StripThisFirst( F f ) {
	return StripThisProxy<typename __ptr<F>::type>().objfirst( f );
}

template<typename F>
typename StripThisProxy<typename __ptr<F>::type>::func_ol StripThisLast( F f ) {
	return StripThisProxy<typename __ptr<F>::type>().objlast( f );
}

//=========================================================

// asbind_func.h
// functor object to call script-function

// first define structs to get/set arguments (struct to partial-specialize)
template<typename T>
struct SetArg {
	void operator()( asIScriptContext *ctx, int idx, T &t ) {
		ctassert<sizeof( T ) == 0>();
	}
};
template<typename T>
struct GetArg {
	T operator()() {
		ctassert<sizeof( T ) == 0>();
		return T();
	}
};

template<>
struct SetArg<signed int>{
	void operator()( asIScriptContext *ctx, int idx, signed int &t ) { ctx->SetArgDWord( idx, t ); }
};
template<>
struct SetArg<unsigned int>{
	void operator()( asIScriptContext *ctx, int idx, unsigned int &t ) { ctx->SetArgDWord( idx, t ); }
};
template<>
struct SetArg<signed short>{
	void operator()( asIScriptContext *ctx, int idx, signed short &t ) { ctx->SetArgWord( idx, t ); }
};
template<>
struct SetArg<unsigned short>{
	void operator()( asIScriptContext *ctx, int idx, unsigned short &t ) { ctx->SetArgWord( idx, t ); }
};
template<>
struct SetArg<char>{
	void operator()( asIScriptContext *ctx, int idx, char &t ) { ctx->SetArgByte( idx, t ); }
};
template<>
struct SetArg<signed char>{
	void operator()( asIScriptContext *ctx, int idx, signed char &t ) { ctx->SetArgByte( idx, t ); }
};
template<>
struct SetArg<unsigned char>{
	void operator()( asIScriptContext *ctx, int idx, unsigned char &t ) { ctx->SetArgByte( idx, t ); }
};
template<>
struct SetArg<float>{
	void operator()( asIScriptContext *ctx, int idx, float &t ) { ctx->SetArgFloat( idx, t ); }
};
template<>
struct SetArg<double>{
	void operator()( asIScriptContext *ctx, int idx, double &t ) { ctx->SetArgDouble( idx, t ); }
};
template<>
struct SetArg<int64_t>{
	void operator()( asIScriptContext *ctx, int idx, int64_t &t ) { ctx->SetArgQWord( idx, t ); }
};
template<>
struct SetArg<uint64_t>{
	void operator()( asIScriptContext *ctx, int idx, uint64_t &t ) { ctx->SetArgQWord( idx, t ); }
};
// bool FIXME: 32-bits on PowerPC
template<>
struct SetArg<bool>{
	void operator()( asIScriptContext *ctx, int idx, bool &t ) { ctx->SetArgByte( idx, (unsigned char)t ); }
};
// pointers and references
template<typename T>
struct SetArg<T*>{
	void operator()( asIScriptContext *ctx, int idx, T *t ) { ctx->SetArgAddress( idx, (void*)t ); }
};
template<typename T>
struct SetArg<const T*>{
	void operator()( asIScriptContext *ctx, int idx, const T *t ) { ctx->SetArgAddress( idx, (void*)t ); }
};
template<typename T>
struct SetArg<T&>{
	void operator()( asIScriptContext *ctx, int idx, T &t ) { ctx->SetArgAddress( idx, (void*)&t ); }
};
template<typename T>
struct SetArg<const T&>{
	void operator()( asIScriptContext *ctx, int idx, const T &t ) { ctx->SetArgAddress( idx, (void*)&t ); }
};

//==== RETURN
template<>
struct GetArg<signed int>{
	signed int operator()( asIScriptContext *ctx ) { return ctx->GetReturnDWord(); }
};
template<>
struct GetArg<unsigned int>{
	unsigned int operator()( asIScriptContext *ctx ) { return ctx->GetReturnDWord(); }
};
template<>
struct GetArg<signed short>{
	signed short operator()( asIScriptContext *ctx ) { return ctx->GetReturnWord(); }
};
template<>
struct GetArg<unsigned short>{
	unsigned short operator()( asIScriptContext *ctx ) { return ctx->GetReturnWord(); }
};
template<>
struct GetArg<char>{
	char operator()( asIScriptContext *ctx ) { return ctx->GetReturnByte(); }
};
template<>
struct GetArg<signed char>{
	signed char operator()( asIScriptContext *ctx ) { return ctx->GetReturnByte(); }
};
template<>
struct GetArg<unsigned char>{
	unsigned char operator()( asIScriptContext *ctx ) { return ctx->GetReturnByte(); }
};
template<>
struct GetArg<float>{
	float operator()( asIScriptContext *ctx ) { return ctx->GetReturnFloat(); }
};
template<>
struct GetArg<double>{
	double operator()( asIScriptContext *ctx ) { return ctx->GetReturnDouble(); }
};
template<>
struct GetArg<bool>{
	bool operator()( asIScriptContext *ctx ) { return ctx->GetReturnByte() == 0 ? false : true; }
};
template<>
struct GetArg<int64_t>{
	int64_t operator()( asIScriptContext *ctx ) { return ctx->GetReturnQWord(); }
};
template<>
struct GetArg<uint64_t>{
	uint64_t operator()( asIScriptContext *ctx ) { return ctx->GetReturnQWord(); }
};
// pointers and references
template<typename T>
struct GetArg<T*>{
	T * operator()( asIScriptContext *ctx ) { return ctx->GetReturnAddress(); }
};
template<typename T>
struct GetArg<const T*>{
	const T * operator()( asIScriptContext *ctx ) { return ctx->GetReturnAddress(); }
};
template<typename T>
struct GetArg<T&>{
	T & operator()( asIScriptContext *ctx ) { return *static_cast<T*>( ctx->GetReturnAddress() ); }
};
template<typename T>
struct GetArg<const T&>{
	const T & operator()( asIScriptContext *ctx ) { return *static_cast<T*>( ctx->GetReturnAddress() ); }
};

//====================

struct FunctionPtrBase {
	asIScriptFunction *fptr;
	asIScriptContext *ctx;

	FunctionPtrBase( asIScriptFunction *fptr )
		: fptr( fptr ), ctx( nullptr ) {}

	// never 'new' this or descendant classes!
	// virtual ~FunctionPtrBase() {}

	asIScriptFunction *getPtr( void ) { return fptr; }
	const char *getName( void ) { return fptr != nullptr ? fptr->GetName() : "#NULL#"; }
	bool isValid( void ) { return fptr != nullptr; }
	void addref( void ) {
		if( fptr != nullptr ) {
			fptr->AddRef();
		}
	}
	void release( void ) {
		if( fptr != nullptr ) {
			asIScriptFunction *fptr_ = fptr; fptr = NULL; fptr_->Release();
		}
	}
	void setContext( asIScriptContext *_ctx ) { ctx = _ctx; }
	asIScriptModule *getModule( void ) {
		asIScriptFunction *f = fptr;
		while( f != nullptr && f->GetFuncType() == asFUNC_DELEGATE )
			f = f->GetDelegateFunction();
		return f != nullptr ? f->GetModule() : NULL;
	}

	// general calling function
	void precall( void ) {
		if( fptr != nullptr ) {
			ctx->Prepare( fptr );
		}
	}
	void call( void ) {
		if( ctx != nullptr ) {
			int r = ctx->Execute();
			if( r != asEXECUTION_FINISHED && r != asEXECUTION_SUSPENDED ) {
				Com_Printf( "ASBind::FunctionPtrBase: Execute failed %d (name %s)\n", r, fptr->GetName() );
				// some debug stuff
			#ifdef __FUNCTIONPTR_CALL_THROW__
				throw Exception( "FunctionPtrBase::call Execute failed" );
			#endif
			}
		}
	}
};

//=================

template<typename R>
struct FunctionPtr : FunctionPtrBase {
	FunctionPtr( asIScriptFunction *fptr = NULL ) : FunctionPtrBase( fptr ) {}
	R operator()( void ) {
		ctassert<sizeof( R ) == 0>();
		throw std::runtime_error( "FunctionPtr baseclass called!" );
		return R();
	}
};

//==

template<typename R>
struct FunctionPtr<R()> : FunctionPtrBase {
	typedef R (*func_type)();
	FunctionPtr( asIScriptFunction *fptr = NULL ) : FunctionPtrBase( fptr ) {}
	R operator()( void ) {
		precall();
		call();
		return GetArg<R>()( ctx );
	}
};

template<>
struct FunctionPtr<void()> : FunctionPtrBase {
	typedef void (*func_type)();
	FunctionPtr( asIScriptFunction *fptr = NULL ) : FunctionPtrBase( fptr ) {}
	void operator()( void ) {
		precall();
		call();
	}
};

//==

template<typename R, typename A1>
struct FunctionPtr<R( A1 )> : FunctionPtrBase {
	typedef R (*func_type)( A1 );
	FunctionPtr( asIScriptFunction *fptr = NULL ) : FunctionPtrBase( fptr ) {}
	R operator()( A1 a1 ) {
		precall();
		SetArg<A1>()( ctx, 0, a1 );
		call();
		return GetArg<R>()( ctx );
	}
};

template<typename A1>
struct FunctionPtr<void (A1)> : FunctionPtrBase {
	typedef void (*func_type)( A1 );
	FunctionPtr( asIScriptFunction *fptr = NULL ) : FunctionPtrBase( fptr ) {}
	void operator()( A1 a1 ) {
		precall();
		SetArg<A1>()( ctx, 0, a1 );
		call();
	}
};

//==

template<typename R, typename A1, typename A2>
struct FunctionPtr<R( A1,A2 )> : FunctionPtrBase {
	typedef R (*func_type)( A1,A2 );
	FunctionPtr( asIScriptFunction *fptr = NULL ) : FunctionPtrBase( fptr ) {}
	R operator()( A1 a1, A2 a2 ) {
		precall();
		SetArg<A1>()( ctx, 0, a1 );
		SetArg<A2>()( ctx, 1, a2 );
		call();
		return GetArg<R>()( ctx );
	}
};

template<typename A1, typename A2>
struct FunctionPtr<void (A1,A2)> : FunctionPtrBase {
	typedef void (*func_type)( A1,A2 );
	FunctionPtr( asIScriptFunction *fptr = NULL ) : FunctionPtrBase( fptr ) {}
	void operator()( A1 a1, A2 a2 ) {
		precall();
		SetArg<A1>()( ctx, 0, a1 );
		SetArg<A2>()( ctx, 1, a2 );
		call();
	}
};

//==

template<typename R, typename A1, typename A2, typename A3>
struct FunctionPtr<R( A1,A2,A3 )> : FunctionPtrBase {
	typedef R (*func_type)( A1,A2,A3 );
	FunctionPtr( asIScriptFunction *fptr = NULL ) : FunctionPtrBase( fptr ) {}
	R operator()( A1 a1, A2 a2, A3 a3 ) {
		precall();
		SetArg<A1>()( ctx, 0, a1 );
		SetArg<A2>()( ctx, 1, a2 );
		SetArg<A3>()( ctx, 2, a3 );
		call();
		return GetArg<R>()( ctx );
	}
};

template<typename A1, typename A2, typename A3>
struct FunctionPtr<void (A1,A2,A3)> : FunctionPtrBase {
	typedef void (*func_type)( A1,A2,A3 );
	FunctionPtr( asIScriptFunction *fptr = NULL ) : FunctionPtrBase( fptr ) {}
	void operator()( A1 a1, A2 a2, A3 a3 ) {
		precall();
		SetArg<A1>()( ctx, 0, a1 );
		SetArg<A2>()( ctx, 1, a2 );
		SetArg<A3>()( ctx, 2, a3 );
		call();
	}
};

//==

template<typename R, typename A1, typename A2, typename A3, typename A4>
struct FunctionPtr<R( A1,A2,A3,A4 )> : FunctionPtrBase {
	typedef R (*func_type)( A1,A2,A3,A4 );
	FunctionPtr( asIScriptFunction *fptr = NULL ) : FunctionPtrBase( fptr ) {}
	R operator()( A1 a1, A2 a2, A3 a3, A4 a4 ) {
		precall();
		SetArg<A1>()( ctx, 0, a1 );
		SetArg<A2>()( ctx, 1, a2 );
		SetArg<A3>()( ctx, 2, a3 );
		SetArg<A4>()( ctx, 3, a4 );
		call();
		return GetArg<R>()( ctx );
	}
};

template<typename A1, typename A2, typename A3, typename A4>
struct FunctionPtr<void (A1,A2,A3,A4)> : FunctionPtrBase {
	typedef void (*func_type)( A1,A2,A3,A4 );
	FunctionPtr( asIScriptFunction *fptr = NULL ) : FunctionPtrBase( fptr ) {}
	void operator()( A1 a1, A2 a2, A3 a3, A4 a4 ) {
		precall();
		SetArg<A1>()( ctx, 0, a1 );
		SetArg<A2>()( ctx, 1, a2 );
		SetArg<A3>()( ctx, 2, a3 );
		SetArg<A4>()( ctx, 3, a4 );
		call();
	}
};

//==

// generator function to return the proxy functor object
template<typename F>
FunctionPtr<F> CreateFunctionPtr( asIScriptFunction *fptr, FunctionPtr<F> &f ) {
	return FunctionPtr<F>( fptr );
}

template<typename F>
FunctionPtr<F> CreateFunctionPtr( const char *name, asIScriptModule *mod, FunctionPtr<F> &f ) {
	asIScriptFunction *fptr = mod->GetFunctionByDecl( FunctionString<typename FunctionPtr<F>::func_type>( name ).c_str() );
	return CreateFunctionPtr<F>( fptr, f );
}

//=========================================================

// asbind_class.h
// bind classes and its methods and members (and also global functions as methods)

enum {
	class_ref = 0,          // needs reference [ and opt. factory ] functions
	class_pod = 1,          // simple pod types
	class_class = 2,        // needs constructor/deconstructor, copyconstructor and assignment
	class_singleref = 3,    // there can only be 1 instance that cant be referenced (see AS docs)
	class_pod_allints = 4,  // same as class_pod but with all members being integers
	class_pod_allfloats = 5,// same as class_pod but with all members being floats
	class_nocount = 6,      // no reference counting, memory management is performed solely on app side
};

template<typename T>
void CallCtor( T *t ) { new( t ) T(); }
template<typename T, typename A1>
void CallCtor( T *t, A1 a1 ) { new( t ) T( a1 ); }
template<typename T, typename A1, typename A2>
void CallCtor( T *t, A1 a1, A2 a2 ) { new( t ) T( a1, a2 ); }
template<typename T, typename A1, typename A2, typename A3>
void CallCtor( T *t, A1 a1, A2 a2, A3 a3 ) { new( t ) T( a1, a2, a3 ); }
template<typename T, typename A1, typename A2, typename A3, typename A4>
void CallCtor( T *t, A1 a1, A2 a2, A3 a3, A4 a4 ) { new( t ) T( a1, a2, a3, a4 ); }

template<typename T, typename A1>
struct CallCtorProxy {
	void *operator()() { return 0; }
};
template<typename T>
struct CallCtorProxy<T, void()>{
	typedef void (*func_t)( T* );
	func_t operator()() { return CallCtor<T>; }
};
template<typename T, typename A1>
struct CallCtorProxy<T, void(A1 a1)>{
	typedef void (*func_t)( T*,A1 );
	func_t operator()() { return CallCtor<T,A1>; }
};
template<typename T, typename A1, typename A2>
struct CallCtorProxy<T, void(A1 a1, A2 a2)>{
	typedef void (*func_t)( T*,A1,A2 );
	func_t operator()() { return CallCtor<T,A1,A2>; }
};
template<typename T, typename A1, typename A2,typename A3>
struct CallCtorProxy<T, void(A1 a1, A2 a2,A3 a3)>{
	typedef void (*func_t)( T*,A1,A2,A3 );
	func_t operator()() { return CallCtor<T,A1,A2,A3>; }
};
template<typename T, typename A1, typename A2,typename A3,typename A4>
struct CallCtorProxy<T, void(A1 a1, A2 a2,A3 a3,A4 a4)>{
	typedef void (*func_t)( T*,A1,A2,A3,A4 );
	func_t operator()() { return CallCtor<T,A1,A2,A3,A4>; }
};

template<typename T>
void CallDestructor( T *t ) { t->~T(); }


template<typename T, int class_type = class_ref>
class Class
{
	asIScriptEngine *engine;
	std::string name;
	int id;

	// TODO: flag value/reference
	void registerSelf( void ) {
		int flags = 0;
		int size = 0;
		switch( class_type ) {
			case class_pod:
				flags = asOBJ_APP_CLASS | asOBJ_VALUE | asOBJ_POD;
				size = sizeof( T );
				break;
			case class_class:
				flags = asOBJ_VALUE | asOBJ_APP_CLASS_CDAK;
				size = sizeof( T );
				break;
			case class_singleref:
				flags = asOBJ_REF | asOBJ_NOHANDLE;
				size = 0;
				break;
			case class_pod_allints:
				flags = asOBJ_APP_CLASS | asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLINTS;
				size = sizeof( T );
				break;
			case class_pod_allfloats:
				flags = asOBJ_APP_CLASS | asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS;
				size = sizeof( T );
				break;
			case class_nocount:
				flags = asOBJ_REF | asOBJ_NOCOUNT;
				size = 0;
				break;
			case class_ref:
			default:
				flags = asOBJ_REF;
				size = 0;
				break;
		}

		id = engine->RegisterObjectType( name.c_str(), size, flags );
		if( id < 0 ) {
			throw Exception( va( "ASBind::Class (%s) RegisterObjectType failed %d", name.c_str(), id ) );
		}

#ifdef __DEBUG_COUT_PRINT__
		std::cout << "ASBind::Class registered new class " << name << std::endl;
#endif
#ifdef __DEBUG_COM_PRINTF__
		Com_Printf( "ASBind::Class registered new class %s\n", name.c_str() );
#endif

	}

public:
	// constructors
	Class( asIScriptEngine *engine ) : engine( engine ) {
		name = TypeString<T>();
		registerSelf();
	}

	Class( asIScriptEngine *engine, const char *name )
		: engine( engine ), name( name ) {
		registerSelf();
	}

	// not "public" constructor that doesnt register the type
	// but creates a proxy for it
	Class( asIScriptEngine *engine, const char *name, int id )
		: engine( engine ), name( name ), id( id )
	{}

	// BIND METHOD

	// input actual method function of class
	template<typename F>
	Class & method( F f, const char *fname ) {
		std::string fullname = MethodString<F>( fname );

#ifdef __DEBUG_COUT_PRINT__
		std::cout << TypeString<T>() << "::method " << fullname << std::endl;
#endif
#ifdef __DEBUG_COM_PRINTF__
		Com_Printf( "%s::method %s\n", name.c_str(), fullname.c_str() );
#endif

		int _id = engine->RegisterObjectMethod( name.c_str(), fullname.c_str(),
												asSMethodPtr<sizeof( f )>::Convert( f ), asCALL_THISCALL );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::method (%s::%s) RegisterObjectMethod failed %d", name.c_str(), fullname.c_str(), _id ) );
		}

		return *this;
	}

	// explicit prototype + name
	template<typename F>
	Class & method2( F f, const char *proto ) {
#ifdef __DEBUG_COUT_PRINT__
		std::cout << TypeString<T>() << "::method " << proto << std::endl;
#endif
#ifdef __DEBUG_COM_PRINTF__
		Com_Printf( "%s::method %s\n", name.c_str(), proto );
#endif

		int _id = engine->RegisterObjectMethod( name.c_str(), proto, asSMethodPtr<sizeof( f )>::Convert( f ), asCALL_THISCALL );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::method (%s::%s) RegisterObjectMethod failed %d", name.c_str(), proto, _id ) );
		}

		return *this;
	}

	// const methods get the 'const' qualifier automatically, but this is here in case
	// you want explicit const method from non-const method
	// input actual method function of class TODO: migrate method + constmethod
	template<typename F>
	Class & constmethod( F f, const char *fname ) {
		std::string constname = MethodString<F>( fname ) + " const";
#ifdef __DEBUG_COUT_PRINT__
		std::cout << name << "::constmethod " << constname << std::endl;
#endif
#ifdef __DEBUG_COM_PRINTF__
		Com_Printf( "%s::constmethod %s\n", name.c_str(), constname.c_str() );
#endif

		int _id = engine->RegisterObjectMethod( name.c_str(), constname.c_str(),
												asSMethodPtr<sizeof( f )>::Convert( f ), asCALL_THISCALL );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::constmethod (%s::%s) RegisterObjectMethod failed %d", name.c_str(), constname.c_str(), _id ) );
		}

		return *this;
	}

	// input global function with *this either first or last parameter (obj_first)
	template<typename F>
	Class & method( F f, const char *fname, bool obj_first ) {
		std::string funcname = obj_first ?
							   FunctionString( StripThisFirst( f ), fname ) :
							   FunctionString( StripThisLast( f ), fname );

#ifdef __DEBUG_COUT_PRINT__
		std::cout << name << "::method " << funcname << std::endl;
#endif
#ifdef __DEBUG_COM_PRINTF__
		Com_Printf( "%s::method %s\n", name.c_str(), funcname.c_str() );
#endif

		int _id = engine->RegisterObjectMethod( name.c_str(), funcname.c_str(),
												asFUNCTION( f ), obj_first ? asCALL_CDECL_OBJFIRST : asCALL_CDECL_OBJLAST );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::method (%s::%s) RegisterObjectMethod failed %d", name.c_str(), funcname.c_str(), _id ) );
		}

		return *this;
	}

	// input global function with *this either first or last parameter (obj_first)
	template<typename F>
	Class & method2( F f, const char *fname, bool obj_first ) {
#ifdef __DEBUG_COUT_PRINT__
		std::cout << name << "::method " << fname << std::endl;
#endif
#ifdef __DEBUG_COM_PRINTF__
		Com_Printf( "%s::method %s\n", name.c_str(), fname );
#endif

		int _id = engine->RegisterObjectMethod( name.c_str(), fname,
												asFUNCTION( f ), obj_first ? asCALL_CDECL_OBJFIRST : asCALL_CDECL_OBJLAST );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::method (%s::%s) RegisterObjectMethod failed %d", name.c_str(), fname, _id ) );
		}

		return *this;
	}

	// input global function with *this either first or last parameter (obj_first)
	template<typename F>
	Class & constmethod( F f, const char *fname, bool obj_first ) {
		std::string constname = ( obj_first ?
								  FunctionString( StripThisFirst( f ), fname ) :
								  FunctionString( StripThisLast( f ), fname ) ) + " const";

#ifdef __DEBUG_COUT_PRINT__
		std::cout << name << "::constmethod " << constname << std::endl;
#endif
#ifdef __DEBUG_COM_PRINTF__
		Com_Printf( "%s::constmethod %s\n", name.c_str(), constname.c_str() );
#endif

		int _id = engine->RegisterObjectMethod( name.c_str(), constname.c_str(),
												asFUNCTION( f ), obj_first ? asCALL_CDECL_OBJFIRST : asCALL_CDECL_OBJLAST );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::constmethod (%s::%s) RegisterObjectMethod failed %d", name.c_str(), constname.c_str(), _id ) );
		}

		return *this;
	}

	// BIND MEMBER
	template<typename V>
	Class & member( V T::*v, const char *mname ) {
		std::string fullname = TypeString<V>( mname );

#ifdef __DEBUG_COUT_PRINT__
		std::cout << TypeString<T>() << "::member " << TypeString<V>( mname ) << std::endl;
#endif
#ifdef __DEBUG_COM_PRINTF__
		Com_Printf( "%s::member %s\n", name.c_str(), fullname.c_str() );
#endif

		// int _id = engine->RegisterObjectProperty( name.c_str(), fullname.c_str(), offsetof( T, *pv ) );
		int _id = engine->RegisterObjectProperty( name.c_str(), fullname.c_str(),
												  ( ( size_t ) &( ( (T*)0 )->*v ) ) ); // damn gcc
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::member (%s::%s) RegisterObjectProperty failed %d", name.c_str(), fullname.c_str(), _id ) );
		}

		return *this;
	}

	// BIND MEMBER CONST
	template<typename V>
	Class & constmember( V T::*v, const char *mname ) {
		std::string fullname = TypeString<const V>( mname );

#ifdef __DEBUG_COUT_PRINT__
		std::cout << TypeString<T>() << "::member " << fullname << std::endl;
#endif
#ifdef __DEBUG_COM_PRINTF__
		Com_Printf( "%s::member %s\n", name.c_str(), fullname.c_str() );
#endif

		// int _id = engine->RegisterObjectProperty( name.c_str(), fullname.c_str(), offsetof( T, *pv ) );
		int _id = engine->RegisterObjectProperty( name.c_str(), fullname.c_str(),
												  ( ( size_t ) &( ( (T*)0 )->*v ) ) ); // damn gcc
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::constmember (%s::%s) RegisterObjectProperty failed %d", name.c_str(), fullname.c_str(), _id ) );
		}

		return *this;
	}

	// BEHAVIOURS

	// REFERENCE

	// input methods of the class
	Class & refs( void ( T::*addref )(), void ( T::*release )() ) {
		int _id = engine->RegisterObjectBehaviour( name.c_str(), asBEHAVE_ADDREF,
												   "void f()", asSMethodPtr<sizeof( addref )>::Convert( addref ), asCALL_THISCALL );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::refs (%s) RegisterObjectBehaviour failed %d", name.c_str(), _id ) );
		}

		id = engine->RegisterObjectBehaviour( name.c_str(), asBEHAVE_RELEASE,
											  "void f()", asSMethodPtr<sizeof( release )>::Convert( release ), asCALL_THISCALL );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::refs (%s) RegisterObjectBehaviour failed %d", name.c_str(), _id ) );
		}

		return *this;
	}

	// input global functions
	Class & refs( void ( *addref )( T* ), void ( *release )( T* ) ) {
		int _id = engine->RegisterObjectBehaviour( name.c_str(), asBEHAVE_ADDREF,
												   "void f()", asFUNCTION( addref ), asCALL_CDECL_OBJLAST );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::refs (%s) RegisterObjectBehaviour failed %d", name.c_str(), _id ) );
		}

		id = engine->RegisterObjectBehaviour( name.c_str(), asBEHAVE_RELEASE,
											  "void f()", asFUNCTION( release ), asCALL_CDECL_OBJLAST );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::refs (%s) RegisterObjectBehaviour failed %d", name.c_str(), _id ) );
		}

		return *this;
	}

	// FACTORY
	// input global function that may or may not take parameters
	template<typename F>
	Class & factory( F f ) {
		// FIXME: config CDECL/STDCALL!
		int _id = engine->RegisterObjectBehaviour( name.c_str(), asBEHAVE_FACTORY,
												   FunctionString<F>( "f" ).c_str(), asFUNCTION( f ), asCALL_CDECL );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::factory (%s) RegisterObjectBehaviour failed %d", name.c_str(), _id ) );
		}

		return *this;
	}

	// CONSTRUCTOR
	// input constructor type as <void(parameters)> in template arguments
	template<typename F>
	Class & constructor( void ) {
		int _id = engine->RegisterObjectBehaviour( name.c_str(), asBEHAVE_CONSTRUCT,
												   FunctionString<F>( "f" ).c_str(), asFunctionPtr( CallCtorProxy<T, F>()() ), asCALL_CDECL_OBJFIRST );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::constructor (%s) RegisterObjectBehaviour failed %d", name.c_str(), _id ) );
		}

		return *this;
	}

	// input global function (see helper)
	template<typename F>
	Class & constructor( F f, bool obj_first = false ) {
		std::string funcname = obj_first ?
							   FunctionString<typename StripThisProxy<F>::func_of>( "f" ) :
							   FunctionString<typename StripThisProxy<F>::func_ol>( "f" );

#ifdef __DEBUG_COUT_PRINT__
		std::cout << name << "::constructor " << funcname << std::endl;
#endif
#ifdef __DEBUG_COM_PRINTF__
		Com_Printf( "%s::constructor %s\n", name.c_str(), funcname.c_str() );
#endif

		int _id = engine->RegisterObjectBehaviour( name.c_str(),
												   asBEHAVE_CONSTRUCT, funcname.c_str(), asFUNCTION( f ),
												   obj_first ? asCALL_CDECL_OBJFIRST : asCALL_CDECL_OBJLAST );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::constructor (%s) RegisterObjectBehaviour failed %d", name.c_str(), _id ) );
		}

		return *this;
	}

	// DECONSTRUCTOR
	// automatic object destructor
	Class & destructor() {
		int _id = engine->RegisterObjectBehaviour( name.c_str(), asBEHAVE_DESTRUCT,
												   "void f()", asFUNCTION( CallDestructor<T> ), asCALL_CDECL_OBJLAST );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::destructor (%s) RegisterObjectBehaviour failed %d", name.c_str(), _id ) );
		}

		return *this;
	}

	// input global function (see helper for this)
	Class & destructor( void ( *f )( T* ) ) {
		int _id = engine->RegisterObjectBehaviour( name.c_str(), asBEHAVE_DESTRUCT,
												   "void f()", asFUNCTION( f ), asCALL_CDECL_OBJLAST );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::destructor (%s) RegisterObjectBehaviour failed %d", name.c_str(), _id ) );
		}

		return *this;
	}

	// CAST

	// input method
	template<typename F>
	Class & cast( F f ) {
		std::string funcname = MethodString<F>( "f" );
		int _id = engine->RegisterObjectBehaviour( name.c_str(), asBEHAVE_VALUE_CAST,
												   funcname.c_str(), asSMethodPtr<sizeof( f )>::Convert( f ), asCALL_THISCALL );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::cast (%s) RegisterObjectBehaviour failed %d", name.c_str(), _id ) );
		}

		return *this;
	}

	// input global function with *this either first or last parameter (obj_first)
	template<typename F>
	Class & cast( F f, bool implicit_cast, bool obj_first ) {
		std::string funcname = obj_first ?
							   FunctionString( StripThisFirst( f ), implicit_cast ? "opImplConv" : "opConv" ) :
							   FunctionString( StripThisLast( f ), implicit_cast ? "opImplConv" : "opConv" );

#ifdef __DEBUG_COUT_PRINT__
		std::cout << name << "::cast " << funcname << std::endl;
#endif
#ifdef __DEBUG_COM_PRINTF__
		Com_Printf( "%s::cast %s\n", name.c_str(), funcname.c_str() );
#endif

		int _id = engine->RegisterObjectMethod( name.c_str(), funcname.c_str(),
												   asFUNCTION( f ), obj_first ? asCALL_CDECL_OBJFIRST : asCALL_CDECL_OBJLAST );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::cast (%s::%s) RegisterObjectMethod failed %d", name.c_str(), funcname.c_str(), _id ) );
		}

		return *this;
	}

	// input global function with *this either first or last parameter (obj_first)
	template<typename F>
	Class & refcast( F f, bool implicit_cast, bool obj_first ) {
		std::string funcname = obj_first ?
							   FunctionString( StripThisFirst( f ), implicit_cast ? "opImplCast" : "opCast" ) :
							   FunctionString( StripThisLast( f ), implicit_cast ? "opImplCast" : "opCast" );

#ifdef __DEBUG_COUT_PRINT__
		std::cout << name << "::cast " << funcname << std::endl;
#endif
#ifdef __DEBUG_COM_PRINTF__
		Com_Printf( "%s::cast %s\n", name.c_str(), funcname.c_str() );
#endif

		int _id = engine->RegisterObjectMethod( name.c_str(), funcname.c_str(),
												   asFUNCTION( f ), obj_first ? asCALL_CDECL_OBJFIRST : asCALL_CDECL_OBJLAST );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Class::cast (%s::%s) RegisterObjectMethod failed %d", name.c_str(), funcname.c_str(), _id ) );
		}

		return *this;
	}
};

template<typename T>
Class<T> CreateClass( asIScriptEngine *engine ) { return Class<T>( engine ); }
template<typename T>
Class<T> CreateClass( asIScriptEngine *engine, const char *name )
{ return Class<T>( engine, name ); }

template<typename T>
Class<T> GetClass( asIScriptEngine *engine, const char *name = TypeString<T>( ).c_str() ) {
	std::string sname( name );
	int i, count;

	count = engine->GetObjectTypeCount();
	for( i = 0; i < count; i++ ) {
		asITypeInfo *obj = engine->GetObjectTypeByIndex( i );
		if( obj && sname == obj->GetName() ) {
#ifdef __DEBUG_COUT_PRINT__
			std::cout << "GetClass found class " << name << std::endl;
#endif
#ifdef __DEBUG_COM_PRINTF__
			Com_Printf( "GetClass found class %s\n", name );
#endif

			return Class<T>( engine, name, obj->GetTypeId() );
		}
	}

#ifdef __DEBUG_COUT_PRINT__
	std::cout << "GetClass creating new class " << name << std::endl;
#endif
#ifdef __DEBUG_COM_PRINTF__
	Com_Printf( "GetClass creating new class %s\n", name );
#endif

	return Class<T>( engine, name );
}

//=========================================================

// asbind_global.h
// bind global functions and variables

class Global
{
	asIScriptEngine *engine;

public:
	// constructors
	Global( asIScriptEngine *engine ) : engine( engine )
	{}

	// BIND FUNCTION
	// input global function with *this either first or last parameter (obj_first)
	template<typename F>
	Global & function( F f, const char *fname ) {
		std::string funcname = FunctionString<F>( fname );
		#ifdef __DEBUG_COUT_PRINT__
		std::cout << "Global::function " << funcname << std::endl;
		#endif
		#ifdef __DEBUG_COM_PRINTF__
		Com_Printf( "Global::function %s\n", funcname.c_str() );
		#endif
		int _id = engine->RegisterGlobalFunction( funcname.c_str(), asFUNCTION( f ), asCALL_CDECL );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Global::function (%s) RegisterGlobalFunction failed %d", funcname.c_str(), _id ) );
		}

		return *this;
	}

	// BIND VARIABLE
	template<typename V>
	Global & var( V &v, const char *vname ) {
		std::string varname = TypeString<V>( vname );
		#ifdef __DEBUG_COUT_PRINT__
		std::cout << "Global::var " << varname << std::endl;
		#endif
		#ifdef __DEBUG_COM_PRINTF__
		Com_Printf( "Global::var %s\n", varname.c_str() );
		#endif
		int _id = engine->RegisterGlobalProperty( varname.c_str(), (void*)&v );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Global::var (%s) RegisterGlobalProperty failed %d", varname.c_str(), _id ) );
		}

		return *this;
	}

	// BIND VARIABLE as pointer
	template<typename V>
	Global & var( V *v, const char *vname ) {
		std::string varname = TypeString<V>( vname );
		#ifdef __DEBUG_COUT_PRINT__
		std::cout << "Global::var " << varname << std::endl;
		#endif
		#ifdef __DEBUG_COM_PRINTF__
		Com_Printf( "Global::var %s\n", varname.c_str() );
		#endif
		int _id = engine->RegisterGlobalProperty( varname.c_str(), (void*)v );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Global::var (%s) RegisterGlobalProperty failed %d", varname.c_str(), _id ) );
		}

		return *this;
	}

	// BIND constant
	template<typename V>
	Global & constvar( const V &v, const char *vname ) {
		std::string varname = TypeString<const V>( vname );
		#ifdef __DEBUG_COUT_PRINT__
		std::cout << "Global::constvar " << varname << std::endl;
		#endif
		#ifdef __DEBUG_COM_PRINTF__
		Com_Printf( "Global::constvar %s\n", varname.c_str() );
		#endif
		int _id = engine->RegisterGlobalProperty( varname.c_str(), (void*)&v );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Global::constvar (%s) RegisterGlobalProperty failed %d", varname.c_str(), _id ) );
		}

		return *this;
	}

	// BIND constant as pointer
	template<typename V>
	Global & constvar( const V *v, const char *vname ) {
		std::string varname = TypeString<const V>( vname );
		#ifdef __DEBUG_COUT_PRINT__
		std::cout << "Global::constvar " << varname << std::endl;
		#endif
		#ifdef __DEBUG_COM_PRINTF__
		Com_Printf( "Global::constvar %s\n", varname.c_str() );
		#endif
		int _id = engine->RegisterGlobalProperty( varname.c_str(), (void*)v );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Global::constvar (%s) RegisterGlobalProperty failed %d", varname.c_str(), _id ) );
		}

		return *this;
	}

	// BIND FUNCDEF
	template<typename F>
	Global & funcdef( F f, const char *fname ) {
		std::string funcdefname = FuncdefString<F>( fname );
		#ifdef __DEBUG_COUT_PRINT__
		std::cout << "Global::funcdef " << funcdefname << std::endl;
		#endif
		#ifdef __DEBUG_COM_PRINTF__
		Com_Printf( "Global::funcdef %s\n", funcdefname.c_str() );
		#endif
		int _id = engine->RegisterFuncdef( funcdefname.c_str() );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Global::funcdef (%s) RegisterFuncdef failed %d", funcdefname.c_str(), _id ) );
		}

		return *this;
	}

	// CREATE CLASS
	template<typename T>
	Class<T> class_() {
		return Class<T>( engine );
	}

	template<typename T>
	Class<T> class_( const char *name ) {
		return Class<T>( engine, name );
	}
};

//=========================================================

// asbind_enum.h
// bind enumerations

class Enum
{
	asIScriptEngine *engine;
	std::string name;

public:
	Enum( asIScriptEngine *_engine, const char *_name )
		: engine( _engine ), name( _name ) {
		int id = engine->RegisterEnum( _name );
		if( id < 0 ) {
			throw id;
		}
	}

	Enum & add( const char *key, int value ) {
		int _id = engine->RegisterEnumValue( name.c_str(), key, value );
		if( _id < 0 ) {
			throw Exception( va( "ASBind::Enum::add (%s %s) RegisterEnumValue failed %d", name.c_str(), key, _id ) );
		}

		return *this;
	}

	Enum & operator()( const char *key, int value ) {
		return add( key, value );
	}
};

//=========================================================

// asbind_utils.h
// miscellaneous helpers

// bindable reference objects can derive from this class and
// bind AddRef and Release as behaviour functions
struct RefWrapper {
protected:
	int refcount;

public:
	RefWrapper() : refcount( 1 ) {}
	virtual ~RefWrapper() {}

	void AddRef() { refcount++; }
	void Release() {
		refcount--;
		if( refcount <= 0 ) {
			delete this;
		}
	}
};

//===============================

// FACTORY HELPER (new T)
// std::cout << "Factory_New<" << TypeString<T>() << ">" << std::endl;
template<typename T>
T * New() { return new T(); }
template<typename T>
T * New0() { return new T(); }

template<typename T,typename A1>
T * New( A1 a1 ) { return new T( a1 ); }
template<typename T,typename A1>
T * New1( A1 a1 ) { return new T( a1 ); }

template<typename T, typename A1, typename A2>
T * New( A1 a1, A2 a2 ) { return new T( a1, a2 ); }
template<typename T, typename A1, typename A2>
T * New2( A1 a1, A2 a2 ) { return new T( a1, a2 ); }

template<typename T, typename A1, typename A2, typename A3>
T * New( A1 a1, A2 a2, A3 a3 ) { return new T( a1, a2, a3 ); }
template<typename T, typename A1, typename A2, typename A3>
T * New3( A1 a1, A2 a2, A3 a3 ) { return new T( a1, a2, a3 ); }

template<typename T, typename A1, typename A2, typename A3, typename A4>
T * New( A1 a1, A2&a2, A3 a3, A4 a4 ) { return new T( a1, a2, a3, a4 ); }
template<typename T, typename A1, typename A2, typename A3, typename A4>
T * New4( A1 a1, A2&a2, A3 a3, A4 a4 ) { return new T( a1, a2, a3, a4 ); }

//===============================

// CONSTRUCTOR/DECONSTRUCTOR helpers/wrappers
template<typename T>
void Construct( T *t ) { new( t ) T(); }

template<typename T, typename A1>
void Construct( T *t, A1 a1 ) { new( t ) T( a1 ); }

template<typename T, typename A1, typename A2>
void Construct( T *t, A1 a1, A2 a2 ) { new( t ) T( a1, a2 ); }

template<typename T, typename A1, typename A2, typename A3>
void Construct( T *t, A1 a1, A2 a2, A3 a3 ) { new( t ) T( a1, a2, a3 ); }

template<typename T, typename A1, typename A2, typename A3, typename A4>
void Construct( T *t, A1 a1, A2 a2, A3 a3, A4 a4 ) { new( t ) T( a1, a2, a3, a4 ); }

template<typename T>
void Destruct( T *t ) { t->~T(); }

}

#endif
