#pragma once

/*
NEW VERSION
- no more template metaprograms, instead constexprs
- no more helper structs, instead requires
- no more lower and upper
- modeled around glsl
- maybe extensions into matlab syntax (nested tensor implicit * contraction mul)

TODO:
	symmetric and antisymmetric matrices
	operator() reference drilling
	index notation still
*/

#include "Common/String.h"
#include <tuple>
#include <functional>	//for Partial

namespace Tensor {
namespace v2 {

//for giving operators to the Cons and Prim vector classes
//how can you add correctly-typed ops via crtp to a union?
//unions can't inherit.
//until then...

#define TENSOR2_ADD_VECTOR_OP_EQ(classname, op)\
	classname& operator op(classname const & b) {\
		for (int i = 0; i < count; ++i) {\
			s[i] op b.s[i];\
		}\
		return *this;\
	}

#define TENSOR2_ADD_SCALAR_OP_EQ(classname, op)\
	classname& operator op(ScalarType const & b) {\
		for (int i = 0; i < count; ++i) {\
			s[i] op b;\
		}\
		return *this;\
	}

#define TENSOR2_ADD_CMP_OP(classname)\
	bool operator==(classname const & b) const {\
		for (int i = 0; i < count; ++i) {\
			if (s[i] != b.s[i]) return false;\
		}\
		return true;\
	}\
	bool operator!=(classname const & b) const {\
		return !operator==(b);\
	}

// danger ... danger ...
#define TENSOR2_ADD_CAST_BOOL_OP()\
	operator bool() const {\
		for (int i = 0; i < count; ++i) {\
			if (s[i] != T()) return true;\
		}\
		return false;\
	}

// danger ... danger ...
#define TENSOR2_ADD_CAST_OP(classname)\
	template<int dim2, typename U>\
	operator classname<U, dim2>() const {\
		classname<U, dim2> res;\
		for (int i = 0; i < count && i < dim2; ++i) {\
			res.s[i] = (U)s[i];\
		}\
		return res;\
	}
	
#define TENSOR2_ADD_UNM(classname)\
	classname operator-() const {\
		classname result;\
		for (int i = 0; i < count; ++i) {\
			result.s[i] = -s[i];\
		}\
		return result;\
	}

#define TENSOR2_ADD_DOT(classname)\
	T dot(classname const & b) const {\
		T result = {};\
		for (int i = 0; i < count; ++i) {\
			result += s[i] * b.s[i];\
		}\
		return result;\
	}\
	T lenSq() const { return dot(*this); }\
	T length() const { return (T)sqrt(lenSq()); }

// danger ... danger ...
#define TENSOR2_ADD_CTOR_FROM_VEC(classname, othername)\
	template<typename U, int dim2>\
	classname(othername<U, dim2> const & v) {\
		int i = 0;\
		for (; i < count && i < dim2; ++i) {\
			s[i] = (T)v[i];\
		}\
		for (; i < count; ++i) {\
			s[i] = {};\
		}\
	}

// works for nesting _vec's, not for _sym's
// I am using operator[] as the de-facto correct reference
#define TENSOR2_ADD_VECTOR_BRACKET_INDEX()\
	T & operator[](int i) { return s[i]; }\
	T const & operator[](int i) const { return s[i]; }

// operator() should default through operator[]
#define TENSOR2_ADD_RECURSIVE_CALL_INDEX()\
\
	/* a(i1,i2,...) := a_i1_i2_... */\
	template<typename... Rest>\
	auto & operator()(int i, Rest... rest) {\
		return (*this)[i](rest...);\
	}\
\
	template<typename... Rest>\
	auto const & operator()(int i, Rest... rest) const {\
		return (*this)[i](rest...);\
	}

#define TENSOR2_ADD_CALL_INDEX()\
\
	/* a(i) := a_i */\
	auto & operator()(int i) { return (*this)[i]; }\
	auto const & operator()(int i) const { return (*this)[i]; }\
\
	TENSOR2_ADD_RECURSIVE_CALL_INDEX()

// danger ... danger ...
#define TENSOR2_ADD_ASSIGN_OP(classname)\
	classname & operator=(classname const & o) {\
		for (int i = 0; i < count; ++i) {\
			s[i] = o.s[i];\
		}\
		return *this;\
	}

//these are all per-element assignment operators, so they should work fine for vector- and for symmetric-
#define TENSOR2_ADD_OPS(classname)\
	TENSOR2_ADD_VECTOR_OP_EQ(classname, +=)\
	TENSOR2_ADD_VECTOR_OP_EQ(classname, -=)\
	TENSOR2_ADD_VECTOR_OP_EQ(classname, *=)\
	TENSOR2_ADD_VECTOR_OP_EQ(classname, /=)\
	TENSOR2_ADD_SCALAR_OP_EQ(classname, +=)\
	TENSOR2_ADD_SCALAR_OP_EQ(classname, -=)\
	TENSOR2_ADD_SCALAR_OP_EQ(classname, *=)\
	TENSOR2_ADD_SCALAR_OP_EQ(classname, /=)\
	TENSOR2_ADD_UNM(classname)\
	TENSOR2_ADD_DOT(classname)\
	TENSOR2_ADD_CMP_OP(classname)

#define TENSOR2_VECTOR_CLASS_OPS(classname)\
	TENSOR2_ADD_VECTOR_BRACKET_INDEX()\
	TENSOR2_ADD_CALL_INDEX()\
	TENSOR2_ADD_OPS(classname)

#if 0	//hmm, this isn't working when it is run
	//TENSOR2_ADD_ASSIGN_OP(classname)
#endif


/*
use the 'rank' field to check and see if we're in a _vec (or a _sym) or not
 TODO use something more specific to this file in case other classes elsewhere use 'rank'
*/
template<typename T>
constexpr bool is_tensor_v = requires(T const & t) { T::rank; };

// base/scalar case
template<typename T>
struct VectorTraits {
	static constexpr int rank = 0;
	using ScalarType = T;
};

// recursive/vec/matrix/tensor case
template<typename T> requires is_tensor_v<T> 
struct VectorTraits<T> {
	static constexpr int rank = T::rank;
	using ScalarType = typename T::ScalarType;
};


//default
template<typename T, int dim_>
struct _vec {
	// this is this class.  useful for templates.  you'd be surprised.
	using This = _vec;
	
