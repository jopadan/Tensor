#pragma once

#include "Tensor/Vector.h.h"
#include "Tensor/Index.h.h"
#include "Common/Exception.h"
#include "Common/Tuple.h"	//TupleForEach
#include "Common/Meta.h"
#include <tuple>
#include <functional>	// plus minus binary_operator etc

/*
Ok here's a dilemma ... a_i = b_ijk^jk_lm^lm * c_npq^npq
 if I outer-first this can become huge in its storage and calculations 
 if I constract-first then I have to use some extra storage.
which do I choose?
or do I choose both?  std::variant<TensorType, TensorType&> t; ?
tempted to go for storing tensors themselves within IndexAccess objs (which means contract-upon-initialization)
 but that means no more need to construct AST's of tensor expressions... just process the tensor math as the operators are encountered.

a_i = b_ij * c_jk * d_k ...
	becomes:
a_x = sum_j (b_xj * (c_jx * d_x + c_jy * d_y + c_jz * d_z))
a_y = sum_j (b_yj * (c_jx * d_x + c_jy * d_y + c_jz * d_z))
a_z = sum_j (b_zj * (c_jx * d_x + c_jy * d_y + c_jz * d_z))
 ... 4 x 3 (sum) x 3 (dst) = 36 muls
 ... (2 (c*d) + 2 (b*c)) x 3 = 12 adds
	unless i perform contractions up-front and DO NOT maintain the AST but only push IndexAccess objects forward ... then its more like:
result of c_jk d_k = [
	c_xx * d_x + c_xy * d_y + c_xz * d_z,
	c_yx * d_x + c_yy * d_y + c_yz * d_z,
	c_zx * d_x + c_zy * d_y + c_zz * d_z,
]
 ... 9 muls, 6 adds
a_ij = b_ij * (result of c_jk d_k) = [
	b_xx * (c*d)_x + b_xy * (c*d)_y + b_xz * (c*d)_z,
	b_yx * (c*d)_x + b_yy * (c*d)_y + b_yz * (c*d)_z,
	b_zx * (c*d)_x + b_zy * (c*d)_y + b_zz * (c*d)_z,
]
 ... another 9 muls and 6 adds
 ... total 18 muls 12 adds

hmm, 36 muls or 18 muls?  which is more?
seems like more often than not it's better to evalute expressions per-opration rather than all-upon-assignment (which it seems like I see promoted doing this way everywhere)
the only time delaying evaluation until assignment could be good is if you're doing traces or transposes or sums or something ... in which case just store the index remap and don't need to copy the source.
 but wait (theres more) to allow for same-reference reading and writing, i'm already copying the written tensor in assign() before returning it ...
 ... aren't I halfway there?

I guess one option is ... implement both & perf test.
Easiest one to implement first is computing up front rather than preserving expression trees.

orrrr I could even do both.
use expression-trees.  lazy evaluate.  and for certain operations like trace or mul (which is outer+trace) thennnn cache.  and have read() read from the cache.
you can even have some kind of inner class compile-time-dependent for whether a(i,j) IndexAccess should cache or not based on the # of sum-indexes present (0 = dont cache, >0 = do cache)

so ...

lazy-eval expression-trees
but in the case of traces or tensor-multiplies, cache it at that node.

*/

