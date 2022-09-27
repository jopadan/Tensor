#pragma once

#include "Tensor/Vector.h"	// new tensor struct
#include "Common/Exception.h"
#include <cassert>
#include <algorithm>

#if PLATFORM_MSVC
#undef min
#undef max
#endif


namespace Tensor {

//TODO move RangeObj iterator with Grid iterator
template<int rank_>
struct RangeObj {
	static constexpr auto rank = rank_;
	using DerefType = intN<rank>;
	DerefType min, max;

	RangeObj(DerefType min_, DerefType max_) : min(min_), max(max_) {}

	// iterators

	struct iterator {
		DerefType index, min, max;
		
		iterator()  {}
		iterator(DerefType min_, DerefType max_) : index(min_), min(min_), max(max_) {}
		iterator(iterator const &iter) : index(iter.index), min(iter.min), max(iter.max) {}
		
		bool operator==(iterator const &b) const { return index == b.index; }
		bool operator!=(iterator const &b) const { return index != b.index; }

		//converts index to int
		int flatten() const {
			int flatIndex = 0;
			for (int i = rank - 1; i >= 0; --i) {
				flatIndex *= max(i) - min(i);
				flatIndex += index(i) - min(i);
			}
			return flatIndex;
		}
	
		//converts int to index
		void unflatten(int flatIndex) {
			for (int i = 0; i < rank; ++i) {
				int s = max(i) - min(i);
				int n = flatIndex;
				if (i < rank-1) n %= s;
				index(i) = n + min(i);
				flatIndex = (flatIndex - n) / s;
			}
		}

		iterator &operator+=(int offset) {
			unflatten(flatten() + offset);
			return *this;
		}

		iterator &operator-=(int offset) {
			unflatten(flatten() - offset);
			return *this;
		}

		iterator operator+(int offset) const {
			return iterator(*this) += offset;
		}

		iterator operator-(int offset) const {
			return iterator(*this) -= offset;
		}

		int operator-(iterator const &i) const {
			return flatten() - i.flatten();
		}

		iterator &operator++() {
			for (int i = 0; i < rank; ++i) {	//allow the last index to overflow for sake of comparing it to end
				++index(i);
				if (index(i) < max(i)) break;
				if (i < rank-1) index(i) = min(i);
			}
			return *this;
		}

		DerefType &operator*() { return index; }
		DerefType *operator->() { return &index; }
	};

	iterator begin() {
		return iterator(min, max);
	}
	iterator end() {
		iterator i(min, max);
		i.index(rank-1) = i.max(rank-1);
		return i;
	}
	
	struct const_iterator {
		DerefType index, min, max;
		
		const_iterator() {}
		const_iterator(DerefType min_, DerefType max_) : min(min_), max(max_) {}
		const_iterator(const_iterator const &iter) : index(iter.index), min(iter.min), max(iter.max) {}
		
		bool operator==(const_iterator const &b) const { return index == b.index; }
		bool operator!=(const_iterator const &b) const { return index != b.index; }
		
		const_iterator &operator++() {
			for (int i = 0; i < rank; ++i) {	//allow the last index to overflow for sake of comparing it to end
				++index(i);
				if (index(i) < max(i)) break;
				if (i < rank-1) index(i) = min(i);
			}
			return *this;
		}

		DerefType const &operator*() const { return index; }
		DerefType const *operator->() const { return &index; }
	};

	const_iterator begin() const {
		return const_iterator(min, max);
	}
	const_iterator end() const {
		const_iterator i(min, max);
		i.index(rank-1) = i.max(rank-1);
		return i;
	}
};


//rank is templated, but dim is not as it varies per-rank
//so this is dynamically-sized tensor
template<typename Type_, int rank_>
struct Grid {
	using Type = Type_;
	using value_type = Type;
	static constexpr auto rank = rank_;
	using DerefType = intN<rank>;

	DerefType size;
	Type * v = {};
	bool own = {};	//or just use a shared_ptr to v?
	
	//cached for quick access by dot with index vector
	//step[0] = 1, step[1] = size[0], step[j] = product(i=1,j-1) size[i]
	DerefType step;

	Grid(DerefType const &size_ = DerefType(), Type* v_ = {})
	:	size(size_),
		v(v_),
		own(false)
	{
		if (!v) {
			v = new Type[size.volume()]();
			own = true;
		}
		step(0) = 1;
		for (int i = 1; i < rank; ++i) {
			step(i) = step(i-1) * size(i-1);
		}
	}