	// this is the next most nested class, so vector-of-vector is a matrix.
	using InnerType = T;
	
	// this is the child-most nested class that isn't in our math library.
	using ScalarType = typename VectorTraits<T>::ScalarType;
	
	// this is this particular dimension of our vector
	// M = matrix<T,i,j> == vec<vec<T,j>,i> so M::dim == i and M::InnerType::dim == j 
	// I'll make a tuple getter for all dimensions eventually, its size will be 'rank'
	static constexpr int dim = dim_;

	// this is the rank/degree/index/number of letter-indexes of your tensor.
	// for vectors-of-vectors this is the nesting.
	// if you use any (anti?)symmetric then those take up 2 ranks / 2 indexes each instead of 1-rank each.
	static constexpr int rank = 1 + VectorTraits<T>::rank;
	
	//this is the storage size, used for iterting across 's'
	// for vectors etc it is 's'
	// for (anti?)symmetric it is N*(N+1)/2
	static constexpr int count = dim;

	T s[dim] = {};
	_vec() {}
	
	TENSOR2_VECTOR_CLASS_OPS(_vec)
	//TENSOR2_ADD_CAST_OP(_vec)
};


template<typename T>
struct _vec<T,2> {
	using This = _vec;
	using InnerType = T;
	using ScalarType = typename VectorTraits<T>::ScalarType;
	static constexpr int dim = 2;
	static constexpr int rank = 1 + VectorTraits<T>::rank;
	static constexpr int count = dim;

	union {
		struct {
			T x = {};
			T y = {};
		};
		struct {
			T s0;
			T s1;
		};
		T s[dim];
	};
	_vec() {}
	_vec(T x_, T y_) : x(x_), y(y_) {}

	static constexpr auto fields = std::make_tuple(
		std::make_pair("x", &This::x),
		std::make_pair("y", &This::y)
	);

	TENSOR2_VECTOR_CLASS_OPS(_vec)
	//TENSOR2_ADD_CAST_OP(_vec)

	//alright i can do swizzles, but not swizzle-assigns 
	// (or maybe I can, but no promises)
};

template<typename T>
using _vec2 = _vec<T,2>;

using bool2 = _vec2<bool>;
using uchar2 = _vec2<unsigned char>;
using int2 = _vec2<int>;
using uint2 = _vec2<unsigned int>;
using float2 = _vec2<float>;
using double2 = _vec2<double>;

static_assert(std::is_same_v<float2::ScalarType, float>);
static_assert(std::is_same_v<float2::InnerType, float>);
static_assert(float2::rank == 1);
static_assert(float2::dim == 2);


template<typename T>
struct _vec<T,3> {
	using This = _vec;
	using InnerType = T;
	using ScalarType = typename VectorTraits<T>::ScalarType;
	static constexpr int dim = 3;
	static constexpr int rank = 1 + VectorTraits<T>::rank;
	static constexpr int count = dim;

	union {
		struct {
			T x = {};
			T y = {};
			T z = {};
		};
		struct {
			T s0;
			T s1;
			T s2;
		};
		T s[dim];
	};
	_vec() {}
	_vec(T x_, T y_, T z_) : x(x_), y(y_), z(z_) {}

	static constexpr auto fields = std::make_tuple(
		std::make_pair("x", &This::x),
		std::make_pair("y", &This::y),
		std::make_pair("z", &This::z)
	);

	TENSOR2_VECTOR_CLASS_OPS(_vec)
	//TENSOR2_ADD_CAST_OP(_vec)
};

template<typename T>
using _vec3 = _vec<T,3>;

using bool3 = _vec3<bool>;
using uchar3 = _vec3<unsigned char>;
using int3 = _vec3<int>;
using float3 = _vec3<float>;
using double3 = _vec3<double>;

static_assert(std::is_same_v<float3::ScalarType, float>);
static_assert(std::is_same_v<float3::InnerType, float>);
static_assert(float3::rank == 1);
static_assert(float3::dim == 3);


template<typename T>
struct _vec<T,4> {
	using This = _vec;
	using InnerType = T;
	using ScalarType = typename VectorTraits<T>::ScalarType;
	static constexpr int dim = 4;
	static constexpr int rank = 1 + VectorTraits<T>::rank;
	static constexpr int count = dim;

	union {
		struct {
			T x = {};
			T y = {};
			T z = {};
			T w = {};
		};
		struct {
			T s0;
			T s1;
			T s2;
			T s3;
		};
		T s[dim];
	};
	_vec() {}
	_vec(T x_, T y_, T z_, T w_) : x(x_), y(y_), z(z_), w(w_) {}

	static constexpr auto fields = std::make_tuple(
		std::make_pair("x", &This::x),
		std::make_pair("y", &This::y),
		std::make_pair("z", &This::z),
		std::make_pair("w", &This::w)
	);

