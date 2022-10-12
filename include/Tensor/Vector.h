#pragma once

/*
NEW VERSION
- no more template metaprograms, instead constexprs
- no more helper structs, instead requires
- no more lower and upper
- modeled around glsl
- maybe extensions into matlab syntax (nested tensor implicit * contraction mul)
- math indexing.  A.i.j := a_ij, which means row-major storage (sorry OpenGL)

TODO:
	index notation still


alright conventions, esp with my _sym being in the mix ...
operator[i] will denote a single index.  this means for two-rank structs they will have to return an accessor object
operator(i) same
operator(i,j,k,...) should always be allowed for any number of index dereferencing ... which means it will need to be implemented on accessors as well ... and means it may return an accessor

for GLSL / CL convention, every struct should have when possible:
	.s[]  (except for reference types ofc ... but why am I even mixing them in?  cuz swizzle and std::tie)
direct access to the data ... should be done via .s[] for now at least

operators ... scalar / tensor, tensor/scalar, tensor/tensor

the * operator for tensor/tensor should be an outer+contract, ex:
	rank-2 a * rank-2 b = a_ij * b_jk = rank-2 c_ik (matrix mul)
	rank-1 a * rank-1 b = rank-0 c (dot product) ... tho for GLSL compat this is a per-element vector
	rank-3 a * rank-3 b = a_ijk * b_klm = c_ijlm


if I do row-major then
	- C bracket ctor is in the same layout as the matrix
	- C notation matches matrix notation : A[i0][i1] = A_i0_i1
	- memory layout is transposed from nested index order: A_i0_i1 = A[i1 + i0 * size0]
if I do column-major then C inline indexing is transposed
	- C bracket ctor is transposed from the layout of the matrix
	- matrix notation is transposed: A[j][i] == A_ij
	- memory layout matches nested index order: A_i0_i1 = A[i0 + size0 * i1]
	- OpenGL uses this too.
...Row Major it is.

*/

#include "Tensor/AntiSymRef.h"
#include "Tensor/Meta.h"
#include "Common/String.h"
#include "Common/Exception.h"
#include <tuple>
#include <functional>	//reference_wrapper, also function<> is by Partial
#include <cmath>		//sqrt()

#ifdef DEBUG
#define TENSOR_USE_BOUNDS_CHECKING
#endif

#ifdef TENSOR_USE_BOUNDS_CHECKING
#define TENSOR_INSERT_BOUNDS_CHECK(index)\
	if (index < 0 || index >= localCount) throw Common::Exception() << "tried to read oob index " << index << " for size " << localCount << " type " << typeid(This).name();
#else
#define TENSOR_INSERT_BOUNDS_CHECK(index)
#endif

