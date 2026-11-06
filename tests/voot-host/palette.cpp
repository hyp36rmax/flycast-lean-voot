// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
#include <chrono>
#include <intrin.h>
using u8=uint8_t;using u16=uint16_t;using u32=uint32_t;
static u32 bitscanrev(u32 x){unsigned long v;_BitScanReverse(&v,x);return v;}
template<typename Pixel>
class PixelBuffer
{
	Pixel* p_buffer_start = nullptr;
	Pixel* p_current_mipmap = nullptr;
	Pixel* p_current_line = nullptr;
	Pixel* p_current_pixel = nullptr;

	u32 pixels_per_line = 0;

public:
	~PixelBuffer() {
		deinit();
	}

	void init(u32 width, u32 height, bool mipmapped)
	{
		deinit();
		size_t size = width * height * sizeof(Pixel);
		if (mipmapped)
		{
			do
			{
				width /= 2;
				height /= 2;
				size += width * height * sizeof(Pixel);
			}
			while (width != 0 && height != 0);
		}
		p_buffer_start = p_current_line = p_current_pixel = p_current_mipmap = (Pixel *)malloc(size);
		this->pixels_per_line = 1;
	}

	void init(u32 width, u32 height)
	{
		deinit();
		p_buffer_start = p_current_line = p_current_pixel = p_current_mipmap = (Pixel *)malloc(width * height * sizeof(Pixel));
		this->pixels_per_line = width;
	}

	void deinit()
	{
		if (p_buffer_start != nullptr)
		{
			free(p_buffer_start);
			p_buffer_start = p_current_mipmap = p_current_line = p_current_pixel = nullptr;
		}
	}

	void steal_data(PixelBuffer &buffer)
	{
		deinit();
		p_buffer_start = p_current_mipmap = p_current_line = p_current_pixel = buffer.p_buffer_start;
		pixels_per_line = buffer.pixels_per_line;
		buffer.p_buffer_start = buffer.p_current_mipmap = buffer.p_current_line = buffer.p_current_pixel = nullptr;
	}

	void set_mipmap(int level)
	{
		u32 offset = 0;
		for (int i = 0; i < level; i++)
			offset += (1 << (2 * i));
		p_current_mipmap = p_current_line = p_current_pixel = p_buffer_start + offset;
		pixels_per_line = 1 << level;
	}

	Pixel *data(u32 x = 0, u32 y = 0)
	{
		return p_current_mipmap + pixels_per_line * y + x;
	}

	void prel(u32 x, Pixel value)
	{
		p_current_pixel[x] = value;
	}

	void prel(u32 x, u32 y, Pixel value)
	{
		p_current_pixel[y * pixels_per_line + x] = value;
	}

	void rmovex(u32 value)
	{
		p_current_pixel += value;
	}

	void rmovey(u32 value)
	{
		p_current_line += pixels_per_line * value;
		p_current_pixel = p_current_line;
	}

	void amove(u32 x_m, u32 y_m)
	{
		//p_current_pixel=p_buffer_start;
		p_current_line = p_current_mipmap + pixels_per_line * y_m;
		p_current_pixel = p_current_line + x_m;
	}
};

// OpenGL
template<typename Pixel>
struct UnpackerNop {
	using unpacked_type = Pixel;
	static Pixel unpack(Pixel word) {
		return word;
	}
};


