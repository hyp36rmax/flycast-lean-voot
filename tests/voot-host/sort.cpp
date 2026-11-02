// SPDX-License-Identifier: GPL-2.0-or-later
#include "ta_triangle_sort.h"
#include <iostream>
#include <random>
#include <xmmintrin.h>
#include <cstdlib>
#include <new>
static bool failNextAllocation=false;
void* operator new(std::size_t n) {if(failNextAllocation){failNextAllocation=false;throw std::bad_alloc();}if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}

static float fromBits(std::uint32_t bits) {float value;std::memcpy(&value,&bits,4);return value;}
int main() {
    static_assert(sizeof(pvr::TriangleSortEntry)==20);
    const unsigned original=_mm_getcsr();
    std::mt19937 random(566);
    std::vector<pvr::TriangleSortEntry> scratch;
    unsigned cases=0,fastCases=0;
    for(unsigned mode:{0u,0x8040u}) {
        _mm_setcsr((original & ~0x8040u)|mode);
        for(unsigned c=0;c<3000;++c) {
            const std::size_t sizes[]={0,1,2,31,32,127,128,129,255,256,257,277,1024,8192};
            const auto size=c<14?sizes[c]:random()%2049;
            std::vector<pvr::TriangleSortEntry> actual;
            for(std::size_t i=0;i<size;++i) {
                std::uint32_t bits=random();
                bits=(bits & 0x807fffff)|((1+random()%254)<<23);
                if(c%3==0) bits=0x3f000000+(random()%8)*0x100000;
                if(c%5==0) bits=(random()%2)*0x80000000;
                if(c%7==0 && i%17==0) bits=(random()%2)*0x80000000+0x7f800000;
                if(c%11==0 && i==size/2) bits=0x7fc00123;
                if(c%13==0 && i==size/2) bits=0x7f800123;
                if(c%17==0 && i==size/2) bits=0x80000001;
                actual.emplace_back((std::uint32_t)i,(std::uint32_t)(i*3),4,5);
                actual.back().z=fromBits(bits);
            }
            auto expected=actual;
            std::stable_sort(expected.begin(),expected.end());
            const auto before=_mm_getcsr();
            fastCases+=pvr::stableDepthSort(actual,scratch);
            if((_mm_getcsr()&0xffc0)!=(before&0xffc0) || actual.size()!=expected.size()
                || (size && std::memcmp(actual.data(),expected.data(),size*sizeof(actual[0]))!=0)) {
                std::cerr<<"FAILED case="<<c<<" size="<<size<<" MXCSR mode="<<mode<<'\n';return 1;
            }
            ++cases;
        }
    }
    std::vector<pvr::TriangleSortEntry> oomInput;
    for(unsigned i=0;i<512;++i) {oomInput.emplace_back(i,0,0,0);oomInput.back().z=float(512-i);}
    auto oomExpected=oomInput;
    std::stable_sort(oomExpected.begin(),oomExpected.end());
    std::vector<pvr::TriangleSortEntry> emptyScratch;
    failNextAllocation=true;
    if(pvr::stableDepthSort(oomInput,emptyScratch) || std::memcmp(oomInput.data(),oomExpected.data(),oomInput.size()*sizeof(oomInput[0]))!=0) return 2;
    _mm_setcsr(original);
    std::cout<<"PASS "<<cases<<" differential lists, "<<fastCases<<" radix cases; ties/signed zeros/infinities/NaNs/subnormals; both DAZ/FTZ modes; FPU controls preserved; allocation-failure fallback passed\n";
}