namespace Tensor {

template<typename A, typename B>
concept IsBinaryTensorOp =
	is_tensor_v<A>
	&& is_tensor_v<B>
//	&& std::is_same_v<typename A::Scalar, typename B::Scalar>	// TODO meh?
;

template<typename T>
concept IsSquareTensor =
	is_tensor_v<T>
	&& T::isSquare;

template<typename A, typename B>
concept IsBinaryTensorOpWithMatchingNeighborDims =
	IsBinaryTensorOp<A,B>
	&& A::template dim<A::rank-1> == B::template dim<0>;

template<typename A, typename B>
concept IsBinaryTensorR3xR3Op =
	IsBinaryTensorOp<A,B>
	&& A::dims == 3
	&& B::dims == 3;

template<typename A, typename B>
concept IsBinaryTensorDiffTypeButMatchingDims =
	IsBinaryTensorOp<A,B>
	&& !std::is_same_v<A,B>
	&& A::dims == B::dims; // equal types means we use .operator== which is constexpr
	

//forward-declare everything

template<typename Inner, int localDim> struct _vec;
template<typename Inner, int localDim> struct _ident;
template<typename Inner, int localDim> struct _sym;
template<typename Inner, int localDim> struct _asym;
template<typename Inner, int localDim, int localRank> struct _symR;
template<typename Inner, int localDim, int localRank> struct _asymR;

//convention?  row-major to match math indexing, easy C inline ctor,  so A_ij = A[i][j]
// ... but OpenGL getFloatv(GL_...MATRIX) uses column-major so uploads have to be transposed
// ... also GLSL is column-major so between this and GLSL the indexes have to be transposed.
template<typename T, int dim1, int dim2> using _mat = _vec<_vec<T, dim2>, dim1>;


// specific-sized templates
template<typename T> using _vec2 = _vec<T,2>;
template<typename T> using _vec3 = _vec<T,3>;
template<typename T> using _vec4 = _vec<T,4>;
template<typename T> using _mat2x2 = _vec2<_vec2<T>>;
template<typename T> using _mat2x3 = _vec2<_vec3<T>>;
template<typename T> using _mat2x4 = _vec2<_vec4<T>>;
template<typename T> using _mat3x2 = _vec3<_vec2<T>>;
template<typename T> using _mat3x3 = _vec3<_vec3<T>>;
template<typename T> using _mat3x4 = _vec3<_vec4<T>>;
template<typename T> using _mat4x2 = _vec4<_vec2<T>>;
template<typename T> using _mat4x3 = _vec4<_vec3<T>>;
template<typename T> using _mat4x4 = _vec4<_vec4<T>>;
template<typename T> using _sym2 = _sym<T,2>;
template<typename T> using _sym3 = _sym<T,3>;
template<typename T> using _sym4 = _sym<T,4>;
template<typename T> using _asym2 = _asym<T,2>;
template<typename T> using _asym3 = _asym<T,3>;
template<typename T> using _asym4 = _asym<T,4>;

//index-access classes
struct IndexBase {};

template<char ident>
struct Index : public IndexBase {};

//forward-declare for index-access
template<typename Tensor_, typename IndexVector>
struct IndexAccess;

//forward-declare, body is below all tensor classes.
template<typename T>
requires is_tensor_v<T>
T elemMul(T const & a, T const & b);

template<typename A, typename B>
requires IsBinaryTensorOp<A, B>
typename A::Scalar inner(A const & a, B const & b);

template<typename T> requires is_tensor_v<T>
typename T::Scalar lenSq(T const & v);

template<typename T> requires (is_tensor_v<T>)
typename T::Scalar length(T const & v);

template<typename A, typename B>
requires IsBinaryTensorOp<A, B>
typename A::Scalar distance(A const & a, B const & b);

template<typename T>
requires (is_tensor_v<T>)
T normalize(T const & v);

template<typename A, typename B>
requires IsBinaryTensorR3xR3Op<A,B>
auto cross(A const & a, B const & b);

template<typename A, typename B>
requires IsBinaryTensorOp<A, B>
auto outer(A const & a, B const & b);

template<typename... T>
auto outerProduct(T&&... args);

template<int m=0, int n=1, typename T>
requires (
	is_tensor_v<T>
	&& T::rank >= 2
)
auto transpose(T const & t);

template<int m=0, int n=1, typename T>
requires (is_tensor_v<T>
	&& m < T::rank
	&& n < T::rank
	&& T::template dim<m> == T::template dim<n>
)
auto contract(T const & t);

template<int m=0, int n=1, typename T>
auto trace(T const & o);

template<int index=0, int count=1, typename A>
requires (is_tensor_v<A>)
auto contractN(A const & a);

template<int num=1, typename A, typename B>
requires IsBinaryTensorOp<A,B>
auto interior(A const & a, B const & b);

template<int m=0, typename T>
requires (is_tensor_v<T>)
auto diagonal(T const & t);

template<typename T>
requires IsSquareTensor<T>
auto makeSym(T const & t);

template<typename T>
requires IsSquareTensor<T>
auto makeAsym(T const & t);

template<typename A, typename B>
requires IsBinaryTensorOp<A,B>
auto wedge(A const & a, B const & b);

template<typename T>
requires IsSquareTensor<T>
auto hodgeDual(T const & a);

template<typename A, typename B>
requires IsBinaryTensorOpWithMatchingNeighborDims<A,B>
auto operator*(A const & a, B const & b);

// these are in Inverse.h:
// they are giving me link errors.
// do I have to forward-declare all template partial specializations to fix it?
// idk, never fixed it, just avoid using .determinant() instead 

template<typename T>
requires is_tensor_v<T>
inline typename T::Scalar determinant(T const & a);

template<typename T>
requires is_tensor_v<T>
inline T inverse(
	T const & a,
	typename T::Scalar const & det
);

template<typename T>
requires is_tensor_v<T>
inline T inverse(T const & a);




// Template<> is used for rearranging internal structure when performing linear operations on tensors
// Template can't go in TENSOR_HEADER cuz Quat<> uses TENSOR_HEADER and doesn't fit the template form
#define TENSOR_THIS(classname)\
	using This = classname;\
	static constexpr bool isTensorFlag = {};

#define TENSOR_SET_INNER_LOCALDIM_LOCALRANK(Inner_, localDim_, localRank_)\
\
	/*  This is the next most nested class, so vector-of-vector is a matrix. */\
	using Inner = Inner_;\
\
	/* this is this particular dimension of our vector */\
	/* M = matrix<T,i,j> == vec<vec<T,j>,i> so M::localDim == i and M::Inner::localDim == j  */\
	/*  i.e. M::dims = int2(i,j) and M::dim<0> == i and M::dim<1> == j */\
	static constexpr int localDim = localDim_;\
\
	/*how much does this structure contribute to the overall rank. */\
	/* for _vec it is 1, for _sym it is 2 */\
	static constexpr int localRank = localRank_;

// used by _quat
#define TENSOR_TEMPLATE_T(classname)\
\
	template<typename Inner2>\
	using Template = classname<Inner2>;\
\
	template<typename NewInner>\
	using ReplaceInner = Template<NewInner>;\
\
	template<int newLocalDim>\
	using ReplaceLocalDim = Template<Inner>;

#define TENSOR_TEMPLATE_T_I(classname)\
\
	template<typename Inner2, int localDim2>\
	using Template = classname<Inner2, localDim2>;\
\
	template<typename NewInner>\
	using ReplaceInner = Template<NewInner, localDim>;\
\
	template<int newLocalDim>\
	using ReplaceLocalDim = Template<Inner, newLocalDim>;

// used for ExpandIthIndex.  all tensors use _tensorr, except _symR and _asymR can use _symR and _asymR.
#define TENSOR_EXPAND_TEMPLATE_TENSORR()\
	template<int index>\
	using ExpandTensorRankTemplate = _tensorr<Inner, localDim, localRank>;

#if 0
/* 
ok I enjoyed the model of inner classes that look like:
template<...> struct ... {
	static constexpr auto value() {
		if constexpr (cond1) {
			return TypeWrapper<type1>();
		} else if constexpr (cond2) {
			return TypeWrapper<type2>();
		} else ...
	}
	using type = typename decltype(value())::type;
};
... but the compile times are ridiculously slow, and I suspect this might be one reason why.
Also with gdb it will actually list out all these inner classes, which makes debugging a pain (good thing lldb doesn't).
*/
namespace details {

template<typename T>
struct ScalarImpl {
	using type = T;
};
template<typename T>
requires is_tensor_v<T> // or TODO 'requires has_Scalar_v` ?
struct ScalarImpl<T> {
	using type = typename ScalarImpl<typename T::Inner>::type;
};

}
#endif

/*
This contains definitions of types and values used in all tensors.
Defines the following:
Scalar
rank
intN
numNestings
intW
ReplaceNested<int, typename>
ExpandIthIndex<int> ... requires numNestingsToIndex<int>
ExpandIndex<int...>
ExpandIndexSeq<std::integer_sequence<T, T...>>
ExpandAllIndexes<>
RemoveIthNesting<int>
RemoveIthIndex<int>
RemoveIndex<int...>
RemoveIndexSeq<std::integer_sequence<T, T...>>
ReplaceDim<int, int>
Nested<int>
count<int>
numNestingsToIndex<int>
InnerForIndex<int>
dim<int>
dims
ReplaceScalar<typename>
*/
#define TENSOR_HEADER()\
\
	/* TENSOR_HEADER goes right after the struct-specific header which defines localCount...*/\
	static_assert(localCount > 0);\
\
	/*  this is the child-most nested class that isn't in our math library. */\
	struct ScalarImpl {\
		static constexpr auto value() {\
			if constexpr (is_tensor_v<Inner>) {\
				return Inner::ScalarImpl::value();\
			} else {\
				return TypeWrapper<Inner>();\
			}\
		}\
		using type = typename decltype(value())::type;\
	};\
	using Scalar = typename ScalarImpl::type;\
\
	/* how many _vec<_vec<...'s there are ... no extra +'s for multi-rank nestings */\
	/* so _sym<> has numNestings=1 and rank=2, _vec<> has numNestings=1 and rank=1, _vec_vec<>>==_mat<> has numNestings=2 and rank=2 */\
	struct NumNestingImpl {\
		static constexpr int value() {\
			if constexpr (is_tensor_v<Inner>) {\
				return 1 + Inner::NumNestingImpl::value();\
			} else {\
				return 1;\
			}\
		};\
	};\
	static constexpr int numNestings = NumNestingImpl::value();\
\
	/* Get the first index that this nesting represents. */\
	template<int nest>\
	struct IndexForNestingImpl {\
		static constexpr int value() {\
			if constexpr (nest == 0) {\
				return 0;\
			} else if constexpr (nest == 1) {\
				return localRank;\
			} else {\
				return localRank + Inner::template IndexForNestingImpl<nest-1>::value();\
			}\
		}\
	};\
	template<int nest>\
	static constexpr int indexForNesting = IndexForNestingImpl<nest>::value();\
\
	/*  this is the rank/degree/index/number of letter-indexes of your tensor. */\
	/*  for vectors-of-vectors this is the nesting. */\
	/*  if you use any (anti?)symmetric then those take up 2 ranks / 2 indexes each instead of 1-rank each. */\
	/**/\
	static constexpr int rank = indexForNesting<numNestings>;\
\
	/* used for vector-dereferencing into the tensor. */\
	using intN = _vec<int,rank>;\
\
	/* used for write-iterators and converting write-iterator indexes to read index vectors */\
	using intW = _vec<int,numNestings>;\
\
	/* used for getLocalReadForWriteIndex() return type */\
	/* and ofc the write local index is always 1 */\
	using intNLocal = _vec<int,localRank>;\
\
	template<int i, typename NewNestedInner>\
	struct ReplaceNestedImpl {\
		static constexpr auto value() {\
			if constexpr (i == 0) {\
				return NewNestedInner();\
			} else {\
				using NewType = typename Inner::template ReplaceNestedImpl<i-1, NewNestedInner>::type;\
				return ReplaceInner<NewType>();\
			}\
		}\
		using type = decltype(value());\
	};\
	template<int i, typename NewType>\
	using ReplaceNested = typename ReplaceNestedImpl<i, NewType>::type;\
\
	/* expand the storage of the i'th index */\
	template<int index>\
	struct ExpandIthIndexImpl {\
		static_assert(index >= 0 && index < rank);\
		/* another This::template that behaves inside a static constexpr method ... */\
		static constexpr auto value() {\
			constexpr int nest = numNestingsToIndex<index>;\
			using N = typename This::template Nested<nest>;\
			using R = typename This\
				::ReplaceNested<\
					nest,\
					typename N::template ExpandTensorRankTemplate<\
						index - indexForNesting<nest>\
					>\
				>;\
			return R();\
		}\
		using type = decltype(value());\
	};\
	template<int index>\
	using ExpandIthIndex = typename ExpandIthIndexImpl<index>::type;\
\
	template<int i1, int... I>\
	struct ExpandIndexImpl {\
		using type = typename This\
			::template ExpandIthIndex<i1>\
			::template ExpandIndexImpl<I...>::type;\
	};\
	template<int i1>\
	struct ExpandIndexImpl<i1> {\
		using type = This::template ExpandIthIndex<i1>;\
	};\
	template<int... I>\
	using ExpandIndex = typename ExpandIndexImpl<I...>::type;\
\
	template<typename Seq>\
	struct ExpandIndexSeqImpl {};\
	template<int... I>\
	struct ExpandIndexSeqImpl<std::integer_sequence<int, I...>> {\
		using type = ExpandIndex<I...>;\
	};\
	template<typename Seq>\
	using ExpandIndexSeq = typename ExpandIndexSeqImpl<Seq>::type;\
\
	template<int deferRank = rank> /* evaluation needs to be deferred */\
	using ExpandAllIndexes = ExpandIndexSeq<std::make_integer_sequence<int, deferRank>>;\
\
	/* remove the i'th nesting, i from 0 to numNestings-1 */\
	template<int i>\
	struct RemoveIthNestingImpl {\
		static constexpr auto value() {\
			if constexpr (i == 0) {\
				return This::Inner();\
			} else {\
				using recursiveCase = typename Inner::template RemoveIthNestingImpl<i-1>::type;\
				return ReplaceInner<recursiveCase>();\
			}\
		}\
		using type = decltype(value());\
	};\
	template<int i>\
	using RemoveIthNesting = typename RemoveIthNestingImpl<i>::type;\
\
	/* same as Expand but now with RemoveIthIndex, RemoveIndex, RemoveIndexSeq */\
	template<int i>\
	struct RemoveIthIndexImpl {\
		static constexpr auto value() {\
			using expanded = This::template ExpandIthIndex<i>;\
			constexpr int indexNesting = expanded::template numNestingsToIndex<i>;\
			return typename expanded::template RemoveIthNesting<indexNesting>();\
		}\
		using type = decltype(value());\
	};\
	template<int i>\
	using RemoveIthIndex = typename RemoveIthIndexImpl<i>::type;\
\
	/* assumes Seq is a integer_sequence<int, ...> */\
	/*  and assumes it is already sorted */\
	template<typename Seq>\
	struct RemoveIndexImpl;\
	template<int i1, int... I>\
	struct RemoveIndexImpl<std::integer_sequence<int, i1,I...>>  {\
		using tmp = This::template RemoveIthIndex<i1>;\
		using type = typename tmp\
			::template RemoveIndexImpl<\
				std::integer_sequence<int, I...>\
			>::type;\
	};\
	template<int i1>\
	struct RemoveIndexImpl<std::integer_sequence<int, i1>> {\
		using type = This::template RemoveIthIndex<i1>;\
	};\
	/* RemoveIndex sorts the list, so you don't need to worry about order of arguments */\
	template<int... I>\
	using RemoveIndex = typename RemoveIndexImpl<\
		Common::seq_reverse_t<\
			Common::seq_sort_t<\
				std::integer_sequence<int, I...>\
			>\
		>\
	>::type;\
\
	template<typename Seq>\
	struct RemoveIndexSeqImpl {};\
	template<int... I>\
	struct RemoveIndexSeqImpl<std::integer_sequence<int, I...>> {\
		using type = RemoveIndex<I...>;\
	};\
	template<typename Seq>\
	using RemoveIndexSeq = typename RemoveIndexSeqImpl<Seq>::type;\
\
	template<int index, int newDim>\
	struct ReplaceDimImpl {\
		static constexpr auto value() {\
			if constexpr (This::template dim<index> == newDim) {\
				return TypeWrapper<This>();\
			} else {\
				using expanded = typename This\
					::template ExpandIndex<index>;\
				using type = typename expanded\
					::template ReplaceNested<\
						expanded::template numNestingsToIndex<index>,\
						typename expanded\
							::template InnerForIndex<index>\
							::template ReplaceLocalDim<newDim>\
					>;\
				return TypeWrapper<type>();\
			}\
		}\
		using type = typename decltype(value())::type;\
	};\
	template<int index, int newDim>\
	using ReplaceDim = typename ReplaceDimImpl<index, newDim>::type;\
\
	template<int index>\
	struct NestedImpl {\
		static constexpr auto value() {\
			static_assert(index >= 0);\
			if constexpr (index >= numNestings) {\
				return TypeWrapper<Scalar>();\
			} else if constexpr (index == 0) {\
				return TypeWrapper<This>();\
			} else {\
				return Inner::template NestedImpl<index-1>::value();\
			}\
		}\
		using type = typename decltype(value())::type;\
	};\
	template<int index>\
	using Nested = typename NestedImpl<index>::type;\
\
	template<int i>\
	static constexpr int count = Nested<i>::localCount;\
\
	/* get the number of nestings to the j'th index */\
	template<int index>\
	struct NestingForIndexImpl {\
		static constexpr int value() {\
			static_assert(index >= 0 && index <= rank);\
			/* to get the scalar-most nesting, query rank */\
			if constexpr (index == rank) {\
				return numNestings;\
			} else if constexpr (index < localRank) {\
				return 0;\
			} else {\
				using Impl = typename Inner::template NestingForIndexImpl<index - localRank>;\
				return 1 + Impl::value();\
			}\
		}\
	};\
	template<int index>\
	static constexpr int numNestingsToIndex = NestingForIndexImpl<index>::value();\
\
	/* todo just 'Inner' and Inner => 'LocalInner' ? */\
	template<int index>\
	using InnerForIndex = Nested<numNestingsToIndex<index>>;\
\
	template<int index>\
	static constexpr int dim = InnerForIndex<index>::localDim;\
\
	/* .. then for loop iterator in dims() function and just a single exceptional case in the dims() function is needed */\
	static constexpr auto DimsImpl() {\
		/* if this is a vector-of-scalars, such that the dims would be an int1, just use int */\
		if constexpr (rank == 1) {\
			return localDim;\
		} else {\
			/* use an int[localDim] */\
			intN dimv;\
			/* TODO constexpr for loop.  I could use my template-based one, but that means one more inline'd class, which is ugly. */\
			/* TODO can I change the template unroll metaprogram to use constexpr lambdas? */\
			/* static_foreach<localRank>([&dimv](int i) constexpr {\
				dimv.s[i] = localDim;\
			}); */\
			for (int i = 0; i < localRank; ++i) {\
				dimv.s[i] = localDim;\
			}\
			if constexpr (localRank < rank) {\
				/* special case reading from int */\
				if constexpr (Inner::rank == 1) {\
					dimv.s[localRank] = Inner::DimsImpl();\
				} else {\
					/* assigning sub-vector */\
					auto innerDim = Inner::DimsImpl();\
					for (int i = 0; i < rank-localRank; ++i) {\
						dimv.s[i+localRank] = innerDim.s[i];\
					}\
				}\
			}\
			return dimv;\
		}\
	}\
	static constexpr auto dims = DimsImpl();\
	/* TODO index_sequence of dims? */\
\
	struct IsSquareImpl {\
		static constexpr bool value() {\
			if constexpr (is_tensor_v<Inner>) {\
				return Inner::isSquare && Inner::localDim == localDim;\
			} else {\
				return true;\
			}\
		}\
	};\
	static constexpr bool isSquare = IsSquareImpl::value();\
\
	template<typename NewScalar>\
	struct ReplaceScalarImpl {\
		static constexpr auto value() {\
			if constexpr (numNestings == 1) {\
				return NewScalar();\
			} else {\
				return typename Inner::template ReplaceScalar<NewScalar>();\
			}\
		}\
		using type = decltype(value());\
	};\
	template<typename NewScalar>\
	using ReplaceScalar = ReplaceInner<typename ReplaceScalarImpl<NewScalar>::type>;\
\
	struct SumWithScalarResultImpl {\
		static constexpr auto value() {\
			if constexpr (std::is_same_v<Inner, Scalar>) {\
				return TypeWrapper<\
					typename This::template LocalSumWithScalarResult<\
						Scalar\
					>\
				>();\
			} else {\
				return TypeWrapper<\
					typename This::template LocalSumWithScalarResult<\
						typename Inner::SumWithScalarResult\
					>\
				>();\
			}\
		}\
		using type = typename decltype(value())::type;\
	};\
	using SumWithScalarResult = typename SumWithScalarResultImpl::type;

//for giving operators to tensor classes
//how can you add correctly-typed ops via crtp to a union?
//unions can't inherit.
//until then...

#define TENSOR_ADD_VECTOR_OP_EQ(op)\
	This & operator op(This const & b) {\
		for (int i = 0; i < localCount; ++i) {\
			s[i] op b.s[i];\
		}\
		return *this;\
	}

#define TENSOR_ADD_SCALAR_OP_EQ(op)\
	This & operator op(Scalar const & b) {\
		for (int i = 0; i < localCount; ++i) {\
			s[i] op b;\
		}\
		return *this;\
	}

// for comparing like types, use the member operator==, because it is constexpr
// for non-like tensors there's a non-member non-constexpr below
#define TENSOR_ADD_CMP_OP()\
	constexpr bool operator==(This const & b) const {\
		for (int i = 0; i < localCount; ++i) {\
			if (s[i] != b.s[i]) return false;\
		}\
		return true;\
	}\
	constexpr bool operator!=(This const & b) const {\
		return !operator==(b);\
	}

// danger ... danger ...
#define TENSOR_ADD_CAST_BOOL_OP()\
	operator bool() const {\
		for (int i = 0; i < localCount; ++i) {\
			if (s[i] != Inner()) return true;\
		}\
		return false;\
	}

// danger ... danger ...
// don't mix overload='s with overload ctors
#define TENSOR_ADD_ASSIGN_OP()\
	This & operator=(This const & o) {\
		for (int i = 0; i < localCount; ++i) {\
			s[i] = o.s[i];\
		}\
		return *this;\
	}

#define TENSOR_ADD_UNM()\
	This operator-() const {\
		This result;\
		for (int i = 0; i < localCount; ++i) {\
			result.s[i] = -s[i];\
		}\
		return result;\
	}

// for rank-1 objects (_vec, _sym::Accessor, _asym::Accessor)
// I'm using use operator(int) as the de-facto for rank-1, operator(int,int) for rank-1 etc
#define TENSOR_ADD_VECTOR_CALL_INDEX_PRIMARY()\
\
	/* a(i) := a_i */\
	constexpr decltype(auto) operator()(int i) {\
		TENSOR_INSERT_BOUNDS_CHECK(i);\
		return s[i];\
	}\
	constexpr decltype(auto) operator()(int i) const {\
		TENSOR_INSERT_BOUNDS_CHECK(i);\
		return s[i];\
	}

#define TENSOR_ADD_BRACKET_FWD_TO_CALL()\
	/* a[i] := a_i */\
	/* operator[int] calls through operator(int) */\
	constexpr decltype(auto) operator[](int i) { return (*this)(i); }\
	constexpr decltype(auto) operator[](int i) const { return (*this)(i); }

//operator[int] forwards to operator(int)
//operator(int...) forwards to operator(int)(int...)
#define TENSOR_ADD_RANK1_CALL_INDEX_AUX()\
\
	TENSOR_ADD_BRACKET_FWD_TO_CALL()\
\
	/* a(i1,i2,...) := a_i1_i2_... */\
	/* operator(int, int...) calls through operator(int) */\
	template<typename... Ints>\
	requires is_all_v<int, Ints...>\
	constexpr decltype(auto) operator()(int i, Ints... is) { return (*this)(i)(is...); }\
	template<typename... Ints>\
	requires is_all_v<int, Ints...>\
	constexpr decltype(auto) operator()(int i, Ints... is) const { return (*this)(i)(is...); }

// operator(_vec<int,...>) forwards to operator(int...)
#define TENSOR_ADD_INT_VEC_CALL_INDEX()\
	/* a(intN(i,...)) */\
	/* operator(_vec<int,N>) calls through operator(int...) */\
	template<int N>\
	constexpr decltype(auto) operator()(_vec<int,N> const & i) { return std::apply(*this, i.s); }\
	template<int N>\
	constexpr decltype(auto) operator()(_vec<int,N> const & i) const { return std::apply(*this, i.s); }

#define TENSOR_ADD_SCALAR_CTOR(classname)\
	/* would be nice to have this constructor for non-tensors, non-lambdas*/\
	/* but lambas aren't invocable! */\
	/*template<typename T>*/\
	/*constexpr classname(T const & x)*/\
	/*requires (!is_tensor_v<T> && !std::is_invocable_v<T>)*/\
	/* so instead ... */\
	constexpr classname(Scalar const & x) {\
		for (int i = 0; i < localCount; ++i) {\
			s[i] = x;\
		}\
	}

// vector cast operator
// TODO not sure how to write this to generalize into _sym and others (or if I should try to?)
// explicit 'this->s' so subclasses can use this macro (like _quat)
#define TENSOR_ADD_CTOR_FOR_GENERIC_TENSORS(classname)\
	template<typename U>\
	/* TODO find a way to compare 'dims' instead of 'rank', then bounds would be guaranteed */\
	/* or do I want to force matching bounds? */\
	requires (is_tensor_v<U> && rank == U::rank)\
	constexpr classname(U const & t) {\
		auto w = write();\
		for (auto i = w.begin(); i != w.end(); ++i) {\
			/* TODO instead an index range iterator that spans the minimum of dims of this and t */\
			if (U::validIndex(i.readIndex)) {\
				/* If we use operator()(intN<>) access working for _asym ... */\
				/**i = (Scalar)t(i.readIndex);*/\
				/* ... or just replace the internal storage with std::array ... */\
				*i = std::apply(t, i.readIndex.s);\
			} else {\
				*i = Scalar();\
			}\
		}\
	}

// lambda ctor
#define TENSOR_ADD_LAMBDA_CTOR(classname)\
	/* use _vec<int, rank> as our lambda index: */\
	classname(std::function<Scalar(intN)> f) {\
		auto w = write();\
		for (auto i = w.begin(); i != w.end(); ++i) {\
			*i = f(i.readIndex);\
		}\
	}\
\
	/* use (int...) as the lambda index */\
	/* since I can't just accept function(Scalar(int,...)), I need to require the type to match */\
	/* mind you in C++ I can't just say the signature is FunctionFromLambda<Lambda>::FuncType ... */\
	/* no ... I have to accept all and then requires that part */\
	template<typename Lambda>\
	classname(Lambda lambda)\
	requires (\
		std::is_same_v<\
			Common::FunctionFromLambda<Lambda>,\
			Common::FunctionFromTupleArgs<\
				Scalar,\
				Common::tuple_rep_t<int, rank>\
			>\
		>\
	) {\
		using Func = typename Common::FunctionFromLambda<Lambda>::FuncType;\
		Func f(lambda);\
		auto w = write();\
		for (auto i = w.begin(); i != w.end(); ++i) {\
			*i = std::apply(f, i.readIndex.s);\
		}\
	}

#define TENSOR_ADD_LIST_CTOR(classname)\
	classname(std::initializer_list<Inner> l)\
	/* only do list constructor for non-specialized types */\
	/*(cuz they already accept lists via matching with their ctor args) */\
	/* a better requires would check for these ctors existence */\
	requires (localDim > 4)\
	{\
		auto src = l.begin();\
		auto dst = this->begin();\
		for (; src != l.end() && dst != this->end(); ++src, ++dst) {\
			*dst = *src;\
		}\
		for (; dst != this->end(); ++dst) {\
			*dst = {};\
		}\
	}

// TODO vararg ctor
#define TENSOR_ADD_ARG_CTOR(classname)\
\
	/* vec2 */\
	constexpr classname(\
		Inner const & s0_,\
		Inner const & s1_)\
	requires (localCount >= 2)\
	: s({s0_, s1_}) {}\
\
	/* vec3, sym2, asym3 */\
	constexpr classname(\
		Inner const & s0_,\
		Inner const & s1_,\
		Inner const & s2_)\
	requires (localCount >= 3)\
	: s({s0_, s1_, s2_}) {}\
\
	/* vec4, quat */\
	constexpr classname(\
		Inner const & s0_,\
		Inner const & s1_,\
		Inner const & s2_,\
		Inner const & s3_)\
	requires (localCount >= 4)\
	: s({s0_, s1_, s2_, s3_}) {}\
\
	/* sym3, asym4 */\
	constexpr classname(\
		Inner const & s0_,\
		Inner const & s1_,\
		Inner const & s2_,\
		Inner const & s3_,\
		Inner const & s4_,\
		Inner const & s5_)\
	requires (localCount >= 6)\
	: s({s0_, s1_, s2_, s3_, s4_, s5_}) {}\
\
	/* sym4 */\
	constexpr classname(\
		Inner const & s0_,\
		Inner const & s1_,\
		Inner const & s2_,\
		Inner const & s3_,\
		Inner const & s4_,\
		Inner const & s5_,\
		Inner const & s6_,\
		Inner const & s7_,\
		Inner const & s8_,\
		Inner const & s9_)\
	requires (localCount >= 10)\
	: s({s0_, s1_, s2_, s3_, s4_, s5_, s6_, s7_, s8_, s9_}) {}

#define TENSOR_ADD_CTORS(classname)\
	TENSOR_ADD_SCALAR_CTOR(classname)\
	TENSOR_ADD_CTOR_FOR_GENERIC_TENSORS(classname)\
	TENSOR_ADD_LAMBDA_CTOR(classname)\
	TENSOR_ADD_LIST_CTOR(classname)\
	TENSOR_ADD_ARG_CTOR(classname)

/*
ReadIterator and WriteIterator
depends on operator()(intN) for ReadIterator
depends on getLocalReadForWriteIndex() for its getReadForWriteIndex()

TODO InnerIterator (iterator i1 first) vs OuterIterator (iterator iN first)
ReadIterator vs WriteIterator
(Based on) ReadIndexIterator vs WriteIndexIterator
*/
#define TENSOR_ADD_ITERATOR()\
\
	/* inc 0 first */\
	template<int i>\
	struct ReadIncInner {\
		static constexpr bool exec(intN & index) {\
			++index[i];\
			if (index[i] < This::template dim<i>) return true;\
			if (i < rank-1) index[i] = 0;\
			return false;\
		}\
		static constexpr intN end() {\
			intN index;\
			index[rank-1] = This::template dim<rank-1>;\
			return index;\
		}\
	};\
\
	/* inc n-1 first */\
	template<int i>\
	struct ReadIncOuter {\
		static constexpr bool exec(intN & index) {\
			constexpr int j = rank-1-i;\
			++index[j];\
			if (index[j] < This::template dim<j>) return true;\
			if (j > 0) index[j] = 0;\
			return false;\
		}\
		static constexpr intN end() {\
			intN index;\
			index[0] = This::template dim<0>;\
			return index;\
		}\
	};\
\
	/* not in memory order, but ... meh idk why */\
	/*template<int i> using ReadInc = ReadIncInner<i>;*/\
	/* in memory order */\
	template<int i> using ReadInc = ReadIncOuter<i>;\
\
	/* read iterator */\
	/* - sized to the tensor rank (includes multiple-rank nestings) */\
	/* - range is 0's to the tensor dims() */\
	template<typename IteratorOwnerConst>\
	struct ReadIterator {\
		using intN = typename IteratorOwnerConst::intN;\
		IteratorOwnerConst & owner;\
		intN index;\
		ReadIterator(IteratorOwnerConst & owner_, intN index_ = {}) : owner(owner_), index(index_) {}\
		ReadIterator(ReadIterator const & o) : owner(o.owner), index(o.index) {}\
		ReadIterator(ReadIterator && o) : owner(o.owner), index(o.index) {}\
		ReadIterator & operator=(ReadIterator const & o) {\
			owner = o.owner;\
			index = o.index;\
			return *this;\
		}\
		ReadIterator & operator=(ReadIterator && o) {\
			owner = o.owner;\
			index = o.index;\
			return *this;\
		}\
		constexpr bool operator==(ReadIterator const & o) const {\
			return &owner == &o.owner && index == o.index;\
		}\
		constexpr bool operator!=(ReadIterator const & o) const {\
			return !operator==(o);\
		}\
		decltype(auto) operator*() const {\
			return owner(index);\
		}\
\
		ReadIterator & operator++() {\
			Common::ForLoop<0,rank,ReadInc>::exec(index);\
			return *this;\
		}\
		ReadIterator & operator++(int) {\
			Common::ForLoop<0,rank,ReadInc>::exec(index);\
			return *this;\
		}\
\
		std::ostream & to_ostream(std::ostream & o) const {\
			return o << "ReadIterator(owner=" << &owner << ", index=" << index << ")";\
		}\
\
		static ReadIterator begin(IteratorOwnerConst & v) {\
			return ReadIterator(v);\
		}\
		static ReadIterator end(IteratorOwnerConst & v) {\
			return ReadIterator(v, ReadInc<0>::end());\
		}\
	};\
\
	using iterator = ReadIterator<This>;\
	iterator begin() { return iterator::begin(*this); }\
	iterator end() { return iterator::end(*this); }\
	using const_iterator = ReadIterator<This const>;\
	const_iterator begin() const { return const_iterator::begin(*this); }\
	const_iterator end() const { return const_iterator::end(*this); }\
	const_iterator cbegin() const { return const_iterator::begin(*this); }\
	const_iterator cend() const { return const_iterator::end(*this); }\
\
	/* helper functions for WriteIterator */\
	static constexpr intN getReadForWriteIndex(intW const & i) {\
		intN res;\
		res.template subset<This::localRank, 0>() = This::getLocalReadForWriteIndex(i[0]);\
		if constexpr (numNestings > 1) {\
			/*static_assert(rank - This::localRank == Inner::rank);*/\
			res.template subset<rank-This::localRank, This::localRank>() = Inner::getReadForWriteIndex(i.template subset<numNestings-1,1>());\
		}\
		return res;\
	}\
\
	template<typename ThisConst, int numNestings_>\
	struct GetByWriteIndexImpl {\
		static constexpr decltype(auto) value(ThisConst & t, intW const & index) {\
			TENSOR_INSERT_BOUNDS_CHECK(index.s[0]);\
			return Inner::getByWriteIndex(\
				t.s[index.s[0]],\
				index.template subset<numNestings-1,1>()\
			);\
		}\
	};\
	template<typename ThisConst>\
	struct GetByWriteIndexImpl<ThisConst, 1> {\
		static constexpr decltype(auto) value(ThisConst & t, intW const & index) {\
			TENSOR_INSERT_BOUNDS_CHECK(index.s[0]);\
			return t.s[index.s[0]];\
		}\
	};\
	template<typename ThisConst>\
	static constexpr decltype(auto) getByWriteIndex(ThisConst & t, intW const & index) {\
		return GetByWriteIndexImpl<ThisConst, numNestings>::value(t, index);\
	}\
\
	template<typename WriteOwnerConst>\
	struct Write {\
		using intW = _vec<int, numNestings>;\
		using intR = intN;\
\
		WriteOwnerConst & owner;\
\
		Write(WriteOwnerConst & owner_) : owner(owner_) {}\
		Write(Write const & o) : owner(o.owner) {}\
		Write(Write && o) : owner(o.owner) {}\
		Write & operator=(Write const & o) { owner = o.owner; return *this; }\
		Write & operator=(Write && o) { owner = o.owner; return *this; }\
\
		/* inc 0 first */\
		template<int i>\
		struct WriteIncInner {\
			static constexpr bool exec(intW & writeIndex) {\
				++writeIndex[i];\
				if (writeIndex[i] < This::template count<i>) return true;\
				if (i < numNestings-1) writeIndex[i] = 0;\
				return false;\
			}\
			static constexpr intW end() {\
				intR writeIndex;\
				writeIndex[numNestings-1] = This::template count<numNestings-1>;\
				return writeIndex;\
			}\
		};\
\
		/* inc n-1 first */\
		template<int i>\
		struct WriteIncOuter {\
			static constexpr bool exec(intW & writeIndex) {\
				constexpr int j = numNestings-1-i;\
				++writeIndex[j];\
				if (writeIndex[j] < This::template count<j>) return true;\
				if (j > 0) writeIndex[j] = 0;\
				return false;\
			}\
			static constexpr intW end() {\
				intW writeIndex;\
				writeIndex[0] = This::template count<0>;\
				return writeIndex;\
			}\
		};\
\
		/* not in memory order, but ... meh idk why */\
		/*template<int i> using WriteInc = WriteIncInner<i>;*/\
		/* in memory order */\
		template<int i> using WriteInc = WriteIncOuter<i>;\
\
		/* write iterator */\
		/* - sized to the # of nestings */\
		/* - range is 0's to each nesting's .localCount */\
		template<typename IteratorOwnerConst>\
		struct WriteIterator {\
			IteratorOwnerConst & owner;\
			intW writeIndex;\
			intR readIndex;\
			WriteIterator(IteratorOwnerConst & owner_, intW writeIndex_ = {})\
			: owner(owner_),\
				writeIndex(writeIndex_),\
				readIndex(getReadForWriteIndex(writeIndex)) {}\
			WriteIterator(WriteIterator const & o)\
				: owner(o.owner),\
				writeIndex(o.writeIndex),\
				readIndex(o.readIndex) {}\
			WriteIterator(WriteIterator && o)\
				: owner(o.owner),\
				writeIndex(o.writeIndex),\
				readIndex(o.readIndex) {}\
			WriteIterator & operator=(WriteIterator const & o) {\
				owner = o.owner;\
				writeIndex = o.writeIndex;\
				readIndex = o.readIndex;\
				return *this;\
			}\
			WriteIterator & operator=(WriteIterator && o) {\
				owner = o.owner;\
				writeIndex = o.writeIndex;\
				readIndex = o.readIndex;\
				return *this;\
			}\
			constexpr bool operator==(WriteIterator const & o) const {\
				return &owner == &o.owner && writeIndex == o.writeIndex;\
			}\
			constexpr bool operator!=(WriteIterator const & o) const {\
				return !operator==(o);\
			}\
			decltype(auto) operator*() const {\
				/* cuz it takes less operations than by-read-writeIndex */\
				return getByWriteIndex<IteratorOwnerConst>(owner, writeIndex);\
			}\
\
			WriteIterator & operator++() {\
				Common::ForLoop<0,numNestings,WriteInc>::exec(writeIndex);\
				readIndex = getReadForWriteIndex(writeIndex);\
				return *this;\
			}\
			WriteIterator & operator++(int) {\
				Common::ForLoop<0,numNestings,WriteInc>::exec(writeIndex);\
				readIndex = getReadForWriteIndex(writeIndex);\
				return *this;\
			}\
\
			static WriteIterator begin(IteratorOwnerConst & v) {\
				return WriteIterator(v);\
			}\
			static WriteIterator end(IteratorOwnerConst & v) {\
				return WriteIterator(v, WriteInc<0>::end());\
			}\
		};\
\
		/* TODO if .write() was called by a const object, so WriteOwnerConst is 'This const' */\
		/* then should .begin() .end() be allowed?  or should they be forced to be 'This const' as well? */\
		using iterator = WriteIterator<This>;\
		iterator begin() { return iterator::begin(owner); }\
		iterator end() { return iterator::end(owner); }\
		using const_iterator = WriteIterator<This const>;\
		const_iterator begin() const { return const_iterator::begin(owner); }\
		const_iterator end() const { return const_iterator::end(owner); }\
		const_iterator cbegin() const { return const_iterator::begin(owner); }\
		const_iterator cend() const { return const_iterator::end(owner); }\
	};\
\
	Write<This> write() { return Write<This>(*this); }\
	/* wait, if Write<This> write() is called by a const object ... then the return type is const ... could I detect that from within Write to forward on to Write's inner class ctor? */\
	Write<This const> write() const { return Write<This const>(*this); }

// TODO unroll the loop / constexpr lambda
#define TENSOR_ADD_VALID_INDEX()\
	template<int j>\
	struct ValidIndexImpl {\
		static constexpr bool value(intN const & i) {\
			if constexpr (j == rank) {\
				return true;\
			} else {\
				if (i(j) < 0 || i(j) >= dim<j>) return false;\
				return ValidIndexImpl<j+1>::value(i);\
			}\
		}\
	};\
	static constexpr bool validIndex(typename This::intN const & i) {\
		return ValidIndexImpl<0>::value(i);\
	}

/*
member methods as wrappers to forward to Tensor namespace methods.

Bit of a hack: MOst these are written in terms of 'This'
'This' is usu my name for the same class we are currently in.
*/
#define TENSOR_ADD_MATH_MEMBER_FUNCS()\
\
	auto elemMul(This const & o) const {\
		return Tensor::elemMul(*this, o);\
	}\
	auto elemMul(This && o) && {\
		return Tensor::elemMul(std::move(*this), std::forward<This>(o));\
	}\
\
	template<typename T>\
	auto matrixCompMult(T const & o) const {\
		return Tensor::elemMul(*this, o);\
	}\
	template<typename... T>\
	auto matrixCompMult(T && ... o) && {\
		return Tensor::elemMul(std::move(*this), std::forward<T>(o)...);\
	}\
\
	template<typename T>\
	auto hadamard(T const & o) const {\
		return Tensor::elemMul(*this, o);\
	}\
	template<typename... T>\
	auto hadamard(T && ... o) && {\
		return Tensor::elemMul(std::move(*this), std::forward<T>(o)...);\
	}\
\
	template<typename B>\
	requires is_tensor_v<std::decay_t<B>>\
	Scalar inner(B const & o) const {\
		return Tensor::inner(*this, o);\
	}\
	template<typename B>\
	requires is_tensor_v<std::decay_t<B>>\
	Scalar inner(B && o) && {\
		return Tensor::inner(std::move(*this), std::forward<B>(o));\
	}\
\
	auto dot(This const & o) const {\
		return Tensor::inner(*this, o);\
	}\
	auto dot(This && o) && {\
		return Tensor::inner(std::move(*this), std::forward<This>(o));\
	}\
\
	Scalar lenSq() const { return Tensor::lenSq(*this); }\
\
	Scalar length() const { return Tensor::length(*this); }\
\
	Scalar distance(This const & o) const {\
		return Tensor::distance(*this, o);\
	}\
	Scalar distance(This && o) && {\
		return Tensor::distance(std::move(*this), std::forward<This>(o));\
	}\
\
	This normalize() const {\
		return Tensor::normalize(*this);\
	}\
\
	template<typename B>\
	requires IsBinaryTensorR3xR3Op<This, B>\
	auto cross(B const & b) const {\
		return Tensor::cross(*this, b);\
	}\
	template<typename B>\
	requires IsBinaryTensorR3xR3Op<This, B>\
	auto cross(B && b) && {\
		return Tensor::cross(std::move(*this), std::forward<B>(b));\
	}\
\
	template<typename B>\
	requires is_tensor_v<std::decay_t<B>>\
	auto outer(B const & b) const {\
		return outer(*this, b);\
	}\
\
	template<typename B>\
	auto outerProduct(B const & o) const {\
		return outerProduct(*this, o);\
	}\
\
	template<int m=0, int n=1>\
	requires (rank >= 2)\
	auto transpose() const {\
		return Tensor::transpose<m,n>(*this);\
	}\
\
	template<int m=0, int n=1>\
	requires (\
		m < rank\
		&& n < rank\
		&& This::template dim<m> == This::template dim<n>\
	) auto contract() const {\
		return Tensor::contract<m, n>(*this);\
	}\
\
	template<int index=0, int count=1>\
	auto contractN() const {\
		return Tensor::contractN<index, count>(*this);\
	}\
\
	template<int num=1, typename T>\
	auto interior(T const & o) const {\
		return Tensor::interior<num,This,T>(*this, o);\
	}\
\
	template<int m=0, int n=1>\
	auto trace() const {\
		return Tensor::trace<m,n,This>(*this);\
	}\
\
	template<int m=0>\
	auto diagonal() const {\
		return Tensor::diagonal<m>(*this);\
	}\
\
	auto makeSym() const\
	requires (isSquare) {\
		return Tensor::makeSym(*this);\
	}\
\
	auto makeAsym() const\
	requires (isSquare) {\
		return Tensor::makeAsym(*this);\
	}\
\
	template<typename B>\
	requires (is_tensor_v<B>)\
	auto wedge(B const & b) const {\
		return Tensor::wedge(*this, b);\
	}\
\
	auto hodgeDual() const\
	requires (isSquare) {\
		return Tensor::hodgeDual(*this);\
	}\
\
	template<typename B>\
	requires(\
		IsBinaryTensorOpWithMatchingNeighborDims<This, B>\
		&& B::rank == 2 /* ar - 1 + br - 1 == ar <=> br == 2 */\
	) auto operator*=(B const & b) {\
		*this = *this * b;\
	}\
\
	/* TODO can't get this to work */\
	/*Scalar determinant() const {*/\
		/*return (Scalar)Tensor::determinant<This>((This const &)(*this));*/\
	/*}*/\
\
	This inverse(Scalar const & det) const {\
		return Tensor::inverse(*this, det);\
	}\
\
	This inverse() const {\
		return Tensor::inverse(*this);\
	}

#define TENSOR_ADD_INDEX_NOTATION_CALL()\
	template<typename IndexType, typename... IndexTypes>\
	requires (std::is_base_of_v<IndexBase, IndexType>)\
	decltype(auto) operator()(IndexType, IndexTypes...) {\
		return IndexAccess<This, std::tuple<IndexType, IndexTypes...>>(*this);\
	}

//these are all per-element assignment operators,
// so they should work fine for all tensors: _vec, _sym, _asym, and subsequent nestings.
#define TENSOR_ADD_OPS(classname)\
	TENSOR_ADD_ITERATOR() /* needed by TENSOR_ADD_CTORS and a lot of tensor methods */\
	TENSOR_ADD_VALID_INDEX() /* used by generic tensor ctor */\
	TENSOR_ADD_CTORS(classname) /* ctors, namely lambda ctor, needs read iterators*/ \
	TENSOR_ADD_VECTOR_OP_EQ(+=)\
	TENSOR_ADD_VECTOR_OP_EQ(-=)\
	TENSOR_ADD_VECTOR_OP_EQ(*=)\
	TENSOR_ADD_VECTOR_OP_EQ(/=)\
	TENSOR_ADD_SCALAR_OP_EQ(+=)\
	TENSOR_ADD_SCALAR_OP_EQ(-=)\
	TENSOR_ADD_SCALAR_OP_EQ(*=)\
	TENSOR_ADD_SCALAR_OP_EQ(/=)\
	TENSOR_ADD_UNM()\
	TENSOR_ADD_CMP_OP()\
	TENSOR_ADD_MATH_MEMBER_FUNCS()
	/*TENSOR_ADD_INDEX_NOTATION_CALL()*/

// vector-specific macros:


// this is the header specific to vectors.
#define TENSOR_HEADER_VECTOR_SPECIFIC()\
\
	/*this is the storage size, used for iterting across 's' */\
	/* for vectors etc it is 's' */\
	/* for (anti?)symmetric it is N*(N+1)/2 */\
	/* TODO make a 'count<int nesting>', same as dim? */\
	static constexpr int localCount = localDim;\
\
	/* tensor/scalar +- ops will sometimes have a different result tensor type */\
	template<typename T>\
	using LocalSumWithScalarResult = _vec<T, localDim>;

#define TENSOR_HEADER_VECTOR(classname, Inner_, localDim_)\
	TENSOR_THIS(classname)\
	TENSOR_SET_INNER_LOCALDIM_LOCALRANK(Inner_, localDim_, 1)\
	TENSOR_TEMPLATE_T_I(classname)\
	TENSOR_HEADER_VECTOR_SPECIFIC()\
	TENSOR_EXPAND_TEMPLATE_TENSORR()\
	TENSOR_HEADER()

#define TENSOR_VECTOR_LOCAL_READ_FOR_WRITE_INDEX()\
	/* accepts int into .s[] storage, returns _intN<localRank> of how to index it using operator() */\
	static constexpr intNLocal getLocalReadForWriteIndex(int writeIndex) {\
		return intNLocal{writeIndex};\
	}

// TODO is this safe?
#define TENSOR_ADD_SUBSET_ACCESS()\
\
	/* assumes packed tensor */\
		/* compile-time offset */\
	template<int subdim, int offset>\
	_vec<Inner,subdim> & subset() {\
		static_assert(subdim >= 0);\
		static_assert(offset >= 0);\
		static_assert(offset + subdim <= localCount);\
		return *(_vec<Inner,subdim>*)(&s[offset]);\
	}\
	template<int subdim, int offset>\
	_vec<Inner,subdim> const & subset() const {\
		static_assert(subdim >= 0);\
		static_assert(offset >= 0);\
		static_assert(offset + subdim <= localCount);\
		return *(_vec<Inner,subdim>*)(&s[offset]);\
	}\
		/* runtime offset */\
	template<int subdim>\
	_vec<Inner,subdim> & subset(int offset) {\
		static_assert(subdim >= 0);\
		static_assert(subdim <= localCount);\
		return *(_vec<Inner,subdim>*)(&s[offset]);\
	}\
	template<int subdim>\
	_vec<Inner,subdim> const & subset(int offset) const {\
		static_assert(subdim >= 0);\
		static_assert(subdim <= localCount);\
		return *(_vec<Inner,subdim>*)(&s[offset]);\
	}

// vector.volume() == the volume of a size reprensted by the vector
// assumes Inner operator* exists
// TODO name this 'product' since 'volume' is ambiguous cuz it could alos mean product-of-dims
#define TENSOR_ADD_VOLUME()\
	Inner volume() const {\
		Inner res = s[0];\
		for (int i = 1; i < localCount; ++i) {\
			res *= s[i];\
		}\
		return res;\
	}


// only add these to _vec and specializations
// ... so ... 'classname' is always '_vec' for this set of macros
#define TENSOR_VECTOR_CLASS_OPS(classname)\
	TENSOR_VECTOR_LOCAL_READ_FOR_WRITE_INDEX() /* needed by TENSOR_ADD_ITERATOR in TENSOR_ADD_OPS */\
	TENSOR_ADD_OPS(classname)\
	TENSOR_ADD_VECTOR_CALL_INDEX_PRIMARY() /* operator(int) to access .s and operator[] */\
	TENSOR_ADD_RANK1_CALL_INDEX_AUX() /* operator(int, int...), operator[] */\
	TENSOR_ADD_INT_VEC_CALL_INDEX()\
	TENSOR_ADD_SUBSET_ACCESS()\
	TENSOR_ADD_VOLUME()


// type of a tensor with specific rank and dimension (for all indexes)
// used by some _vec members

template<typename Scalar, int dim, int rank>
struct _tensorr_impl {
	using type = _vec<typename _tensorr_impl<Scalar, dim, rank-1>::type, dim>;
};
template<typename Scalar, int dim>
struct _tensorr_impl<Scalar, dim, 0> {
	using type = Scalar;
};
template<typename Src, int dim, int rank>
using _tensorr = typename _tensorr_impl<Src, dim, rank>::type;

// default

// this is this class.  useful for templates.  you'd be surprised.
template<typename Inner_, int localDim_>
struct _vec {
	TENSOR_HEADER_VECTOR(_vec, Inner_, localDim_)
	std::array<Inner, localCount> s = {};
	constexpr _vec() {}
	TENSOR_VECTOR_CLASS_OPS(_vec)
};

// size == 2 specialization

template<typename Inner_>
struct _vec<Inner_,2> {
	TENSOR_HEADER_VECTOR(_vec, Inner_, 2)