namespace Tensor {

struct IndexBase {};

template<char ident>
struct Index : public IndexBase {};

template<typename T>
constexpr bool is_IndexExpr_v = requires(T const & t) { &T::isIndexExprFlag; };

template<typename A, typename B>
concept IsMatchingRankExpr = 
	is_IndexExpr_v<A>
	&& is_IndexExpr_v<B>
	&& A::rank == B::rank;

template<char ch>
std::ostream & operator<<(std::ostream & o, Index<ch> const & i) {
	return o << ch;
}

//shorthand if you don't want to declare your lhs first and dereference it first ...
// TODO get all dims of the IndexExpr
#define TENSOR_EXPR_ADD_ASSIGNR()\
	template<typename R, typename IndexType, typename... IndexTypes>\
	struct AssignImpl {\
		static decltype(auto) exec(This const & this_) {\
			R r;\
			auto ri = IndexAccess<R, std::tuple<IndexType, IndexTypes...>>(r);\
			ri = this_;\
			return r;\
		}\
	};\
	template<typename R, typename IndexType, typename... IndexTypes>\
	requires (\
		is_tensor_v<R>\
		/*&& R::dims() == dims()*/\
		&& R::rank == rank\
		&& is_all_base_of_v<IndexBase, IndexType, IndexTypes...>\
	)\
	decltype(auto) assignR(IndexType, IndexTypes...) const {\
		return AssignImpl<R, IndexType, IndexTypes...>::exec(*this);\
	}

#define TENSOR_EXPR_ADD_ASSIGN()\
	template<typename IndexType, typename... IndexTypes>\
	requires (is_all_base_of_v<IndexBase, IndexType, IndexTypes...>)\
	decltype(auto) assign(IndexType, IndexTypes...) const {\
		using R = typename OutputTensorType::template ExpandAllIndexes<>;\
		return AssignImpl<R, IndexType, IndexTypes...>::exec(*this);\
	}


template<typename T>
using GetFirst = typename T::first_type;

/*
GatherIndexes<std::integer_sequence<int, ...>> has ...
	type = std::tuple<
		std::pair<
			Index<ch1>,
			std::integer_sequence<int, i1, ...>
		>,
		std::pair<
			Index<ch2>,
			std::integer_sequence<int, i1, ...>
		>,
	... so type[i][0] represents which index we are looking for
	... and type[i][1] represents where it is found
	... and indexes = type map get 2nd of pair
	([] notation represents element_t<> access) 
*/
template<typename IndexTuple, typename indexSeq>
struct GatherIndexesImpl;
template<typename IndexType, typename... IndexTypes, int i1, int... is>
requires (is_all_base_of_v<IndexBase, IndexType, IndexTypes...>)
struct GatherIndexesImpl<std::tuple<IndexType, IndexTypes...>, std::integer_sequence<int, i1, is...>> {
	static constexpr auto value() {
		using Next = GatherIndexesImpl<std::tuple<IndexTypes...>, std::integer_sequence<int, is...>>;
		using NextType = typename Next::type;
		constexpr int resultLoc = Common::tuple_find_v<IndexType, typename Next::indexes>;
		if constexpr (resultLoc == -1) {
			return (
				Common::tuple_cat_t<
					std::tuple<std::pair<
						IndexType,
						std::integer_sequence<int, i1>
					>>,
					NextType
				>
			*)nullptr;
		} else {
			return (
				Common::element_replace_t<
					NextType,
					resultLoc,
					std::pair<
						IndexType,
						Common::seq_cat_t<
							typename std::tuple_element_t<
								resultLoc,
								NextType
							>::second_type,
							std::integer_sequence<int, i1>
						>
					>
				>
			*)nullptr;
		}
	}
	using type = typename std::remove_pointer_t<decltype(value())>;
	using indexes = Common::TupleTypeMap<type, GetFirst>;
};
template<>
struct GatherIndexesImpl<std::tuple<>, std::integer_sequence<int>> {
	using type = std::tuple<>;
	using indexes = std::tuple<>;
};
template<typename IndexTuple>
using GatherIndexes = typename GatherIndexesImpl<
	IndexTuple,
	std::make_integer_sequence<int, std::tuple_size_v<IndexTuple>>
>::type;

// T is a member of the tuple within GatherIndexes<>'s results
template<typename T>
struct HasMoreThanOneIndex {
	static constexpr bool value = T::second_type::size() > 1;
};

template<typename IndexTuple>
struct GetTupleIthElem {
	template<int i>
	using Get = std::tuple_element_t<i, IndexTuple>;
};

template<typename TensorType>
struct GetTensorIthDim {
	template<int i>
	struct Get {
		static constexpr int value = TensorType::template dim<i>;
	};
};


template<typename T, typename Seq>
struct ApplyTracesImpl;
// assume they're sorted, apply in descending order
template<typename T, typename I, I i1, I i2, I... is>
struct ApplyTracesImpl<T, std::integer_sequence<I, i1, i2, is...>> {
	static constexpr auto exec(T & t) {
		return trace<i1, i2>(
			ApplyTracesImpl<T, std::integer_sequence<I, is...>>::exec(t)
		);
	}
};
template<typename T, typename I>
struct ApplyTracesImpl<T, std::integer_sequence<I>> {
	static constexpr auto exec(T & t) { return t; }
};
template<typename T, typename Seq>
auto applyTraces(T & x) {
	return ApplyTracesImpl<T, Seq>::exec(x);
}

/*
rather than this matching a Tensor for index dereferencing,
this needs its index access abstracted so that binary operations can provide their own as well

TODO instead of .assign() how about just give IndexAccess its own operator()(IndexBase...), and have that produce a tensor.
*/
template<typename InputTensorType_, typename IndexTuple_>
requires is_tensor_v<InputTensorType_>
struct IndexAccess {
	static constexpr bool isIndexExprFlag = {};
	using This = IndexAccess;
	using InputTensorType = InputTensorType_;

