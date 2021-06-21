#ifndef __UI_COMMON_H__
#define __UI_COMMON_H__

/*
 * ui_common.h
 * common header inclusions and definitions
 */

#define __WARSOW_CUSTOM_NEW__   1

namespace WSWUI
{

// some useful shortcuts
typedef Rml::Core::Vector2i Vector2i;

//==================================================================

// forward definitions
class UI_Main;
class ASInterface;

//==================================================================

// TODO: move to STL area
inline bool nullary_true( void ) { return true; }

template<typename T>
inline bool unary_true( T t ) { return true; }

template<typename T>
inline bool binary_true( T t1, T t2 ) { return true; }

template<typename C, typename Function>
inline Function for_each( C &container, Function f ) {
	return std::for_each( container.begin(), container.end(), f );
}

template<typename C, typename T>
inline typename C::iterator find( C &container, const T& value ) {
	return std::find( container.begin(), container.end(), value );
}

template<typename C, typename Predicate>
inline typename C::iterator find_if( C &container, Predicate pred ) {
	return std::find_if( container.begin(), container.end(), pred );
}

template<typename C, typename T>
inline typename C::iterator lower_bound( C &container, const T &value ) {
	return std::lower_bound( container.begin(), container.end(), value );
}

template<typename C, typename T, typename Comp>
inline typename C::iterator lower_bound( C &container, const T &value, Comp comp ) {
	return std::lower_bound( container.begin(), container.end(), value, comp );
}

//==================================================================

// TODO: move these to memory.h or smth

// TODO: change all new/delete in warsow ui to use the special macros below
// __new__ and __delete__ (and __deletea for arrays)

// Global allocation functions
inline void *__operator_new__( size_t size ) {
	void *ptr = trap::Mem_Alloc( size, __FILE__, __LINE__ );
	if( !ptr ) {
		throw std::bad_alloc();
	}

	return ptr;
}

// same as above but with external file and line
inline void *__operator_new2__( size_t size, const char *file, int line ) {
	void *ptr = trap::Mem_Alloc( size, file, line );
	if( !ptr ) {
		throw std::bad_alloc();
	}

	return ptr;
}

inline void __operator_delete__( void *ptr ) {
	trap::Mem_Free( ptr, __FILE__, __LINE__ );
}

// as above but with external file and line
inline void __operator_delete2__( void *ptr, const char *file, int line ) {
	trap::Mem_Free( ptr, file, line );
}

/*
    Placement new/delete macros
    TODO: change all new/delete in warsow ui to use the special macros below

        normal new:		Object *o = new Object(param);
        placement new:	Object *o = new(memory) Object(param);
        macro version:	Object *o = __new__(Object)(param); <--- USE

        for arrays we have:	Object *ous = new Object[500];
        placement new:		Object *ous = new(memory) Object[500];
        macro version:		Object *ous = __newa_(Object, 500); <--- USE

        yes you had to put the array specifier inside parenthesis :/

        normal delete:	delete o;
        placement delete:	K-18
        macro delete:	__delete__(o);	<-- USE

        array delete:	delete[] o;
        placement delete:	K-18
        macro delete	__deletea__(ous, number_of_ous);	<-- USE

    note that on simple-type arrays you can use __delete__ because __deletea__
    exists only for those that need to have their destructor called.
    __delete__ is specialized for simple types to not call destructors for them.
*/

#ifdef __WARSOW_CUSTOM_NEW__
	#define __new__( T )  new( WSWUI::__operator_new2__( sizeof( T ), __FILE__, __LINE__ ) )( T )
	#define __newa__( T,C ) new( WSWUI::__operator_new2__( sizeof( T ) * ( C ), __FILE__, __LINE__ ) )( T )

// delete requires templates to deduce the destructor
template<typename T>
inline void __delete__impl__( T *ptr, const char *file, int line ) {
	// call the destructor
	ptr->~T();
	// free the memory
	trap::Mem_Free( ptr, file, line );
}

// same for arrays, require size/count parameter
template<typename T>
inline void __deletea__impl__( T *ptr, size_t count, const char *file, int line ) {
	// call the destructors
	while( count )
		ptr[--count].~T();
	// free the memory
	trap::Mem_Free( ptr, file, line );
}

// specializations for simple types that dont have destructor
	#define __delete_no_destructor( type )    \
	template<>      \
	inline void __delete__impl__<type>( type * ptr, const char *file, int line ) \
	{   trap::Mem_Free( ptr, file, line );  }   \
	template<>      \
	inline void __deletea__impl__<type>( type * ptr, size_t count, const char *file, int line )  \
	{   trap::Mem_Free( ptr, file, line ); }

// c++ simple types
__delete_no_destructor( bool )
__delete_no_destructor( char )
__delete_no_destructor( signed char )
__delete_no_destructor( unsigned char )
__delete_no_destructor( wchar_t )
__delete_no_destructor( short )
__delete_no_destructor( unsigned short )
__delete_no_destructor( int )
__delete_no_destructor( unsigned int )
__delete_no_destructor( long )
__delete_no_destructor( unsigned long )
__delete_no_destructor( float )
__delete_no_destructor( double )
__delete_no_destructor( long double )
__delete_no_destructor( void )

// and then the macros for __delete__ / __deletea__
	#define __delete__( ptr ) WSWUI::__delete__impl__( ( ptr ), __FILE__, __LINE__ )
	#define __deletea__( ptr,count ) WSWUI::__delete__impl__( ( ptr ), ( count ), __FILE__, __LINE__ )
#else
	#define __new__( T )              new ( T )
	#define __newa__( T,C )           new ( sizeof( T ) * ( C ) )
	#define __delete__( ptr )         delete ( ptr )
	#define __deletea__( ptr,count )  delete ( ptr )
#endif

