#pragma once

// wrapper of a ref of a type that reads and writes the negative of the value its wrapping

#include <functional>	//std::reference_wrapper
#include <optional>
#include <ostream>		//std::ostream

namespace Tensor {

/*
Detects if a class is a "tensor".
These include _vec _sym _asym and subclasses (like _quat).
It's defined in the class in the TENSOR_HEADER, as a static constexpr field.

TODO put this in another file.  Tensor/Meta.h?

TODO should this decay_t T or should I rely on the invoker to is_tensor_v<decay_t<T>> ?
*/
template<typename T>
constexpr bool is_tensor_v = requires(T const & t) { &T::isTensorFlag; };

//https://stackoverflow.com/a/61040973
namespace {
    template <typename, template <typename...> typename>
    struct is_instance_impl : public std::false_type {};

    template <template <typename...> typename U, typename...Ts>
    struct is_instance_impl<U<Ts...>, U> : public std::true_type {};
}
template <typename T, template <typename ...> typename U>
using is_instance = is_instance_impl<std::decay_t<T>, U>;
template <typename T, template <typename ...> typename U>
constexpr bool is_instance_v = is_instance<T, U>::value;

typedef enum {
	POSITIVE, // == 0 // so that how1 ^ how2 produces the sign for AntiSymRef1-of-AntiSymRef2
	NEGATIVE, // == 1
	ZERO,
} AntiSymRefHow;

template<typename T>
struct AntiSymRef {
	using This = AntiSymRef;
	using Type = T;
	
	static_assert(!is_instance_v<T, AntiSymRef>);	//never wrap AntiSymRef<AntiSymRef<T>>, instead just use 1 wrapper, flip 'how' if necessary.

	std::optional<std::reference_wrapper<T>> x;

	AntiSymRefHow how = ZERO;
	
	AntiSymRef() {}
	AntiSymRef(std::reference_wrapper<T> const & x_, AntiSymRefHow how_) : x(x_), how(how_) {}
	AntiSymRef(AntiSymRef const & r) : x(r.x), how(r.how) {}
	AntiSymRef(AntiSymRef && r) : x(r.x), how(r.how) {}
	
	This & operator=(T const & y) {
		if (how == AntiSymRefHow::POSITIVE) {
			(*x).get() = y;
		} else if (how == AntiSymRefHow::NEGATIVE) {
			(*x).get() = -y;
		} else {//if (how == ZERO) {
			//ZERO should only be wrapping temp elements outside the antisymmetric matrix
		}
		return *this;
	}
	
	This & operator=(T && y) {
		if (how == AntiSymRefHow::POSITIVE) {
			(*x).get() = y;
		} else if (how == AntiSymRefHow::NEGATIVE) {
			(*x).get() = -y;
		} else {//if (how == ZERO) {
			// no write
		}
		return *this;
	}

	operator T() const {
		if (how == AntiSymRefHow::POSITIVE) {
			return (*x).get();
		} else if (how == AntiSymRefHow::NEGATIVE) {
			return -(*x).get();
		} else {//if (how == ZERO) {
			return {};
		}
	}

	template<typename... Args>
	auto operator()(Args&&... args) const
	//requires (std::is_invocable_v<T()>) 
	requires (is_tensor_v<T>)
	{
		using R = std::decay_t<decltype((*x).get()(std::forward<Args>(args)...))>;
		if constexpr (is_instance_v<R, AntiSymRef>) {
			using RT = typename R::Type;	// TODO nested-most?  verified for two-nestings deep, what about 3?
			using RRT = std::conditional_t<std::is_const_v<T>, const RT, RT>;
			if (how != AntiSymRefHow::POSITIVE && how != AntiSymRefHow::NEGATIVE) {
				return AntiSymRef<RRT>();
			} else {
				R && r = (*x).get()(std::forward<Args>(args)...);
				if (r.how != AntiSymRefHow::POSITIVE && r.how != AntiSymRefHow::NEGATIVE) {
					return AntiSymRef<RRT>();
				} else {
					return AntiSymRef<RRT>(*r.x, (AntiSymRefHow)((int)how ^ (int)r.how));
				}
			}
		} else {
			using RRT = std::conditional_t<std::is_const_v<T>, const R, R>;
			if (how != AntiSymRefHow::POSITIVE && how != AntiSymRefHow::NEGATIVE) {
				return AntiSymRef<RRT>();
			} else {
				return AntiSymRef<RRT>(std::ref(
					(*x).get()(std::forward<Args>(args)...)
				), how);
			}
		}
	}

	// return ref or no?
	constexpr This flip() const {
		if (how == AntiSymRefHow::POSITIVE || how == AntiSymRefHow::NEGATIVE) {
			return AntiSymRef(*x, (AntiSymRefHow)((int)how ^ 1));
		}
		return *this;
	}
};

static_assert(is_instance_v<AntiSymRef<double>, AntiSymRef>);
static_assert(is_instance_v<AntiSymRef<AntiSymRef<double>>, AntiSymRef>);

template<typename T>
bool operator==(AntiSymRef<T> const & a, T const & b) {
	return a.operator T() == b;
}

template<typename T>
bool operator==(T const & a, AntiSymRef<T> const & b) {
	return a == b.operator T();
}

template<typename T>
bool operator==(AntiSymRef<T> const & a, AntiSymRef<T> const & b) {
	return a.operator T() == b.operator T();
}

template<typename T> bool operator!=(AntiSymRef<T> const & a, T const & b) { return !operator==(a,b); }
template<typename T> bool operator!=(T const & a, AntiSymRef<T> const & b) { return !operator==(a,b); }
template<typename T> bool operator!=(AntiSymRef<T> const & a, AntiSymRef<T> const & b) { return !operator==(a,b); }

template<typename T>
std::ostream & operator<<(std::ostream & o, AntiSymRef<T> const & t) {
	return o << t.operator T();
}

}
