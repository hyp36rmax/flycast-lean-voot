// SPDX-License-Identifier: GPL-2.0-or-later

#include <xbyak/xbyak.h>
#include <xbyak/xbyak_util.h>
#include <cstdint>
#include <vector>
#include <cstdio>
using u32=uint32_t;using Entry=uintptr_t;using namespace Xbyak::util;
constexpr u32 FPCB_MASK=0xffffff;
bool mmuOn=false;unsigned calls=0;
struct Context {Entry* fpcb;};Context context;Context* p_sh4rcb=&context;
Entry bm_GetCodeByVAddr(u32 pc){++calls;return mmuOn?((uintptr_t(pc)^0x12345678)+3):p_sh4rcb->fpcb[(pc>>1)&FPCB_MASK];}
struct Lookup : Xbyak::CodeGenerator {
 using Xbyak::CodeGenerator::call;
 void call(Entry(*fn)(u32)){mov(r11,(uintptr_t)fn);Xbyak::CodeGenerator::call(r11);}
 Lookup(){
  const Xbyak::Reg32 call_regs[]={ecx};const Xbyak::Reg64 call_regs64[]={rcx};
  sub(rsp,40);
		// MMU state can change between blocks; retain the translated lookup path.
		Xbyak::Label translated_lookup, lookup_done;
		mov(rax, (uintptr_t)&mmuOn);
		cmp(byte[rax], 0);
		jne(translated_lookup);
		shr(call_regs[0], 1);
		and_(call_regs[0], FPCB_MASK);
		mov(rax, (uintptr_t)p_sh4rcb->fpcb);
		mov(rax, qword[rax + call_regs64[0] * 8]);
		jmp(lookup_done);
		L(translated_lookup);
		call(bm_GetCodeByVAddr);
		L(lookup_done);
  add(rsp,40);ret();ready();
 }
};
int main(){std::vector<Entry> table(size_t(FPCB_MASK)+1);context.fpcb=table.data();Lookup code;auto lookup=code.getCode<Entry(*)(u32)>();
 unsigned seed=37;for(unsigned i=0;i<200000;i++){
  seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;const u32 pc=i<8?u32(i==0?0:i==1?1:i==2?0xffffffff:i==3?0xfffffffe:0x8c000000+i):seed;
  auto expected=table[(pc>>1)&FPCB_MASK]=((uintptr_t(seed)<<32)|(~seed));mmuOn=(i&1)!=0;
  if(mmuOn)expected=(uintptr_t(pc)^0x12345678)+3;calls=0;auto actual=lookup(pc);
  if(actual!=expected||calls!=(mmuOn?1u:0u)){printf("FAIL %u\n",i);return 1;}
 }
 puts("PASS 200000 generated x64 lookup cases: full 32-bit PCs, odd/even/mirror/boundary indices, table mutation after code generation, runtime MMU toggles and native fallback dispatch.");
}