	// std::tuple<Index<char>... >
	// this is the input indexes wrapping the tensor, like a(i,j,k ...)
	using IndexTuple = IndexTuple_;	
	
	STATIC_ASSERT_EQ(InputTensorType::rank, (std::tuple_size_v<IndexTuple>));
	
	/*
	TODO HERE before rank is established,
	need to find duplicate indexes and flag them as sum indexes
	then the rank is going to be whats left
	*/
	//using SumIndexSeq = GatherSumIndexes<IndexTuple>;	//pairs of offsets into IndexTuple
	//using AssignIndexSeq = GatherAssignIndexes<IndexTuple>; //offsets into IndexTuple of non-summed indexes
	//static constexpr rank = std::tuple_size_v<AssignIndexes>;
	//using dims = MapValues<AssignIndexes, InputTensorType::dimseq>;
	using GatheredIndexes = GatherIndexes<IndexTuple>;
	using GetSingleVsDouble = Common::tuple_get_filtered_indexes_t<GatheredIndexes, HasMoreThanOneIndex>;
	// TODO static-assert there index counts are always either 1 or 2.  no a_i = b_ijjj
	// collect the offsets of the Index's that are duplicated (summed) into one sequence ...
	using SumIndexSeq = typename GetSingleVsDouble::has; 
	// ... and the single Index's (used for assignment) into another sequence.
	// this is what we need to match up when performing operations like = + etc
	using AssignIndexSeq = typename GetSingleVsDouble::hasnot;
	using AssignIndexTuple = Common::SeqToTupleMap<AssignIndexSeq, GetTupleIthElem<IndexTuple>::template Get>;
	//"dimseq" is the dimensions associated with the assignment-indexes 
	using dimseq = Common::SeqToSeqMap<AssignIndexSeq, GetTensorIthDim<InputTensorType>::template Get>;
	
	using Scalar = typename InputTensorType::Scalar;
	// TODO if instead of picking out dims then rebuilding _tensor with them,
	//  if instead you remove dims one-by-one from the source type (and add a transpose-type template for preserving symmetry and antisymmetric)
	//  then maybe this could be optimized
	using OutputTensorType = tensorScalarSeq<Scalar, dimseq>;
	
	//"rank" of this expression is the # of assignment-indexes ... excluding the sum-indexes
	static constexpr auto rank = AssignIndexSeq::size();
	// TODO "intOutputN" vs "intInputN = InputTensorType::intN"
	using intN = _vec<int, rank>;

	// if it's + - etc then lazy-eval
	// if we're using lazy-eval then just store a reference
	// if we're not then store a tensor ... after traces have been computed
	struct StorageLazy {
		// If storageLazy is used then assert ...
		static_assert(std::is_same_v<typename InputTensorType::template ExpandAllIndexes<>, OutputTensorType>);
		using type = InputTensorType &;
		static type process(InputTensorType & x) { return x; }
	};
	// if it's trace or * then evaluate up front
	struct StorageEval {
		using type = OutputTensorType;
		static type process(InputTensorType & x) {
			auto trs = applyTraces<InputTensorType, SumIndexSeq>(x);
			// NOTICE here I'm making an assertion that AssignIndexSeq is matching the indexes when the sum-indexes are all removed.
			// if it's wrong then ranks (and dims) won't match
			STATIC_ASSERT_EQ(trs.rank, type::rank);
			return (type)trs;
		}
	};