namespace reference {
u32 palette16_ram[1024],palette32_ram[1024],palette_index,detwiddle[2][11][1024];const u8* vq_codebook;
static u32 twiddle_slow(u32 x, u32 y, u32 x_sz, u32 y_sz)
{
	u32 rv = 0; // low 2 bits are directly passed  -> needs some misc stuff to work.However
				// Pvr internally maps the 64b banks "as if" they were twiddled

	u32 sh = 0;
	x_sz >>= 1;
	y_sz >>= 1;
	while (x_sz != 0 || y_sz != 0)
	{
		if (y_sz != 0)
		{
			u32 temp = y & 1;
			rv |= temp << sh;

			y_sz >>= 1;
			y >>= 1;
			sh++;
		}
		if (x_sz != 0)
		{
			u32 temp = x & 1;
			rv |= temp << sh;

			x_sz >>= 1;
			x >>= 1;
			sh++;
		}
	}
	return rv;
}
static u32 twop(u32 x, u32 y, u32 bcx, u32 bcy) {
	return detwiddle[0][bcy][x] + detwiddle[1][bcx][y];
}
template<typename Pixel>
struct UnpackerPalToRgb {
	using unpacked_type = Pixel;
	static Pixel unpack(u8 col)
	{
		u32 *pal = sizeof(Pixel) == 2 ? &palette16_ram[palette_index] : &palette32_ram[palette_index];
		return pal[col];
	}
};
template<typename Unpacker>
struct ConvertTwiddlePal4
{
	using unpacked_type = typename Unpacker::unpacked_type;
	static constexpr u32 xpp = 4;
	static constexpr u32 ypp = 4;
	static void Convert(PixelBuffer<unpacked_type> *pb, const u8 *data)
	{
		const u8 *p_in = data;

		pb->prel(0, 0, Unpacker::unpack(p_in[0] & 0xF));
		pb->prel(0, 1, Unpacker::unpack((p_in[0] >> 4) & 0xF)); p_in++;
		pb->prel(1, 0, Unpacker::unpack(p_in[0] & 0xF));
		pb->prel(1, 1, Unpacker::unpack((p_in[0] >> 4) & 0xF)); p_in++;

		pb->prel(0, 2, Unpacker::unpack(p_in[0] & 0xF));
		pb->prel(0, 3, Unpacker::unpack((p_in[0] >> 4) & 0xF)); p_in++;
		pb->prel(1, 2, Unpacker::unpack(p_in[0] & 0xF));
		pb->prel(1, 3, Unpacker::unpack((p_in[0] >> 4) & 0xF)); p_in++;

		pb->prel(2, 0, Unpacker::unpack(p_in[0] & 0xF));
		pb->prel(2, 1, Unpacker::unpack((p_in[0] >> 4) & 0xF)); p_in++;
		pb->prel(3, 0, Unpacker::unpack(p_in[0] & 0xF));
		pb->prel(3, 1, Unpacker::unpack((p_in[0] >> 4) & 0xF)); p_in++;

		pb->prel(2, 2, Unpacker::unpack(p_in[0] & 0xF));
		pb->prel(2, 3, Unpacker::unpack((p_in[0] >> 4) & 0xF)); p_in++;
		pb->prel(3, 2, Unpacker::unpack(p_in[0] & 0xF));
		pb->prel(3, 3, Unpacker::unpack((p_in[0] >> 4) & 0xF)); p_in++;
	}
};
template<typename Unpacker>
struct ConvertTwiddlePal8
{
	using unpacked_type = typename Unpacker::unpacked_type;
	static constexpr u32 xpp = 2;
	static constexpr u32 ypp = 4;
	static void Convert(PixelBuffer<unpacked_type> *pb, const u8 *data)
	{
		const u8* p_in = (const u8 *)data;

		pb->prel(0, 0, Unpacker::unpack(p_in[0])); p_in++;
		pb->prel(0, 1, Unpacker::unpack(p_in[0])); p_in++;
		pb->prel(1, 0, Unpacker::unpack(p_in[0])); p_in++;
		pb->prel(1, 1, Unpacker::unpack(p_in[0])); p_in++;

		pb->prel(0, 2, Unpacker::unpack(p_in[0])); p_in++;
		pb->prel(0, 3, Unpacker::unpack(p_in[0])); p_in++;
		pb->prel(1, 2, Unpacker::unpack(p_in[0])); p_in++;
		pb->prel(1, 3, Unpacker::unpack(p_in[0])); p_in++;
	}
};
template<typename PixelConvertor>
void texture_TW(PixelBuffer<typename PixelConvertor::unpacked_type>* pb, const u8* p_in, u32 width, u32 height)
{
	pb->amove(0, 0);

	const u32 divider = PixelConvertor::xpp * PixelConvertor::ypp;

	const u32 bcx = bitscanrev(width);
	const u32 bcy = bitscanrev(height);

	for (u32 y = 0; y < height; y += PixelConvertor::ypp)
	{
		for (u32 x = 0; x < width; x += PixelConvertor::xpp)
		{
			const u8* p = &p_in[(twop(x, y, bcx, bcy) / divider) << 3];
			PixelConvertor::Convert(pb, p);

			pb->rmovex(PixelConvertor::xpp);
		}
		pb->rmovey(PixelConvertor::ypp);
	}
}
template<typename PixelConvertor>
void texture_VQ(PixelBuffer<typename PixelConvertor::unpacked_type>* pb, const u8* p_in, u32 width, u32 height)
{
	pb->amove(0, 0);

	const u32 divider = PixelConvertor::xpp * PixelConvertor::ypp;
	const u32 bcx = bitscanrev(width);
	const u32 bcy = bitscanrev(height);

	for (u32 y = 0; y < height; y += PixelConvertor::ypp)
	{
		for (u32 x = 0; x < width; x += PixelConvertor::xpp)
		{
			u8 p = p_in[twop(x, y, bcx, bcy) / divider];
			PixelConvertor::Convert(pb, &vq_codebook[p * 8]);

			pb->rmovex(PixelConvertor::xpp);
		}
		pb->rmovey(PixelConvertor::ypp);
	}
}
}