	TENSOR2_VECTOR_CLASS_OPS(_vec)
	//TENSOR2_ADD_CAST_OP(_vec)
};

template<typename T>
using _vec4 = _vec<T,4>;

using bool4 = _vec4<bool>;
using uchar4 = _vec4<unsigned char>;
using int4 = _vec4<int>;
using uint4 = _vec4<unsigned int>;
using float4 = _vec4<float>;
using double4 = _vec4<double>;

static_assert(std::is_same_v<float4::ScalarType, float>);
static_assert(std::is_same_v<float4::InnerType, float>);
static_assert(float4::rank == 1);
static_assert(float4::dim == 4);


template<int dim> using boolN = _vec<bool, dim>;
template<int dim> using ucharN = _vec<unsigned char, dim>;
template<int dim> using intN = _vec<int, dim>;
template<int dim> using uintN = _vec<unsigned int, dim>;
template<int dim> using floatN = _vec<float, dim>;
template<int dim> using doubleN = _vec<double, dim>;


//convention?  row-major to match math indexing, easy C inline ctor,  so A_ij = A[i][j]
// ... but OpenGL getFloatv(GL_...MATRIX) uses column-major so uploads have to be transposed
// ... also GLSL is column-major so between this and GLSL the indexes have to be transposed.
template<typename T, int dim1, int dim2> using _mat = _vec<_vec<T, dim2>, dim1>;

template<typename T> using _mat2x2 = _vec2<_vec2<T>>;
using bool2x2 = _mat2x2<bool>;
using uchar2x2 = _mat2x2<unsigned char>;
using int2x2 = _mat2x2<int>;
using uint2x2 = _mat2x2<uint>;
using float2x2 = _mat2x2<float>;
using double2x2 = _mat2x2<double>;

template<typename T> using _mat2x3 = _vec2<_vec3<T>>;
using bool2x3 = _mat2x3<bool>;
using uchar2x3 = _mat2x3<unsigned char>;
using int2x3 = _mat2x3<int>;
using uint2x3 = _mat2x3<uint>;
using float2x3 = _mat2x3<float>;
using double2x3 = _mat2x3<double>;

template<typename T> using _mat2x4 = _vec2<_vec4<T>>;
using bool2x4 = _mat2x4<bool>;
using uchar2x4 = _mat2x4<unsigned char>;
using int2x4 = _mat2x4<int>;
using uint2x4 = _mat2x4<uint>;
using float2x4 = _mat2x4<float>;
using double2x4 = _mat2x4<double>;

template<typename T> using _mat3x2 = _vec3<_vec2<T>>;
using bool3x2 = _mat3x2<bool>;
using uchar3x2 = _mat3x2<unsigned char>;
using int3x2 = _mat3x2<int>;
using uint3x2 = _mat3x2<uint>;
using float3x2 = _mat3x2<float>;
using double3x2 = _mat3x2<double>;

template<typename T> using _mat3x3 = _vec3<_vec3<T>>;
using bool3x3 = _mat3x3<bool>;
using uchar3x3 = _mat3x3<unsigned char>;
using int3x3 = _mat3x3<int>;
using uint3x3 = _mat3x3<uint>;
using float3x3 = _mat3x3<float>;
using double3x3 = _mat3x3<double>;

template<typename T> using _mat3x4 = _vec3<_vec4<T>>;
using bool3x4 = _mat3x4<bool>;
using uchar3x4 = _mat3x4<unsigned char>;
using int3x4 = _mat3x4<int>;
using uint3x4 = _mat3x4<uint>;
using float3x4 = _mat3x4<float>;
using double3x4 = _mat3x4<double>;

template<typename T> using _mat4x2 = _vec4<_vec2<T>>;
using bool4x2 = _mat4x2<bool>;
using uchar4x2 = _mat4x2<unsigned char>;
using int4x2 = _mat4x2<int>;
using uint4x2 = _mat4x2<uint>;
using float4x2 = _mat4x2<float>;
using double4x2 = _mat4x2<double>;

template<typename T> using _mat4x3 = _vec4<_vec3<T>>;
using bool4x3 = _mat4x3<bool>;
using uchar4x3 = _mat4x3<unsigned char>;
using int4x3 = _mat4x3<int>;
using uint4x3 = _mat4x3<uint>;
using float4x3 = _mat4x3<float>;
using double4x3 = _mat4x3<double>;

template<typename T> using _mat4x4 = _vec4<_vec4<T>>;
using bool4x4 = _mat4x4<bool>;
using uchar4x4 = _mat4x4<unsigned char>;
using int4x4 = _mat4x4<int>;
using uint4x4 = _mat4x4<uint>;
using float4x4 = _mat4x4<float>;
using double4x4 = _mat4x4<double>;

static_assert(std::is_same_v<float4x4::ScalarType, float>);
static_assert(std::is_same_v<float4x4::InnerType, float4>);
static_assert(float4x4::rank == 2);
static_assert(float4x4::dim == 4);
static_assert(float4x4::InnerType::dim == 4);

// vector op vector, matrix op matrix, and tensor op tensor per-component operators

#define TENSOR2_ADD_VECTOR_VECTOR_OP(op)\
template<typename T, int dim>\
_vec<T,dim> operator op(_vec<T,dim> const & a, _vec<T,dim> const & b) {\
	_vec<T,dim> c;\
	for (int i = 0; i < dim; ++i) {\
		c[i] = a[i] op b[i];\
	}\
	return c;\
}

TENSOR2_ADD_VECTOR_VECTOR_OP(+)
TENSOR2_ADD_VECTOR_VECTOR_OP(-)
TENSOR2_ADD_VECTOR_VECTOR_OP(/)

// vector * vector
//TENSOR2_ADD_VECTOR_VECTOR_OP(*) will cause ambiguous evaluation of matrix/matrix mul
// so it has to be constrained to only T == _vec<T,dim>:ScalarType
// c_i := a_i * b_i
template<typename T, int dim>
requires std::is_same_v<typename _vec<T,dim>::ScalarType, T>
_vec<T,dim> operator*(_vec<T, dim> const & a, _vec<T,dim> const & b) {
	_vec<T,dim> c;
	for (int i = 0; i < dim; ++i) {
		c[i] = a[i] * b[i];
	}
	return c;
}


// vector op scalar, scalar op vector, matrix op scalar, scalar op matrix, tensor op scalar, scalar op tensor operations
// need to require that T == _vec<T,dim>::ScalarType otherwise this will spill into vector/matrix operations
// c_i := a_i * b
// c_i := a * b_i

#define TENSOR2_ADD_VECTOR_SCALAR_OP(op)\
template<typename T, int dim>\
requires std::is_same_v<typename _vec<T,dim>::ScalarType, T>\
_vec<T,dim> operator op(_vec<T,dim> const & a, T const & b) {\
	_vec<T,dim> c;\
	for (int i = 0; i < dim; ++i) {\
		c[i] = a[i] op b;\
	}\
	return c;\
}\
template<typename T, int dim>\
requires std::is_same_v<typename _vec<T,dim>::ScalarType, T>\
_vec<T,dim> operator op(T const & a, _vec<T,dim> const & b) {\
	_vec<T,dim> c;\
	for (int i = 0; i < dim; ++i) {\
		c[i] = a op b[i];\
	}\
	return c;\
}

TENSOR2_ADD_VECTOR_SCALAR_OP(+)
TENSOR2_ADD_VECTOR_SCALAR_OP(-)
TENSOR2_ADD_VECTOR_SCALAR_OP(*)
TENSOR2_ADD_VECTOR_SCALAR_OP(/)


// matrix * matrix operators
// c_ik := a_ij * b_jk
template<typename T, int dim1, int dim2, int dim3>
requires std::is_same_v<typename _mat<T,dim1,dim2>::ScalarType, T>
_mat<T,dim1,dim3> operator*(_mat<T,dim1,dim2> const & a, _mat<T,dim2,dim3> const & b) {
	_mat<T,dim1,dim3> c;
	for (int i = 0; i < dim1; ++i) {
		for (int j = 0; j < dim3; ++j) {
			T sum = {};
			for (int k = 0; k < dim2; ++k) {
				sum += a[i][k] * b[k][j];
			}
			c[i][j] = sum;
		}
	}
	return c;
}

// (row-)vector * matrix operator
// c_j := a_i * b_ij

template<typename T, int dim1, int dim2>
requires std::is_same_v<typename _mat<T,dim1,dim2>::ScalarType, T>
_vec<T,dim2> operator*(_vec<T,dim1> const & a, _mat<T,dim1,dim2> const & b) {
	_vec<T,dim2> c;
	for (int j = 0; j < dim2; ++j) {
		T sum = {};
		for (int i = 0; i < dim1; ++i) {
			sum += a[i] * b[i][j];
		}
		c[j] = sum;
	}
	return c;
}

// matrix * (column-)vector operator
// c_i := a_ij * b_j

template<typename T, int dim1, int dim2>
requires std::is_same_v<typename _mat<T,dim1,dim2>::ScalarType, T>
_vec<T,dim1> operator*(_mat<T,dim1,dim2> const & a, _vec<T,dim2> const & b) {
	_vec<T,dim1> c;
	for (int i = 0; i < dim1; ++i) {
		T sum = {};
		for (int j = 0; j < dim2; ++j) {
			sum += a[i][j] * b[j];
		}
		c[i] = sum;
	}
	return c;
}


// TODO GENERALIZE TO TENSOR MULTIPLICATIONS
// c_i1...i{p}_j1_..._j{q} = Σ_k1...k{r} a_i1_..._i{p}_k1_...k{r} * b_k1_..._k{r}_j1_..._j{q}

//matrixCompMult = component-wise multiplication, GLSL name compat
// c_ij := a_ij * b_ij

template<typename T, int dim1, int dim2>
requires std::is_same_v<typename _mat<T,dim1,dim2>::ScalarType, T>
_mat<T,dim1,dim2> matrixCompMult(_mat<T,dim1,dim2> const & a, _mat<T,dim1,dim2> const & b) {
	_mat<T,dim1,dim2> c;
	for (int i = 0; i < dim1; ++i) {
		for (int j = 0; j < dim2; ++j) {
			c[i][j] = a[i][j] * b[i][j];
		}
	}
	return c;
}

// vector functions

// c := a_i * b_i
// TODO generalize
template<typename T, int dim>
requires std::is_same_v<typename _vec<T,dim>::ScalarType, T>
T dot(_vec<T,dim> const & a, _vec<T,dim> const & b) {
	T sum = {};
	for (int i = 0; i < dim; ++i) {
		sum += a[i] * b[i];
	}
	return sum;
}

template<typename T, int dim>
T lenSq(_vec<T,dim> const & v) {
	return dot(v,v);
}

template<typename T, int dim>
T length(_vec<T,dim> const & v) {
	return sqrt(lenSq(v));
}

template<typename T, int dim>
T distance(_vec<T,dim> const & a, _vec<T,dim> const & b) {
	return length(b - a);
}

template<typename T, int dim>
T normalize(_vec<T,dim> const & v) {
	return v / length(v);
}

// c_i := ε_ijk * b_j * c_k
template<typename T>
requires std::is_same_v<typename _vec3<T>::ScalarType, T>
_vec3<T> cross(_vec3<T> const & a, _vec3<T> const & b) {
	return _vec3<T>(
		a[1] * b[2] - a[2] * b[1],
		a[2] * b[0] - a[0] * b[2],
		a[0] * b[1] - a[1] * b[0]);
}

// c_ij := a_i * b_j
// TODO generalize to tensors c_i1_..ip_j1_..jq = a_i1..ip * b_j1..jq
template<typename T, int dim1, int dim2>
requires std::is_same_v<typename _vec<T,dim1>::ScalarType, T>
_mat<T,dim1,dim2> outerProduct(_vec<T,dim1> const & a, _vec<T,dim2> const & b) {
	_mat<T,dim1,dim2> c;
	for (int i = 0; i < dim1; ++i) {
		for (int j = 0; j < dim2; ++j) {
			c[i][j] = a[i] * b[j];
		}
	}
	return c;
}

// matrix functions
// TODO generalize to any sort of tensor swizzle

template<typename T, int dim1, int dim2>
_mat<T,dim2,dim1> transpose(_mat<T,dim1,dim2> const & a) {
	_mat<T,dim2,dim1> at;
	for (int i = 0; i < dim2; ++i) {
		for (int j = 0; j < dim1; ++j) {
			at[i][j] = a[j][i];
		}
	}
	return at;
}

// determinant
// TODO generalize or at least expose Levi-Civita tensor as constexpr 
// for implementing cross in higher dimensions, determinant, and whatever else.

template<typename T, int dim>
T determinant(_mat<T,dim,dim> const & a);

template<typename T>
T determinant(_mat2x2<T> const & a) {
	return a.x.x * a.y.y - a.x.y * a.y.x;
}
template<typename T>
T determinant(_mat3x3<T> const & a) {
	return a.x.x * a.y.y * a.z.z
		+ a.y.x * a.z.y * a.x.z
		+ a.z.x * a.x.y * a.y.z
		- a.x.z * a.y.y * a.z.x
		- a.y.z * a.z.y * a.x.x
		- a.z.z * a.x.y * a.y.x;
}

template<typename T>
T determinant(_mat4x4<T> const & a) {
	//autogen'd with symmath
	T const tmp1 = a.s2.s2 * a.s3.s3;
	T const tmp2 = a.s2.s3 * a.s3.s2;
	T const tmp3 = a.s2.s1 * a.s3.s3;
	T const tmp4 = a.s2.s3 * a.s3.s1;
	T const tmp5 = a.s2.s1 * a.s3.s2;
	T const tmp6 = a.s2.s2 * a.s3.s1;
	T const tmp7 = a.s2.s0 * a.s3.s3;
	T const tmp8 = a.s2.s3 * a.s3.s0;
	T const tmp9 = a.s2.s0 * a.s3.s2;
	T const tmp10 = a.s2.s2 * a.s3.s0;
	T const tmp11 = a.s2.s0 * a.s3.s1;
	T const tmp12 = a.s2.s1 * a.s3.s0;
	return a.s0.s0 * a.s1.s1 * tmp1 
		- a.s0.s0 * a.s1.s1 * tmp2 
		- a.s0.s0 * a.s1.s2 * tmp3
		+ a.s0.s0 * a.s1.s2 * tmp4
		+ a.s0.s0 * a.s1.s3 * tmp5 
		- a.s0.s0 * a.s1.s3 * tmp6 
		- a.s0.s1 * a.s1.s0 * tmp1
		+ a.s0.s1 * a.s1.s0 * tmp2
		+ a.s0.s1 * a.s1.s2 * tmp7 
		- a.s0.s1 * a.s1.s2 * tmp8 
		- a.s0.s1 * a.s1.s3 * tmp9
		+ a.s0.s1 * a.s1.s3 * tmp10
		+ a.s0.s2 * a.s1.s0 * tmp3 
		- a.s0.s2 * a.s1.s0 * tmp4 
		- a.s0.s2 * a.s1.s1 * tmp7
		+ a.s0.s2 * a.s1.s1 * tmp8
		+ a.s0.s2 * a.s1.s3 * tmp11 
		- a.s0.s2 * a.s1.s3 * tmp12 
		- a.s0.s3 * a.s1.s0 * tmp5
		+ a.s0.s3 * a.s1.s0 * tmp6
		+ a.s0.s3 * a.s1.s1 * tmp9 
		- a.s0.s3 * a.s1.s1 * tmp10
		+ a.s0.s3 * a.s1.s2 * tmp12 
		- a.s0.s3 * a.s1.s2 * tmp11;
}

// inverse

template<typename T, int dim>
_mat<T,dim,dim> inverse(_mat<T,dim,dim> const & a, T const det);

template<typename T>
_mat3x3<T> inverse(_mat3x3<T> const & a, T const det) {
	T const invdet = T(1) / det;
	return {
		{
			invdet * (-a.s1.s2 * a.s2.s1 + a.s1.s1 * a.s2.s2),
			invdet * (a.s0.s2 * a.s2.s1 + -a.s0.s1 * a.s2.s2),
			invdet * (-a.s0.s2 * a.s1.s1 + a.s0.s1 * a.s1.s2)
		}, {
			invdet * (a.s1.s2 * a.s2.s0 + -a.s1.s0 * a.s2.s2),
			invdet * (-a.s0.s2 * a.s2.s0 + a.s0.s0 * a.s2.s2),
			invdet * (a.s0.s2 * a.s1.s0 + -a.s0.s0 * a.s1.s2)
		}, {
			invdet * (-a.s1.s1 * a.s2.s0 + a.s1.s0 * a.s2.s1),
			invdet * (a.s0.s1 * a.s2.s0 + -a.s0.s0 * a.s2.s1),
			invdet * (-a.s0.s1 * a.s1.s0 + a.s0.s0 * a.s1.s1)
		}
	};
}

//from : https://stackoverflow.com/questions/1148309/inverting-a-4x4-matrix
template<typename T>
_mat4x4<T> inverse(_mat4x4<T> const & a, T const det) {
	T const a2323 = a.s2.s2 * a.s3.s3 - a.s2.s3 * a.s3.s2;
	T const a1323 = a.s2.s1 * a.s3.s3 - a.s2.s3 * a.s3.s1;
	T const a1223 = a.s2.s1 * a.s3.s2 - a.s2.s2 * a.s3.s1;
	T const a0323 = a.s2.s0 * a.s3.s3 - a.s2.s3 * a.s3.s0;
	T const a0223 = a.s2.s0 * a.s3.s2 - a.s2.s2 * a.s3.s0;
	T const a0123 = a.s2.s0 * a.s3.s1 - a.s2.s1 * a.s3.s0;
	T const a2313 = a.s1.s2 * a.s3.s3 - a.s1.s3 * a.s3.s2;
	T const a1313 = a.s1.s1 * a.s3.s3 - a.s1.s3 * a.s3.s1;
	T const a1213 = a.s1.s1 * a.s3.s2 - a.s1.s2 * a.s3.s1;
	T const a2312 = a.s1.s2 * a.s2.s3 - a.s1.s3 * a.s2.s2;
	T const a1312 = a.s1.s1 * a.s2.s3 - a.s1.s3 * a.s2.s1;
	T const a1212 = a.s1.s1 * a.s2.s2 - a.s1.s2 * a.s2.s1;
	T const a0313 = a.s1.s0 * a.s3.s3 - a.s1.s3 * a.s3.s0;
	T const a0213 = a.s1.s0 * a.s3.s2 - a.s1.s2 * a.s3.s0;
	T const a0312 = a.s1.s0 * a.s2.s3 - a.s1.s3 * a.s2.s0;
	T const a0212 = a.s1.s0 * a.s2.s2 - a.s1.s2 * a.s2.s0;
	T const a0113 = a.s1.s0 * a.s3.s1 - a.s1.s1 * a.s3.s0;
	T const a0112 = a.s1.s0 * a.s2.s1 - a.s1.s1 * a.s2.s0;
	T const invdet = T(1) / det;
	return { 
		{
		   invdet *  (a.s1.s1 * a2323 - a.s1.s2 * a1323 + a.s1.s3 * a1223),
		   invdet * -(a.s0.s1 * a2323 - a.s0.s2 * a1323 + a.s0.s3 * a1223),
		   invdet *  (a.s0.s1 * a2313 - a.s0.s2 * a1313 + a.s0.s3 * a1213),
		   invdet * -(a.s0.s1 * a2312 - a.s0.s2 * a1312 + a.s0.s3 * a1212),
		}, {
		   invdet * -(a.s1.s0 * a2323 - a.s1.s2 * a0323 + a.s1.s3 * a0223),
		   invdet *  (a.s0.s0 * a2323 - a.s0.s2 * a0323 + a.s0.s3 * a0223),
		   invdet * -(a.s0.s0 * a2313 - a.s0.s2 * a0313 + a.s0.s3 * a0213),
		   invdet *  (a.s0.s0 * a2312 - a.s0.s2 * a0312 + a.s0.s3 * a0212),
		}, {
		   invdet *  (a.s1.s0 * a1323 - a.s1.s1 * a0323 + a.s1.s3 * a0123),
		   invdet * -(a.s0.s0 * a1323 - a.s0.s1 * a0323 + a.s0.s3 * a0123),
		   invdet *  (a.s0.s0 * a1313 - a.s0.s1 * a0313 + a.s0.s3 * a0113),
		   invdet * -(a.s0.s0 * a1312 - a.s0.s1 * a0312 + a.s0.s3 * a0112),
		}, {
		   invdet * -(a.s1.s0 * a1223 - a.s1.s1 * a0223 + a.s1.s2 * a0123),
		   invdet *  (a.s0.s0 * a1223 - a.s0.s1 * a0223 + a.s0.s2 * a0123),
		   invdet * -(a.s0.s0 * a1213 - a.s0.s1 * a0213 + a.s0.s2 * a0113),
		   invdet *  (a.s0.s0 * a1212 - a.s0.s1 * a0212 + a.s0.s2 * a0112),
		}
	};
}

template<typename T, int dim>
_mat<T,dim,dim> inverse(_mat<T,dim,dim> const & a) {
	return inverse(a, determinant(a));
}

template<typename T, int dim>
_mat<T,dim,dim> diagonalMatrix(_vec<T,dim> const & v) {
	_mat<T,dim,dim> a;
	for (int i = 0; i < dim; ++i) {
		a[i][i] = v[i];
	}
	return a;
}

#if 1

template<typename T, int dim, int... dims>
struct NestedTensor;

template<typename T, int dim>
struct NestedTensor<T, dim> {
	using tensor = _vec<T,dim>;
};

template<typename T, int dim, int... dims>
struct NestedTensor {
	using tensor = _vec<typename NestedTensor<T, dims...>::tensor, dim>;
};

template<typename T, int dim, int... dims>
using _tensor = NestedTensor<T, dim, dims...>::tensor;

#else

template<typename T, int dim, int... dims>
using _tensor;

template<typename T, int dim>
using _tensor<T, dim> = _vec<T, dim>;

template<typename T, int dim, int... dims>
using _tensor = _vec<_tensor<T, dims...>, dim>;

#endif

// symmetric matrices

template<int dim>
int symIndex(int i, int j) {
	if (j > i) return symIndex<dim>(j,i);
	return ((i * (i + 1)) >> 1) + j;
}

//also symmetric has to define 1-arg operator()
// that means I can't use the default so i have to make a 2-arg recursive case
#define TENSOR2_SYMMETRIC_ADD_RECURSIVE_CALL_INDEX()\
\
	/* a(i1,i2,...) := a_i1_i2_... */\
	template<typename... Rest>\
	auto & operator()(int i, int j, Rest... rest) {\
		return (*this)(i,j)(rest...);\
	}\
\
	template<typename... Rest>\
	auto const & operator()(int i, int j, Rest... rest) const {\
		return (*this)(i,j)(rest...);\
	}


/*
for the call index operator
a 1-param is incomplete, so it should return an accessor (same as operator[])
but a 2-param is complete
and a more-than-2 will return call on the []
and therein risks applying a call to an accessor
so the accessors need nested call indexing too
*/
#define TENSOR2_SYMMETRIC_MATRIX_CLASS_OPS(classname)\
	TENSOR2_ADD_OPS(classname)\
\
	/* a(i,j) := a_ij = a_ji */\
	/* this is the direct acces */\
	InnerType & operator()(int i, int j) { return s[symIndex<dim>(i,j)]; }\
	InnerType const & operator()(int i, int j) const { return s[symIndex<dim>(i,j)]; }\
\
	struct Accessor {\
		classname & owner;\
		int i;\
		Accessor(classname & owner_, int i_) : owner(owner_), i(i_) {}\
		InnerType & operator[](int j) { return owner(i,j); }\
		TENSOR2_ADD_CALL_INDEX()\
	};\
	Accessor operator[](int i) { return Accessor(*this, i); }\
\
	struct ConstAccessor {\
		classname const & owner;\
		int i;\
		ConstAccessor(classname const & owner_, int i_) : owner(owner_), i(i_) {}\
		InnerType const & operator[](int j) { return owner(i,j); }\
		TENSOR2_ADD_CALL_INDEX()\
	};\
	ConstAccessor operator[](int i) const { return ConstAccessor(*this, i); }\
\
	/* a(i) := a_i */\
	/* this is incomplete so it returns the operator[] which returns the accessor */\
	Accessor & operator()(int i) { return (*this)[i]; }\
	ConstAccessor & operator()(int i) const { return (*this)[i]; }\
\
	TENSOR2_SYMMETRIC_ADD_RECURSIVE_CALL_INDEX()


template<typename T, int dim_>
struct _sym {
	using This = _sym;
	using InnerType = T;
	using ScalarType = typename VectorTraits<T>::ScalarType;
	static constexpr int dim = dim_;
	static constexpr int rank = 2 + VectorTraits<T>::rank;
	static constexpr int count = (dim * (dim + 1)) >> 1;