	// to cache the tensor in the expression-tree, or to lazy-eval?
	//  for traces, cache. (also in TensorMulExpr, cache)
	//  for others, lazy.
	static constexpr bool useLazyEval = SumIndexSeq::size() > 0;
	using StorageDetails = 
		//std::conditional_t<
		//	useLazyEval,
			StorageLazy
		//	StorageEval
		//>
	;
	using StorageType = typename StorageDetails::type;

	
	TENSOR_EXPR_ADD_ASSIGNR()
	TENSOR_EXPR_ADD_ASSIGN()

	StorageType t;

	IndexAccess(InputTensorType & t_) : t(StorageDetails::process(t_)) {}

	template<typename B>
	requires is_IndexExpr_v<B>
	IndexAccess(B const & read) {
		doAssign<B>(read);
	}
	template<typename B>
	requires is_IndexExpr_v<B>
	IndexAccess(B && read) {
		doAssign<B>(read);
	}

	template<typename B>
	requires is_IndexExpr_v<B>
	IndexAccess & operator=(B const & read) {
		doAssign<B>(read);
		return *this;
	}
	template<typename B>
	requires is_IndexExpr_v<B>
	IndexAccess & operator=(B && read) {
		doAssign<B>(std::forward<B>(read));
		return *this;
	}

	/*
	uses operations of Tensor template parameter:
	Tensor:
		t->write().begin(), .end()	<- write iteration
		t->operator()(intN)	<- write operator
	
	Tensor2Type:
		readSource.operator=	<- copy operation in case source and dest are the same (how else to circumvent this?)
		readSource.operator()(intN)

	If read and write are the same then we want 'read' to be copied before swizzling, to prevent overwrites of transpose operations.
	If read has higher rank then a contration is required before the copy.
	
	- compare write indexes to read indexes.
		Find all read indexes that are not in write indexes.
		Make sure they are in pairs -- complain otherwise.
	*/
	template<typename B>//typename Tensor2Type, typename IndexTuple2>
	requires IsMatchingRankExpr<This, B>
	void doAssign(B const & src) {
		// B is the source tensor type
		// This is the destination tensor type
		// so This's indexes should have no sums in them
		static_assert(SumIndexSeq::size() == 0);
		// and its assign index seq should be 0...rank-1
		static_assert(std::is_same_v<AssignIndexSeq, std::make_integer_sequence<int, std::tuple_size_v<IndexTuple>>>);
		static_assert(rank == std::tuple_size_v<IndexTuple>);
		// and its assign indexes should equal its total indexes
		static_assert(std::is_same_v<AssignIndexTuple, IndexTuple>);

		//assign using write iterator so the result will be pushed on stack before overwriting the write tensor
		// this way we get a copy to buffer changes between read and write, in case the same tensor is used for both
#if 0
		// same as TENSOR_ADD_CTOR_FOR_GENERIC_TENSORS
		// downside is it can invalidate itself if you're reading and writing to the same tensor
		auto w = t.write();
		for (auto i = w.begin(); i != w.end(); ++i) {
			*i = src.template read<AssignIndexTuple>(i.readIndex);
		};
#else	// requires an extra object on the stack but can read and write to the same tensor
		// OutputTensorType is expanded, for the sake of caching mid-expression 
		// so just use InputTensorType, that's what wraps the lhs tensor: a(i,j) = b(j,i) , InputTensorType is decltype(a)
		t = InputTensorType([&](intN i) -> Scalar {
			return src.template read<AssignIndexTuple>(i);
		});
#endif
	}

	// a(j,i) = b(i,j)
	// 'this' is b
	// 'i' is the index in a's type's ctor, so its the dest-i 
	// DstAssignIndexTuple is a's (j,i) tuple
	// if we're just wrapping a tensor ... the read operation is just a tensor access
	// TODO decltype(auto) 
	//  to do that, constexpr the validIndex
	//  to do that, i should be constexpr
	template<typename DstAssignIndexTuple>
	constexpr Scalar read(intN const & i) const {
		auto dstI = getIndex<DstAssignIndexTuple>(i);
		if (OutputTensorType::validIndex(dstI)) {
			return t(dstI);
		} else {
			return Scalar();
		}
	}

	//template<AssignIndexTuple> but inner class provides
	template<typename Src>
	struct FindInAssignIndexTuple {
		static constexpr int value = Common::tuple_find_v<Src, AssignIndexTuple>;
	};