namespace candidate {
u32 palette16_ram[1024],palette32_ram[1024],palette_index,detwiddle[2][11][1024];const u8* vq_codebook;
static u32 twiddle_slow(u32 x, u32 y, u32 x_sz, u32 y_sz)
{
	u32 rv = 0; // low 2 bits are directly passed  -> needs some misc stuff to work.However
				// Pvr internally maps the 64b banks "as if" they were twiddled

	u32 sh = 0;
	x_sz >>= 1;
	y_sz >>= 1;
	while (x_sz != 0 || y_sz != 0)
	{
		if (y_sz != 0)
		{
			u32 temp = y & 1;
			rv |= temp << sh;

			y_sz >>= 1;
			y >>= 1;
			sh++;
		}
		if (x_sz != 0)
		{
			u32 temp = x & 1;
			rv |= temp << sh;

			x_sz >>= 1;
			x >>= 1;
			sh++;
		}
	}
	return rv;
}
static u32 twop(u32 x, u32 y, u32 bcx, u32 bcy) {
	return detwiddle[0][bcy][x] + detwiddle[1][bcx][y];
}
template<typename Pixel>
struct UnpackerPalToRgb {
	using unpacked_type = Pixel;
	const u32* palette = sizeof(Pixel) == 2 ? &palette16_ram[palette_index] : &palette32_ram[palette_index];
	Pixel unpack(u8 col) const
	{
		return palette[col];
	}
};
template<typename Unpacker>
struct ConvertTwiddlePal4
{
	using unpacked_type = typename Unpacker::unpacked_type;
	Unpacker unpacker;
	static constexpr u32 xpp = 4;
	static constexpr u32 ypp = 4;
	void Convert(PixelBuffer<unpacked_type> *pb, const u8 *data)
	{
		const u8 *p_in = data;

		pb->prel(0, 0, unpacker.unpack(p_in[0] & 0xF));
		pb->prel(0, 1, unpacker.unpack((p_in[0] >> 4) & 0xF)); p_in++;
		pb->prel(1, 0, unpacker.unpack(p_in[0] & 0xF));
		pb->prel(1, 1, unpacker.unpack((p_in[0] >> 4) & 0xF)); p_in++;

		pb->prel(0, 2, unpacker.unpack(p_in[0] & 0xF));
		pb->prel(0, 3, unpacker.unpack((p_in[0] >> 4) & 0xF)); p_in++;
		pb->prel(1, 2, unpacker.unpack(p_in[0] & 0xF));
		pb->prel(1, 3, unpacker.unpack((p_in[0] >> 4) & 0xF)); p_in++;

		pb->prel(2, 0, unpacker.unpack(p_in[0] & 0xF));
		pb->prel(2, 1, unpacker.unpack((p_in[0] >> 4) & 0xF)); p_in++;
		pb->prel(3, 0, unpacker.unpack(p_in[0] & 0xF));
		pb->prel(3, 1, unpacker.unpack((p_in[0] >> 4) & 0xF)); p_in++;

		pb->prel(2, 2, unpacker.unpack(p_in[0] & 0xF));
		pb->prel(2, 3, unpacker.unpack((p_in[0] >> 4) & 0xF)); p_in++;
		pb->prel(3, 2, unpacker.unpack(p_in[0] & 0xF));
		pb->prel(3, 3, unpacker.unpack((p_in[0] >> 4) & 0xF)); p_in++;
	}
};
template<typename Unpacker>
struct ConvertTwiddlePal8
{
	using unpacked_type = typename Unpacker::unpacked_type;
	Unpacker unpacker;
	static constexpr u32 xpp = 2;
	static constexpr u32 ypp = 4;
	void Convert(PixelBuffer<unpacked_type> *pb, const u8 *data)
	{
		const u8* p_in = (const u8 *)data;

		pb->prel(0, 0, unpacker.unpack(p_in[0])); p_in++;
		pb->prel(0, 1, unpacker.unpack(p_in[0])); p_in++;
		pb->prel(1, 0, unpacker.unpack(p_in[0])); p_in++;
		pb->prel(1, 1, unpacker.unpack(p_in[0])); p_in++;

		pb->prel(0, 2, unpacker.unpack(p_in[0])); p_in++;
		pb->prel(0, 3, unpacker.unpack(p_in[0])); p_in++;
		pb->prel(1, 2, unpacker.unpack(p_in[0])); p_in++;
		pb->prel(1, 3, unpacker.unpack(p_in[0])); p_in++;
	}
};
template<typename PixelConvertor>
void texture_TW(PixelBuffer<typename PixelConvertor::unpacked_type>* pb, const u8* p_in, u32 width, u32 height)
{
	pb->amove(0, 0);
	PixelConvertor converter;

	const u32 divider = PixelConvertor::xpp * PixelConvertor::ypp;

	const u32 bcx = bitscanrev(width);
	const u32 bcy = bitscanrev(height);

	for (u32 y = 0; y < height; y += PixelConvertor::ypp)
	{
		for (u32 x = 0; x < width; x += PixelConvertor::xpp)
		{
			const u8* p = &p_in[(twop(x, y, bcx, bcy) / divider) << 3];
			converter.Convert(pb, p);

			pb->rmovex(PixelConvertor::xpp);
		}
		pb->rmovey(PixelConvertor::ypp);
	}
}
template<typename PixelConvertor>
void texture_VQ(PixelBuffer<typename PixelConvertor::unpacked_type>* pb, const u8* p_in, u32 width, u32 height)
{
	pb->amove(0, 0);
	PixelConvertor converter;

	const u32 divider = PixelConvertor::xpp * PixelConvertor::ypp;
	const u32 bcx = bitscanrev(width);
	const u32 bcy = bitscanrev(height);

	for (u32 y = 0; y < height; y += PixelConvertor::ypp)
	{
		for (u32 x = 0; x < width; x += PixelConvertor::xpp)
		{
			u8 p = p_in[twop(x, y, bcx, bcy) / divider];
			converter.Convert(pb, &vq_codebook[p * 8]);

			pb->rmovex(PixelConvertor::xpp);
		}
		pb->rmovey(PixelConvertor::ypp);
	}
}
}