	T s[(dim*(dim+1))>>1] = {};
	_sym() {}

	TENSOR2_SYMMETRIC_MATRIX_CLASS_OPS(_sym)
	//TENSOR2_ADD_CAST_OP(_sym)
};

template<typename T>
struct _sym<T,2> {
	using This = _sym;
	using InnerType = T;
	using ScalarType = typename VectorTraits<T>::ScalarType;
	static constexpr int dim = 2;
	static constexpr int rank = 2 + VectorTraits<T>::rank;
	static constexpr int count = (dim * (dim + 1)) >> 1;

	union {
		struct {
			T xx = {};
			T xy = {};
			T yy = {};
		};
		struct {
			T s00;
			T s01;
			T s11;
		};
		T s[(dim*(dim+1))>>1];
	};
	_sym() {}
	_sym(T xx_, T xy_, T yy_) 
	: xx(xx_), xy(xy_), yy(yy_) {}

	static constexpr auto fields = std::make_tuple(
		std::make_pair("xx", &This::xx),
		std::make_pair("xy", &This::xy),
		std::make_pair("yy", &This::yy)
	);

	TENSOR2_SYMMETRIC_MATRIX_CLASS_OPS(_sym)
	//TENSOR2_ADD_CAST_OP(_sym)
};

template<typename T>
struct _sym<T,3> {
	using This = _sym;
	using InnerType = T;
	using ScalarType = typename VectorTraits<T>::ScalarType;
	static constexpr int dim = 3;
	static constexpr int rank = 2 + VectorTraits<T>::rank;
	static constexpr int count = (dim * (dim + 1)) >> 1;

