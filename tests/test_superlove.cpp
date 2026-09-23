/* Bit-exact Super Love Core checksums. No Rack.
   g++ -std=c++11 -O2 -I. -o test_superlove.exe test_superlove.cpp && test_superlove.exe
*/
#include "../src/modules/SuperLove/SuperLoveFilter.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

static const float SR = 48000.f;
static const int N = 4096;

static uint64_t hashBits(const float* x, int n) {
	uint64_t h = 14695981039346656037ull;
	for (int i = 0; i < n; i++) {
		uint32_t u = 0;
		std::memcpy(&u, &x[i], 4);
		h ^= u;
		h *= 1099511628211ull;
	}
	return h;
}

static uint64_t runCase(fmd::super_love::Mode mode, float freq, float res, float drive, float spread) {
	fmd::super_love::Core core;
	core.setSampleRate(SR);
	core.reset();
	float acc[N];
	for (int i = 0; i < N; i++) {
		float ph = float(i) / 64.f;
		ph -= float(int(ph));
		float s = 5.f * (2.f * ph - 1.f);
		float in[2] = {s, s * 0.7f};
		float out[2] = {0.f, 0.f};
		core.process(in, out, freq, res, 0.f, drive, spread, mode);
		acc[i] = out[0] + 0.5f * out[1];
	}
	return hashBits(acc, N);
}

struct Case {
	const char* name;
	fmd::super_love::Mode mode;
	float freq, res, drive, spread;
	uint64_t expect;
};

int main() {
	Case cases[] = {
		{"lp18", fmd::super_love::MODE_LP18, 0.50f, 0.30f, 1.00f, 0.00f, 0x391098c35284d22bull},
		{"lp24", fmd::super_love::MODE_LP24, 0.50f, 0.30f, 1.00f, 0.00f, 0xa10d64e6d7a6361cull},
		{"hp",   fmd::super_love::MODE_HP,   0.50f, 0.30f, 1.00f, 0.00f, 0x82df6614218d124aull},
		{"bp",   fmd::super_love::MODE_BP,   0.50f, 0.30f, 1.00f, 0.00f, 0xa3a9fde1270bd6c1ull},
		{"lp18-hires-drive", fmd::super_love::MODE_LP18, 0.72f, 0.85f, 2.50f, 0.40f, 0x49f548cb3a07a1a2ull},
		{"hp-spread", fmd::super_love::MODE_HP, 0.35f, 0.10f, 0.80f, -0.70f, 0x6b8e489e6ce44751ull},
		{"bp-hot", fmd::super_love::MODE_BP, 0.62f, 0.95f, 4.00f, 0.20f, 0x7e8f9da541d120bcull},
		{"lp24-low", fmd::super_love::MODE_LP24, 0.12f, 0.00f, 0.50f, 0.00f, 0x8f25acdfd143f7e0ull},
	};
	const int n = int(sizeof(cases) / sizeof(cases[0]));
	int fail = 0;
	for (int i = 0; i < n; i++) {
		uint64_t h = runCase(cases[i].mode, cases[i].freq, cases[i].res, cases[i].drive, cases[i].spread);
		std::printf("%-18s %016llx\n", cases[i].name, (unsigned long long)h);
		if (cases[i].expect != 0 && h != cases[i].expect) {
			std::printf("  FAIL expected %016llx\n", (unsigned long long)cases[i].expect);
			fail++;
		}
	}
	return fail ? 1 : 0;
}