static unsigned cases=0;
static void checkBytes(const void* a,const void* b,size_t n){if(memcmp(a,b,n)){printf("FAIL case %u\n",cases);exit(1);}++cases;}
template<typename T> void check(unsigned w,unsigned h,bool pal4,bool vq,bool indexed,unsigned bank,const std::vector<u8>& data){
 PixelBuffer<T> a,b;a.init(w,h);b.init(w,h);memset(a.data(),0xa5,w*h*sizeof(T));memset(b.data(),0xa5,w*h*sizeof(T));
 reference::palette_index=candidate::palette_index=bank;
 #define RUN(NS,PB,FMT,UNP) if(vq) NS::texture_VQ<NS::FMT<UNP>>(&PB,data.data(),w,h);else NS::texture_TW<NS::FMT<UNP>>(&PB,data.data(),w,h)
 if(pal4){if(indexed){RUN(reference,a,ConvertTwiddlePal4,UnpackerNop<T>);RUN(candidate,b,ConvertTwiddlePal4,UnpackerNop<T>);}else{RUN(reference,a,ConvertTwiddlePal4,reference::UnpackerPalToRgb<T>);RUN(candidate,b,ConvertTwiddlePal4,candidate::UnpackerPalToRgb<T>);}}
 else{if(indexed){RUN(reference,a,ConvertTwiddlePal8,UnpackerNop<T>);RUN(candidate,b,ConvertTwiddlePal8,UnpackerNop<T>);}else{RUN(reference,a,ConvertTwiddlePal8,reference::UnpackerPalToRgb<T>);RUN(candidate,b,ConvertTwiddlePal8,candidate::UnpackerPalToRgb<T>);}}
 checkBytes(a.data(),b.data(),w*h*sizeof(T));
 #undef RUN
}
int main(){
 unsigned rng=11;auto next=[&](){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;};
 std::vector<u8> data(1024*1024*2+64),codebook(2048);for(auto& x:data)x=next();for(auto& x:codebook)x=next();reference::vq_codebook=candidate::vq_codebook=codebook.data();
 for(unsigned i=0;i<1024;i++){reference::palette16_ram[i]=candidate::palette16_ram[i]=next();reference::palette32_ram[i]=candidate::palette32_ram[i]=next();}
 for(unsigned s=0;s<11;s++)for(unsigned i=0;i<1024;i++){
  reference::detwiddle[0][s][i]=candidate::detwiddle[0][s][i]=reference::twiddle_slow(i,0,1024,1u<<s);
  reference::detwiddle[1][s][i]=candidate::detwiddle[1][s][i]=reference::twiddle_slow(0,i,1u<<s,1024);
 }
 for(unsigned w=8;w<=1024;w*=2)for(unsigned h=8;h<=1024;h*=2)for(bool p4:{false,true})for(bool vq:{false,true})for(unsigned bank:{0u,256u,768u}){
  check<u16>(w,h,p4,vq,false,bank,data);check<u32>(w,h,p4,vq,false,bank,data);check<u8>(w,h,p4,vq,true,bank,data);
 }
 printf("PASS %u byte-identical palette conversion cases (TW/VQ, PAL4/8, RGB16/32/index8, square/rectangular, multiple banks)\n",cases);
}