	union {
		struct {
			T xx = {};
			T xy = {};
			T yy = {};
			T xz = {};
			T yz = {};
			T zz = {};
		};
		struct {
			T s00;
			T s01;
			T s11;
			T s02;
			T s12;
			T s22;
		};
		T s[(dim*(dim+1))>>1];
	};
	_sym() {}
	_sym(
		T const & xx_,
		T const & xy_,
		T const & yy_,
		T const & xz_,
		T const & yz_,
		T const & zz_
	) : xx(xx_),
		xy(xy_),
		yy(yy_),
		xz(xz_),
		yz(yz_),
		zz(zz_) {}

	static constexpr auto fields = std::make_tuple(
		std::make_pair("xx", &This::xx),
		std::make_pair("xy", &This::xy),
		std::make_pair("yy", &This::yy),
		std::make_pair("xz", &This::xz),
		std::make_pair("yz", &This::yz),
		std::make_pair("zz", &This::zz)
	);

	TENSOR2_SYMMETRIC_MATRIX_CLASS_OPS(_sym)
	//TENSOR2_ADD_CAST_OP(_sym)
};

template<typename T>
struct _sym<T,4> {
	using This = _sym;
	using InnerType = T;
	using ScalarType = typename VectorTraits<T>::ScalarType;
	static constexpr int dim = 4;
	static constexpr int rank = 2 + VectorTraits<T>::rank;
	static constexpr int count = (dim * (dim + 1)) >> 1;