	union {
		struct {
			Inner x, y;
		};
		struct {
			Inner s0, s1;
		};
		std::array<Inner, localCount> s = {};
	};
	constexpr _vec() {}

	static constexpr auto fields = std::make_tuple(
		std::make_pair("x", &This::x),
		std::make_pair("y", &This::y)
	);

	TENSOR_VECTOR_CLASS_OPS(_vec)

	// 2-component swizzles
#define TENSOR_VEC2_ADD_SWIZZLE2_ij(i, j)\
	auto i ## j () { return _vec<std::reference_wrapper<Inner>, 2>(i, j); }
#define TENSOR_VEC2_ADD_SWIZZLE2_i(i)\
	TENSOR_VEC2_ADD_SWIZZLE2_ij(i,x)\
	TENSOR_VEC2_ADD_SWIZZLE2_ij(i,y)
#define TENSOR3_VEC2_ADD_SWIZZLE2()\
	TENSOR_VEC2_ADD_SWIZZLE2_i(x)\
	TENSOR_VEC2_ADD_SWIZZLE2_i(y)
	TENSOR3_VEC2_ADD_SWIZZLE2()
	
	// 3-component swizzles
#define TENSOR_VEC2_ADD_SWIZZLE3_ijk(i, j, k)\
	auto i ## j ## k() { return _vec<std::reference_wrapper<Inner>, 3>(i, j, k); }
#define TENSOR_VEC2_ADD_SWIZZLE3_ij(i,j)\
	TENSOR_VEC2_ADD_SWIZZLE3_ijk(i,j,x)\
	TENSOR_VEC2_ADD_SWIZZLE3_ijk(i,j,y)
#define TENSOR_VEC2_ADD_SWIZZLE3_i(i)\
	TENSOR_VEC2_ADD_SWIZZLE3_ij(i,x)\
	TENSOR_VEC2_ADD_SWIZZLE3_ij(i,y)
#define TENSOR3_VEC2_ADD_SWIZZLE3()\
	TENSOR_VEC2_ADD_SWIZZLE3_i(x)\
	TENSOR_VEC2_ADD_SWIZZLE3_i(y)
	TENSOR3_VEC2_ADD_SWIZZLE3()

