#include "Test/Test.h"

void test_TotallyAntisymmetric() {
	using float3a3a3 = Tensor::float3a3a3;
	static_assert(sizeof(float3a3a3) == sizeof(float));
	
	using float3x3x3 = Tensor::_tensorr<float, 3, 3>;
	
	{
		auto L = float3a3a3();
		L(0,0,0) = 1;
		TEST_EQ(L(0,0,0), 0);
		L(0,0,1) = 1;
		TEST_EQ(L(0,0,1), 0);
		L(0,1,0) = 1;
		TEST_EQ(L(0,1,0), 0);
		L(1,0,0) = 1;
		TEST_EQ(L(1,0,0), 0);
		
		L(0,1,2) = 1;
		TEST_EQ(L(0,1,2), 1);
		TEST_EQ(L(1,2,0), 1);
		TEST_EQ(L(2,0,1), 1);
		TEST_EQ(L(2,1,0), -1);
		TEST_EQ(L(1,0,2), -1);
		TEST_EQ(L(0,2,1), -1);
	}
	
	{
		// do iteration bounds match between rank-3 and asym-3?
		auto x = float3x3x3();
		auto a = float3a3a3();
		auto ix = x.begin();
		auto ia = a.begin();
		for (;;) {
			TEST_EQ(ix.index, ia.index);
			TEST_EQ(*ix, *ia);	//mind you *ia will be an AntiSymRef, but the value will evaluate to Scalar(0)
			++ix;
			++ia;
			if (ix == x.end()) {
				TEST_EQ(ia, a.end());
				break;
			}
			if (ia == a.end()) throw Common::Exception() << "looks like iterator doesn't end in the right place";
		}
	}

	//iterator test inline
	{
		auto a = float3a3a3();
		auto b = float3x3x3();
		for (auto i = a.begin(); i != a.end(); ++i) {
			ECHO(i.index);
			Tensor::int3 sortedi = i.index;
			auto sign = antisymSortAndCountFlips(sortedi);
			ECHO(sortedi);
			ECHO(sign);
			auto writeIndex = float3a3a3::getLocalWriteForReadIndex(sortedi);
			ECHO(writeIndex);
			TEST_BOOL(
				sign == Tensor::Sign::ZERO ||
				writeIndex == 0	// requires input to be sorted
			);
			TEST_EQ(a(i.index), b(i.index));
		}
	}
	{
		auto a = float3x3x3();
		auto b = float3a3a3();
		for (auto i = a.begin(); i != a.end(); ++i) {
			ECHO(i.index);
			Tensor::int3 sortedi = i.index;
			auto sign = antisymSortAndCountFlips(sortedi);
			ECHO(sortedi);
			ECHO(sign);
			auto writeIndex = float3a3a3::getLocalWriteForReadIndex(sortedi);
			ECHO(writeIndex);
			TEST_BOOL(
				sign == Tensor::Sign::ZERO ||
				writeIndex == 0	// requires input to be sorted
			);
			TEST_EQ(a(i.index), b(i.index));
		}
	}

	{
		// does = work between two tensor types of matching dims?
		TEST_EQ(float3a3a3(), float3x3x3());	// works
		TEST_EQ(float3x3x3(), float3a3a3());	// fails
	}

// parity flipping chokes the decltype(auto)
	{
		// lambda for Levi-Civita permutation tensor
		auto f = [](int i, int j, int k) -> float {
			return sign(i-j) * sign(j-k) * sign(k-i);
		};
		auto t = float3a3a3(f);
		verifyAccessRank3<decltype(t)>(t, f);
		verifyAccessRank3<decltype(t) const>(t, f);
		operatorScalarTest(t);
	}

	{
		using float3 = Tensor::float3;
		auto L = float3a3a3(1);	//Levi-Civita permutation tensor
		ECHO(L);
		auto x = float3(1,0,0);
		ECHO(x);
		auto dualx = x * L;
		ECHO(dualx);
		static_assert(std::is_same_v<decltype(dualx), Tensor::float3a3>);
		auto y = float3(0,1,0);
		auto z = y * dualx;
		TEST_EQ(z, float3(0,0,1));
	}

	{
		auto n = Tensor::float3(3,-4,8); // 3 floats :: 3-vector
		constexpr auto L = Tensor::float3a3a3(1); // 1 real ... value 1
		auto dualn = n * L; // 3 floats :: 3x3 antisymmetric matrix
		static_assert(std::is_same_v<decltype(dualn), Tensor::float3a3>);
		ECHO((Tensor::float3x3)dualn);	// {{0, 8, 4}, {-8, 0, 3}, {-4, -3, 0}}
// TODO this returns an Accessor.  Would be nice if we had ctors that could handle them.		
// would be nice if Accessors had other tensor operations (like lenSq())
#if 0
		Tensor::float3 nx = dualn(0);
		ECHO(nx);
		Tensor::float3 ny = dualn(1);
		ECHO(ny);
		Tensor::float3 nz = dualn(2);
		ECHO(nz);
#else	//until then ... you still have to expand the basii before doing operations on it
		Tensor::float3x3 dualnm = dualn;
		Tensor::float3 nx = dualnm(0);
		ECHO(nx);
		Tensor::float3 ny = dualnm(1);
		ECHO(ny);
		Tensor::float3 nz = dualnm(2);
		ECHO(nz);
#endif
	}

	{
		auto n = Tensor::float3(3,-4,8);
		auto ns = Tensor::hodgeDual(n);
		static_assert(std::is_same_v<decltype(ns), Tensor::float3a3>);
		ECHO(ns);
		TEST_EQ(ns, n * Tensor::float3a3a3(1));
		TEST_EQ((Tensor::float3x3)ns, (Tensor::float3x3)(n * Tensor::float3a3a3(1)));
	}
}