	template<typename DstAssignIndexTuple>
	static constexpr intN getIndex(intN const & i) {

		// i'th destseq = for the i'th element in DstAssignIndexTuple,
		//  find its position in AssignIndexTuple
		using destseq = Common::TupleToSeqMap<int, DstAssignIndexTuple, FindInAssignIndexTuple>;
		auto dstForSrcIndex = intN(destseq());
	
		//for the j'th index of i ...
		// ... find indexes(j) coinciding with read.indexes(k)
		// ... and put that there
		intN destI;
		for (int j = 0; j < rank; ++j) {
			destI(j) = i(dstForSrcIndex(j));
		}
		return destI;
	}


};

template<typename T, typename Is>
std::ostream & operator<<(std::ostream & o, IndexAccess<T,Is> const & ti) {
	o << "[" << ti.t << "_";
	Common::TupleForEach(Is(), [&o](auto x, size_t i) constexpr -> bool {
		o << x;
		return false;
	});
	o << "]";
	return o;
}


/*
C++ reference Transparent operator wrappers
DONE plus
DONE minus
DONE negate
DONE multiplies (except tensor/tensor)
DONE divides
TODO (should the rest be integral types?)
modulus
bit_and
bit_or
bit_not
bit_xor
equal_to
not_equal_to
greater
less
greater_equal
less_equal
logical_and
logical_or
logical_not
*/

// tensor + tensor

template<typename A, typename B, template<typename> typename op>
requires IsMatchingRankExpr<A, B>
struct TensorTensorExpr {
	static constexpr bool isIndexExprFlag = {};
	using This = TensorTensorExpr;
	static constexpr auto rank = A::rank;
	using intN = _vec<int,rank>;
	using Scalar = decltype(op()(typename A::Scalar(), typename B::Scalar()));
	TENSOR_EXPR_ADD_ASSIGNR()
	
	A const & a;
	B const & b;
	
	TensorTensorExpr(A const & a_, B const & b_) : a(a_), b(b_) {}

	template<typename DstIndexTuple>
	constexpr Scalar read(intN const & i) const {
		return op<Scalar>()(
			a.template read<DstIndexTuple>(i),
			b.template read<DstIndexTuple>(i));
	}
};

#define TENSOR_TENSOR_EXPR_OP(op, optype)\
template<typename A, typename B>\
requires IsMatchingRankExpr<A, B>\
decltype(auto) operator op(A const & a, B const & b) {\
	return TensorTensorExpr<A, B, optype>(a,b);\
}

// TODO ensure A's dims == B's dims
// but that means recalculating dims and rank for IndexAccess and its expression-trees based on its 
// and direct assignment doesn't assert this -- instead it bounds-checks ... soo ...
//static_assert(A::TensorType::dims() == B::TensorType::dims());
TENSOR_TENSOR_EXPR_OP(+, std::plus)
TENSOR_TENSOR_EXPR_OP(-, std::minus)
TENSOR_TENSOR_EXPR_OP(/, std::divides)


// tensor * tensor

// TODO 
#if 0
template<typename A, typename B, template<typename> typename op>
requires IsBinaryTensorOp<A, B>
struct TensorMulExpr {
	static constexpr bool isIndexExprFlag = {};
	using This = TensorMulExpr;
	
	TENSOR_EXPR_ADD_ASSIGNR()
	
	// result-rank = A rank + B rank - duplicate indexes
	struct RankImpl {
		static constexpr int value() {
			constexpr typename A::intN indexAinB(-1);
			Common::ForLoop<
				0,
				rank,
				FindDstForSrcOuter<
					typename B::IndexTuple,
					typename A::IndexTuple
				>::template Find
			>::exec(indexAinB);
			constexpr int rank = A::rank - B::rank;
			for (int i = 0; i < A::rank; ++i) {
				if (indexAinB(i) != -1) {
					rank -= 2;
				}
			}
			return rank;
		}
	};
	static constexpr auto rank = RankImpl::value();
	
	using intN = _vec<int,rank>;
	using Scalar = decltype(op()(typename A::Scalar(), typename B::Scalar()));
	A const & a;
	B const & b;
	
	TensorMulExpr(A const & a_, B const & b_) : a(a_), b(b_) {}