	// 4-component swizzles
#define TENSOR_VEC2_ADD_SWIZZLE4_ijkl(i, j, k, l)\
	auto i ## j ## k ## l() { return _vec<std::reference_wrapper<Inner>, 4>(i, j, k, l); }
#define TENSOR_VEC2_ADD_SWIZZLE4_ijk(i,j,k)\
	TENSOR_VEC2_ADD_SWIZZLE4_ijkl(i,j,k,x)\
	TENSOR_VEC2_ADD_SWIZZLE4_ijkl(i,j,k,y)
#define TENSOR_VEC2_ADD_SWIZZLE4_ij(i,j)\
	TENSOR_VEC2_ADD_SWIZZLE4_ijk(i,j,x)\
	TENSOR_VEC2_ADD_SWIZZLE4_ijk(i,j,y)
#define TENSOR_VEC2_ADD_SWIZZLE4_i(i)\
	TENSOR_VEC2_ADD_SWIZZLE4_ij(i,x)\
	TENSOR_VEC2_ADD_SWIZZLE4_ij(i,y)
#define TENSOR3_VEC2_ADD_SWIZZLE4()\
	TENSOR_VEC2_ADD_SWIZZLE4_i(x)\
	TENSOR_VEC2_ADD_SWIZZLE4_i(y)
	TENSOR3_VEC2_ADD_SWIZZLE4()
};

// size == 3 specialization

template<typename Inner_>
struct _vec<Inner_,3> {
	TENSOR_HEADER_VECTOR(_vec, Inner_, 3)

	union {
		struct {
			Inner x, y, z;
		};
		struct {
			Inner s0, s1, s2;
		};
		std::array<Inner, localCount> s = {};
	};
	constexpr _vec() {}

	static constexpr auto fields = std::make_tuple(
		std::make_pair("x", &This::x),
		std::make_pair("y", &This::y),
		std::make_pair("z", &This::z)
	);

	TENSOR_VECTOR_CLASS_OPS(_vec)

	// 2-component swizzles
#define TENSOR_VEC3_ADD_SWIZZLE2_ij(i, j)\
	auto i ## j () { return _vec<std::reference_wrapper<Inner>, 2>(i, j); }
#define TENSOR_VEC3_ADD_SWIZZLE2_i(i)\
	TENSOR_VEC3_ADD_SWIZZLE2_ij(i,x)\
	TENSOR_VEC3_ADD_SWIZZLE2_ij(i,y)\
	TENSOR_VEC3_ADD_SWIZZLE2_ij(i,z)
#define TENSOR3_VEC3_ADD_SWIZZLE2()\
	TENSOR_VEC3_ADD_SWIZZLE2_i(x)\
	TENSOR_VEC3_ADD_SWIZZLE2_i(y)\
	TENSOR_VEC3_ADD_SWIZZLE2_i(z)
	TENSOR3_VEC3_ADD_SWIZZLE2()
	
	// 3-component swizzles
#define TENSOR_VEC3_ADD_SWIZZLE3_ijk(i, j, k)\
	auto i ## j ## k() { return _vec<std::reference_wrapper<Inner>, 3>(i, j, k); }
#define TENSOR_VEC3_ADD_SWIZZLE3_ij(i,j)\
	TENSOR_VEC3_ADD_SWIZZLE3_ijk(i,j,x)\
	TENSOR_VEC3_ADD_SWIZZLE3_ijk(i,j,y)\
	TENSOR_VEC3_ADD_SWIZZLE3_ijk(i,j,z)
#define TENSOR_VEC3_ADD_SWIZZLE3_i(i)\
	TENSOR_VEC3_ADD_SWIZZLE3_ij(i,x)\
	TENSOR_VEC3_ADD_SWIZZLE3_ij(i,y)\
	TENSOR_VEC3_ADD_SWIZZLE3_ij(i,z)
#define TENSOR3_VEC3_ADD_SWIZZLE3()\
	TENSOR_VEC3_ADD_SWIZZLE3_i(x)\
	TENSOR_VEC3_ADD_SWIZZLE3_i(y)\
	TENSOR_VEC3_ADD_SWIZZLE3_i(z)
	TENSOR3_VEC3_ADD_SWIZZLE3()

	// 4-component swizzles
#define TENSOR_VEC3_ADD_SWIZZLE4_ijkl(i, j, k, l)\
	auto i ## j ## k ## l() { return _vec<std::reference_wrapper<Inner>, 4>(i, j, k, l); }
#define TENSOR_VEC3_ADD_SWIZZLE4_ijk(i,j,k)\
	TENSOR_VEC3_ADD_SWIZZLE4_ijkl(i,j,k,x)\
	TENSOR_VEC3_ADD_SWIZZLE4_ijkl(i,j,k,y)\
	TENSOR_VEC3_ADD_SWIZZLE4_ijkl(i,j,k,z)
#define TENSOR_VEC3_ADD_SWIZZLE4_ij(i,j)\
	TENSOR_VEC3_ADD_SWIZZLE4_ijk(i,j,x)\
	TENSOR_VEC3_ADD_SWIZZLE4_ijk(i,j,y)\
	TENSOR_VEC3_ADD_SWIZZLE4_ijk(i,j,z)
#define TENSOR_VEC3_ADD_SWIZZLE4_i(i)\
	TENSOR_VEC3_ADD_SWIZZLE4_ij(i,x)\
	TENSOR_VEC3_ADD_SWIZZLE4_ij(i,y)\
	TENSOR_VEC3_ADD_SWIZZLE4_ij(i,z)
#define TENSOR3_VEC3_ADD_SWIZZLE4()\
	TENSOR_VEC3_ADD_SWIZZLE4_i(x)\
	TENSOR_VEC3_ADD_SWIZZLE4_i(y)\
	TENSOR_VEC3_ADD_SWIZZLE4_i(z)
	TENSOR3_VEC3_ADD_SWIZZLE4()
};

// TODO specialization for & types -- don't initialize the s[] array (cuz in C++ you can't)
/// tho a workaround is just use std::reference<>

template<typename Inner_>
struct _vec<Inner_,4> {
	TENSOR_HEADER_VECTOR(_vec, Inner_, 4)

	union {
		struct {
			Inner x, y, z, w;
		};
		struct {
			Inner s0, s1, s2, s3;
		};
		std::array<Inner, localCount> s = {};
	};
	constexpr _vec() {}

	static constexpr auto fields = std::make_tuple(
		std::make_pair("x", &This::x),
		std::make_pair("y", &This::y),
		std::make_pair("z", &This::z),
		std::make_pair("w", &This::w)
	);

	TENSOR_VECTOR_CLASS_OPS(_vec)

	// 2-component swizzles
#define TENSOR_VEC4_ADD_SWIZZLE2_ij(i, j)\
	auto i ## j () { return _vec<std::reference_wrapper<Inner>, 2>(i, j); }
#define TENSOR_VEC4_ADD_SWIZZLE2_i(i)\
	TENSOR_VEC4_ADD_SWIZZLE2_ij(i,x)\
	TENSOR_VEC4_ADD_SWIZZLE2_ij(i,y)\
	TENSOR_VEC4_ADD_SWIZZLE2_ij(i,z)\
	TENSOR_VEC4_ADD_SWIZZLE2_ij(i,w)
#define TENSOR3_VEC4_ADD_SWIZZLE2()\
	TENSOR_VEC4_ADD_SWIZZLE2_i(x)\
	TENSOR_VEC4_ADD_SWIZZLE2_i(y)\
	TENSOR_VEC4_ADD_SWIZZLE2_i(z)\
	TENSOR_VEC4_ADD_SWIZZLE2_i(w)
	TENSOR3_VEC4_ADD_SWIZZLE2()
	
	// 3-component swizzles
#define TENSOR_VEC4_ADD_SWIZZLE3_ijk(i, j, k)\
	auto i ## j ## k() { return _vec<std::reference_wrapper<Inner>, 3>(i, j, k); }
#define TENSOR_VEC4_ADD_SWIZZLE3_ij(i,j)\
	TENSOR_VEC4_ADD_SWIZZLE3_ijk(i,j,x)\
	TENSOR_VEC4_ADD_SWIZZLE3_ijk(i,j,y)\
	TENSOR_VEC4_ADD_SWIZZLE3_ijk(i,j,z)\
	TENSOR_VEC4_ADD_SWIZZLE3_ijk(i,j,w)
#define TENSOR_VEC4_ADD_SWIZZLE3_i(i)\
	TENSOR_VEC4_ADD_SWIZZLE3_ij(i,x)\
	TENSOR_VEC4_ADD_SWIZZLE3_ij(i,y)\
	TENSOR_VEC4_ADD_SWIZZLE3_ij(i,z)\
	TENSOR_VEC4_ADD_SWIZZLE3_ij(i,w)
#define TENSOR3_VEC4_ADD_SWIZZLE3()\
	TENSOR_VEC4_ADD_SWIZZLE3_i(x)\
	TENSOR_VEC4_ADD_SWIZZLE3_i(y)\
	TENSOR_VEC4_ADD_SWIZZLE3_i(z)\
	TENSOR_VEC4_ADD_SWIZZLE3_i(w)
	TENSOR3_VEC4_ADD_SWIZZLE3()

	// 4-component swizzles
#define TENSOR_VEC4_ADD_SWIZZLE4_ijkl(i, j, k, l)\
	auto i ## j ## k ## l() { return _vec<std::reference_wrapper<Inner>, 4>(i, j, k, l); }
#define TENSOR_VEC4_ADD_SWIZZLE4_ijk(i,j,k)\
	TENSOR_VEC4_ADD_SWIZZLE4_ijkl(i,j,k,x)\
	TENSOR_VEC4_ADD_SWIZZLE4_ijkl(i,j,k,y)\
	TENSOR_VEC4_ADD_SWIZZLE4_ijkl(i,j,k,z)\
	TENSOR_VEC4_ADD_SWIZZLE4_ijkl(i,j,k,w)
#define TENSOR_VEC4_ADD_SWIZZLE4_ij(i,j)\
	TENSOR_VEC4_ADD_SWIZZLE4_ijk(i,j,x)\
	TENSOR_VEC4_ADD_SWIZZLE4_ijk(i,j,y)\
	TENSOR_VEC4_ADD_SWIZZLE4_ijk(i,j,z)\
	TENSOR_VEC4_ADD_SWIZZLE4_ijk(i,j,w)
#define TENSOR_VEC4_ADD_SWIZZLE4_i(i)\
	TENSOR_VEC4_ADD_SWIZZLE4_ij(i,x)\
	TENSOR_VEC4_ADD_SWIZZLE4_ij(i,y)\
	TENSOR_VEC4_ADD_SWIZZLE4_ij(i,z)\
	TENSOR_VEC4_ADD_SWIZZLE4_ij(i,w)
#define TENSOR3_VEC4_ADD_SWIZZLE4()\
	TENSOR_VEC4_ADD_SWIZZLE4_i(x)\
	TENSOR_VEC4_ADD_SWIZZLE4_i(y)\
	TENSOR_VEC4_ADD_SWIZZLE4_i(z)\
	TENSOR_VEC4_ADD_SWIZZLE4_i(w)
	TENSOR3_VEC4_ADD_SWIZZLE4()
};

// rank-2 optimized storage

inline constexpr int triangleSize(int n) {
	return (n * (n + 1)) / 2;
}

constexpr int symIndex(int i, int j) {
	if (i > j) return symIndex(j,i);
	return i + triangleSize(j);
}

// this assumes the class has a member template<ThisConst> Accessor for its [] and (int) access
#define TENSOR_ADD_RANK2_CALL_INDEX_AUX()\
\
	/* a(i1,i2,...) := a_i1_i2_... */\
	template<typename... Ints> requires is_all_v<int, Ints...>\
	constexpr decltype(auto) operator()(int i, int j, Ints... is) { return (*this)(i,j)(is...); }\
	template<typename... Ints> requires is_all_v<int, Ints...>\
	constexpr decltype(auto) operator()(int i, int j, Ints... is) const { return (*this)(i,j)(is...); }\
\
	/* a(i) := a_i */\
	/* this is incomplete so it returns the Accessor */\
	constexpr decltype(auto) operator()(int i) { return Accessor<This>(*this, i); }\
	constexpr decltype(auto) operator()(int i) const { return Accessor<This const>(*this, i); }\
\
	TENSOR_ADD_BRACKET_FWD_TO_CALL()\
	TENSOR_ADD_INT_VEC_CALL_INDEX()

// symmetric matrices

#define TENSOR_HEADER_SYMMETRIC_MATRIX_SPECIFIC()\
\
	static constexpr int localCount = triangleSize(localDim);\
\
	template<typename T>\
	using LocalSumWithScalarResult = _sym<T, localDim>;

#define TENSOR_HEADER_SYMMETRIC_MATRIX(classname, Inner_, localDim_)\
	TENSOR_THIS(classname)\
	TENSOR_SET_INNER_LOCALDIM_LOCALRANK(Inner_, localDim_, 2)\
	TENSOR_TEMPLATE_T_I(classname)\
	TENSOR_HEADER_SYMMETRIC_MATRIX_SPECIFIC()\
	TENSOR_EXPAND_TEMPLATE_TENSORR()\
	TENSOR_HEADER()

/*
Tank-2 based default indexing access.  ALl other indexing of rank-2 's relies on this.

This depends on getLocalWriteForReadIndex() which is in TENSR_*_LOCAL_READ_FOR_WRITE_INDEX.

This is used by _sym , while _asym uses something different since it uses the AntiSymRef accessors.
*/
#define TENSOR_ADD_SYMMETRIC_MATRIX_CALL_INDEX()\
\
	/* a(i,j) := a_ij = a_ji */\
	/* this is the direct acces */\
	/* the type should always be Inner (const) & */\
	/* symmetric has to define 1-arg operator() */\
	/* that means I can't use the default so i have to make a 2-arg recursive case */\
	constexpr decltype(auto) operator()(int i, int j) {\
		TENSOR_INSERT_BOUNDS_CHECK(getLocalWriteForReadIndex(i,j));\
		return s[getLocalWriteForReadIndex(i,j)];\
	}\
	constexpr decltype(auto) operator()(int i, int j) const {\
		TENSOR_INSERT_BOUNDS_CHECK(getLocalWriteForReadIndex(i,j));\
		return s[getLocalWriteForReadIndex(i,j)];\
	}

// TODO double-check this is upper-triangular
// currently set to upper-triangular
// swap iread 0 and 1 to get lower-triangular
#define TENSOR_SYMMETRIC_MATRIX_LOCAL_READ_FOR_WRITE_INDEX()\
	static constexpr intNLocal getLocalReadForWriteIndex(int writeIndex) {\
		intNLocal iread;\
		int w = writeIndex+1;\
		for (int i = 1; w > 0; ++i) {\
			++iread(1);\
			w -= i;\
		}\
		--iread(1);\
		iread(0) = writeIndex - triangleSize(iread(1));\
		return iread;\
	}\
\
	static constexpr int getLocalWriteForReadIndex(int i, int j) {\
		return symIndex(i,j);\
	}

template<typename AccessorOwnerConst>
struct Rank2Accessor {
	//properties for Accessor as a tensor:
#if 1	// NOTICE all of this is only for the Accessor-as-tensor interoperability.  You can disable it and just don't use Accessors as tensors.	
	TENSOR_THIS(Rank2Accessor)
	TENSOR_SET_INNER_LOCALDIM_LOCALRANK(
		typename AccessorOwnerConst::Inner,
		AccessorOwnerConst::localDim,
		1); // localRank==1 always cuz we hand off to the next Accessor or Owner's Inner
	//begin TENSOR_TEMPLATE_*
	template<typename T> using Template = Rank2Accessor<T>;
	template<typename NewInner> using ReplaceInner = Rank2Accessor<typename AccessorOwnerConst::template ReplaceInner<NewInner>>;
	template<int newLocalDim> using ReplaceLocalDim = Rank2Accessor<typename AccessorOwnerConst::template ReplaceLocalDim<newLocalDim>>;
	//end TENSOR_TEMPLATE_*
	//begin TENSOR_HEADER_*_SPECIFIC
	static constexpr int localCount = AccessorOwnerConst::localDim;
	template<typename T> using LocalSumWithScalarResult = typename AccessorOwnerConst::template LocalSumWithScalarResult<T>;
	//end TENSOR_HEADER_*_SPECIFIC
	TENSOR_EXPAND_TEMPLATE_TENSORR()
	TENSOR_HEADER()
#endif

	static constexpr bool isAccessorFlag = {};
	AccessorOwnerConst & owner;
	int i;

	Rank2Accessor(AccessorOwnerConst & owner_, int i_) : owner(owner_), i(i_) {}

	/* these should call into _sym(int,int) which is always Inner (const) & */
	constexpr decltype(auto) operator()(int j) { return owner(i,j); }
	constexpr decltype(auto) operator()(int j) const { return owner(i,j); }

	/* this provides the rest of the () [] operators that will be driven by operator(int) */
	TENSOR_ADD_RANK1_CALL_INDEX_AUX()
	TENSOR_ADD_INT_VEC_CALL_INDEX()

	TENSOR_ADD_VALID_INDEX()	// this is needed for other tensors to generic-tensor-ctor using this tensor
	TENSOR_VECTOR_LOCAL_READ_FOR_WRITE_INDEX() // needed by TENSOR_ADD_ITERATOR in TENSOR_ADD_OPS
	TENSOR_ADD_ITERATOR()
	TENSOR_ADD_MATH_MEMBER_FUNCS()
};

// Accessor is used to return between-rank indexes, so if sym is rank-2 this lets you get s[i] even if the storage only represents s[i,j]
#define TENSOR_ADD_RANK2_ACCESSOR()\
	template<typename AccessorOwnerConst>\
	using Accessor = Rank2Accessor<AccessorOwnerConst>;

/*
for the call index operator
a 1-param is incomplete, so it should return an accessor (same as operator[])
but a 2-param is complete
and a more-than-2 will return call on the []
and therein risks applying a call to an accessor
so the accessors need nested call indexing too
*/
#define TENSOR_SYMMETRIC_MATRIX_CLASS_OPS(classname)\
	TENSOR_ADD_RANK2_ACCESSOR()\
	TENSOR_SYMMETRIC_MATRIX_LOCAL_READ_FOR_WRITE_INDEX()\
	TENSOR_ADD_OPS(classname)\
	TENSOR_ADD_SYMMETRIC_MATRIX_CALL_INDEX()\
	TENSOR_ADD_RANK2_CALL_INDEX_AUX()

template<typename Inner_, int localDim_>
struct _sym {
	TENSOR_HEADER_SYMMETRIC_MATRIX(_sym, Inner_, localDim_)
	std::array<Inner,localCount> s = {};
	constexpr _sym() {}
	TENSOR_SYMMETRIC_MATRIX_CLASS_OPS(_sym)
};

template<typename Inner_>
struct _sym<Inner_,2> {
	TENSOR_HEADER_SYMMETRIC_MATRIX(_sym, Inner_, 2)

	union {
		struct {
			Inner x_x;
			union { Inner x_y; Inner y_x; };
			Inner y_y;
		};
		struct {
			Inner s00, s01, s11;
		};
		std::array<Inner, localCount> s = {};
	};
	constexpr _sym() {}

	static constexpr auto fields = std::make_tuple(
		std::make_pair("x_x", &This::x_x),
		std::make_pair("x_y", &This::x_y),
		std::make_pair("y_y", &This::y_y)
	);

	TENSOR_SYMMETRIC_MATRIX_CLASS_OPS(_sym)
};

template<typename Inner_>
struct _sym<Inner_,3> {
	TENSOR_HEADER_SYMMETRIC_MATRIX(_sym, Inner_, 3)