	union {
		struct {
			T xx = {};
			T xy = {};
			T yy = {};
			T xz = {};
			T yz = {};
			T zz = {};
			T xw = {};
			T yw = {};
			T zw = {};
			T ww = {};
		};
		struct {
			T s00;
			T s01;
			T s11;
			T s02;
			T s12;
			T s22;
			T s03;
			T s13;
			T s23;
			T s33;	
		};
		T s[(dim*(dim+1))>>1];
	};
	_sym() {}
	_sym(
		T const & xx_,
		T const & xy_,
		T const & yy_,
		T const & xz_,
		T const & yz_,
		T const & zz_,
		T const & xw_,
		T const & yw_,
		T const & zw_,
		T const & ww_
	) : xx(xx_),
		xy(xy_),
		yy(yy_),
		xz(xz_),
		yz(yz_),
		zz(zz_),
		xw(xw_),
		yw(yw_),
		zw(zw_),
		ww(ww_) {}

	static constexpr auto fields = std::make_tuple(
		std::make_pair("xx", &This::xx),
		std::make_pair("xy", &This::xy),
		std::make_pair("yy", &This::yy),
		std::make_pair("xz", &This::xz),
		std::make_pair("yz", &This::yz),
		std::make_pair("zz", &This::zz),
		std::make_pair("xw", &This::xw),
		std::make_pair("yw", &This::yw),
		std::make_pair("zw", &This::zw),
		std::make_pair("ww", &This::ww)
	);