	#define __SAFE_DELETE_NULLIFY( a ) \
	if( ( a ) ) { __delete__( a ); a = nullptr; }

//==========================================

// stl - TODO: move to separate wsw_stl.h
// TODO: custom allocator to interface with trap::Mem_Alloc/Mem_Free
// TODO: research EASTL

/*
    Container adapters have 2 versions
    __adapter - used default container
    __adapterc - takes container as default parameter

    Map/set also have 2 versions
    __type - uses default comparison operation
    __typec - takes comparison operator as default parameter

    rationale to do like this:
    1) makes using custom allocators easier,
    cause they will be included in the macro
        #define __stl_list(T)	std::list<T, my_custom_allocator<T> >

    so instead of
        std::list<int, my_custom_allocator<int> > ilist;
        std::list<int, my_custom_allocator<int> >::iterator it = ilist.begin();

    we can do
        __stl_list(int)	ilist;
        __stl_list(int)::iterator it = ilist.begin();

    2) using alt. stl implementation optimized for games, like EASTL
       (there we could have just declared our own namespace replacer,
        that could've been either std:: or EASTL::, but that wouldnt
        resolve 1)

    // #define __stl_namespace		std
    #define __stl_namespace			EASTL

        #define __stl_list(T)	__stl_namespace::list<T>
        #define __stl_vector(T)	__stl_namespace::vector<T>
*/

// basic containers
	#define __stl_deque( T )      std::deque<T>
	#define __stl_list( T )       std::list<T>
	#define __stl_vector( T )     std::vector<T>

// container adapters
// TODO: std::priority_queue
	#define __stl_queue( T )      std::queue<T>
	#define __stl_queuec( T,C )   std::queue<T,C>
	#define __stl_stack( T )      std::stack<T>
	#define __stl_stackc( T,C )   std::stack<T,C>

// map/set (TODO: hash_*)
	#define __stl_map( T,U )          std::map<T,U>
	#define __stl_mapc( T,U,Comp )    std::map<T,U,Comp>
	#define __stl_set( T )            std::set<T>
	#define __stl_setc( T,Comp )      std::set<T,Comp>
	#define __stl_multimap( T,U )         std::multimap<T,U>
	#define __stl_multimapc( T,U,Comp )   std::multimap<T,U,Comp>
	#define __stl_multiset( T )           std::multiset<T>
	#define __stl_multisetc( T,Comp )     std::multiset<T,Comp>

// misc
	#define __stl_bitset( N )     std::bitset<N>
// TODO; this can be typedef'd
// TODO: basic_string<char>
	#define __stl_string        std::string
	#define __stl_stringstream  std::stringstream
}

// these are redundant after __new__ and __delete__?
#define __DEFINE_ALLOCS void *operator new( size_t size ); void operator delete( void *p );
#define __IMPLEMENT_ALLOCS( classname )       \
	void *classname::operator new( size_t size ) { return WSWUI::__operator_new__( size ); }    \
	void *classname::operator new[]( size_t size ) { return WSWUI::__operator_new__( size ); }  \
	void classname::operator delete( void *ptr ) { WSWUI::__operator_delete( ptr ); } \
	void classname::operator delete[]( void *ptr ) { WSWUI::__operator_delete( ptr ); }

#define __IMPLEMENT_ALLOCS_INLINE       \
	void *operator new( size_t size ) { return WSWUI::__operator_new__( size ); }   \
	void *operator new[]( size_t size ) { return WSWUI::__operator_new__( size ); } \
	void operator delete( void *ptr ) { WSWUI::__operator_delete( ptr ); } \
	void operator delete[]( void *ptr ) { WSWUI::__operator_delete( ptr ); }

#endif