	union {
		struct {
			Inner x_x;
			union { Inner x_y; Inner y_x; };
			Inner y_y;
			union { Inner x_z; Inner z_x; };
			union { Inner y_z; Inner z_y; };
			Inner z_z;
		};
		struct {
			Inner s00, s01, s11, s02, s12, s22;
		};
		std::array<Inner, localCount> s = {};
	};
	constexpr _sym() {}

	static constexpr auto fields = std::make_tuple(
		std::make_pair("x_x", &This::x_x),
		std::make_pair("x_y", &This::x_y),
		std::make_pair("y_y", &This::y_y),
		std::make_pair("x_z", &This::x_z),
		std::make_pair("y_z", &This::y_z),
		std::make_pair("z_z", &This::z_z)
	);

	TENSOR_SYMMETRIC_MATRIX_CLASS_OPS(_sym)
};

template<typename Inner_>
struct _sym<Inner_,4> {
	TENSOR_HEADER_SYMMETRIC_MATRIX(_sym, Inner_, 4)

	union {
		struct {
			Inner x_x;
			union { Inner x_y; Inner y_x; };
			Inner y_y;
			union { Inner x_z; Inner z_x; };
			union { Inner y_z; Inner z_y; };
			Inner z_z;
			union { Inner x_w; Inner w_x; };
			union { Inner y_w; Inner w_y; };
			union { Inner z_w; Inner w_z; };
			Inner w_w;
		};
		struct {
			Inner s00, s01, s11, s02, s12, s22, s03, s13, s23, s33;
		};
		std::array<Inner, localCount> s = {};
	};
	constexpr _sym() {}

	static constexpr auto fields = std::make_tuple(
		std::make_pair("x_x", &This::x_x),
		std::make_pair("x_y", &This::x_y),
		std::make_pair("y_y", &This::y_y),
		std::make_pair("x_z", &This::x_z),
		std::make_pair("y_z", &This::y_z),
		std::make_pair("z_z", &This::z_z),
		std::make_pair("x_w", &This::x_w),
		std::make_pair("y_w", &This::y_w),
		std::make_pair("z_w", &This::z_w),
		std::make_pair("w_w", &This::w_w)
	);

	TENSOR_SYMMETRIC_MATRIX_CLASS_OPS(_sym)
};

// symmetric, identity ...
// only a single storage required
// ... so it's just a wrapper
// so I guess dimension doesn't really matter.
// but meh, it works for outer product / multiply optimizations I guess?

#define TENSOR_HEADER_IDENTITY_MATRIX_SPECIFIC()\
\
	static constexpr int localCount = 1;\
\
	/* _ident + or - a scalar is no longer ident, but still is symmetric */\
	template<typename T>\
	using LocalSumWithScalarResult = _sym<T, localDim>;

#define TENSOR_HEADER_IDENTITY_MATRIX(classname, Inner_, localDim_)\
	TENSOR_THIS(classname)\
	TENSOR_SET_INNER_LOCALDIM_LOCALRANK(Inner_, localDim_, 2)\
	TENSOR_TEMPLATE_T_I(classname)\
	TENSOR_HEADER_IDENTITY_MATRIX_SPECIFIC()\
	TENSOR_EXPAND_TEMPLATE_TENSORR()\
	TENSOR_HEADER()

#define TENSOR_ADD_IDENTITY_MATRIX_CALL_INDEX()\
	constexpr decltype(auto) operator()(int i, int j) {\
		if (i != j) return AntiSymRef<Inner>();\
		return AntiSymRef<Inner>(std::ref(s[0]), Sign::POSITIVE);\
	}\
	constexpr decltype(auto) operator()(int i, int j) const {\
		if (i != j) return AntiSymRef<Inner const>();\
		return AntiSymRef<Inner const>(std::ref(s[0]), Sign::POSITIVE);\
	}

// shouldn't even need this, cuz nobody should be calling i
#define TENSOR_IDENTITY_MATRIX_LOCAL_READ_FOR_WRITE_INDEX()\
	static constexpr intNLocal getLocalReadForWriteIndex(int writeIndex) {\
		return intNLocal();\
	}\
	static constexpr int getLocalWriteForReadIndex(int i, int j) {\
		return 0;\
	}

#define TENSOR_IDENTITY_MATRIX_CLASS_OPS(classname)\
	TENSOR_ADD_RANK2_ACCESSOR()\
	TENSOR_IDENTITY_MATRIX_LOCAL_READ_FOR_WRITE_INDEX() /* also works for Identity ... also any rank-2? */\
	TENSOR_ADD_OPS(classname)\
	TENSOR_ADD_IDENTITY_MATRIX_CALL_INDEX()\
	TENSOR_ADD_RANK2_CALL_INDEX_AUX()

template<typename Inner_, int localDim_>
struct _ident {
	TENSOR_HEADER_IDENTITY_MATRIX(_ident, Inner_, localDim_)
	std::array<Inner_, 1> s = {};
	constexpr _ident() {}
	TENSOR_IDENTITY_MATRIX_CLASS_OPS(_ident)
};

// TODO symmetric, scale
// TODO symmetric, diagonal

// antisymmetric matrices

#define TENSOR_HEADER_ANTISYMMETRIC_MATRIX_SPECIFIC()\
\
	static constexpr int localCount = triangleSize(localDim - 1);\
\
	/* _asym + or - a scalar is no longer _asym */\
	template<typename T>\
	using LocalSumWithScalarResult = _tensorr<T, localDim, 2>;

#define TENSOR_HEADER_ANTISYMMETRIC_MATRIX(classname, Inner_, localDim_)\
	TENSOR_THIS(classname)\
	TENSOR_SET_INNER_LOCALDIM_LOCALRANK(Inner_, localDim_, 2)\
	TENSOR_TEMPLATE_T_I(classname)\
	TENSOR_HEADER_ANTISYMMETRIC_MATRIX_SPECIFIC()\
	TENSOR_EXPAND_TEMPLATE_TENSORR()\
	TENSOR_HEADER()

// make sure this (and the using) is set before the specific-named accessors
#define TENSOR_ADD_ANTISYMMETRIC_MATRIX_CALL_INDEX()\
\
	/* a(i,j) := a_ij = -a_ji */\
	/* this is the direct acces */\
	template<typename ThisConst>\
	static constexpr decltype(auto) callImpl(ThisConst & this_, int i, int j) {\
		using InnerConst = typename constness_of<ThisConst>::template apply_to_t<Inner>;\
		if (i == j) return AntiSymRef<InnerConst>();\
		if (i > j) return callImpl<ThisConst>(this_, j, i).flip();\
		TENSOR_INSERT_BOUNDS_CHECK(symIndex(i,j-1));\
		return AntiSymRef<InnerConst>(std::ref(this_.s[symIndex(i,j-1)]), Sign::POSITIVE);\
	}\
	constexpr decltype(auto) operator()(int i, int j) {\
		return callImpl<This>(*this, i, j);\
	}\
	constexpr decltype(auto) operator()(int i, int j) const {\
		return callImpl<This const>(*this, i, j);\
	}

// TODO double-check this is upper-triangular
// currently set to upper-triangular, so i < j
// swap iread 0 and 1 to get lower-triangular
#define TENSOR_ANTISYMMETRIC_MATRIX_LOCAL_READ_FOR_WRITE_INDEX()\
	static constexpr intNLocal getLocalReadForWriteIndex(int writeIndex) {\
		intNLocal iread;\
		int w = writeIndex+1;\
		for (int i = 1; w > 0; ++i) {\
			++iread(1);\
			w -= i;\
		}\
		--iread(1);\
		iread(0) = writeIndex - triangleSize(iread(1));\
		++iread(1); /* for antisymmetric, skip past diagonals*/\
		return iread;\
	}

#define TENSOR_ANTISYMMETRIC_MATRIX_CLASS_OPS(classname)\
	TENSOR_ADD_RANK2_ACCESSOR()\
	TENSOR_ANTISYMMETRIC_MATRIX_LOCAL_READ_FOR_WRITE_INDEX()\
	TENSOR_ADD_OPS(classname)\
	TENSOR_ADD_ANTISYMMETRIC_MATRIX_CALL_INDEX()\
	TENSOR_ADD_RANK2_CALL_INDEX_AUX()

/*
for asym because I need to return reference-wrappers to support the + vs - depending on the index
this means I could expose some elements as fields and some as methods to return references
but that seems inconsistent
so next thought, expose all as methods to return references
but then if I'm not using any fields then I don't need any specializations
so no specialized sizes for _asym
*/
template<typename Inner_, int localDim_>
struct _asym {
	TENSOR_HEADER_ANTISYMMETRIC_MATRIX(_asym, Inner_, localDim_)

	std::array<Inner, localCount> s = {};
	constexpr _asym() {}

	// I figured I could do the union/struct thing like in _sym, but then half would be methods that returned refs and the other half would be fields..
	// so if I just make everything methods then there is some consistancy.
	AntiSymRef<Inner		> x_x() 		requires (localDim > 0) { return (*this)(0,0); }
	AntiSymRef<Inner const	> x_x() const 	requires (localDim > 0) { return (*this)(0,0); }
	
	AntiSymRef<Inner		> x_y() 		requires (localDim > 1) { return (*this)(0,1); }
	AntiSymRef<Inner const	> x_y() const 	requires (localDim > 1) { return (*this)(0,1); }
	AntiSymRef<Inner		> y_x() 		requires (localDim > 1) { return (*this)(1,0); }
	AntiSymRef<Inner const	> y_x() const 	requires (localDim > 1) { return (*this)(1,0); }
	AntiSymRef<Inner		> y_y() 		requires (localDim > 1) { return (*this)(1,1); }
	AntiSymRef<Inner const	> y_y() const 	requires (localDim > 1) { return (*this)(1,1); }
	
	AntiSymRef<Inner		> x_z() 		requires (localDim > 2) { return (*this)(0,2); }
	AntiSymRef<Inner const	> x_z() const 	requires (localDim > 2) { return (*this)(0,2); }
	AntiSymRef<Inner		> z_x() 		requires (localDim > 2) { return (*this)(2,0); }
	AntiSymRef<Inner const	> z_x() const 	requires (localDim > 2) { return (*this)(2,0); }
	AntiSymRef<Inner		> y_z() 		requires (localDim > 2) { return (*this)(1,2); }
	AntiSymRef<Inner const	> y_z() const 	requires (localDim > 2) { return (*this)(1,2); }
	AntiSymRef<Inner		> z_y() 		requires (localDim > 2) { return (*this)(2,1); }
	AntiSymRef<Inner const	> z_y() const 	requires (localDim > 2) { return (*this)(2,1); }
	AntiSymRef<Inner		> z_z() 		requires (localDim > 2) { return (*this)(2,2); }
	AntiSymRef<Inner const	> z_z() const 	requires (localDim > 2) { return (*this)(2,2); }
	
	AntiSymRef<Inner		> x_w() 		requires (localDim > 3) { return (*this)(0,3); }
	AntiSymRef<Inner const	> x_w() const 	requires (localDim > 3) { return (*this)(0,3); }
	AntiSymRef<Inner		> w_x() 		requires (localDim > 3) { return (*this)(3,0); }
	AntiSymRef<Inner const	> w_x() const 	requires (localDim > 3) { return (*this)(3,0); }
	AntiSymRef<Inner		> y_w() 		requires (localDim > 3) { return (*this)(1,3); }
	AntiSymRef<Inner const	> y_w() const 	requires (localDim > 3) { return (*this)(1,3); }
	AntiSymRef<Inner		> w_y() 		requires (localDim > 3) { return (*this)(3,1); }
	AntiSymRef<Inner const	> w_y() const 	requires (localDim > 3) { return (*this)(3,1); }
	AntiSymRef<Inner		> z_w() 		requires (localDim > 3) { return (*this)(2,3); }
	AntiSymRef<Inner const	> z_w() const 	requires (localDim > 3) { return (*this)(2,3); }
	AntiSymRef<Inner		> w_z() 		requires (localDim > 3) { return (*this)(3,2); }
	AntiSymRef<Inner const	> w_z() const 	requires (localDim > 3) { return (*this)(3,2); }
	AntiSymRef<Inner		> w_w() 		requires (localDim > 3) { return (*this)(3,3); }
	AntiSymRef<Inner const	> w_w() const 	requires (localDim > 3) { return (*this)(3,3); }

	TENSOR_ANTISYMMETRIC_MATRIX_CLASS_OPS(_asym)
};

// higher/arbitrary-rank tensors:

#define TENSOR_TEMPLATE_T_I_I(classname)\
\
	template<typename Inner2, int localDim2, int localRank2>\
	using Template = classname<Inner2, localDim2, localRank2>;\
\
	template<typename NewInner>\
	using ReplaceInner = Template<NewInner, localDim, localRank>;\
\
	template<int newLocalDim>\
	using ReplaceLocalDim = Template<Inner, newLocalDim, localRank>;

// helper functions used for the totally-symmetric and totally-antisymmetric tensors:

/*
higher-rank totally-symmetri (might replace _sym)
https://math.stackexchange.com/a/3795166
# of elements in rank-M dim-N storage: (d + r - 1) choose r
so for r=2 we get (d+1)! / (2! (d-1)!) = d * (d + 1) / 2
*/

//https://en.cppreference.com/w/cpp/language/constexpr
inline constexpr int factorial(int n) {
	return n <= 1 ? 1 : (n * factorial(n-1));
}

//https://stackoverflow.com/a/9331125
constexpr int nChooseR(int n, int k) {
    if (k > n) return 0;
    if (k << 1 > n) k = n - k;
    if (k == 0) return 1;
    int result = n;
    // TODO can you guarantee that /=i will always have 'i' as a divisor? or do we need two loops?
	for (int i = 2; i <= k; ++i) {
        result *= n - i + 1;
        result /= i;
    }
    return result;
}

// totally-symmetric

inline constexpr int symmetricSize(int d, int r) {
	return nChooseR(d + r - 1, r);
}

#define TENSOR_HEADER_TOTALLY_SYMMETRIC_SPECIFIC()\
\
	static constexpr int localCount = symmetricSize(localDim, localRank);\
\
	template<typename T>\
	using LocalSumWithScalarResult = _symR<T, localDim, localRank>;

// Expand index 0 of asym^N => vec ⊗ asym^(N-1)
// Expand index N-1 of asym^N => asym^(N-1) ⊗ vec
// TODO Remove of an index will Expand it first ... but we can also shorcut Remove to *always* use this type.
#define TENSOR_EXPAND_TEMPLATE_TOTALLY_SYMMETRIC()\
	template<int index>\
	struct ExpandTensorRankTemplateImpl {\
		static constexpr auto value() {\
			if constexpr (index == 0) {\
				if constexpr (localRank == 2) {\
					return TypeWrapper<_vec<_vec<Inner, localDim>, localDim>>();\
				} else if constexpr (localRank == 3) {\
					return TypeWrapper<_vec<_sym<Inner, localDim>, localDim>>();\
				} else {\
					return TypeWrapper<_vec<_symR<Inner, localDim, localRank-1>, localDim>>();\
				}\
			} else if constexpr (index == localRank-1) {\
				if constexpr (localRank == 2) {\
					return TypeWrapper<_vec<_vec<Inner, localDim>, localDim>>();\
				} else if constexpr (localRank == 3) {\
					return TypeWrapper<_sym<_vec<Inner, localDim>, localDim>>();\
				} else {\
					return TypeWrapper<_symR<_vec<Inner, localDim>, localDim, localRank-1>>();\
				}\
			} else {\
				return TypeWrapper<_tensorr<Inner, localDim, localRank>>();\
			}\
		}\
		using type = typename decltype(value())::type;\
	};\
	template<int index>\
	using ExpandTensorRankTemplate = typename This::template ExpandTensorRankTemplateImpl<index>::type;

#define TENSOR_HEADER_TOTALLY_SYMMETRIC(classname, Inner_, localDim_, localRank_)\
	TENSOR_THIS(classname)\
	TENSOR_SET_INNER_LOCALDIM_LOCALRANK(Inner_, localDim_, localRank_)\
	TENSOR_TEMPLATE_T_I_I(classname)\
	TENSOR_HEADER_TOTALLY_SYMMETRIC_SPECIFIC()\
	TENSOR_EXPAND_TEMPLATE_TOTALLY_SYMMETRIC()\
	TENSOR_HEADER()

// using 'upper-triangular' i.e. i<=j<=k<=...
// right now i'm counting into this.  is there a faster way?
#define TENSOR_TOTALLY_SYMMETRIC_LOCAL_READ_FOR_WRITE_INDEX()\
	template<int j>\
	struct GetLocalReadForWriteIndexImpl {\
		static constexpr bool exec(intNLocal & iread) {\
			if constexpr (j < 0) {\
				return true;\
			} else {\
				iread[j]++;\
				if (iread[j] >= localDim) {\
					if (GetLocalReadForWriteIndexImpl<j-1>::exec(iread)) return true;\
					iread[j] = j == 0 ? 0 : iread[j-1];\
				}\
				return false;\
			}\
		}\
	};\
	static constexpr intNLocal getLocalReadForWriteIndex(int writeIndex) {\
		intNLocal iread = {};\
		for (int i = 0; i < writeIndex; ++i) {\
			if (GetLocalReadForWriteIndexImpl<localRank-1>::exec(iread)) break;\
		}\
		return iread;\
	}\
\
	/* I'm tempted to use a int... pack for targetReadIndex but the values would still be runtime so I can' exactly use a template-sort on them, so I might as well use std::sort, so it might as well be in an array ... */\
	static constexpr int getLocalWriteForReadIndex(intNLocal targetReadIndex) {\
		/* put indexes in increasing order */\
		std::sort(targetReadIndex.s.begin(), targetReadIndex.s.end());\
		/* loop until you find the element */\
		intNLocal iread;\
		for (int writeIndex = 0; writeIndex < localCount; ++writeIndex) {\
			if (iread == targetReadIndex) return writeIndex;\
			if (GetLocalReadForWriteIndexImpl<localRank-1>::exec(iread)) break;\
		}\
		/* how should I deal with bad indexes? */\
		/*throw Common::Exception() << "failed to find write index";*/\
		/* return oob range? for iteration's sake? */\
		return localCount;\
	}

// making operator()(int...) the primary, and operator()(intN<>) the secondary
// TODO forwarding of args
#define TENSOR_ADD_TOTALLY_SYMMETRIC_CALL_INDEX()\
\
	template<typename ThisConst, typename TupleSoFar, typename... Ints> requires is_all_v<int, Ints...>\
	static constexpr decltype(auto) callGtLocalRankImplFwd(ThisConst & t, TupleSoFar sofar, int arg, Ints... is) {\
		return callGtLocalRankImpl<ThisConst>(\
			t,\
			std::tuple_cat( sofar, std::make_tuple(arg)),\
			is...);\
	}\
	template<typename ThisConst, typename TupleSoFar, typename... Ints> requires is_all_v<int, Ints...>\
	static constexpr decltype(auto) callGtLocalRankImpl(ThisConst & t, TupleSoFar sofar, Ints... is) {\
		if constexpr (std::tuple_size_v<TupleSoFar> == localRank) {\
			return t.s[getLocalWriteForReadIndex(std::make_from_tuple<intNLocal>(sofar))](is...);\
		} else {\
			return callGtLocalRankImplFwd<ThisConst>(t, sofar, is...);\
		}\
	};\
\
	/* TODO any better way to write two functions at once with differing const-ness? */\
	template<typename ThisConst, typename... Ints> requires is_all_v<int, Ints...>\
	static constexpr decltype(auto) callImpl(ThisConst & this_, Ints... is) {\
		constexpr int N = sizeof...(Ints);\
		if constexpr (N == localRank) {\
			return this_.s[getLocalWriteForReadIndex(intNLocal(is...))];\
		} else if constexpr (N < localRank) {\
			return Accessor<ThisConst, N>(this_, intNLocal(is...));\
		} else if constexpr (N > localRank) {\
			return callGtLocalRankImpl<ThisConst, std::tuple<>, Ints...>(this_, std::make_tuple(), is...);\
		}\
	}\
\
	template<typename... Ints> requires is_all_v<int, Ints...>\
	constexpr decltype(auto) operator()(Ints... is) {\
		return callImpl<This>(*this, is...);\
	}\
	template<typename... Ints> requires is_all_v<int, Ints...>\
	constexpr decltype(auto) operator()(Ints... is) const {\
		return callImpl<This const>(*this, is...);\
	}\
\
	TENSOR_ADD_INT_VEC_CALL_INDEX()\
	TENSOR_ADD_BRACKET_FWD_TO_CALL()

template<typename AccessorOwnerConst, int subRank>
struct RankNAccessor {
	//properties of owner, mapped here so names dont' collide with Accessor's tensor properties:
	static constexpr int ownerLocalRank = AccessorOwnerConst::localRank;
	using ownerIntN = typename AccessorOwnerConst::intN;