	~Grid() {
		if (own) {
			delete[] v;
		}
	}

	// dereference by vararg ints

	template<int offset, typename... Rest>
	struct BuildDeref;

	template<int offset, typename... Rest>
	struct BuildDeref<offset, int, Rest...> {
		static DerefType exec(int next, Rest ... rest) {
			DerefType index = BuildDeref<offset+1, Rest...>::exec(rest...);
			index(offset) = next;
			return index;
		}
	};

	template<int offset>
	struct BuildDeref<offset, int> {
		static DerefType exec(int last) {
			static_assert(offset == rank-1, "didn't provide enough arguments for dereference");
			DerefType index;
			index(offset) = last;
			return index;
		}
	};

	template<typename... Rest>
	Type &operator()(int first, Rest... rest) {
		return getValue(BuildDeref<0, int, Rest...>::exec(first, rest...));
	}

	template<typename... Rest>
	Type const &operator()(int first, Rest... rest) const {
		return getValue(BuildDeref<0, int, Rest...>::exec(first, rest...));
	}

	//dereference by a vector of ints

	//typical access will be only for the Type's sake
	Type &operator()(DerefType const &deref) { return getValue(deref); }
	Type const &operator()(DerefType const &deref) const { return getValue(deref); }

	//but other folks (currently only our initialization of our indexes) will want the whole value
	Type &getValue(DerefType const &deref) { 
#ifdef DEBUG
		for (int i = 0; i < rank; ++i) {
			if (deref(i) < 0 || deref(i) >= size(i)) {
				// TODO why can't I use operator<<?  no, 'using ::operator<<' doen't help
				// doesn't work:
				//throw Common::Exception() << "size is " << size << " but dereference is " << deref;
				// works: ... sometimes ...not always ... hmm
				throw Common::Exception() << "failed";//"size is " << std::to_string(size) << " but dereference is " << std::to_string(deref);
			}
		}
#endif
		int flat_deref = deref.dot(step);
		assert(flat_deref >= 0 && flat_deref < size.volume());
		return v[flat_deref];
	}
	Type const &getValue(DerefType const &deref) const { 
#ifdef DEBUG
		for (int i = 0; i < rank; ++i) {
			if (deref(i) < 0 || deref(i) >= size(i)) {
				// same as above
				throw Common::Exception() << "failed";//"size is " << std::to_string(size) << " but dereference is " << std::to_string(deref);
			}
		}
#endif
		int flat_deref = deref.dot(step);
		assert(flat_deref >= 0 && flat_deref < size.volume());
		return v[flat_deref];
	}

	using iterator = Type*;
	using const_iterator = Type const*;
	Type *begin() { return v; }
	Type *end() { return v + size.volume(); }
	Type const *begin() const { return v; }
	Type const *end() const { return v + size.volume(); }

	RangeObj<rank> range() const {
		return RangeObj<rank>(DerefType(), size);
	}

	//dereference by vararg ints 
	
	template<typename... Rest>
	void resize(int first, Rest... rest) {
		resize(BuildDeref<0, int, Rest...>::exec(first, rest...));
	}

	//dereference by a vector of ints

	void resize(DerefType const& newSize) {
		if (size == newSize) return;
		
		DerefType oldSize = size;
		DerefType oldStep = step;
		Type* oldV = v;
		
		size = newSize;
		v = new Type[newSize.volume()];
		step(0) = 1;
		for (int i = 1; i < rank; ++i) {
			step(i) = step(i-1) * size(i-1);
		}

		DerefType minSize;
		for (int i = 0; i < rank; ++i) {
			minSize(i) = size(i) < oldSize(i) ? size(i) : oldSize(i);
		}

		RangeObj<rank> range(DerefType(), minSize);		
		for (typename RangeObj<rank>::iterator iter = range.begin(); iter != range.end(); ++iter) {
			DerefType index = *iter;
			int oldOffset = oldStep.dot(index);
			(*this)(index) = oldV[oldOffset];
		}

		delete[] oldV;
	}

	Grid& operator=(Grid& src) {
		resize(src.dims());

		Type* srcv = src.v;
		Type* dstv = v;
		for (int i = size.volume()-1; i >= 0; --i) {
			*(dstv++) = *(srcv++);
		}
		return *this;
	}
};

}