	// TODO not all indexes here are provided
	//  so for the duplicates, sum over all possible values
	template<typename DstIndexTuple>
	constexpr Scalar read(intN const & i) const {
		
		typename A::intN indexAinDst(-1);
		Common::ForLoop<
			0,
			rank,
			FindDstForSrcOuter<
				DstIndexTuple,
				typename A::IndexTuple
			>::template Find
		>::exec(indexAinDst);

		typename B::intN indexBinDst(-1);
		Common::ForLoop<
			0,
			rank,
			FindDstForSrcOuter<
				DstIndexTuple,
				typename B::IndexTuple
			>::template Find
		>::exec(indexBinDst);

		// so all A's indexes in B's indexes, create a new tmp dim & assert dim match
		//  then iterate over all these tmp dims in a range iterator
		//  and then sum

		return op<Scalar>()(
			a.template read<DstIndexTuple>(i),
			b.template read<DstIndexTuple>(i));
	}
};

template<typename A, typename B>
requires IsBinaryTensorOp<A, B>
decltype(auto) operator*(A const & a, B const & b) {
	return TensorMulExpr<A, B>(a,b);
}
#endif

// tensor + scalar

template<typename T, template<typename> typename op>
requires is_IndexExpr_v<T>
struct TensorScalarExpr {
	static constexpr bool isIndexExprFlag = {};
	using This = TensorScalarExpr;
	static constexpr auto rank = T::rank;
	using intN = _vec<int,rank>;
	using Scalar = typename T::Scalar; // TODO which Scalar to use?
	TENSOR_EXPR_ADD_ASSIGNR()
	
	T const & a;
	Scalar const & b;
	
	TensorScalarExpr(T const & a_, Scalar const & b_) : a(a_), b(b_) {}

	template<typename DstIndexTuple>
	constexpr Scalar read(intN const & i) const {
		return op<Scalar>()(a.template read<DstIndexTuple>(i), b);
	}
};

// scalar + tensor

template<typename T, template<typename> typename op>
requires is_IndexExpr_v<T>
struct ScalarTensorExpr {
	static constexpr bool isIndexExprFlag = {};
	using This = ScalarTensorExpr;
	static constexpr auto rank = T::rank;
	using intN = _vec<int, rank>;
	using Scalar = typename T::Scalar; // TODO which Scalar to use?
	TENSOR_EXPR_ADD_ASSIGNR()
	
	Scalar const & a;
	T const & b;
	
	ScalarTensorExpr(Scalar const & a_, T const & b_) : a(a_), b(b_) {}

	template<typename DstIndexTuple>
	constexpr Scalar read(intN const & i) const {
		return op<Scalar>()(a, b.template read<DstIndexTuple>(i));
	}
};

#define TENSOR_SCALAR_EXPR_OP(op, optype)\
template<typename T>\
requires is_IndexExpr_v<T>\
decltype(auto) operator op(T const & a, typename T::Scalar const & b) {\
	return TensorScalarExpr<T, optype>(a,b);\
}\
template<typename T>\
requires is_IndexExpr_v<T>\
decltype(auto) operator op(typename T::Scalar const & a, T const & b) {\
	return ScalarTensorExpr<T, optype>(a,b);\
}

TENSOR_SCALAR_EXPR_OP(+, std::plus)
TENSOR_SCALAR_EXPR_OP(-, std::minus)
TENSOR_SCALAR_EXPR_OP(*, std::multiplies)
TENSOR_SCALAR_EXPR_OP(/, std::divides)

// unary

template<typename T, template<typename> typename op>
requires is_IndexExpr_v<T>
struct UnaryTensorExpr {
	static constexpr bool isIndexExprFlag = {};
	using This = UnaryTensorExpr;
	static constexpr auto rank = T::rank;
	using intN = _vec<int, rank>;
	using Scalar = typename T::Scalar;
	TENSOR_EXPR_ADD_ASSIGNR()
	
	T const & t;

	UnaryTensorExpr(T const & t_) : t(t_) {}

	template<typename DstIndexTuple>
	constexpr Scalar read(intN const & i) const {
		return op<Scalar>()(t.template read<DstIndexTuple>(i));
	}
};
template<typename T>
requires is_IndexExpr_v<T>
decltype(auto) operator-(T const & t) {
	return UnaryTensorExpr<T, std::negate>(t);
}

}