	//properties for Accessor as a tensor:
#if 1	// NOTICE all of this is only for the Accessor-as-tensor interoperability.  You can disable it and just don't use Accessors as tensors.	
	TENSOR_THIS(RankNAccessor)
	TENSOR_SET_INNER_LOCALDIM_LOCALRANK(
		typename AccessorOwnerConst::Inner,
		AccessorOwnerConst::localDim,
		1); // localRank==1 always cuz we hand off to the next Accessor or Owner's Inner
	//begin TENSOR_TEMPLATE_*
	template<typename T, int s> using Template = RankNAccessor<T, s>;
	template<typename NewInner> using ReplaceInner = RankNAccessor<typename AccessorOwnerConst::template ReplaceInner<NewInner>, subRank>;
	template<int newLocalDim> using ReplaceLocalDim = RankNAccessor<typename AccessorOwnerConst::template ReplaceLocalDim<newLocalDim>, subRank>;
	//end TENSOR_TEMPLATE_*
	//begin TENSOR_HEADER_*_SPECIFIC
	static constexpr int localCount = AccessorOwnerConst::localDim;
	template<typename T> using LocalSumWithScalarResult = typename AccessorOwnerConst::template LocalSumWithScalarResult<T>;
	//end TENSOR_HEADER_*_SPECIFIC
	TENSOR_EXPAND_TEMPLATE_TENSORR()
	TENSOR_HEADER()
#endif

	static_assert(subRank > 0 && subRank < ownerLocalRank);
	static constexpr bool isAccessorFlag = {};
	AccessorOwnerConst & owner;
	_vec<int,subRank> i;

	RankNAccessor(AccessorOwnerConst & owner_, _vec<int,subRank> i_)
	: owner(owner_), i(i_) {}

	/* until I can think of how to split off parameter-packs at a specific length, I'll just do this in _vec<int> first like below */
	/* if subRank + sizeof...(Ints) > ownerLocalRank then call-through into owner's inner */
	/* if subRank + sizeof...(Ints) == ownerLocalRank then just call owner */
	/* if subRank + sizeof...(Ints) < ownerLocalRank then produce another Accessor */
	template<int N, typename ThisConst>
	constexpr decltype(auto) callImpl(ThisConst & this_, _vec<int,N> const & i2) {
		if constexpr (subRank + N < ownerLocalRank) {
			/* returns Accessor */
			_vec<int,subRank+N> fulli;
			fulli.template subset<subRank,0>() = i;
			fulli.template subset<N,subRank>() = i2;
			return RankNAccessor<AccessorOwnerConst, subRank+N>(owner, fulli);
		} else if constexpr (subRank + N == ownerLocalRank) {
			/* returns Inner, or for asym returns AntiSymRef<Inner> */
			ownerIntN fulli;
			fulli.template subset<subRank,0>() = i;
			fulli.template subset<N,subRank>() = i2;
			return this_.owner(fulli);
		} else if constexpr (subRank + N > ownerLocalRank) {
			/* returns something further than Inner */
			ownerIntN firstI;
			firstI.template subset<subRank,0>() = i;
			firstI.template subset<ownerLocalRank-subRank,subRank>() = i2.template subset<ownerLocalRank-subRank, 0>();
			_vec<int, N - (ownerLocalRank - subRank)> restI = i2.template subset<N - (ownerLocalRank-subRank), ownerLocalRank-subRank>();
			return this_.owner(firstI)(restI);
		}
	}
	// TODO can't I just get rid of the non-const version?  no need to fwd?  nope.  need both.
	template<int N>
	constexpr decltype(auto) operator()(_vec<int,N> const & i2) {
		return callImpl<N, This>(*this, i2);
	}
	template<int N>
	constexpr decltype(auto) operator()(_vec<int,N> const & i2) const {
		return callImpl<N, This const>(*this, i2);
	}

	template<typename... Ints>
	requires is_all_v<int, Ints...>
	constexpr decltype(auto) operator()(Ints... is) {
		return (*this)(_vec<int,sizeof...(Ints)>(is...));
	}
	template<typename... Ints>
	requires is_all_v<int, Ints...>
	constexpr decltype(auto) operator()(Ints... is) const {
		return (*this)(_vec<int,sizeof...(Ints)>(is...));
	}

	TENSOR_ADD_BRACKET_FWD_TO_CALL()