	TENSOR2_SYMMETRIC_MATRIX_CLASS_OPS(_sym)
	//TENSOR2_ADD_CAST_OP(_sym)
};

// symmetric op symmetric

#define TENSOR2_ADD_SYMMETRIC_SYMMETRIC_OP(op)\
template<typename T, int dim>\
_sym<T,dim> operator op(_sym<T,dim> const & a, _sym<T,dim> const & b) {\
	_sym<T,dim> c;\
	for (int i = 0; i < c.count; ++i) {\
		c.s[i] = a.s[i] op b.s[i];\
	}\
	return c;\
}

TENSOR2_ADD_SYMMETRIC_SYMMETRIC_OP(+)
TENSOR2_ADD_SYMMETRIC_SYMMETRIC_OP(-)
TENSOR2_ADD_SYMMETRIC_SYMMETRIC_OP(/)

// symmetric op scalar, scalar op symmetric

#define TENSOR2_ADD_SYMMETRIC_MATRIX_SCALAR_OP(op)\
template<typename T, int dim>\
requires std::is_same_v<typename _sym<T,dim>::ScalarType, T>\
_sym<T,dim> operator op(_sym<T,dim> const & a, T const & b) {\
	_sym<T,dim> c;\
	for (int i = 0; i < c.count; ++i) {\
		c.s[i] = a.s[i] op b;\
	}\
	return c;\
}\
template<typename T, int dim>\
requires std::is_same_v<typename _sym<T,dim>::ScalarType, T>\
_sym<T,dim> operator op(T const & a, _sym<T,dim> const & b) {\
	_sym<T,dim> c;\
	for (int i = 0; i < c.count; ++i) {\
		c.s[i] = a op b.s[i];\
	}\
	return c;\
}

TENSOR2_ADD_SYMMETRIC_MATRIX_SCALAR_OP(+)
TENSOR2_ADD_SYMMETRIC_MATRIX_SCALAR_OP(-)
TENSOR2_ADD_SYMMETRIC_MATRIX_SCALAR_OP(*)
TENSOR2_ADD_SYMMETRIC_MATRIX_SCALAR_OP(/)


//  partial derivatives

template<typename Type, int rank> 
Type getOffset(
	std::function<Type(intN<rank>)> f,
	intN<rank> index,
	int dim,
	int offset)
{
	index(dim) += offset;
	return f(index);
}

/*
partial derivative index operator
(partial derivative alone one coordinate)

finite difference coefficients for center-space finite-difference partial derivatives found at
http://en.wikipedia.org/wiki/Finite_difference_coefficients
*/

template<typename Real, int order>
struct PartialDerivCoeffs;

template<typename Real>
struct PartialDerivCoeffs<Real, 2> {
	static Real const coeffs[1];
};
template<typename Real>
Real const PartialDerivCoeffs<Real, 2>::coeffs[1] = { 1./2. };

template<typename Real>
struct PartialDerivCoeffs<Real, 4> {
	static Real const coeffs[2];
};
template<typename Real>
Real const PartialDerivCoeffs<Real, 4>::coeffs[2] = { 2./3., -1./12. };

template<typename Real>
struct PartialDerivCoeffs<Real, 6> {
	static Real const coeffs[3];
};
template<typename Real>
Real const PartialDerivCoeffs<Real, 6>::coeffs[3] = { 3./4., -3./20., 1./60. };

template<typename Real>
struct PartialDerivCoeffs<Real, 8> {
	static Real const coeffs[4];
};
template<typename Real>
Real const PartialDerivCoeffs<Real, 8>::coeffs[4] = { 4./5., -1./5., 4./105., -1./280. };

/*
partial derivative operator
for now let's use 2-point everywhere: d/dx^i f(x) ~= (f(x + dx^i) - f(x - dx^i)) / (2 * |dx^i|)
	index = index in grid of cell to pull the specified field
	k = dimension to differentiate across
*/
template<int order, typename Real, int dim, typename InputType>
struct PartialDerivativeClass;

template<int order, typename Real, int dim, typename InputType>
requires is_tensor_v<InputType> && std::is_same_v<Real, typename InputType::ScalarType>
struct PartialDerivativeClass<order, Real, dim, InputType> {
	using RealN = _vec<Real, dim>;
	using OutputType = _vec<InputType, dim>;
	using FuncType = std::function<InputType(intN<dim> index)>;
	static constexpr auto rank = InputType::rank;
	OutputType operator()(
		intN<dim> const & gridIndex,
		RealN const & dx,
		FuncType f)
	{
		using Coeffs = PartialDerivCoeffs<Real, order>;
		return OutputType([&](intN<rank+1> dstIndex){
			int gradIndex = dstIndex(0);
			intN<rank> srcIndex;
			for (int i = 0; i < rank; ++i) {
				srcIndex(i) = dstIndex(i+1);
			}
			Real sum = {};
			for (int i = 1; i <= (int)numberof(Coeffs::coeffs); ++i) {
				sum += (
					getOffset<InputType, dim>(f, gridIndex, gradIndex, i)(srcIndex) 
					- getOffset<InputType, dim>(f, gridIndex, gradIndex, -i)(srcIndex)
				) * Coeffs::coeffs[i-1];
			}
			return sum / dx(gradIndex);
		});
	}
};

template<int order, typename Real, int dim>
struct PartialDerivativeClass<order, Real, dim, Real> {
	using RealN = _vec<Real, dim>;
	using InputType = Real;
	using OutputType = RealN;
	using FuncType = std::function<InputType(intN<dim> index)>;
	OutputType operator()(
		intN<dim> const &gridIndex,
		RealN const &dx, 
		FuncType f)
	{
		using Coeffs = PartialDerivCoeffs<Real, order>;
		return OutputType([&](intN<1> dstIndex){
			int gradIndex = dstIndex(0);
			Real sum = {};
			for (int i = 1; i <= (int)numberof(Coeffs::coeffs); ++i) {
				sum += (
					getOffset<InputType, dim>(f, gridIndex, gradIndex, i)
					- getOffset<InputType, dim>(f, gridIndex, gradIndex, -i)
				) * Coeffs::coeffs[i-1];
			}
			return sum / dx(gradIndex);		
		});
	}
};

template<int order, typename Real, int dim, typename InputType>
typename PartialDerivativeClass<order, Real, dim, InputType>::OutputType
partialDerivative(
	intN<dim> const &index,
	Vector<Real, dim> const &dx,
	typename PartialDerivativeClass<order, Real, dim, InputType>::FuncType f)
{
	return PartialDerivativeClass<order, Real, dim, InputType>()(index, dx, f);
}




} //v2
} //Tensor
