// Copyright (c) Zhirnov Andrey. For more information see 'LICENSE'

#include <cstdint>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>

#define ND_				[[nodiscard]]
#define OUT
#define INOUT
#define null			nullptr
#define STATIC_ASSERT	static_assert

namespace AE::PipelineCompiler
{
	using uint			= uint32_t;
	using slong			= int64_t;
	using ulong			= uint64_t;
	using usize			= size_t;
	using ssize			= intptr_t;

	using String		= std::string;
	using StringView	= std::string_view;
	using Path			= std::filesystem::path;

	template <typename T>				using Array			= std::vector< T >;
	template <typename F, typename S>	using Pair			= std::pair< F, S >;
	template <typename T, usize S>		using StaticArray	= std::array< T, S >;

	template <typename T, typename H = std::hash<T>>
	using HashSet = std::unordered_set< T, H >;

	template <typename K, typename V, typename H = std::hash<K>>
	using HashMap = std::unordered_map< K, V, H >;


	struct Bytes
	{
	private:
		ulong	_value	= 0;

	public:
		Bytes () {}
		explicit Bytes (ulong val) : _value{val} {}

		explicit operator ulong ()	const	{ return _value; }
	};
	

	struct _UMax
	{
		template <typename T>
		ND_ constexpr operator const T () const
		{
			STATIC_ASSERT( T(~T{0}) > T{0} );
			return T(~T{0});
		}

		template <typename T>
		ND_ friend constexpr bool  operator == (const T& left, const _UMax &right) {
			return T(right) == left;
		}

		template <typename T>
		ND_ friend constexpr bool  operator != (const T& left, const _UMax &right) {
			return T(right) != left;
		}
	};
	static constexpr _UMax	UMax {};


	struct _DefaultType
	{
		constexpr _DefaultType () {}
		
		template <typename T>
		ND_ static constexpr T  _GetDefault ()
		{
			if constexpr( std::is_enum_v<T> )
				return T::Unknown;

			if constexpr( std::is_floating_point_v<T>	or
						  std::is_integral_v<T>			or
						  std::is_pointer_v<T> )
				return T{0};

			// default ctor
			return {};
		}

		template <typename T>
		ND_ constexpr operator T () const {
			return _GetDefault<T>();
		}

		template <typename T>
		ND_ friend constexpr bool  operator == (const T& lhs, const _DefaultType &) {
			return lhs == _GetDefault<T>();
		}

		template <typename T>
		ND_ friend constexpr bool  operator != (const T& lhs, const _DefaultType &rhs) {
			return not (lhs == rhs);
		}
	};
	static constexpr _DefaultType	Default = {};
	

	template <typename T>
	struct THashVal final
	{
	private:
		T	_value	= 0;

	public:
		constexpr THashVal ()									{}
		explicit constexpr THashVal (T val)						: _value{val} {}

		template <typename B>
		explicit constexpr THashVal (THashVal<B> h)				: _value{static_cast<T>(B{h})} {}

		ND_ constexpr bool	operator == (const THashVal &rhs)	const	{ return _value == rhs._value; }
		ND_ constexpr bool	operator != (const THashVal &rhs)	const	{ return not (*this == rhs); }
		ND_ constexpr bool	operator >  (const THashVal &rhs)	const	{ return _value > rhs._value; }
		ND_ constexpr bool  operator <  (const THashVal &rhs)	const	{ return _value < rhs._value; }

		constexpr THashVal&  operator << (const THashVal &rhs)			{ Append( rhs );  return *this; }
		constexpr THashVal&  operator += (const THashVal &rhs)			{ Append( rhs );  return *this; }

		constexpr void  Append (const THashVal &rhs)
		{
			const T	mask	= (sizeof(_value)*8 - 1);
			T		val		= rhs._value;
			T		shift	= 8;

			shift  &= mask;
			_value ^= (val << shift) | (val >> ( ~(shift-1) & mask ));
		}

		ND_ constexpr const THashVal<T>  operator + (const THashVal<T> &rhs) const
		{
			return THashVal<T>(*this) << rhs;
		}

		ND_ explicit constexpr operator T ()					const	{ return _value; }
	};
	using HashVal = THashVal<usize>;


	template <typename T>
	ND_ HashVal  HashOf (const T &value)
	{
		return HashVal( std::hash<T>()( value ));
	}

	
	template <typename LT, typename RT>
	ND_ constexpr auto  Min (const LT &lhs, const RT &rhs)
	{
		if constexpr( std::is_same_v<LT, RT> )
		{
			return lhs > rhs ? rhs : lhs;
		}
		else
		{
			using T = decltype(lhs + rhs);
			return T(lhs) > T(rhs) ? T(rhs) : T(lhs);
		}
	}

	template <typename LT, typename RT>
	ND_ constexpr auto  Max (const LT &lhs, const RT &rhs)
	{
		if constexpr( std::is_same_v<LT, RT> )
		{
			return lhs > rhs ? lhs : rhs;
		}
		else
		{
			using T = decltype(lhs + rhs);
			return lhs > rhs ? T(lhs) : T(rhs);
		}
	}

	template <typename T0, typename ...Types>
	ND_ constexpr auto  Max (const T0 &arg0, const Types& ...args)
	{
		if constexpr( sizeof...(Types) == 0 )
			return arg0;
		else
		if constexpr( sizeof...(Types) == 1 )
			return Max( arg0, args... );
		else
			return Max( arg0, Max( args... ));
	}

	
	template <typename... Args>
	constexpr void  Unused (Args&& ...) {}

	template <typename T>
	ND_ constexpr std::remove_reference_t<T> &&  RVRef (T& arg)
	{
		return static_cast< std::remove_reference_t<T> && >( arg );
	}

	template <typename T>
	ND_ constexpr T &&  FwdArg (std::remove_reference_t<T>& arg)
	{
		return static_cast<T &&>( arg );
	}

	template <typename T>
	ND_ constexpr T &&  FwdArg (std::remove_reference_t<T>&& arg)
	{
		STATIC_ASSERT( not std::is_lvalue_reference_v<T> );
		return static_cast<T &&>( arg );
	}


	template <typename T>
	ND_ constexpr ssize  Distance (T *lhs, T *rhs) {
		return std::distance< T *>( lhs, rhs );
	}

	template <typename T>
	ND_ constexpr ssize  Distance (const T *lhs, T *rhs) {
		return std::distance< T const *>( lhs, rhs );
	}

	template <typename T>
	ND_ constexpr ssize  Distance (T *lhs, const T *rhs) {
		return std::distance< T const *>( lhs, rhs );
	}


	template <typename Lhs, typename Rhs0, typename ...Rhs>
	ND_ constexpr bool  AnyEqual (const Lhs &lhs, const Rhs0 &rhs0, const Rhs& ...rhs)
	{
		if constexpr( sizeof... (Rhs) == 0 )
			return ( lhs == rhs0 );
		else
			return ( lhs == rhs0 ) | AnyEqual( lhs, rhs... );
	}
}