	TENSOR_ADD_VALID_INDEX()	// this is needed for other tensors to generic-tensor-ctor using this tensor
	TENSOR_VECTOR_LOCAL_READ_FOR_WRITE_INDEX() // needed by TENSOR_ADD_ITERATOR in TENSOR_ADD_OPS
	TENSOR_ADD_ITERATOR()
	TENSOR_ADD_MATH_MEMBER_FUNCS()
};

#define TENSOR_ADD_RANK_N_ACCESSOR()\
	template<typename AccessorOwnerConst, int subRank>\
	using Accessor = RankNAccessor<AccessorOwnerConst, subRank>;

#define TENSOR_TOTALLY_SYMMETRIC_CLASS_OPS()\
	TENSOR_ADD_RANK_N_ACCESSOR()\
	TENSOR_TOTALLY_SYMMETRIC_LOCAL_READ_FOR_WRITE_INDEX() /* needed before TENSOR_ADD_ITERATOR in TENSOR_ADD_OPS */\
	TENSOR_ADD_OPS(_symR)\
	TENSOR_ADD_TOTALLY_SYMMETRIC_CALL_INDEX()

template<typename Inner_, int localDim_, int localRank_>
struct _symR {
	TENSOR_HEADER_TOTALLY_SYMMETRIC(_symR, Inner_, localDim_, localRank_)
	std::array<Inner, localCount> s = {};
	constexpr _symR() {}
	TENSOR_TOTALLY_SYMMETRIC_CLASS_OPS()
};

// totally antisymmetric

inline constexpr int antisymmetricSize(int d, int r) {
	return nChooseR(d, r);
}

template<typename T>
struct initIntVecWithSequence {};
template<typename T, T... I>
struct initIntVecWithSequence<std::integer_sequence<T, I...>> {
	static constexpr auto value() {
		return _vec<T, sizeof...(I)>{I...};
	}
};

// bubble-sorts 'i', sets 'sign' if an odd # of flips were required to sort it
//  returns 'sign' or 'ZERO' if any duplicate indexes were found (and does not finish sorting)
template<int N>
Sign antisymSortAndCountFlips(_vec<int,N> & i) {
	Sign sign = Sign::POSITIVE;
	for (int k = 0; k < N-1; ++k) {
		for (int j = 0; j < N-k-1; ++j) {
			if (i[j] == i[j+1]) {
				return Sign::ZERO;
			} else if (i[j] > i[j+1]) {
				std::swap(i[j], i[j+1]);
				sign = !sign;
			}
		}
	}
	return sign;
}

#define TENSOR_HEADER_TOTALLY_ANTISYMMETRIC_SPECIFIC()\
\
	static constexpr int localCount = antisymmetricSize(localDim, localRank);\
\
	/* _asymR + or - a scalar is no longer _asymR */\
	template<typename T>\
	using LocalSumWithScalarResult = _tensorr<T, localDim, localRank>;

// Expand index 0 of asym^N => vec ⊗ asym^(N-1)
// Expand index N-1 of asym^N => asym^(N-1) ⊗ vec
// TODO Remove of an index will Expand it first ... but we can also shorcut Remove to *always* use this type.
#define TENSOR_EXPAND_TEMPLATE_TOTALLY_ANTISYMMETRIC()\
	template<int index>\
	struct ExpandTensorRankTemplateImpl {\
		static constexpr auto value() {\
			if constexpr (index == 0) {\
				if constexpr (localRank == 2) {\
					return TypeWrapper<_vec<_vec<Inner, localDim>, localDim>>();\
				} else if constexpr (localRank == 3) {\
					return TypeWrapper<_vec<_asym<Inner, localDim>, localDim>>();\
				} else {\
					return TypeWrapper<_vec<_asymR<Inner, localDim, localRank-1>, localDim>>();\
				}\
			} else if constexpr (index == localRank-1) {\
				if constexpr (localRank == 2) {\
					return TypeWrapper<_vec<_vec<Inner, localDim>, localDim>>();\
				} else if constexpr (localRank == 3) {\
					return TypeWrapper<_asym<_vec<Inner, localDim>, localDim>>();\
				} else {\
					return TypeWrapper<_asymR<_vec<Inner, localDim>, localDim, localRank-1>>();\
				}\
			} else {\
				return TypeWrapper<_tensorr<Inner, localDim, localRank>>();\
			}\
		}\
		using type = typename decltype(value())::type;\
	};\
	template<int index>\
	using ExpandTensorRankTemplate = typename This::template ExpandTensorRankTemplateImpl<index>::type;

#define TENSOR_HEADER_TOTALLY_ANTISYMMETRIC(classname, Inner_, localDim_, localRank_)\
	TENSOR_THIS(classname)\
	TENSOR_SET_INNER_LOCALDIM_LOCALRANK(Inner_, localDim_, localRank_)\
	TENSOR_TEMPLATE_T_I_I(classname)\
	TENSOR_HEADER_TOTALLY_ANTISYMMETRIC_SPECIFIC()\
	TENSOR_EXPAND_TEMPLATE_TOTALLY_ANTISYMMETRIC()\
	TENSOR_HEADER()

// using 'upper-triangular' i.e. i<=j<=k<=...
// right now i'm counting into this.  is there a faster way?
#define TENSOR_TOTALLY_ANTISYMMETRIC_LOCAL_READ_FOR_WRITE_INDEX()\
	template<int j>\
	struct GetLocalReadForWriteIndexImpl {\
		static constexpr bool execInner(intNLocal & iread) {\
			if constexpr (j < 0) {\
				return true;\
			} else {\
				iread[j]++;\
				if (iread[j] >= localDim) {\
					if (GetLocalReadForWriteIndexImpl<j-1>::execInner(iread)) return true;\
					iread[j] = j == 0 ? 0 : iread[j-1];\
				}\
				return false;\
			}\
		}\
		static constexpr bool exec(intNLocal & iread) {\
			for (;;) {\
				if (execInner(iread)) return true;\
				bool skip = false;\
				for (int k = 0; k < localRank-1; ++k) {\
					if (iread[k] == iread[k+1]) {\
						skip = true;\
						break;\
					}\
				}\
				if (!skip) return false;\
			}\
		}\
	};\
	static constexpr intNLocal getLocalReadForWriteIndex(int writeIndex) {\
		intNLocal iread = initIntVecWithSequence<std::make_integer_sequence<int, localRank>>::value();\
		for (int i = 0; i < writeIndex; ++i) {\
			if (GetLocalReadForWriteIndexImpl<localRank-1>::exec(iread)) break;\
		}\
		return iread;\
	}\
\
	/* NOTICE this assumes targetReadIndex is already sorted */\
	static constexpr int getLocalWriteForReadIndex(intNLocal targetReadIndex) {\
		/* loop until you find the element */\
		intNLocal iread = initIntVecWithSequence<std::make_integer_sequence<int, localRank>>::value();\
		for (int writeIndex = 0; writeIndex < localCount; ++writeIndex) {\
			if (iread == targetReadIndex) return writeIndex;\
			if (GetLocalReadForWriteIndexImpl<localRank-1>::exec(iread)) break;\
		}\
		/* how should I deal with bad indexes? */\
		/*throw Common::Exception() << "failed to find write index";*/\
		/* return oob range? for iteration's sake? */\
		return localCount;\
	}

// TODO bubble-sort, count # of flips, use that as parity, and then if any duplicate indexes exist, use a zero reference
/*
from my symmath/tensor/LeviCivita.lua
			local indexes = {...}
			-- duplicates mean 0
			for i=1,#indexes-1 do
				for j=i+1,#indexes do
					if indexes[i] == indexes[j] then return 0 end
				end
			end
			-- bubble sort, count the flips
			local parity = 1
			for i=1,#indexes-1 do
				for j=1,#indexes-i do
					if indexes[j] > indexes[j+1] then
						indexes[j], indexes[j+1] = indexes[j+1], indexes[j]
						parity = -parity
					end
				end
			end

*/
#define TENSOR_ADD_TOTALLY_ANTISYMMETRIC_CALL_INDEX()\
\
	/* In the case of AntiSymRef, if we have a partial-indexing then the Accessor is returned but the signs aren't flipped.*/\
	/* It's not until the full indexing is made that the sign is determined in the AntiSymRef. */\
	/* This way the Accessor doesn't need to remember the AntiSymRef::how state. */\
	/* and that lets us reuse the Accessor for both sym and asym. */\
	/* So equivalently, here we don't want to bubble-sort indexes until our index size is >= our localRank */\
	template<typename ThisConst, int N>\
	static constexpr decltype(auto) callVecImpl(ThisConst & this_, _vec<int,N> i) {\
		if constexpr (N < localRank) {\
			return Accessor<ThisConst, N>(this_, i);\
		} else if constexpr (N == localRank) {\
			using InnerConst = typename constness_of<ThisConst>::template apply_to_t<Inner>;\
			auto sign = antisymSortAndCountFlips(i);\
			if (sign == Sign::ZERO) return AntiSymRef<InnerConst>();\
			return AntiSymRef<InnerConst>(this_.s[getLocalWriteForReadIndex(i)], sign);\
		} else if constexpr (N > localRank) {\
			auto sign = antisymSortAndCountFlips(i);\
			if (sign == Sign::ZERO) {\
				using R = decltype(this_(i.template subset<localRank,0>())(i.template subset<N-localRank,localRank>()));\
				return R();\
			}\
			/* call-thru of AntiSymRef returns another AntiSymRef ... */\
			auto result = this_(i.template subset<localRank,0>())(i.template subset<N-localRank,localRank>());\
			if (sign == Sign::NEGATIVE) result.flip();\
			return result;\
		}\
	}\
	template<int N>\
	constexpr decltype(auto) operator()(_vec<int,N> i) {\
		return callVecImpl<This>(*this, i);\
	}\
	template<int N>\
	constexpr decltype(auto) operator()(_vec<int,N> i) const {\
		return callVecImpl<This const>(*this, i);\
	}\
\
	template<typename... Ints>\
	requires is_all_v<int, Ints...>\
	constexpr decltype(auto) operator()(Ints... is) {\
		return (*this)(_vec<int,sizeof...(Ints)>(is...));\
	}\
	template<typename... Ints>\
	requires is_all_v<int, Ints...>\
	constexpr decltype(auto) operator()(Ints... is) const {\
		return (*this)(_vec<int,sizeof...(Ints)>(is...));\
	}\
\
	TENSOR_ADD_BRACKET_FWD_TO_CALL()

#define TENSOR_TOTALLY_ANTISYMMETRIC_CLASS_OPS()\
	TENSOR_ADD_RANK_N_ACCESSOR()\
	TENSOR_TOTALLY_ANTISYMMETRIC_LOCAL_READ_FOR_WRITE_INDEX() /* needed before TENSOR_ADD_ITERATOR in TENSOR_ADD_OPS */\
	TENSOR_ADD_OPS(_asymR)\
	TENSOR_ADD_TOTALLY_ANTISYMMETRIC_CALL_INDEX()

template<typename Inner_, int localDim_, int localRank_>
struct _asymR {
	TENSOR_HEADER_TOTALLY_ANTISYMMETRIC(_asymR, Inner_, localDim_, localRank_)
	std::array<Inner, localCount> s = {};
	constexpr _asymR() {}
	TENSOR_TOTALLY_ANTISYMMETRIC_CLASS_OPS()
};

// dense vec-of-vec

// some template metaprogram helpers
//  needed for the math function
//  including operators, esp *

// _tensori helpers:
// _tensor<T, index_vec<dim>, index_vec<dim2>, ..., index_vec<dimN>>
//  use index_sym<> index_asym<> for injecting storage optimization
// _tensor<T, index_sym<dim1>, ..., dimN>

template<int dim>
struct index_vec {
	template<typename T>
	using type = _vec<T,dim>;
	// so (hopefully) index_vec<dim><T> == _vec<dim,T>
};

template<int dim>
struct index_sym {
	template<typename T>
	using type = _sym<T,dim>;
};

template<int dim>
struct index_asym {
	template<typename T>
	using type = _asym<T,dim>;
};

template<int dim>
struct index_ident {
	template<typename T>
	using type = _ident<T,dim>;
};

template<int dim, int rank>
struct index_symR {
	template<typename T>
	using type = _symR<T,dim,rank>;
};

template<int dim, int rank>
struct index_asymR {
	template<typename T>
	using type = _asymR<T,dim,rank>;
};


// hmm, I'm trying to use these index_*'s in combination with is_instance_v<T, index_*<dim>::template type> but it's failing, so here they are specialized
template<typename T> struct is_vec : public std::false_type {};
template<typename T, int d> struct is_vec<_vec<T,d>> : public std::true_type {};
template<typename T> constexpr bool is_vec_v = is_vec<T>::value;

template<typename T> struct is_ident : public std::false_type {};
template<typename T, int d> struct is_ident<_ident<T,d>> : public std::true_type {};
template<typename T> constexpr bool is_ident_v = is_ident<T>::value;

template<typename T> struct is_sym : public std::false_type {};
template<typename T, int d> struct is_sym<_sym<T,d>> : public std::true_type {};
template<typename T> constexpr bool is_sym_v = is_sym<T>::value;

template<typename T> struct is_asym : public std::false_type {};
template<typename T, int d> struct is_asym<_asym<T,d>> : public std::true_type {};
template<typename T> constexpr bool is_asym_v = is_asym<T>::value;

template<typename T> struct is_symR : public std::false_type {};
template<typename T, int d, int r> struct is_symR<_symR<T,d,r>> : public std::true_type {};
template<typename T> constexpr bool is_symR_v = is_symR<T>::value;

template<typename T> struct is_asymR : public std::false_type {};
template<typename T, int d, int r> struct is_asymR<_asymR<T,d,r>> : public std::true_type {};
template<typename T> constexpr bool is_asymR_v = is_asymR<T>::value;

// can I shorthand this? what is the syntax?
// this has a template and not a type on the lhs so I think no?
//template<int dim> using _vecR = index_vec<dim>::type;
//template<int dim> using _symR = index_sym<dim>::type;
//template<int dim> using _asymR = index_asym<dim>::type;

// useful helper macros, same as above but with transposed order

// _tensori:
// tensor which allows custom nested storage, such as symmetric indexes

// TODO I could switch this to a list of templates-of-<type,int> for nestings ... hmm ... how ugly would that look?
template<typename T, typename Index, typename... Indexes>
struct _tensori_impl {
	using tensor = typename Index::template type<typename _tensori_impl<T, Indexes...>::tensor>;
};

template<typename T, typename Index>
struct _tensori_impl<T, Index> {
	using tensor = typename Index::template type<T>;
};

template<typename T, typename Index, typename... Indexes>
using _tensori = typename _tensori_impl<T, Index, Indexes...>::tensor;


// make a tensor from a list of dimensions
// ex: _tensor<T, dim1, ..., dimN>
// fully expanded storage - no spatial optimizations
// TODO can I accept template args as int or Index?
// maybe vararg function return type and decltype()?

template<typename T, int dim, int... dims>
struct _tensor_impl {
	using tensor = _vec<typename _tensor_impl<T, dims...>::tensor, dim>;
};

template<typename T, int dim>
struct _tensor_impl<T, dim> {
	using tensor = _vec<T,dim>;
};

template<typename T, int dim, int... dims>
using _tensor = typename _tensor_impl<T, dim, dims...>::tensor;

// tensor operations

template<typename A, typename B>
requires IsBinaryTensorDiffTypeButMatchingDims<A,B>
bool operator==(A const & a, B const & b) {
	for (auto i = a.begin(); i != a.end(); ++i) {
		if (a(i.index) != b(i.index)) return false;
	}
	return true;
}

template<typename A, typename B>
requires IsBinaryTensorDiffTypeButMatchingDims<A,B>
bool operator!=(A const & a, A const & b) {
	return !operator==(a,b);
}

//  tensor/scalar sum and scalar/tensor sum
/*
result type for tensor storage and scalar operation
	_vec	_ident	_sym	_asym	_symR	_asymR
+	_vec	_sym	_sym	_mat	_symR	_tensorr
-	_vec	_sym	_sym	_mat	_symR	_tensorr
*	_vec	_ident	_sym	_asym	_symR	_asymR
/	_vec	_ident	_sym	_asym	_symR	_asymR
*/


#define TENSOR_SCALAR_MUL_OP(op)\
template<typename T>\
requires (is_tensor_v<T>)\
decltype(auto) operator op(T const & a, typename T::Scalar const & b) {\
	return T([&](auto... is) -> typename T::Scalar {\
		return a(is...) op b;\
	});\
}\
\
template<typename T>\
requires (is_tensor_v<T>)\
decltype(auto) operator op(typename T::Scalar const & a, T const & b) {\
	return T([&](auto... is) -> typename T::Scalar {\
		return a op b(is...);\
	});\
}


#define TENSOR_SCALAR_SUM_OP(op)\
template<typename T>\
requires (is_tensor_v<T>)\
decltype(auto) operator op(T const & a, typename T::Scalar const & b) {\
	using R = typename T::SumWithScalarResult;\
	return R([&](auto... is) -> typename T::Scalar {\
		return a(is...) op b;\
	});\
}\
\
template<typename T>\
requires (is_tensor_v<T>)\
decltype(auto) operator op(typename T::Scalar const & a, T const & b) {\
	using R = typename T::SumWithScalarResult;\
	return R([&](auto... is) -> typename T::Scalar {\
		return a op b(is...);\
	});\
}

TENSOR_SCALAR_SUM_OP(+)
TENSOR_SCALAR_SUM_OP(-)
TENSOR_SCALAR_MUL_OP(*)

//TENSOR_SCALAR_MUL_OP(/) 
// would be using the same type if not for divide-by-zeros
//TENSOR_SCALAR_SUM_OP(/)
// but tensor / scalar should still preserve structure
// just not scalar / tensor
// sooo .... here it is manually:

template<typename T>
requires (is_tensor_v<T>)
decltype(auto) operator /(T const & a, typename T::Scalar const & b) {
	return T([&](auto... is) -> typename T::Scalar {
		return a(is...) / b;
	});
}

template<typename T>
requires (is_tensor_v<T>)
decltype(auto) operator /(typename T::Scalar const & a, T const & b) {
	using R = typename T::SumWithScalarResult;
	return R([&](auto... is) -> typename T::Scalar {
		return a / b(is...);
	});
}

//  tensor/tensor op

#define TENSOR_TENSOR_OP(op)\
\
/* works with arbitrary storage.  so sym+asym = mat */\
/* TODO PRESERVE MATCHING STORAGE OPTIMIZATIONS */\
template<typename A, typename B>\
/*requires IsBinaryTensorDiffTypeButMatchingDims<A,B>*/\
requires (\
	IsBinaryTensorOp<A,B>\
	&& A::dims == B::dims\
	&& !std::is_same_v<A, B> /* because that is caught next, until I get this to preserve storage opts...*/\
)\
typename A::template ExpandAllIndexes<> operator op(A const & a, B const & b) {\
	return typename A::template ExpandAllIndexes<>(\
		[&](auto... is) -> typename A::Scalar {\
			return a(is...) op b(is...);\
		});\
}\
\
/* until I get down preserving the storage, lets match to like types */\
template<typename T>\
requires (is_tensor_v<T>)\
T operator op(T const & a, T const & b) {\
	return T([&](auto... is) -> typename T::Scalar {\
		return a(is...) op b(is...);\
	});\
}

TENSOR_TENSOR_OP(+)
TENSOR_TENSOR_OP(-)
TENSOR_TENSOR_OP(/)

//element-wise multiplication
// c_i1_i2_... := a_i1_i2_... * b_i1_i2_...
// Hadamard product / per-element multiplication
// TODO let 'a' and 'b' be dif types, so long as rank and dim match i.e. so long as the read-iterator domain matches.
//  pick the result to be the generalization of the two, so if one has sym<> indexes and the other doesn't use the not-sym in the result
template<typename T>
requires is_tensor_v<T>
T elemMul(T const & a, T const & b) {
	return T([&](typename T::intN i) -> typename T::Scalar {
		return a(i) * b(i);
	});
}

// GLSL naming compat
// TODO only for matrix? meh?
template<typename... T>
auto matrixCompMult(T&&... args) {
	return elemMul(std::forward<T>(args)...);
}

// more name compat
template<typename... T>
auto hadamard(T&&... args) {
	return elemMul(std::forward<T>(args)...);
}

// dot product.  To generalize this I'll consider it to be the Frobenius norm, since * will already be contraction
// TODO get rid of member functions  ...
// .... or have the member functions call into these.
// 	c := Σ_i1_i2_... a_i1_i2_... * b_i1_i2_...
template<typename A, typename B>
requires IsBinaryTensorOp<A,B>
typename A::Scalar inner(A const & a, B const & b) {
	auto i = a.begin();
	auto sum = a(i.index) * b(i.index);
	for (++i; i != a.end(); ++i) {
		sum += a(i.index) * b(i.index);
	}
	return sum;
}

// naming compat
template<typename... T>
auto dot(T&&... args) {
	return inner(std::forward<T>(args)...);
}

template<typename T>
requires is_tensor_v<T>
typename T::Scalar lenSq(T const & v) {
	return dot(v, v);
}

template<typename T>
requires (is_tensor_v<T>)
typename T::Scalar length(T const & v) {
	// TODO should I recast to Scalar, or just let it preserve double, or use sqrtf or what?
	return (typename T::Scalar)sqrt(lenSq(v));
}

template<typename A, typename B>
requires IsBinaryTensorOp<A,B>
typename A::Scalar distance(A const & a, B const & b) {
	return length(b - a);
}

template<typename T>
requires (is_tensor_v<T>)
T normalize(T const & v) {
	return v / length(v);
}

// c_i := ε_ijk * b_j * c_k
template<typename A, typename B>
requires IsBinaryTensorR3xR3Op<A,B>
auto cross(A const & a, B const & b) {
	return A(
		a[1] * b[2] - a[2] * b[1],
		a[2] * b[0] - a[0] * b[2],
		a[0] * b[1] - a[1] * b[0]);
}

// outer product of tensors c_i1_..ip_j1_..jq = a_i1..ip * b_j1..jq
// for vectors: c_ij := a_i * b_j
template<typename A, typename B>
requires IsBinaryTensorOp<A,B>
auto outer(A const & a, B const & b) {
	using AB = typename A::template ReplaceScalar<B>;
	//another way to implement would be a per-elem .map(), and just return the new elems as a(i) * b
	return AB([&](typename AB::intN i) -> typename A::Scalar {
		static_assert(decltype(i)::template dim<0> == A::rank + B::rank);
		return a(i.template subset<A::rank, 0>()) * b(i.template subset<B::rank, A::rank>());
	});
}

// GLSL naming compat
template<typename... T>
auto outerProduct(T&&... args) {
	return outer(std::forward<T>(args)...);
}

// matrix functions

#if 0
//https://stackoverflow.com/a/50471331
template<typename T, std::size_t N, typename... Ts>
constexpr std::array<T, N> permuteArray(
	std::array<T, N> const & arr,
	std::array<int, N> const & permutation,
	Ts&&... processed
) {
	if constexpr (sizeof...(Ts) == N) {
		return std::array<T, N>{ std::forward<Ts>(processed)... };
	} else {
		return permute(
			arr,
			permutation,
			std::forward<Ts>(processed)...,
			arr[permutation[sizeof...(Ts)]]
		);
	}
}

//aka 'reshape' ?
// nah reshape in matlab is for resizing dimensions and maintaining storage.
// this is more of a swizzle (dimension-swap) but for ranks ... rank-swap ... index-swap ...
template<typename T, typename I, I... Is>
require (is_tensor_v<T>)
auto permuteIndexes(T const & t) {
	using R =
		T
			... for each i ...
			::template ReplaceDim<i, T::template dim<Is[i]>>;
	return R([](typename R::intN i) -> typename R::Scalar {
		return t(permuteArray(i));
	});
}
#endif

/*
Transpose, i.e. exchange two indexes

If the two indexes are sequential and _sym symmetric storage is used then nothing is changed.
TODO if the two indexes are sequential and _asym storage is used then just negative the values.
Otherwise expand the internal storage at indexes m and n (i.e. convert it from _sym or _asym into _vec),
then exchange the dimensions.
*/
template<int m/*=0*/, int n/*=1*/, typename T>
requires (
	is_tensor_v<T>
	&& T::rank >= 2
)
auto transpose(T const & t) {
	if constexpr (m == n) {
		//don't reshape if we're flipping the same index with itself
		return t;
	} else if constexpr (
		// don't reshape symmetric -- their transpose is identity
		T::template numNestingsToIndex<m> == T::template numNestingsToIndex<n>
		&& is_sym_v<T>
	) {
		return t;
	} else if constexpr (
		// don't reshape antisymmetric -- their transpose is negative
		T::template numNestingsToIndex<m> == T::template numNestingsToIndex<n>
		&& is_asym_v<T>
	) {
		return -t;
	} else {	// m < n and they are different storage nestings
		constexpr int mdim = T::template dim<m>;
		constexpr int ndim = T::template dim<n>;
		// now re-index E to exchange dimensions
		// replace the storage at index m with an equivalent one but of dimension of n's index
		using Tnm = typename T
			::template ExpandIndex<m, n> //ReplaceDim doesn't guarantee to expand if the dims match
			::template ReplaceDim<m, ndim>
			::template ReplaceDim<n, mdim>;
		return Tnm([&](typename Tnm::intN i) {
			std::swap(i(m), i(n));
			return t(i);
		});
	}
}

// contraction of two indexes of a tensor
template<int m/*=0*/, int n/*=1*/, typename T>
requires (is_tensor_v<T>
	&& m < T::rank
	&& n < T::rank
	&& T::template dim<m> == T::template dim<n>
)
auto contract(T const & t) {
	using S = typename T::Scalar;
	if constexpr (m > n) {
		//ensure m < n esp for swizzling index's sake
		return contract<n,m,T>(t);
	} else if constexpr (m == n) {
		// hmmmm Σ_i g^ii a_i is a technically-incorrect tensor operation
		if constexpr (T::rank == 1) {
			S sum = t(0);
			for (int k = 1; k < T::template dim<0>; ++k) {
				sum += t(k);
			}
			return sum;
		} else {
			using R = typename T::template RemoveIndex<m>;
			// TODO a macro to remove the m'th element from 'int... i'
			//return R([](auto... is) -> S {
			// or TODO implement intN access to asym (and fully sym)
			return R([&](typename R::intN i) -> S {
				// static_assert R::intN::dims == T::intN::dims-1
				auto j = typename T::intN([&](int jk) -> int {
					if (jk == m) return 0; // set j[m] = 0
					if (jk > m) --jk;
					return i[jk];
				});
				//j[m] == 0 ...
				S sum = t(j);
				for (int k = 1; k < T::template dim<m>; ++k) {
					j[m] = k;
					sum += t(j);
				}
				return sum;
			});
		}
	} else { // m < n
		if constexpr (T::rank == 2) {
			S sum = t(0,0);
			for (int k = 1; k < T::template dim<m>; ++k) {
				sum += t(k,k);
			}
			return sum;
		} else {
			using R = typename T::template RemoveIndex<m,n>;
			return R([&](typename R::intN i) -> S {
				// static_assert R::intN::dims == T::intN::dims-2
				auto j = typename T::intN([&](int jk) -> int {
					if (jk == m) return 0; // j[m] = 0
					if (jk == n) return 0; // j[n] = 0
					// make sure you compare in decreasing order or else a decrement can destroy iteration and lead to oob lookups
					if (jk > n) --jk; // if m != n then do this twice
					if (jk > m) --jk; //  but (case above) if m == n do this once
					return i[jk];
				});
				// j[m] = j[n] = 0 ...
				S sum = t(j);
				for (int k = 1; k < T::template dim<m>; ++k) {
					j[m] = j[n] = k;
					sum += t(j);
				}
				return sum;
			});
		}
	}
}

// naming compat
template<int m, int n, typename T>
auto trace(T const & t) {
	return contract<m,n,T>(t);
}

//contracts the first index with the next count index and repeat count times
template<int index, int count, typename A>
requires (is_tensor_v<A>)
auto contractN(A const & a) {
	static_assert(index >= 0 && index < A::rank);
	static_assert(index + count >= 0 && index + count < A::rank);
	if constexpr (count == 0) {
		return a;
	} else {
		auto ac = contract<index,index+count>(a);
		if constexpr (count == 1) {
			return ac;
		} else {
			return contractN<index,count-1>(ac);
		}
	}
}

// this isn't really interior, but more of a mix of interior + outer + contract
// it is an interior product provided the num. of indexes == A::rank
// it is matrix-mul if num == 1
// TODO this could stand to be optimized
template<int num, typename A, typename B>
requires IsBinaryTensorOp<A,B>
auto interior(A const & a, B const & b) {
	return contractN<A::rank-num,num>(outer(a,b));
}

// symmetrize or antisymmetrize a tensor
//  I am not convinced this should be the default casting operation from non-(a)sym to (a)sym since it incurs a few more operations
// but it should def be made available

template<typename T>
requires IsSquareTensor<T>
auto makeSym(T const & t) {
	using S = typename T::Scalar;
	using intN = typename T::intN;
	using R = std::conditional_t<T::rank == 2,
		_sym<S, T::template dim<0>>,
		_symR<S, T::template dim<0>, T::rank>>;
	// iterate over write index, then iterate over all permutations of the read index and sum
	return R([&](intN i) -> S {
		S result = {};
		//'j' is our permutation
		intN j = initIntVecWithSequence<std::make_integer_sequence<int, T::rank>>::value();
		do {
			// 'ij' is 'i' permuted by 'j'
			intN ij = [&](int k) -> int { return i[j[k]]; };
			result += t(ij);
		} while (std::next_permutation(j.s.begin(), j.s.end()));
		return result / (S)factorial(T::rank);
	});
}

//that's right, same function, just different return type
template<typename T>
requires IsSquareTensor<T>
auto makeAsym(T const & t) {
	using S = typename T::Scalar;
	using intN = typename T::intN;
	using R = std::conditional_t<T::rank == 2,
		_asym<S, T::template dim<0>>,
		_asymR<S, T::template dim<0>, T::rank>>;
	// iterate over write index, then iterate over all permutations of the read index and sum
	return R([&](intN i) -> S {
		S result = {};
		//'j' is our permutation
		intN j = initIntVecWithSequence<std::make_integer_sequence<int, T::rank>>::value();
		do {
			//count # of flips
			// TODO combine this with next_permutation so you don't have to keep recounting
			intN sortedj = j;
			auto sign = antisymSortAndCountFlips(sortedj);
			if (sign == Sign::ZERO) {
				throw Common::Exception() << "shouldn't get here";
			}
			// 'ij' is 'i' permuted by 'j'
			intN ij = [&](int k) -> int { return i[j[k]]; };
			if (sign == Sign::NEGATIVE) {
				result -= t(ij);
			} else {
				result += t(ij);
			}
		} while (std::next_permutation(j.s.begin(), j.s.end()));
		return result / (S)factorial(T::rank);
	});
}

// wedge product

template<typename A, typename B>
requires IsBinaryTensorOp<A,B>
auto wedge(A const & a, B const & b) {
	return makeAsym(outer(a,b));
}

// Hodge dual

template<typename A>
requires IsSquareTensor<A>
auto hodgeDual(A const & a) {
	static constexpr int rank = A::rank;
	static constexpr int dim = A::template dim<0>;
	static_assert(rank <= dim);
	using S = typename A::Scalar;
	return interior<rank>(a, _asymR<S, dim, dim>(1)) / (S)factorial(rank);
}

// operator* is contract(outer(a,b)) ... aka interior1(a,b)
// TODO maybe generalize further with the # of indexes to contract:
// c_i1...i{p}_j1_..._j{q} = Σ_k1...k{r} a_i1_..._i{p}_k1_...k{r} * b_k1_..._k{r}_j1_..._j{q}

template<typename A, typename B>
requires IsBinaryTensorOpWithMatchingNeighborDims<A,B>
auto operator*(A const & a, B const & b) {
#if 1	// lazy way.  inline the lambdas and don't waste the outer()'s operation expenses
	//return contract<A::rank-1, A::rank>(outer(a,b));
	return interior<1>(a,b);
#else	// some optimizations, no wasted storage
// TODO FIXME has some bugs.
	using S = typename A::Scalar;
	if constexpr (A::rank == 1 && B::rank == 1) {
		// rank-0 result case
		static_assert(A::dims == B::dims);	//thanks to the 3rd requires condition
		//scalar return case
		S sum = a(0) * b(0);
		for (int k = 1; k < A::template dim<0>; ++k) {
			sum += a(k) * b(k);
		}
		return sum;
	} else {
		using R = typename A
			::template ReplaceScalar<B>
			::template RemoveIndex<A::rank-1, A::rank>;
		return R([&](typename R::intN i) -> S {
			auto ai = typename A::intN([&](int j) -> int {
				if (j == A::rank-1) return 0;
				return i[j];
			});
			auto bi = typename B::intN([&](int j) -> int {
				if (j == 0) return 0;
				j -= A::rank-1;
				return i[j];
			});
			S sum = a(ai) * b(bi);
			for (int k = 1; k < A::template dim<A::rank-1>; ++k) {
				ai(A::rank-1) = k;
				bi(0) = k;
				sum += a(ai) * b(bi);
			}
			return sum;
		});
	}
#endif
}

// diagonalize an index
// well if it's diagonal, might as well use _sym
template<int m/*=0*/, typename T>
requires (is_tensor_v<T>)
auto diagonal(T const & t) {
	static_assert(m >= 0 && m < T::rank);
	/* TODO ::InsertIndex that uses _tensor index indicators for what kind of storage to insert
	using R = T
		::ExpandIndex<m>
		::InsertIndex<m, index_vec<T::dim<m>>>;
	... but can't use InsertIndex to insert a _sym with the expanded index
	... maybe instead
	using R = T
		::RemoveIndex<m>
		::InsertIndex<m, index_sym<T::dim<m>>>
	*/
	using E = typename T::template ExpandIndex<m>;
	constexpr int nest = E::template numNestingsToIndex<m>;
	using R = typename E
		::template ReplaceNested<
			nest,
			_sym<
				typename E::template Nested<nest+1>,
				E::template dim<m>
			>
		>;
	return R([&](typename R::intN i) {
		if (i[m] != i[m+1]) return typename T::Scalar();
		auto isrc = typename T::intN([&](int j) {
			if (j >= m) return i[j+1];
			return i[j];
		});
		return t(isrc);
	});
}


// specific typed vectors

// TODO i think these slow the build down a bit
//  put it in a test cpp file
//  but that means reconstructing the macros that build the nick etc
//#ifdef DEBUG
//#define TENSOR_USE_STATIC_ASSERTS
//#endif

#ifdef TENSOR_USE_STATIC_ASSERTS

#define TENSOR_ADD_VECTOR_STATIC_ASSERTS(nick,ctype,dim)\
static_assert(sizeof(nick##dim1) == sizeof(ctype) * dim1);\
static_assert(std::is_same_v<nick##dim1::Scalar, ctype>);\
static_assert(std::is_same_v<nick##dim1::Inner, ctype>);\
static_assert(nick##dim1::rank == 1);\
static_assert(nick##dim1::dim<0> == dim1);\
static_assert(nick##dim1::numNestings == 1);\
static_assert(nick##dim1::count<0> == dim1);

#define TENSOR_ADD_MATRIX_STATIC_ASSERTS(nick, ctype, dim1, dim2)\
static_assert(sizeof(nick##dim1##x##dim2) == sizeof(ctype) * dim1 * dim2);\
static_assert(nick##dim1##x##dim2::rank == 2);\
static_assert(nick##dim1##x##dim2::dim<0> == dim1);\
static_assert(nick##dim1##x##dim2::dim<1> == dim2);\
static_assert(nick##dim1##x##dim2::numNestings == 2);\
static_assert(nick##dim1##x##dim2::count<0> == dim1);\
static_assert(nick##dim1##x##dim2::count<1> == dim2);

#define TENSOR_ADD_SYMMETRIC_STATIC_ASSERTS(nick, ctype, dim12)\
static_assert(sizeof(nick##dim12##s##dim12) == sizeof(ctype) * triangleSize(dim12));\
static_assert(std::is_same_v<typename nick##dim12##s##dim12::Scalar, ctype>);\
static_assert(nick##dim12##s##dim12::rank == 2);\
static_assert(nick##dim12##s##dim12::dim<0> == dim12);\
static_assert(nick##dim12##s##dim12::dim<1> == dim12);\
static_assert(nick##dim12##s##dim12::numNestings == 1);\
static_assert(nick##dim12##s##dim12::count<0> == triangleSize(dim12));

#define TENSOR_ADD_ANTISYMMETRIC_STATIC_ASSERTS(nick, ctype, dim12)\
static_assert(sizeof(nick##dim12##a##dim12) == sizeof(ctype) * triangleSize(dim12-1));\
static_assert(std::is_same_v<typename nick##dim12##a##dim12::Scalar, ctype>);\
static_assert(nick##dim12##a##dim12::rank == 2);\
static_assert(nick##dim12##a##dim12::dim<0> == dim12);\
static_assert(nick##dim12##a##dim12::dim<1> == dim12);\
static_assert(nick##dim12##a##dim12::numNestings == 1);\
static_assert(nick##dim12##a##dim12::count<0> == triangleSize(dim12-1));

#define TENSOR_ADD_IDENTITY_STATIC_ASSERTS(nick, ctype, dim12)\
static_assert(sizeof(nick##dim12##i##dim12) == sizeof(ctype));\
static_assert(std::is_same_v<typename nick##dim12##i##dim12::Scalar, ctype>);\
static_assert(nick##dim12##i##dim12::rank == 2);\
static_assert(nick##dim12##i##dim12::dim<0> == dim12);\
static_assert(nick##dim12##i##dim12::dim<1> == dim12);\
static_assert(nick##dim12##i##dim12::numNestings == 1);\
static_assert(nick##dim12##i##dim12::count<0> == 1);

#define TENSOR_ADD_TOTALLY_SYMMETRIC_STATIC_ASSERTS(nick, ctype, localDim, localRank, suffix)\
static_assert(sizeof(nick##suffix) == sizeof(ctype) * symmetricSize(localDim, localRank));\
static_assert(std::is_same_v<typename nick##suffix::Scalar, ctype>);\
static_assert(nick##suffix::rank == localRank);\
static_assert(nick##suffix::dim<0> == localDim); /* TODO repeat depending on dimension */\
static_assert(nick##suffix::numNestings == 1);\
static_assert(nick##suffix::count<0> == symmetricSize(localDim, localRank));

#define TENSOR_ADD_TOTALLY_ANTISYMMETRIC_STATIC_ASSERTS(nick, ctype, localDim, localRank, suffix)\
static_assert(sizeof(nick##suffix) == sizeof(ctype) * antisymmetricSize(localDim, localRank));\
static_assert(std::is_same_v<typename nick##suffix::Scalar, ctype>);\
static_assert(nick##suffix::rank == localRank);\
static_assert(nick##suffix::dim<0> == localDim); /* TODO repeat depending on dimension */\
static_assert(nick##suffix::numNestings == 1);\
static_assert(nick##suffix::count<0> == antisymmetricSize(localDim, localRank));

#else //TENSOR_USE_STATIC_ASSERTS

#define TENSOR_ADD_VECTOR_STATIC_ASSERTS(nick,ctype,dim)
#define TENSOR_ADD_MATRIX_STATIC_ASSERTS(nick, ctype, dim1, dim2)
#define TENSOR_ADD_SYMMETRIC_STATIC_ASSERTS(nick, ctype, dim12)
#define TENSOR_ADD_ANTISYMMETRIC_STATIC_ASSERTS(nick, ctype, dim12)
#define TENSOR_ADD_IDENTITY_STATIC_ASSERTS(nick, ctype, dim12)
#define TENSOR_ADD_TOTALLY_SYMMETRIC_STATIC_ASSERTS(nick, ctype, localDim, localRank, suffix)
#define TENSOR_ADD_TOTALLY_ANTISYMMETRIC_STATIC_ASSERTS(nick, ctype, localDim, localRank, suffix)

#endif //TENSOR_USE_STATIC_ASSERTS

#define TENSOR_ADD_VECTOR_NICKCNAME_TYPE_DIM(nick, ctype, dim1)\
using nick##dim1 = nick##N<dim1>;\
TENSOR_ADD_VECTOR_STATIC_ASSERTS(nick, ctype,dim1);

#define TENSOR_ADD_MATRIX_NICKNAME_TYPE_DIM(nick, ctype, dim1, dim2)\
using nick##dim1##x##dim2 = nick##MxN<dim1,dim2>;\
TENSOR_ADD_MATRIX_STATIC_ASSERTS(nick, ctype, dim1, dim2)

#define TENSOR_ADD_SYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, dim12)\
using nick##dim12##s##dim12 = nick##NsN<dim12>;\
TENSOR_ADD_SYMMETRIC_STATIC_ASSERTS(nick, ctype, dim12)

#define TENSOR_ADD_ANTISYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, dim12)\
using nick##dim12##a##dim12 = nick##NaN<dim12>;\
TENSOR_ADD_ANTISYMMETRIC_STATIC_ASSERTS(nick, ctype, dim12)

#define TENSOR_ADD_IDENTITY_NICKNAME_TYPE_DIM(nick, ctype, dim12)\
using nick##dim12##i##dim12 = nick##NiN<dim12>;\
TENSOR_ADD_IDENTITY_STATIC_ASSERTS(nick, ctype, dim12)

#define TENSOR_ADD_TOTALLY_SYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, localDim, localRank, suffix)\
using nick##suffix = nick##NsR<localDim, localRank>;\
TENSOR_ADD_TOTALLY_SYMMETRIC_STATIC_ASSERTS(nick, ctype, localDim, localRank, suffix)

#define TENSOR_ADD_TOTALLY_ANTISYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, localDim, localRank, suffix)\
using nick##suffix = nick##NaR<localDim, localRank>;\
TENSOR_ADD_TOTALLY_ANTISYMMETRIC_STATIC_ASSERTS(nick, ctype, localDim, localRank, suffix)

#define TENSOR_ADD_NICKNAME_TYPE(nick, ctype)\
/* typed vectors */\
template<int N> using nick##N = _vec<ctype, N>;\
TENSOR_ADD_VECTOR_NICKCNAME_TYPE_DIM(nick, ctype, 2)\
TENSOR_ADD_VECTOR_NICKCNAME_TYPE_DIM(nick, ctype, 3)\
TENSOR_ADD_VECTOR_NICKCNAME_TYPE_DIM(nick, ctype, 4)\
/* typed matrices */\
template<int M, int N> using nick##MxN = _mat<ctype, M, N>;\
TENSOR_ADD_MATRIX_NICKNAME_TYPE_DIM(nick, ctype, 2, 2)\
TENSOR_ADD_MATRIX_NICKNAME_TYPE_DIM(nick, ctype, 2, 3)\
TENSOR_ADD_MATRIX_NICKNAME_TYPE_DIM(nick, ctype, 2, 4)\
TENSOR_ADD_MATRIX_NICKNAME_TYPE_DIM(nick, ctype, 3, 2)\
TENSOR_ADD_MATRIX_NICKNAME_TYPE_DIM(nick, ctype, 3, 3)\
TENSOR_ADD_MATRIX_NICKNAME_TYPE_DIM(nick, ctype, 3, 4)\
TENSOR_ADD_MATRIX_NICKNAME_TYPE_DIM(nick, ctype, 4, 2)\
TENSOR_ADD_MATRIX_NICKNAME_TYPE_DIM(nick, ctype, 4, 3)\
TENSOR_ADD_MATRIX_NICKNAME_TYPE_DIM(nick, ctype, 4, 4)\
/* identity matrix */\
template<int N> using nick##NiN = _ident<ctype, N>;\
TENSOR_ADD_IDENTITY_NICKNAME_TYPE_DIM(nick, ctype, 2)\
TENSOR_ADD_IDENTITY_NICKNAME_TYPE_DIM(nick, ctype, 3)\
TENSOR_ADD_IDENTITY_NICKNAME_TYPE_DIM(nick, ctype, 4)\
/* typed symmetric matrices */\
template<int N> using nick##NsN = _sym<ctype, N>;\
TENSOR_ADD_SYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, 2)\
TENSOR_ADD_SYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, 3)\
TENSOR_ADD_SYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, 4)\
/* typed antisymmetric matrices */\
template<int N> using nick##NaN = _asym<ctype, N>;\
TENSOR_ADD_ANTISYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, 2)\
TENSOR_ADD_ANTISYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, 3)\
TENSOR_ADD_ANTISYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, 4)\
/* totally symmetric tensors */\
template<int D, int R> using nick##NsR = _symR<ctype, D, R>;\
TENSOR_ADD_TOTALLY_SYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, 2, 3, 2s2s2)\
TENSOR_ADD_TOTALLY_SYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, 3, 3, 3s3s3)\
TENSOR_ADD_TOTALLY_SYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, 4, 3, 4s4s4)\
TENSOR_ADD_TOTALLY_SYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, 2, 4, 2s2s2s2)\
TENSOR_ADD_TOTALLY_SYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, 3, 4, 3s3s3s3)\
TENSOR_ADD_TOTALLY_SYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, 4, 4, 4s4s4s4)\
/* totally antisymmetric tensors */\
template<int D, int R> using nick##NaR = _asymR<ctype, D, R>;\
/* can't exist: TENSOR_ADD_TOTALLY_ANTISYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, 2, 3, 2a2a2)*/\
TENSOR_ADD_TOTALLY_ANTISYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, 3, 3, 3a3a3)\
TENSOR_ADD_TOTALLY_ANTISYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, 4, 3, 4a4a4)\
/* can't exist: TENSOR_ADD_TOTALLY_ANTISYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, 2, 4, 2a2a2a2)*/\
/* can't exist: TENSOR_ADD_TOTALLY_ANTISYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, 3, 4, 3a3a3a3)*/\
TENSOR_ADD_TOTALLY_ANTISYMMETRIC_NICKNAME_TYPE_DIM(nick, ctype, 4, 4, 4a4a4a4)


#define TENSOR_ADD_UTYPE(x)	TENSOR_ADD_NICKNAME_TYPE(u##x,unsigned x)


#define TENSOR_ADD_TYPE(x)	TENSOR_ADD_NICKNAME_TYPE(x,x)


TENSOR_ADD_TYPE(bool)
TENSOR_ADD_TYPE(char)	// TODO 'schar' for 'signed char' or just 'char' for 'signed char' even tho that's not C convention?
TENSOR_ADD_UTYPE(char)
TENSOR_ADD_TYPE(short)
TENSOR_ADD_UTYPE(short)
TENSOR_ADD_TYPE(int)
TENSOR_ADD_UTYPE(int)
TENSOR_ADD_TYPE(float)
TENSOR_ADD_TYPE(double)
TENSOR_ADD_NICKNAME_TYPE(size, size_t)
TENSOR_ADD_NICKNAME_TYPE(intptr, intptr_t)
TENSOR_ADD_NICKNAME_TYPE(uintptr, uintptr_t)
TENSOR_ADD_NICKNAME_TYPE(ldouble, long double)


// ostream
// _vec does have .fields
// and I do have my default .fields ostream
// but here's a manual override anyways
// so that the .fields vec2 vec3 vec4 and the non-.fields other vecs all look the same
#if 0 // use array.  fails cuz quat (wsubclass of _vec) can't deduce whether _vec or _quat which both are used here:
template<typename T>
requires (is_tensor_v<T>)
std::ostream & operator<<(std::ostream & o, T const & t) {
	return o << t.s;
}
#endif
#if 0 // just use iterator
template<typename T>
requires (is_tensor_v<T>)
std::ostream & operator<<(std::ostream & o, T const & t) {
	return Common::iteratorToOStream(o, t);
}
#endif
#if 1 // use array fields
template<typename T, int N>
std::ostream & operator<<(std::ostream & o, _vec<T,N> const & t) {
	return o << t.s;
}
template<typename T, int N>
std::ostream & operator<<(std::ostream & o, _sym<T,N> const & t) {
	return o << t.s;
}
template<typename T, int N>
std::ostream & operator<<(std::ostream & o, _asym<T,N> const & t) {
	return o << t.s;
}
template<typename T, int N>
std::ostream & operator<<(std::ostream & o, _ident<T,N> const & t) {
	return o << t.s;
}
template<typename T, int N, int R>
std::ostream & operator<<(std::ostream & o, _symR<T,N,R> const & t) {
	return o << t.s;
}
template<typename T, int N, int R>
std::ostream & operator<<(std::ostream & o, _asymR<T,N,R> const & t) {
	return o << t.s;
}
#endif
#if 0 // just use iterator.  but i guess AntiSymRef doesn't?
template<typename T, int N>
std::ostream & operator<<(std::ostream & o, _vec<T,N> const & t) {
	return Common::iteratorToOStream(o, t);
}
template<typename T, int N>
std::ostream & operator<<(std::ostream & o, _sym<T,N> const & t) {
	return Common::iteratorToOStream(o, t);
}
template<typename T, int N>
std::ostream & operator<<(std::ostream & o, _asym<T,N> const & t) {
	return Common::iteratorToOStream(o, t);
}
template<typename T, int N>
std::ostream & operator<<(std::ostream & o, _ident<T,N> const & t) {
	return Common::iteratorToOStream(o, t);
}
template<typename T, int N, int R>
std::ostream & operator<<(std::ostream & o, _symR<T,N,R> const & t) {
	return Common::iteratorToOStream(o, t);
}
template<typename T, int N, int R>
std::ostream & operator<<(std::ostream & o, _asymR<T,N,R> const & t) {
	return Common::iteratorToOStream(o, t);
}
#endif

} // namespace Tensor

namespace std {

// tostring

template<typename T, int n>
std::string to_string(Tensor::_vec<T, n> const & x) {
	return Common::objectStringFromOStream(x);
}

#if 0
// half baked idea of making std::apply compatible with Tensor::_vec
// I would just use std::array as the internal storage of _vec, but subset() does some memory casting which maybe I shouldn't be doing .. */
// tuple_size, make this match array i.e. storage size, returns localCount

template<typename T, int N>
struct tuple_size<Tensor::_vec<T,N>> {
	static constexpr auto value = Tensor::_vec<T,N>::localCount;
};

template<typename T, int N>
struct tuple_size<Tensor::_sym<T,N>> {
	static constexpr auto value = Tensor::_sym<T,N>::localCount; 	// (N*(N+1))/2
};

// std::get ... all the dif impls ...

template<std::size_t I, typename T, std::size_t N> constexpr T & get(Tensor::_vec<T,N> & v) noexcept { return v[I]; }
template<std::size_t I, typename T, std::size_t N> constexpr T && get(Tensor::_vec<T,N> && v) noexcept { return v[I]; }
template<std::size_t I, typename T, std::size_t N> constexpr T const & get(Tensor::_vec<T,N> const & v) noexcept { return v[I]; }
template<std::size_t I, typename T, std::size_t N> constexpr T const && get(Tensor::_vec<T,N> const && v) noexcept { return v[I]; }

#endif

}

// why do I have an #include at the bottom of this file?
// because at the top I have the forward-declaration to the functions in this include
// so it had better come next
#include "Tensor/Inverse.h"	
#include "Tensor/Index.h"
