/*
	Copyright 2021 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "dx11_renderer.h"
#include "hosted_startup.h"
#include "hw/pvr/ta_parse_profile.h"
#include "hw/pvr/ta_triangle_sort.h"
#include "dx11context.h"
#include "hw/pvr/ta.h"
#include "hw/pvr/pvr_mem.h"
#include "ui/gui.h"
#include "rend/sorter.h"

#include <memory>
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
#include "ui/mainui.h"
#include "rend/frame_delivery_trace.h"
#include <chrono>
#include <cmath>
#include <map>
#include <cstdlib>
#include <fstream>
#include <locale>

// Opt-in, two-frame geometry evidence. These records have polygon IDs, not guest object IDs.
// Bounded, opt-in gameplay measurements; no per-frame file I/O.
static const char* perfOutput = [] {
	const char* output = std::getenv("FLYCAST_DX11_PERF_OUTPUT");
	return output && *output ? output : "performance.csv";
}();
static unsigned stateSelections = 0, temporaryOwners = 0;
static ULONG64 perfCyclesStart = 0;
static double parseUs = 0;
static ULONG64 parseCycles = 0;
static std::chrono::steady_clock::time_point perfStart, previousRender;
static void recordRenderPerformance(const rend_context& context)
{
	if (!perfOutput || !*perfOutput) return;
	static bool armed=false;
	if (!armed) {
		std::ifstream arm(std::string(perfOutput) + ".arm");
		if (!arm) return;
		armed=true;
		pvr::parserProfileEnabled=true;
		frame_delivery_trace::armed.store(true,std::memory_order_relaxed);
	}
	static bool complete=false;
	if(complete) return;
	using Clock = std::chrono::steady_clock;
	const auto end = Clock::now();
	ULONG64 cyclesEnd = 0;
	QueryThreadCycleTime(GetCurrentThread(), &cyclesEnd);
	static HANDLE emulationThread = nullptr;
	if (!emulationThread) {
		const auto id = frame_delivery_trace::emulationThreadId.load(std::memory_order_relaxed);
		if (id) emulationThread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, id);
	}
	ULONG64 emulationCycles=0, processCycles=0;
	if (emulationThread) QueryThreadCycleTime(emulationThread, &emulationCycles);
	QueryProcessCycleTime(GetCurrentProcess(), &processCycles);
	FILETIME created{}, exited{}, kernel{}, user{};
	auto cpuTime = [&](bool process) {
		if (process) GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
		else if (emulationThread) GetThreadTimes(emulationThread, &created, &exited, &kernel, &user);
		return ((static_cast<ULONG64>(kernel.dwHighDateTime) << 32) | kernel.dwLowDateTime)
			+ ((static_cast<ULONG64>(user.dwHighDateTime) << 32) | user.dwLowDateTime);
	};
	const auto emulationCpu=cpuTime(false), processCpu=cpuTime(true);
	static ULONG64 previousEmulationCycles=0,previousProcessCycles=0,previousEmulationCpu=0,previousProcessCpu=0;
	const auto linkWait=frame_delivery_trace::linkWaitNs.load(std::memory_order_relaxed);
	const auto linkCycles=frame_delivery_trace::linkWaitCycles.load(std::memory_order_relaxed);
	const auto linkPolls=frame_delivery_trace::linkPolls.load(std::memory_order_relaxed);
	const auto linkTimeouts=frame_delivery_trace::linkTimeouts.load(std::memory_order_relaxed);
	static uint64_t previousLinkWait=0,previousLinkCycles=0,previousLinkPolls=0,previousLinkTimeouts=0;
	struct Sample { double wallUs, intervalUs; unsigned requests, calls; ULONG64 cycles; double parseUs; ULONG64 parseCycles; std::size_t vertices,indices; pvr::DepthSortStats sort; pvr::ParserProfile parser; ULONG64 emulationCycles,processCycles,emulationCpu,processCpu; uint64_t linkWait,linkCycles,linkPolls,linkTimeouts; };
	static Sample samples[600];
	static unsigned frames = 0;
	const double wallUs = std::chrono::duration<double, std::micro>(end - perfStart).count();
	const double intervalUs = std::chrono::duration<double, std::micro>(perfStart - previousRender).count();
	previousRender = perfStart;
	if (frames >= 120 && frames < 720)
		samples[frames - 120] = {wallUs, intervalUs, stateSelections, temporaryOwners, cyclesEnd - perfCyclesStart, parseUs, parseCycles, context.verts.size(), context.idx.size(), pvr::depthSortStats, pvr::parserProfile, emulationCycles-previousEmulationCycles,processCycles-previousProcessCycles,emulationCpu-previousEmulationCpu,processCpu-previousProcessCpu,linkWait-previousLinkWait,linkCycles-previousLinkCycles,linkPolls-previousLinkPolls,linkTimeouts-previousLinkTimeouts};
	previousLinkWait=linkWait; previousLinkCycles=linkCycles; previousLinkPolls=linkPolls; previousLinkTimeouts=linkTimeouts;
	previousEmulationCycles=emulationCycles; previousProcessCycles=processCycles;
	previousEmulationCpu=emulationCpu; previousProcessCpu=processCpu;
	if (++frames == 720) {
		std::ofstream out(perfOutput);
		out.imbue(std::locale::classic());
		out << "sample,render_wall_us,render_interval_us,state_selections,temporary_owners,render_thread_cycles,parse_us,parse_thread_cycles,vertices,indices,sort_ns,triangles,standard_sorts,radix_sorts,scratch_growths,reset_ns,decode_ns,post_ns,region_ns,texture_sample_ns,texture_calls,texture_samples,texture_updates,command_bytes,update_ns,update_setup_ns,convert_ns,protect_ns,upload_ns,palette_convert_ns,direct_convert_ns,converted_pixels,emulation_cycles,process_cycles,emulation_cpu_100ns,process_cpu_100ns,link_wait_ns,link_wait_cycles,link_polls,link_timeouts\n";
		for (unsigned i = 0; i < 600; ++i)
			out << i << ',' << samples[i].wallUs << ',' << samples[i].intervalUs << ',' << samples[i].requests << ',' << samples[i].calls << ',' << samples[i].cycles << ',' << samples[i].parseUs << ',' << samples[i].parseCycles << ',' << samples[i].vertices << ',' << samples[i].indices << ',' << samples[i].sort.ns << ',' << samples[i].sort.triangles << ',' << samples[i].sort.standardCalls << ',' << samples[i].sort.radixCalls << ',' << samples[i].sort.scratchGrowths << ',' << samples[i].parser.resetNs << ',' << samples[i].parser.decodeNs << ',' << samples[i].parser.postNs << ',' << samples[i].parser.regionNs << ',' << samples[i].parser.textureNs << ',' << samples[i].parser.textureCalls << ',' << samples[i].parser.textureSamples << ',' << samples[i].parser.textureUpdates << ',' << samples[i].parser.commandBytes << ',' << samples[i].parser.updateNs << ',' << samples[i].parser.setupNs << ',' << samples[i].parser.convertNs << ',' << samples[i].parser.protectNs << ',' << samples[i].parser.uploadNs << ',' << samples[i].parser.paletteConvertNs << ',' << samples[i].parser.directConvertNs << ',' << samples[i].parser.convertedPixels << ',' << samples[i].emulationCycles << ',' << samples[i].processCycles << ',' << samples[i].emulationCpu << ',' << samples[i].processCpu << ',' << samples[i].linkWait << ',' << samples[i].linkCycles << ',' << samples[i].linkPolls << ',' << samples[i].linkTimeouts << '\n';
		if(emulationThread) { CloseHandle(emulationThread); emulationThread=nullptr; }
		complete=true;
	}
}
static std::ofstream vootDrawTrace;
static std::string vootGeometryPrefix;
static void traceVootGeometry(TA_context* ctx)
{
	vootDrawTrace.close();
	vootGeometryPrefix.clear();
	static unsigned captures = 0;
	const char* prefix = std::getenv("FLYCAST_VOOT_GEOMETRY_TRACE");
	const char* requestedFrames = std::getenv("FLYCAST_VOOT_GEOMETRY_TRACE_FRAMES");
	// HUD ownership research must include small contexts omitted by world-only traces.
	const char* hudTraceOption = std::getenv("FLYCAST_VOOT_HUD_TRACE");
	const bool hudTrace = hudTraceOption && std::strcmp(hudTraceOption, "1") == 0;
	const unsigned limit = requestedFrames ? std::max(1, std::min(120, std::atoi(requestedFrames))) : 2;
	if (!prefix || !*prefix || captures >= limit || ctx->rend.isRTT
			|| settings.content.gameId != "VIRTUAL-ON ORATORIO TANGRAM"
			|| (!hudTrace && ctx->rend.verts.size() < 1000))
		return;
	if (limit > 2)
	{
		// Camera-turn traces are externally armed and bounded; ordinary two-frame captures stay unchanged.
		if (!std::ifstream(std::string(prefix) + ".arm")) return;
		static unsigned eligibleFrames = 0;
		const char* requestedStride = std::getenv("FLYCAST_VOOT_GEOMETRY_TRACE_STRIDE");
		const unsigned stride = requestedStride ? std::max(1, std::min(600, std::atoi(requestedStride))) : (hudTrace ? 1 : 6);
		if (eligibleFrames++ % stride != 0) return;
	}
	vootGeometryPrefix = std::string(prefix) + "." + std::to_string(captures++);
	std::ofstream stamp(vootGeometryPrefix + ".time.txt");
	stamp.imbue(std::locale::classic());
	stamp << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() << '\n';
	unsigned child = 0;
	for (TA_context* current = ctx; current; current = current->nextContext)
	{
		std::ofstream raw(vootGeometryPrefix + ".ta" + std::to_string(child++) + ".bin", std::ios::binary);
		raw.write(reinterpret_cast<const char*>(current->getTADataBegin()),
				current->getTADataEnd() - current->getTADataBegin());
		if (!raw) ERROR_LOG(RENDERER, "Voot geometry TA evidence write failed");
	}
	std::ofstream vertices(vootGeometryPrefix + ".vertices.csv");
	vertices.imbue(std::locale::classic());
	vertices.precision(9);
	vertices << "id,x,y,z,u,v,b,g,r,a\n";
	for (size_t i = 0; i < ctx->rend.verts.size(); i++)
	{
		const Vertex& v = ctx->rend.verts[i];
		vertices << i << ',' << v.x << ',' << v.y << ',' << v.z << ',' << v.u << ',' << v.v;
		for (u8 channel : v.col) vertices << ',' << unsigned(channel);
		vertices << '\n';
	}
	std::ofstream polygons(vootGeometryPrefix + ".polygons.csv");
	polygons.imbue(std::locale::classic());
	polygons << "list,id,first,count,pcw,isp,tsp,tcw,tileclip\n";
	auto list = [&](const char* name, const std::vector<PolyParam>& params) {
		for (size_t i = 0; i < params.size(); i++) {
			const PolyParam& p = params[i];
			polygons << name << ',' << i << ',' << p.first << ',' << p.count << ','
					<< p.pcw.full << ',' << p.isp.full << ',' << p.tsp.full << ',' << p.tcw.full << ',' << p.tileclip << '\n';
		}
	};
	list("op", ctx->rend.global_param_op);
	list("pt", ctx->rend.global_param_pt);
	list("tr", ctx->rend.global_param_tr);
	if (hudTrace)
	{
		// Sorted translucent parameters retain vertex ranges; record their GPU index ranges separately.
		std::ofstream sorted(vootGeometryPrefix + ".sorted.csv");
		sorted.imbue(std::locale::classic());
		sorted << "poly,first,count\n";
		for (const SortedTriangle& triangle : ctx->rend.sortedTriangles)
			sorted << triangle.polyIndex << ',' << triangle.first << ',' << triangle.count << '\n';
		if (!sorted) ERROR_LOG(RENDERER, "Voot HUD sorted-range evidence write failed");
	}
	if (!vertices || !polygons) ERROR_LOG(RENDERER, "Voot geometry parsed evidence write failed");
	vootDrawTrace.clear();
	vootDrawTrace.open(vootGeometryPrefix + ".draws.csv");
	vootDrawTrace.imbue(std::locale::classic());
	vootDrawTrace << "list,first,count,cull,depth,tileclip\n";
	NOTICE_LOG(RENDERER, "Voot geometry evidence captured: %s", vootGeometryPrefix.c_str());
}
#endif

void os_VideoRoutingTermDX();

const D3D11_INPUT_ELEMENT_DESC MainLayout[]
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(Vertex, x), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, (UINT)offsetof(Vertex, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, (UINT)offsetof(Vertex, spc), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(Vertex, u),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(Vertex, nx),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
};
const D3D11_INPUT_ELEMENT_DESC ModVolLayout[]
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(ModTriangle, x0), D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

bool DX11Renderer::Init()
{
	NOTICE_LOG(RENDERER, "DX11 renderer initializing");
	device = DX11Context::Instance()->getDevice();
	deviceContext = DX11Context::Instance()->getDeviceContext();
	if (!device || !deviceContext)
	{
		WARN_LOG(RENDERER, "Null device or device context. Aborting");
		return false;
	}

	shaders = &DX11Context::Instance()->getShaders();
	samplers = &DX11Context::Instance()->getSamplers();
	bool success = (bool)shaders->getVertexShader(true, true);
	ComPtr<ID3DBlob> blob = shaders->getVertexShaderBlob();
	success = success && SUCCEEDED(device->CreateInputLayout(MainLayout, std::size(MainLayout), blob->GetBufferPointer(), blob->GetBufferSize(), &mainInputLayout.get()));
	blob = shaders->getMVVertexShaderBlob();
	success = success && SUCCEEDED(device->CreateInputLayout(ModVolLayout, std::size(ModVolLayout), blob->GetBufferPointer(), blob->GetBufferSize(), &modVolInputLayout.get()));

	// Constants buffers
	{
		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = sizeof(VertexConstants);
		desc.ByteWidth = (((desc.ByteWidth - 1) >> 4) + 1) << 4;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		success = success && SUCCEEDED(device->CreateBuffer(&desc, nullptr, &vtxConstants.get()));

		desc.ByteWidth = sizeof(PixelConstants);
		desc.ByteWidth = (((desc.ByteWidth - 1) >> 4) + 1) << 4;
		success = success && SUCCEEDED(device->CreateBuffer(&desc, nullptr, &pxlConstants.get()));

		desc.ByteWidth = sizeof(PixelPolyConstants);
		desc.ByteWidth = (((desc.ByteWidth - 1) >> 4) + 1) << 4;
		success = success && SUCCEEDED(device->CreateBuffer(&desc, nullptr, &pxlPolyConstants.get()));
	}

	// Rasterizer state
	{
		D3D11_RASTERIZER_DESC desc{};
		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = D3D11_CULL_NONE;
		desc.FrontCounterClockwise = true;
		desc.ScissorEnable = true;
		desc.DepthClipEnable = false;
		device->CreateRasterizerState(&desc, &rasterCullNone.get());
		desc.CullMode = D3D11_CULL_FRONT;
		device->CreateRasterizerState(&desc, &rasterCullFront.get());
		desc.CullMode = D3D11_CULL_BACK;
		device->CreateRasterizerState(&desc, &rasterCullBack.get());
	}
	// Palette texture
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = 32;
		desc.Height = 32;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.MipLevels = 1;
		device->CreateTexture2D(&desc, nullptr, &paletteTexture.get());

		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = desc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(paletteTexture, &viewDesc, &paletteTextureView.get());
	}
	// Fog texture
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = 128;
		desc.Height = 2;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Format = DXGI_FORMAT_A8_UNORM;
		desc.MipLevels = 1;
		device->CreateTexture2D(&desc, nullptr, &fogTexture.get());

		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = desc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(fogTexture, &viewDesc, &fogTextureView.get());
	}
	// White texture
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = 8;
		desc.Height = 8;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.MipLevels = 1;
		device->CreateTexture2D(&desc, nullptr, &whiteTexture.get());

		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = desc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(whiteTexture, &viewDesc, &whiteTextureView.get());

		u32 texData[8 * 8];
		memset(texData, 0xff, sizeof(texData));
		deviceContext->UpdateSubresource(whiteTexture, 0, nullptr, texData, 8 * sizeof(u32), 8 * sizeof(u32) * 8);
	}

	quad = std::make_unique<Quad>();
	quad->init(device, deviceContext, shaders);
	n2Helper.init(device, deviceContext);

	updateFogTable = true;

	if (!success)
	{
		WARN_LOG(RENDERER, "DirectX 11 renderer initialization failed");
		Term();
	}
	frameRendered = false;

	return success;
}

void DX11Renderer::Term()
{
	NOTICE_LOG(RENDERER, "DX11 renderer terminating");
#ifdef VIDEO_ROUTING
	os_VideoRoutingTermDX();
#endif
	n2Helper.term();
	vtxConstants.reset();
	pxlConstants.reset();
	fbTex.reset();
	fbTextureView.reset();
	fbRenderTarget.reset();
	fbScaledRenderTarget.reset();
	fbScaledTextureView.reset();
	fbScaledTexture.reset();
	quad.reset();
	deviceContext.reset();
	device.reset();
	vrStagingTexture.reset();
	vrStagingTextureSRV.reset();
	vrScaledTexture.reset();
	vrScaledRenderTarget.reset();
}

void DX11Renderer::createDepthTexAndView(ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11DepthStencilView>& view, int width, int height, DXGI_FORMAT format, UINT bindFlags)
{
	view.reset();
	texture.reset();
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | bindFlags;
	HRESULT hr = device->CreateTexture2D(&desc, nullptr, &texture.get());
	if (FAILED(hr))
		WARN_LOG(RENDERER, "Depth/stencil creation failed");

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc{};
	viewDesc.Format = format == DXGI_FORMAT_R32G8X24_TYPELESS ? DXGI_FORMAT_D32_FLOAT_S8X24_UINT : DXGI_FORMAT_D24_UNORM_S8_UINT;
	viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	hr = device->CreateDepthStencilView(texture, &viewDesc, &view.get());
	if (FAILED(hr))
		WARN_LOG(RENDERER, "Depth/stencil view creation failed");
}

void DX11Renderer::createTexAndRenderTarget(ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11RenderTargetView>& renderTarget, int width, int height)
{
	texture.reset();
	renderTarget.reset();
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.MipLevels = 1;

	HRESULT hr = device->CreateTexture2D(&desc, nullptr, &texture.get());
	if (FAILED(hr))
	{
		WARN_LOG(RENDERER, "Framebuffer texture creation failed");
		return;
	}

	hr = device->CreateRenderTargetView(texture, nullptr, &renderTarget.get());
	if (FAILED(hr))
	{
		WARN_LOG(RENDERER, "Framebuffer render target creation failed");
		return;
	}
	FLOAT black[4] = { 0.f, 0.f, 0.f, 0.f };
	deviceContext->ClearRenderTargetView(renderTarget, black);
}

void DX11Renderer::resize(int w, int h)
{
	if (width == (u32)w && height == (u32)h)
		return;
	width = w;
	height = h;

	// Create framebuffer texture
	{
		fbTextureView.reset();
		createTexAndRenderTarget(fbTex, fbRenderTarget, width, height);

		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(fbTex, &viewDesc, &fbTextureView.get());
	}

	// Create depth stencil texture
	createDepthTexAndView(depthTex, depthTexView, width, height);
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
	if (fbTex)
	{
		D3D11_TEXTURE2D_DESC actual{};
		fbTex->GetDesc(&actual);
		NOTICE_LOG(RENDERER, "RenderTarget backend=DX11 requested=%ux%u actual=%ux%u output=%dx%d resolution=%d wide=%d super=%d",
			width, height, actual.Width, actual.Height, settings.display.width, settings.display.height,
			config::RenderResolution.get(), config::Widescreen.get(), config::SuperWidescreen.get());
		// Query the actual output texture; a requested window size is not allocation proof.
		auto& outputView = DX11Context::Instance()->getRenderTarget();
		if (outputView)
		{
			ComPtr<ID3D11Resource> resource;
			outputView->GetResource(&resource.get());
			ComPtr<ID3D11Texture2D> outputTexture;
			if (resource && SUCCEEDED(resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&outputTexture.get())))
			{
				D3D11_TEXTURE2D_DESC output{};
				outputTexture->GetDesc(&output);
				NOTICE_LOG(RENDERER, "OutputBuffer backend=DX11 actual=%ux%u", output.Width, output.Height);
			}
		}
	}
#endif

	frameRendered = false;
	frameRenderedOnce = false;
}

bool DX11Renderer::ensureBufferSize(ComPtr<ID3D11Buffer>& buffer, D3D11_BIND_FLAG bind, u32& currentSize, u32 minSize)
{
	if (minSize <= currentSize && buffer)
		return true;
	if (currentSize == 0)
		currentSize = minSize;
	else
		while (currentSize < minSize)
			currentSize *= 2;
	buffer.reset();
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = currentSize;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = bind;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(device->CreateBuffer(&desc, nullptr, &buffer.get()));
}

BaseTextureCacheData *DX11Renderer::GetTexture(TSP tsp, TCW tcw, int area)
{
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
	std::uint64_t textureStart=0;
	if(pvr::parserProfileEnabled && ((pvr::parserProfile.textureCalls++ & 63)==pvr::textureSamplePhase)) {
		++pvr::parserProfile.textureSamples;
		textureStart=pvr::parserTicks();
	}
#endif
	//lookup texture
	DX11Texture* tf = texCache.getTextureCacheData(tsp, tcw, area);

	//update if needed
	if (tf->NeedsUpdate())
	{
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
		if(pvr::parserProfileEnabled) ++pvr::parserProfile.textureUpdates;
#endif
		#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
		const auto updateStart=pvr::parserProfileEnabled?pvr::parserTicks():0;
#endif
		if (!tf->Update())
			tf = nullptr;
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
		if(updateStart) pvr::parserProfile.updateNs+=pvr::parserTicks()-updateStart;
#endif
	}
	else if (tf->IsCustomTextureAvailable())
	{
		texCache.DeleteLater(tf->texture);
		tf->texture.reset();
		// FIXME textureView
		tf->loadCustomTexture();
	}
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
	if(textureStart) pvr::parserProfile.textureNs+=pvr::parserTicks()-textureStart;
#endif
	return tf;
}

void DX11Renderer::Process(TA_context* ctx)
{
	rendContext = &ctx->rend;
	if (resetTextureCache) {
		texCache.Clear();
		resetTextureCache = false;
	}
	texCache.Cleanup();
	if (!ctx->rend.isRTT && ctx->rend.swapInterval > 0)
		DX11Context::Instance()->setSwapInterval(ctx->rend.swapInterval);

#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
	pvr::depthSortStats = {};
	pvr::parserProfile = {};
	pvr::textureSamplePhase = (pvr::textureSamplePhase + 1) & 63;
	const auto parseStart = perfOutput && *perfOutput ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
	ULONG64 parseBeginCycles = 0;
	if (perfOutput && *perfOutput) QueryThreadCycleTime(GetCurrentThread(), &parseBeginCycles);
#endif
	ta_parse(ctx, true);
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
	if (perfOutput && *perfOutput) {
		ULONG64 endCycles;
		QueryThreadCycleTime(GetCurrentThread(), &endCycles);
		parseUs = std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now() - parseStart).count();
		parseCycles = endCycles - parseBeginCycles;
	}
#endif
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
	traceVootGeometry(ctx);
#endif
}

void DX11Renderer::resetContextState()
{
	// Reset device context state. Very much needed for libretro where current state is unknown.
	deviceContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 0, nullptr, nullptr);
	deviceContext->PSSetShader(nullptr, nullptr, 0);
	deviceContext->GSSetShader(nullptr, nullptr, 0);
	deviceContext->HSSetShader(nullptr, nullptr, 0);
	deviceContext->DSSetShader(nullptr, nullptr, 0);
	deviceContext->CSSetShader(nullptr, nullptr, 0);
	deviceContext->VSSetShader(nullptr, nullptr, 0);
	ID3D11ShaderResourceView *nullview[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] {};
	deviceContext->CSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullview);
	deviceContext->DSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullview);
	deviceContext->GSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullview);
	deviceContext->HSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullview);
	deviceContext->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullview);
	deviceContext->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullview);
	deviceContext->SetPredication(nullptr, false);
	deviceContext->SOSetTargets(0, nullptr, nullptr);
}

void DX11Renderer::configVertexShader()
{
	matrices.CalcMatrices(rendContext, rendContext->framebufferWidth, rendContext->framebufferHeight);
	setBaseScissor();

	if (rendContext->isRTT)
	{
		prepareRttRenderTarget(rendContext->fb_W_SOF1 & VRAM_MASK);
	}
	else
	{
		D3D11_VIEWPORT vp{};
		vp.Width = (FLOAT)rendContext->framebufferWidth;
		vp.Height = (FLOAT)rendContext->framebufferHeight;
		vp.MinDepth = 0.f;
		vp.MaxDepth = 1.f;
		deviceContext->RSSetViewports(1, &vp);
	}
	VertexConstants constant{};
	memcpy(&constant.transMatrix, &matrices.GetNormalMatrix(), sizeof(constant.transMatrix));
	constant.leftPlane[0] = 1;
	constant.leftPlane[3] = 1;
	constant.rightPlane[0] = -1;
	constant.rightPlane[3] = 1;
	constant.topPlane[1] = 1;
	constant.topPlane[3] = 1;
	constant.bottomPlane[1] = -1;
	constant.bottomPlane[3] = 1;
	D3D11_MAPPED_SUBRESOURCE mappedSubres;
	deviceContext->Map(vtxConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
	memcpy(mappedSubres.pData, &constant, sizeof(constant));
	deviceContext->Unmap(vtxConstants, 0);
	deviceContext->VSSetConstantBuffers(0, 1, &vtxConstants.get());
}

void DX11Renderer::uploadGeometryBuffers()
{
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
	if (!vootGeometryPrefix.empty())
	{
		std::ofstream indices(vootGeometryPrefix + ".indices.csv");
		indices.imbue(std::locale::classic());
		indices << "offset,vertex\n";
		for (size_t i = 0; i < rendContext->idx.size(); i++) indices << i << ',' << rendContext->idx[i] << '\n';
		std::ofstream matrix(vootGeometryPrefix + ".matrix.csv");
		matrix.imbue(std::locale::classic());
		matrix.precision(9);
		for (int r = 0; r < 4; r++) {
			for (int c = 0; c < 4; c++) matrix << (c ? "," : "") << matrices.GetViewportMatrix()[c][r];
			matrix << '\n';
		}
		if (!indices || !matrix) ERROR_LOG(RENDERER, "Voot geometry upload evidence write failed");
	}
#endif
	setFirstProvokingVertex(*rendContext);

	vootTitleBackground = nullptr;
	vootTitleSidesDrawn = false;
	Vertex titleSides[8];
	if (vootWideClip && !config::CustomTextures)
		for (const PolyParam& poly : rendContext->global_param_tr)
		{
			if (poly.pcw.full != 0x828e0008 || poly.isp.full != 0x8a000000 || poly.count != 4 || poly.first + 4 > rendContext->verts.size()
					|| poly.tcw.full != 0x36157900 || poly.tsp.full != 0x94b0047e || poly.tileclip != 0x201a04c0
					|| !poly.texture || !matrices.getVootTitleSideFill(&rendContext->verts[poly.first], titleSides))
				continue;
			poly.texture->ComputeHash();
			// Content and palette identity prevent reuse of this VRAM slot by another screen.
			if (poly.texture->texture_hash != 0xcc95911c)
				continue;
			vootTitleBackground = &poly;
			vootTitleFirstVertex = rendContext->verts.size();
			break;
		}
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
	static bool previousTitle = false;
	static unsigned titleTransitions = 0;
	if (previousTitle != (vootTitleBackground != nullptr) && titleTransitions++ < 16)
		NOTICE_LOG(RENDERER, "VootTitleSides active=%d target=%ux%u", vootTitleBackground != nullptr, rendContext->framebufferWidth, rendContext->framebufferHeight);
	previousTitle = vootTitleBackground != nullptr;
#endif
	const size_t originalSize = rendContext->verts.size() * sizeof(decltype(*rendContext->verts.data()));
	size_t size = originalSize + (vootTitleBackground ? sizeof(titleSides) : 0);
	bool rc = ensureBufferSize(vertexBuffer, D3D11_BIND_VERTEX_BUFFER, vertexBufferSize, size);
	verify(rc);
	D3D11_MAPPED_SUBRESOURCE mappedSubres;
	deviceContext->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
	memcpy(mappedSubres.pData, rendContext->verts.data(), originalSize);
	if (vootTitleBackground)
		memcpy(static_cast<u8*>(mappedSubres.pData) + originalSize, titleSides, sizeof(titleSides));
	if (vootWideClip && rendContext->verts.size() >= 4 && !rendContext->global_param_op.empty())
	{
		const auto& background = rendContext->global_param_op.front();
		// FillBGP owns the first four vertices. Extend its flat clear plane only in
		// the upload copy; guest geometry, textured art and the TA context stay intact.
		if (background.first == 0 && background.count == 4 && !background.pcw.Texture)
		{
			Vertex vertices[4];
			memcpy(vertices, rendContext->verts.data(), sizeof(vertices));
			if (matrices.expandBackground(vertices))
			{
				memcpy(mappedSubres.pData, vertices, sizeof(vertices));
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
				static bool reported = false;
				if (!reported)
				{
					NOTICE_LOG(RENDERER, "VootBackground target=%ux%u x=%g:%g uniform=1 textured=0",
						rendContext->framebufferWidth, rendContext->framebufferHeight, vertices[0].x, vertices[1].x);
					reported = true;
				}
#endif
			}
		}
	}
	deviceContext->Unmap(vertexBuffer, 0);

	size = rendContext->idx.size() * sizeof(decltype(*rendContext->idx.data()));
	rc = ensureBufferSize(indexBuffer, D3D11_BIND_INDEX_BUFFER, indexBufferSize, size);
	verify(rc);
	deviceContext->Map(indexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
	memcpy(mappedSubres.pData, rendContext->idx.data(), size);
	deviceContext->Unmap(indexBuffer, 0);

	if (config::ModifierVolumes && !rendContext->modtrig.empty())
	{
		const ModTriangle *data = &rendContext->modtrig[0];
		size = rendContext->modtrig.size() * sizeof(decltype(rendContext->modtrig[0]));
		rc = ensureBufferSize(modvolBuffer, D3D11_BIND_VERTEX_BUFFER, modvolBufferSize, size);
		verify(rc);
		deviceContext->Map(modvolBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
		memcpy(mappedSubres.pData, data, size);
		deviceContext->Unmap(modvolBuffer, 0);
	}
    unsigned int stride = sizeof(Vertex);
    unsigned int offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer.get(), &stride, &offset);
	deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
}

void DX11Renderer::setupPixelShaderConstants()
{
	PixelConstants pixelConstants;
	// VERT and RAM fog color constants
	FOG_COL_VERT.getRGBColor(pixelConstants.fog_col_vert);
	FOG_COL_RAM.getRGBColor(pixelConstants.fog_col_ram);

	// Fog density
	pixelConstants.fogDensity = FOG_DENSITY.get() * config::ExtraDepthScale;
	// Shadow scale
	pixelConstants.shadowScale = FPU_SHAD_SCALE.scale_factor / 256.f;

	// Color clamping
	rendContext->fog_clamp_min.getRGBAColor(pixelConstants.colorClampMin);
	rendContext->fog_clamp_max.getRGBAColor(pixelConstants.colorClampMax);

	// Punch-through alpha ref
	pixelConstants.alphaTestValue = (PT_ALPHA_REF & 0xFF) / 255.0f;

	// Dithering
	dithering = config::EmulateFramebuffer && rendContext->fb_W_CTRL.fb_dither && rendContext->fb_W_CTRL.fb_packmode <= 3;
	if (dithering)
	{
		switch (rendContext->fb_W_CTRL.fb_packmode)
		{
		case 0: // 0555 KRGB 16 bit
		case 3: // 1555 ARGB 16 bit
			pixelConstants.ditherDivisor[0] = pixelConstants.ditherDivisor[1] = pixelConstants.ditherDivisor[2] = 2.f;
		break;
		case 1: // 565 RGB 16 bit
			pixelConstants.ditherDivisor[0] = pixelConstants.ditherDivisor[2] = 2.f;
			pixelConstants.ditherDivisor[1] = 4.f;
			break;
		case 2: // 4444 ARGB 16 bit
			pixelConstants.ditherDivisor[0] = pixelConstants.ditherDivisor[1] = pixelConstants.ditherDivisor[2] = 1.f;
			break;
		default:
			break;
		}
		pixelConstants.ditherDivisor[3] = 1.f;
	}

	D3D11_MAPPED_SUBRESOURCE mappedSubres;
	deviceContext->Map(pxlConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
	memcpy(mappedSubres.pData, &pixelConstants, sizeof(pixelConstants));
	deviceContext->Unmap(pxlConstants, 0);
	ID3D11Buffer *buffers[] { pxlConstants, pxlPolyConstants };
	deviceContext->PSSetConstantBuffers(0, std::size(buffers), buffers);
}

bool DX11Renderer::Render()
{
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
	if (!rendContext->isRTT && perfOutput && *perfOutput) {
		perfStart = std::chrono::steady_clock::now();
		QueryThreadCycleTime(GetCurrentThread(), &perfCyclesStart);
		stateSelections = temporaryOwners = 0;
	}
#endif
	resetContextState();
	bool is_rtt = rendContext->isRTT;
	if (!is_rtt)
	{
		resize(rendContext->framebufferWidth, rendContext->framebufferHeight);
		deviceContext->OMSetRenderTargets(1, &fbRenderTarget.get(), depthTexView);
		deviceContext->ClearDepthStencilView(depthTexView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.f, 0);
		if (rendContext->clearFramebuffer)
		{
			float colors[4];
			VO_BORDER_COL.getRGBColor(colors);
			colors[3] = 1.f;
			deviceContext->ClearRenderTargetView(fbRenderTarget, colors);
		}
	}
	configVertexShader();

	deviceContext->IASetInputLayout(mainInputLayout);

	n2Helper.resetCache();
	uploadGeometryBuffers();

	updateFogTexture();
	updatePaletteTexture();

	setupPixelShaderConstants();

	drawStrips();

#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
	// Read back a bounded set of exact frames before window restoration or resampling.
	static unsigned capturedFrames = 0;
	const char* captureFrames = std::getenv("FLYCAST_VOOT_FRAME_CAPTURE");
	if (!is_rtt && !vootGeometryPrefix.empty() && captureFrames && std::strcmp(captureFrames, "1") == 0 && capturedFrames < 4)
	{
		D3D11_TEXTURE2D_DESC desc{};
		fbTex->GetDesc(&desc);
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.MiscFlags = 0;
		ComPtr<ID3D11Texture2D> capture;
		if (SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &capture.get())))
		{
			deviceContext->CopyResource(capture, fbTex);
			D3D11_MAPPED_SUBRESOURCE pixels{};
			if (SUCCEEDED(deviceContext->Map(capture, 0, D3D11_MAP_READ, 0, &pixels)))
			{
				std::ofstream frame(vootGeometryPrefix + ".frame.ppm", std::ios::binary);
				frame.imbue(std::locale::classic());
				frame << "P6\n" << desc.Width << ' ' << desc.Height << "\n255\n";
				for (u32 y = 0; y < desc.Height; y++)
					for (u32 x = 0; x < desc.Width; x++)
					{
						const u8* p = static_cast<const u8*>(pixels.pData) + y * pixels.RowPitch + x * 4;
						const char rgb[] = { char(p[2]), char(p[1]), char(p[0]) };
						frame.write(rgb, sizeof(rgb));
					}
				deviceContext->Unmap(capture, 0);
				if (!frame) ERROR_LOG(RENDERER, "Voot frame evidence write failed");
				capturedFrames++;
			}
		}
	}
#endif

	if (is_rtt)
	{
		readRttRenderTarget(rendContext->fb_W_SOF1 & VRAM_MASK);
	}
	else if (config::EmulateFramebuffer)
	{
		writeFramebufferToVRAM();
	}
	else
	{
		aspectRatio = getOutputFramebufferAspectRatio();
#ifndef LIBRETRO
		deviceContext->OMSetRenderTargets(1, &DX11Context::Instance()->getRenderTarget().get(), nullptr);
		displayFramebuffer();
		drawOSD();
		renderVideoRouting();
		DX11Context::Instance()->setFrameRendered();
#else
		ID3D11RenderTargetView *nullView = nullptr;
		deviceContext->OMSetRenderTargets(1, &nullView, nullptr);
		DX11Context::Instance()->presentFrame(fbTextureView, width, height);
#endif
		frameRendered = true;
		frameRenderedOnce = true;
		clearLastFrame = false;
	}

#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
	if (!is_rtt) recordRenderPerformance(*rendContext);
#endif
	if (!is_rtt && !config::EmulateFramebuffer) hosted_startup::aspect_rendered(width, height);
	return !is_rtt;
}

void DX11Renderer::displayFramebuffer()
{
#ifndef LIBRETRO
	D3D11_VIEWPORT vp{};
	vp.Width = (FLOAT)settings.display.width;
	vp.Height = (FLOAT)settings.display.height;
	vp.MinDepth = 0.f;
	vp.MaxDepth = 1.f;
	deviceContext->RSSetViewports(1, &vp);

	const D3D11_RECT r = { 0, 0, settings.display.width, settings.display.height };
	deviceContext->RSSetScissorRects(1, &r);
	float colors[4];
	VO_BORDER_COL.getRGBColor(colors);
	colors[3] = 1.f;
	deviceContext->ClearRenderTargetView(DX11Context::Instance()->getRenderTarget(), colors);

	float shiftX, shiftY;
	getVideoShift(shiftX, shiftY);
	shiftX *=  2.f / width;
	shiftY *=  -2.f / height;

	int outwidth = settings.display.width;
	int outheight = settings.display.height;
	float renderAR = aspectRatio;
	if (config::Rotate90) {
		std::swap(outwidth, outheight);
		std::swap(shiftX, shiftY);
		renderAR = 1 / renderAR;
	}
	
	int dy = 0;
	int dx = 0;
	// handles the rotation on its own, so never pass config::Rotate90
	getWindowboxDimensions(outwidth, outheight, renderAR, dx, dy, false);
	
	float x = (float)dx;
	float y = (float)dy;
	float w = (float)(outwidth - 2 * dx);
	float h = (float)(outheight - 2 * dy);

	// Normalize
	x = x * 2.f / outwidth - 1.f;
	w *= 2.f / outwidth;
	y = y * 2.f / outheight - 1.f;
	h *= 2.f / outheight;
	// Shift
	x += shiftX;
	y += shiftY;
	deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
	quad->draw(fbTextureView, samplers->getSampler(config::LinearInterpolation), nullptr, x, y, w, h, config::Rotate90);
#endif
}

void DX11Renderer::setCullMode(int mode)
{
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
	++stateSelections;
#endif
	ID3D11RasterizerState* rasterizer;
	switch (mode)
	{
	case 0:
	case 1:
	default:
		rasterizer = rasterCullNone.get();
		break;
	case 2:
		rasterizer = rasterCullFront.get();
		break;
	case 3:
		rasterizer = rasterCullBack.get();
		break;
	}
	deviceContext->RSSetState(rasterizer);
}

TileClipping DX11Renderer::setTileClip(u32 tileclip, Rect& clip_rect)
{
	TileClipping clipmode = matrices.getTileClip(tileclip, clip_rect, vootWideClip);
	if (clipmode == TileClipping::Outside) {
		RECT rect { clip_rect.origin.x, clip_rect.origin.y,
			clip_rect.origin.x + clip_rect.size.x, clip_rect.origin.y + clip_rect.size.y };
		deviceContext->RSSetScissorRects(1, &rect);
	}
	else {
		deviceContext->RSSetScissorRects(1, &scissorRect);
	}
	return clipmode;
}

template <u32 Type, bool SortingEnabled>
void DX11Renderer::setRenderState(const PolyParam *gp)
{
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
	stateSelections += 4;

#endif
	PixelPolyConstants constants;
	if (gp->pcw.Texture && gp->tsp.FilterMode > 1 && Type != ListType_Punch_Through && gp->tcw.MipMapped == 1)
	{
		constants.trilinearAlpha = 0.25f * (gp->tsp.MipMapD & 0x3);
		if (gp->tsp.FilterMode == 2)
			// Trilinear pass A
			constants.trilinearAlpha = 1.f - constants.trilinearAlpha;
	}
	else
		constants.trilinearAlpha = 1.f;

	bool color_clamp = gp->tsp.ColorClamp && (rendContext->fog_clamp_min.full != 0 || rendContext->fog_clamp_max.full != 0xffffffff);
	int fog_ctrl = config::Fog ? gp->tsp.FogCtrl : 2;

	Rect clip_rect;
	TileClipping clipmode = setTileClip(gp->tileclip, clip_rect);
	DX11Texture *texture = (DX11Texture *)gp->texture;
	int gpuPalette = texture == nullptr || !texture->gpuPalette ? 0
			: gp->tsp.FilterMode + 1;
	if (gpuPalette != 0)
	{
		if (config::TextureFiltering == 1)
			gpuPalette = 1; // force nearest
		else if (config::TextureFiltering == 2)
			gpuPalette = 2; // force linear
	}

	const auto& vertexShader = shaders->getVertexShader(gp->pcw.Gouraud, gp->isNaomi2());
	deviceContext->VSSetShader(vertexShader, nullptr, 0);
	const auto& pixelShader = shaders->getShader(
			gp->pcw.Texture,
			gp->tsp.UseAlpha,
			gp->tsp.IgnoreTexA || gp->tcw.PixelFmt == Pixel565,
			gp->tsp.ShadInstr,
			gp->pcw.Offset,
			fog_ctrl,
			gp->tcw.PixelFmt == PixelBumpMap,
			color_clamp,
			constants.trilinearAlpha != 1.f,
			gpuPalette,
			gp->pcw.Gouraud,
			Type == ListType_Punch_Through,
			clipmode == TileClipping::Inside,
			dithering);
	deviceContext->PSSetShader(pixelShader, nullptr, 0);

	if (gpuPalette != 0)
	{
		if (gp->tcw.PixelFmt == PixelPal4)
			constants.paletteIndex = (float)(gp->tcw.PalSelect << 4);
		else
			constants.paletteIndex = (float)((gp->tcw.PalSelect >> 4) << 8);
	}

	if (clipmode == TileClipping::Inside)
	{
		constants.clipTest[0] = (float)clip_rect.origin.x;
		constants.clipTest[1] = (float)clip_rect.origin.y;
		constants.clipTest[2] = (float)clip_rect.bottomRight().x;
		constants.clipTest[3] = (float)clip_rect.bottomRight().y;
	}
	if (constants.trilinearAlpha != 1.f || gpuPalette != 0 || clipmode == TileClipping::Inside)
	{
		D3D11_MAPPED_SUBRESOURCE mappedSubres;
		deviceContext->Map(pxlPolyConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
		memcpy(mappedSubres.pData, &constants, sizeof(constants));
		deviceContext->Unmap(pxlPolyConstants, 0);
	}

	if (texture != nullptr)
	{
        deviceContext->PSSetShaderResources(0, 1, &texture->textureView.get());
		bool linearFiltering;
		if (gpuPalette != 0)
			linearFiltering = false;
		else if (config::TextureFiltering == 0)
			linearFiltering = gp->tsp.FilterMode != 0;
		else if (config::TextureFiltering == 1)
			linearFiltering = false;
		else
			linearFiltering = true;
        auto sampler = samplers->getSampler(linearFiltering, gp->tsp.ClampU, gp->tsp.ClampV, gp->tsp.FlipU, gp->tsp.FlipV, Type == ListType_Punch_Through);
        deviceContext->PSSetSamplers(0, 1, &sampler.get());
	}

	// Apparently punch-through polys support blending, or at least some combinations
	deviceContext->OMSetBlendState(blendStates.getState(true, gp->tsp.SrcInstr, gp->tsp.DstInstr), nullptr, 0xffffffff);

	setCullMode(gp->isp.CullMode);

	//set Z mode, only if required
	int zfunc;
	if (Type == ListType_Punch_Through || (Type == ListType_Translucent && SortingEnabled))
		zfunc = 6; // GEQ
	else
		zfunc = gp->isp.DepthMode;

	bool zwriteEnable;
	if (SortingEnabled /* && !config::PerStripSorting */)
		zwriteEnable = false;
	else
	{
		// Z Write Disable seems to be ignored for punch-through.
		// Fixes Worms World Party, Bust-a-Move 4 and Re-Volt
		if (Type == ListType_Punch_Through)
			zwriteEnable = true;
		else
			zwriteEnable = !gp->isp.ZWriteDis;
	}
	const u32 stencil = (gp->pcw.Shadow != 0) ? 0x80 : 0;
	deviceContext->OMSetDepthStencilState(depthStencilStates.getState(true, zwriteEnable, zfunc, config::ModifierVolumes), stencil);

	if (gp->isNaomi2())
		n2Helper.setConstants(*gp, 0, *rendContext); // poly number only used in OIT
}

void DX11Renderer::drawVootTitleSides(const PolyParam* params, D3D11_PRIMITIVE_TOPOLOGY resumeTopology)
{
	if (params != vootTitleBackground || vootTitleSidesDrawn)
		return;
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	deviceContext->Draw(4, vootTitleFirstVertex);
	deviceContext->Draw(4, vootTitleFirstVertex + 4);
	deviceContext->IASetPrimitiveTopology(resumeTopology);
	vootTitleSidesDrawn = true;
}

template <u32 Type, bool SortingEnabled>
void DX11Renderer::drawList(const std::vector<PolyParam>& gply, int first, int count)
{
	if (count == 0)
		return;
	deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	const PolyParam* params = &gply[first];

	while (count-- > 0)
	{
		if (params->count > 2)
		{
			if ((Type == ListType_Opaque || (Type == ListType_Translucent && !SortingEnabled)) && params->isp.DepthMode == 0)
			{
				// depthFunc = never
				params++;
				continue;
			}
			setRenderState<Type, SortingEnabled>(params);
			drawVootTitleSides(params, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			deviceContext->DrawIndexed(params->count, params->first, 0);
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
			if (vootDrawTrace.is_open())
				vootDrawTrace << Type << ',' << params->first << ',' << params->count << ',' << params->isp.CullMode << ',' << params->isp.DepthMode << ',' << params->tileclip << '\n';
#endif
		}

		params++;
	}
}

void DX11Renderer::drawSorted(int first, int count, bool multipass)
{
	if (count == 0)
		return;
	deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	int end = first + count;
	for (int p = first; p < end; p++)
	{
		const PolyParam* params = &rendContext->global_param_tr[rendContext->sortedTriangles[p].polyIndex];
		setRenderState<ListType_Translucent, true>(params);
		drawVootTitleSides(params, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		deviceContext->DrawIndexed(rendContext->sortedTriangles[p].count, rendContext->sortedTriangles[p].first, 0);
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
		if (vootDrawTrace.is_open())
			vootDrawTrace << ListType_Translucent << ',' << rendContext->sortedTriangles[p].first << ',' << rendContext->sortedTriangles[p].count << ',' << params->isp.CullMode << ',' << params->isp.DepthMode << ',' << params->tileclip << '\n';
#endif
	}
	if (multipass && config::TranslucentPolygonDepthMask)
	{
		// Write to the depth buffer now. The next render pass might need it. (Cosmic Smash)
		deviceContext->OMSetBlendState(blendStates.getState(false, 0, 0, true), nullptr, 0xffffffff);

		ComPtr<ID3D11VertexShader> vertexShader = shaders->getVertexShader(true, settings.platform.isNaomi2());
		deviceContext->VSSetShader(vertexShader, nullptr, 0);
		ComPtr<ID3D11PixelShader> pixelShader = shaders->getShader(
				false,
				false,
				false,
				0,
				false,
				2,
				false,
				false,
				false,
				false,
				true,
				false,
				false,
				false);
		deviceContext->PSSetShader(pixelShader, nullptr, 0);

		// Enable depth test, enable depth write, >=, disable stencil
		deviceContext->OMSetDepthStencilState(depthStencilStates.getState(true, true, 6, false), 0);
		deviceContext->RSSetScissorRects(1, &scissorRect);

		for (int p = first; p < end; p++)
		{
			const PolyParam* params = &rendContext->global_param_tr[rendContext->sortedTriangles[p].polyIndex];
			if (!params->isp.ZWriteDis)
			{
				setCullMode(params->isp.CullMode);
				deviceContext->DrawIndexed(rendContext->sortedTriangles[p].count, rendContext->sortedTriangles[p].first, 0);
			}
		}
	}
}

void DX11Renderer::drawModVols(int first, int count)
{
	if (count == 0 || rendContext->modtrig.empty() || !config::ModifierVolumes)
		return;

	deviceContext->IASetInputLayout(modVolInputLayout);
    unsigned int stride = 3 * sizeof(float);
    unsigned int offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &modvolBuffer.get(), &stride, &offset);
	deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	deviceContext->OMSetBlendState(blendStates.getState(false, 0, 0, true), nullptr, 0xffffffff);

	deviceContext->PSSetShader(shaders->getModVolShader(), nullptr, 0);

	setCullMode(0);

	const ModifierVolumeParam *params = &rendContext->global_param_mvo[first];

	int mod_base = -1;
	int curMVMat = -1;
	int curProjMat = -1;

	for (int cmv = 0; cmv < count; cmv++)
	{
		const ModifierVolumeParam& param = params[cmv];

		u32 mv_mode = param.isp.DepthMode;

		if (mod_base == -1)
			mod_base = param.first;

		if (param.isNaomi2() && (param.mvMatrix != curMVMat || param.projMatrix != curProjMat))
		{
			curMVMat = param.mvMatrix;
			curProjMat = param.projMatrix;
			n2Helper.setConstants(rendContext->matrices[param.mvMatrix].mat, rendContext->matrices[param.projMatrix].mat);
		}
		deviceContext->VSSetShader(shaders->getMVVertexShader(param.isNaomi2()), nullptr, 0);
		if (!param.isp.VolumeLast && mv_mode > 0)
			// OR'ing (open volume or quad)
			deviceContext->OMSetDepthStencilState(depthStencilStates.getMVState(DepthStencilStates::Or), 2);
		else
			// XOR'ing (closed volume)
			deviceContext->OMSetDepthStencilState(depthStencilStates.getMVState(DepthStencilStates::Xor), 0);

		Rect clip_rect;
		setTileClip(param.tileclip, clip_rect);
		// TODO inside clipping

		if (param.count > 0)
		{
			setCullMode(param.isp.CullMode);
			deviceContext->Draw(param.count * 3, param.first * 3);
		}

		if (mv_mode == 1 || mv_mode == 2)
		{
			// Sum the area
			deviceContext->OMSetDepthStencilState(depthStencilStates.getMVState(mv_mode == 1 ? DepthStencilStates::Inclusion : DepthStencilStates::Exclusion), 1);
			deviceContext->Draw((param.first + param.count - mod_base) * 3, mod_base * 3);
			mod_base = -1;
		}
	}
	//disable culling
	setCullMode(0);
	//enable color writes
	deviceContext->OMSetBlendState(blendStates.getState(true, 4, 5), nullptr, 0xffffffff);
	deviceContext->RSSetScissorRects(1, &scissorRect);

	//black out any stencil with '1'
	//only pixels that are Modvol enabled, and in area 1
	deviceContext->OMSetDepthStencilState(depthStencilStates.getMVState(DepthStencilStates::Final), 0x81);

	deviceContext->IASetInputLayout(mainInputLayout);
    stride = sizeof(Vertex);
    offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer.get(), &stride, &offset);
	deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// Use the background poly as a quad
	deviceContext->VSSetShader(shaders->getMVVertexShader(false), nullptr, 0);
	deviceContext->DrawIndexed(4, 0, 0);
}

void DX11Renderer::drawStrips()
{
	RenderPass previous_pass {};
    for (int render_pass = 0; render_pass < (int)rendContext->render_passes.size(); render_pass++)
    {
        const RenderPass& current_pass = rendContext->render_passes[render_pass];
        u32 op_count = current_pass.op_count - previous_pass.op_count;
        u32 pt_count = current_pass.pt_count - previous_pass.pt_count;
        u32 tr_count = current_pass.tr_count - previous_pass.tr_count;
        u32 mvo_count = current_pass.mvo_count - previous_pass.mvo_count;
        DEBUG_LOG(RENDERER, "Render pass %d OP %d PT %d TR %d MV %d autosort %d", render_pass + 1,
        		op_count, pt_count, tr_count, mvo_count, current_pass.autosort);

		drawList<ListType_Opaque, false>(rendContext->global_param_op, previous_pass.op_count, op_count);

		drawList<ListType_Punch_Through, false>(rendContext->global_param_pt, previous_pass.pt_count, pt_count);

		drawModVols(previous_pass.mvo_count, mvo_count);

		if (current_pass.autosort)
		{
			if (!config::PerStripSorting)
				drawSorted(previous_pass.sorted_tr_count, current_pass.sorted_tr_count - previous_pass.sorted_tr_count, render_pass < (int)rendContext->render_passes.size() - 1);
			else
				drawList<ListType_Translucent, true>(rendContext->global_param_tr, previous_pass.tr_count, tr_count);
		}
		else
		{
			drawList<ListType_Translucent, false>(rendContext->global_param_tr, previous_pass.tr_count, tr_count);
		}
		previous_pass = current_pass;
    }
}

bool DX11Renderer::RenderLastFrame()
{
	if (!frameRenderedOnce || clearLastFrame)
		return false;
	displayFramebuffer();
	return true;
}

void DX11Renderer::RenderFramebuffer(const FramebufferInfo& info)
{
	rendContext = nullptr;
	PixelBuffer<u32> pb;
	int width;
	int height;

	if (info.fb_r_ctrl.fb_enable == 0 || info.vo_control.blank_video == 1)
	{
		// Video output disabled
		width = height = 1;
		pb.init(width, height, false);
		u8 *p = (u8 *)pb.data(0, 0);
		p[0] = info.vo_border_col._blue;
		p[1] = info.vo_border_col._green;
		p[2] = info.vo_border_col._red;
		p[3] = 255;
	}
	else
	{
		ReadFramebuffer<BGRAPacker>(info, pb, width, height);
	}

	if (dcfbTexture)
	{
		D3D11_TEXTURE2D_DESC desc;
		dcfbTexture->GetDesc(&desc);
		if ((int)desc.Width != width || (int)desc.Height != height)
		{
			dcfbTexture.reset();
			dcfbTextureView.reset();
		}
	}
	if (!dcfbTexture)
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = width;
		desc.Height = height;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.MipLevels = 1;

		HRESULT hr = device->CreateTexture2D(&desc, nullptr, &dcfbTexture.get());
		if (FAILED(hr))
			WARN_LOG(RENDERER, "DC Framebuffer texture creation failed");
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = desc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		hr = device->CreateShaderResourceView(dcfbTexture, &viewDesc, &dcfbTextureView.get());
		if (FAILED(hr))
			WARN_LOG(RENDERER, "DC Framebuffer texture view creation failed");
	}
	deviceContext->UpdateSubresource(dcfbTexture, 0, nullptr, pb.data(), width * sizeof(u32), width * sizeof(u32) * height);

#ifndef LIBRETRO
	ID3D11ShaderResourceView *nullResView = nullptr;
    deviceContext->PSSetShaderResources(0, 1, &nullResView);
	resize(width, height);
	deviceContext->OMSetRenderTargets(1, &fbRenderTarget.get(), nullptr);
	float colors[4];
	info.vo_border_col.getRGBColor(colors);
	colors[3] = 1.f;
	deviceContext->ClearRenderTargetView(fbRenderTarget, colors);
	D3D11_VIEWPORT vp{};
	vp.Width = (FLOAT)this->width;
	vp.Height = (FLOAT)this->height;
	vp.MinDepth = 0.f;
	vp.MaxDepth = 1.f;
	deviceContext->RSSetViewports(1, &vp);
	const D3D11_RECT r = { 0, 0, (LONG)this->width, (LONG)this->height };
	deviceContext->RSSetScissorRects(1, &r);
	deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
	deviceContext->GSSetShader(nullptr, nullptr, 0);
	deviceContext->HSSetShader(nullptr, nullptr, 0);
	deviceContext->DSSetShader(nullptr, nullptr, 0);
	deviceContext->CSSetShader(nullptr, nullptr, 0);

	quad->draw(dcfbTextureView, samplers->getSampler(true));

	aspectRatio = getDCFramebufferAspectRatio();

	deviceContext->OMSetRenderTargets(1, &DX11Context::Instance()->getRenderTarget().get(), nullptr);
	displayFramebuffer();
	drawOSD();
	renderVideoRouting();
	DX11Context::Instance()->setFrameRendered();
#else
	ID3D11RenderTargetView *nullView = nullptr;
	deviceContext->OMSetRenderTargets(1, &nullView, nullptr);
	DX11Context::Instance()->presentFrame(dcfbTextureView, width, height);
#endif
	frameRendered = true;
	frameRenderedOnce = true;
	clearLastFrame = false;
}

void DX11Renderer::setBaseScissor()
{
	vootWideClip = config::VootWidescreenClipping && config::Widescreen && !config::Rotate90
		&& !config::EmulateFramebuffer && !rendContext->isRTT && settings.platform.isNaomi()
		&& settings.content.gameId == "VIRTUAL-ON ORATORIO TANGRAM";
	Rect scissor = matrices.getBaseScissor();
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
	// Bounded observation only: submitted vertices are never rewritten.
	if (!rendContext->isRTT && settings.content.gameId == "VIRTUAL-ON ORATORIO TANGRAM")
	{
		static unsigned samples = 0;
		static u32 nextFrame = 0, previousFrame = 0;
		static auto previousTime = std::chrono::steady_clock::now();
		if (samples < 20 && MainFrameCount >= nextFrame)
		{
			auto now = std::chrono::steady_clock::now();
			double seconds = std::chrono::duration<double>(now - previousTime).count();
			float minX = 1.e20f, maxX = -1.e20f, minY = 1.e20f, maxY = -1.e20f;
			unsigned outside = 0;
			for (const auto& v : rendContext->verts)
			{
				if (!std::isfinite(v.x) || !std::isfinite(v.y)) continue;
				minX = std::min(minX, v.x); maxX = std::max(maxX, v.x);
				minY = std::min(minY, v.y); maxY = std::max(maxY, v.y);
				outside += v.x < 0 || v.x >= rendContext->globClip.x;
			}
			const auto& m = matrices.GetViewportMatrix();
			NOTICE_LOG(RENDERER, "VootRender frame=%u target=%ux%u output=%dx%d global=%dx%d fb=%d,%d,%d,%d tile=%d,%d,%d,%d scissor=%d,%d,%d,%d vertices=%u outsideX=%u x=%g:%g y=%g:%g map=%g,%g,%g,%g mainFPS=%.3f",
				MainFrameCount, rendContext->framebufferWidth, rendContext->framebufferHeight, settings.display.width, settings.display.height,
				rendContext->globClip.x, rendContext->globClip.y,
				rendContext->fbClip.origin.x, rendContext->fbClip.origin.y, rendContext->fbClip.size.x, rendContext->fbClip.size.y,
				rendContext->tileClip.origin.x, rendContext->tileClip.origin.y, rendContext->tileClip.size.x, rendContext->tileClip.size.y,
				scissor.origin.x, scissor.origin.y, scissor.size.x, scissor.size.y,
				(unsigned)rendContext->verts.size(), outside, minX, maxX, minY, maxY, m[0][0], m[1][1], m[3][0], m[3][1],
				samples == 0 || seconds <= 0 ? 0.0 : (MainFrameCount - previousFrame) / seconds);
			std::map<u32, unsigned> clips;
			for (const auto* list : { &rendContext->global_param_op, &rendContext->global_param_pt, &rendContext->global_param_tr })
				for (const auto& poly : *list) clips[poly.tileclip] += poly.count;
			unsigned clipSamples = 0;
			for (const auto& entry : clips)
			{
				if (clipSamples++ == 8) break;
				Rect projected{};
				auto mode = matrices.getTileClip(entry.first, projected, vootWideClip);
				NOTICE_LOG(RENDERER, "VootClip frame=%u raw=%08x mode=%d rect=%d,%d,%d,%d indices=%u",
					MainFrameCount, entry.first, (int)mode, projected.origin.x, projected.origin.y,
					projected.size.x, projected.size.y, entry.second);
			}
			previousTime = now; previousFrame = MainFrameCount; nextFrame = MainFrameCount + 300; samples++;
		}
	}
#endif
	scissorRect.left = scissor.origin.x;
	scissorRect.top = scissor.origin.y;
	// DX11 scissor bottom right pixel is clipped
	scissorRect.right = scissor.origin.x + scissor.size.x;
	scissorRect.bottom = scissor.origin.y + scissor.size.y;
	if (rendContext->isRTT) {
		scissorEnable = true;
	}
	else
	{
		if (scissor.origin.x != 0 || scissor.origin.y != 0
				|| scissor.size.x != (int)rendContext->framebufferWidth
				|| scissor.size.y != (int)rendContext->framebufferHeight)
			scissorEnable = true;
		else
			scissorEnable = false;
	}
	deviceContext->RSSetScissorRects(1, &scissorRect);
}

void DX11Renderer::prepareRttRenderTarget(u32 texAddress)
{
	u32 fbw = rendContext->framebufferWidth;
	u32 fbh = rendContext->framebufferHeight;
	DEBUG_LOG(RENDERER, "RTT packmode=%d stride=%d - %d x %d @ %06x",
			rendContext->fb_W_CTRL.fb_packmode, rendContext->fb_W_LINESTRIDE * 8, fbw, fbh, texAddress);

	createTexAndRenderTarget(rttTexture, rttRenderTarget, fbw, fbh);
	createDepthTexAndView(rttDepthTex, rttDepthTexView, fbw, fbh);
	deviceContext->ClearDepthStencilView(rttDepthTexView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.f, 0);
	deviceContext->OMSetRenderTargets(1, &rttRenderTarget.get(), rttDepthTexView);

	D3D11_VIEWPORT vp{};
	vp.Width = (FLOAT)fbw;
	vp.Height = (FLOAT)fbh;
	vp.MinDepth = 0.f;
	vp.MaxDepth = 1.f;
	deviceContext->RSSetViewports(1, &vp);
	setRTTSize(fbw, fbh);
}

void DX11Renderer::readRttRenderTarget(u32 texAddress)
{
	u32 w = rendContext->framebufferWidth;
	u32 h = rendContext->framebufferHeight;
	if (config::RenderToTextureBuffer)
	{
		D3D11_TEXTURE2D_DESC desc;
		rttTexture->GetDesc(&desc);
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		ComPtr<ID3D11Texture2D> stagingTex;
		HRESULT hr = device->CreateTexture2D(&desc, nullptr, &stagingTex.get());
		if (FAILED(hr))
		{
			WARN_LOG(RENDERER, "Staging RTT texture creation failed");
			return;
		}
		deviceContext->CopyResource(stagingTex, rttTexture);

		PixelBuffer<u32> tmp_buf;
		tmp_buf.init(w, h);
		u8 *p = (u8 *)tmp_buf.data();

		D3D11_MAPPED_SUBRESOURCE mappedSubres;
		hr = deviceContext->Map(stagingTex, 0, D3D11_MAP_READ, 0, &mappedSubres);
		if (FAILED(hr))
		{
			WARN_LOG(RENDERER, "Failed to map staging RTT texture");
			return;
		}
		if (w * sizeof(u32) == mappedSubres.RowPitch) {
			memcpy(p, mappedSubres.pData, w * h * sizeof(u32));
		}
		else
		{
			u8 *src = (u8 *)mappedSubres.pData;
			for (u32 y = 0; y < h; y++)
			{
				memcpy(p, src, w * sizeof(u32));
				p += w * sizeof(u32);
				src += mappedSubres.RowPitch;
			}
		}
		deviceContext->Unmap(stagingTex, 0);

		u16 *dst = (u16 *)&vram[texAddress];
		WriteTextureToVRam<2, 1, 0, 3>(w, h, (u8 *)tmp_buf.data(), dst, rendContext->fb_W_CTRL, rendContext->fb_W_LINESTRIDE * 8, rendContext->fbClip);
	}
	else
	{
		//memset(&vram[gl.rtt.texAddress], 0, size);
		int wpo2, hpo2;
		getPvrFramebufferSize(*rendContext, wpo2, hpo2);
		if (wpo2 <= 1024 && hpo2 <= 1024)
		{
			DX11Texture* texture = texCache.getRTTexture(texAddress, rendContext->fb_W_CTRL.fb_packmode, wpo2, hpo2);

			texture->texture = rttTexture;
			rttTexture.reset();
			rttRenderTarget.reset();
			texture->textureView.reset();
			D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
			viewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			viewDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(texture->texture, &viewDesc, &texture->textureView.get());

			texture->dirty = 0;
			texture->unprotectVRam();
		}
	}
}

void DX11Renderer::updatePaletteTexture()
{
	if (updatePalette)
	{
		updatePalette = false;
		deviceContext->UpdateSubresource(paletteTexture, 0, nullptr, palette32_ram, 32 * sizeof(u32), 32 * sizeof(u32) * 32);
	}
    deviceContext->PSSetShaderResources(1, 1, &paletteTextureView.get());
    deviceContext->PSSetSamplers(1, 1, &samplers->getSampler(false).get());
}

void DX11Renderer::updateFogTexture()
{
	if (!config::Fog)
		return;
	if (updateFogTable)
	{
		updateFogTable = false;
		u8 temp_tex_buffer[256];
		MakeFogTexture(temp_tex_buffer);

		deviceContext->UpdateSubresource(fogTexture, 0, nullptr, temp_tex_buffer, 128, 128 * 2);
	}
    deviceContext->PSSetShaderResources(2, 1, &fogTextureView.get());
    deviceContext->PSSetSamplers(2, 1, &samplers->getSampler(true).get());
}

void DX11Renderer::drawOSD()
{
#ifndef LIBRETRO
	DX11Context::Instance()->setOverlay(true);
	gui_display_osd();
	DX11Context::Instance()->setOverlay(false);
#endif
}

void DX11Renderer::writeFramebufferToVRAM()
{
	u32 width = rendContext->globClip.x;
	u32 height = rendContext->globClip.y;
	glm::ivec2 scaledSize;
	Rect finalClip;
	getWriteFBToVramParams(*rendContext, scaledSize, finalClip);

	ComPtr<ID3D11Texture2D> fbTexture = fbTex;

	if (scaledSize.x != (int)width || scaledSize.y != (int)height)
	{
		const u32 scaledW = scaledSize.x;
		const u32 scaledH = scaledSize.y;

		if (fbScaledTexture)
		{
			D3D11_TEXTURE2D_DESC desc;
			fbScaledTexture->GetDesc(&desc);
			if (desc.Width != scaledW || desc.Height != scaledH)
			{
				fbScaledTexture.reset();
				fbScaledTextureView.reset();
				fbScaledRenderTarget.reset();
			}
		}
		if (!fbScaledTexture)
		{
			createTexAndRenderTarget(fbScaledTexture, fbScaledRenderTarget, scaledW, scaledH);

			D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
			viewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			viewDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(fbScaledTexture, &viewDesc, &fbScaledTextureView.get());
		}
		deviceContext->OMSetRenderTargets(1, &fbScaledRenderTarget.get(), nullptr);
		D3D11_VIEWPORT vp{};
		vp.Width = (FLOAT)scaledW;
		vp.Height = (FLOAT)scaledH;
		vp.MinDepth = 0.f;
		vp.MaxDepth = 1.f;
		deviceContext->RSSetViewports(1, &vp);
		deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
		quad->draw(fbTextureView, samplers->getSampler(true));

		width = scaledW;
		height = scaledH;
		fbTexture = fbScaledTexture;
	}
	u32 texAddress = rendContext->fb_W_SOF1 & VRAM_MASK; // TODO SCALER_CTL.interlace, SCALER_CTL.fieldselect
	u32 linestride = rendContext->fb_W_LINESTRIDE * 8;

	D3D11_TEXTURE2D_DESC desc;
	fbTexture->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	ComPtr<ID3D11Texture2D> stagingTex;
	HRESULT hr = device->CreateTexture2D(&desc, nullptr, &stagingTex.get());
	if (FAILED(hr))
	{
		WARN_LOG(RENDERER, "Staging RTT texture creation failed");
		return;
	}
	deviceContext->CopyResource(stagingTex, fbTexture);

	PixelBuffer<u32> tmp_buf;
	tmp_buf.init(width, height);
	u8 *p = (u8 *)tmp_buf.data();

	D3D11_MAPPED_SUBRESOURCE mappedSubres;
	hr = deviceContext->Map(stagingTex, 0, D3D11_MAP_READ, 0, &mappedSubres);
	if (FAILED(hr))
	{
		WARN_LOG(RENDERER, "Failed to map staging RTT texture");
		return;
	}
	if (width * sizeof(u32) == mappedSubres.RowPitch)
		memcpy(p, mappedSubres.pData, width * height * sizeof(u32));
	else
	{
		u8 *src = (u8 *)mappedSubres.pData;
		for (u32 y = 0; y < height; y++)
		{
			memcpy(p, src, width * sizeof(u32));
			p += width * sizeof(u32);
			src += mappedSubres.RowPitch;
		}
	}
	deviceContext->Unmap(stagingTex, 0);

	WriteFramebuffer<2, 1, 0, 3>(width, height, (u8 *)tmp_buf.data(), texAddress, rendContext->fb_W_CTRL, linestride, finalClip);
}

bool DX11Renderer::GetLastFrame(std::vector<u8>& data, int& width, int& height)
{
	if (!frameRenderedOnce)
		return false;

	if (width != 0) {
		height = width / aspectRatio;
	}
	else if (height != 0) {
		width = aspectRatio * height;
	}
	else
	{
		width = this->width;
		height = this->height;
		if (config::Rotate90)
			std::swap(width, height);
		// We need square pixels for PNG
		int w = aspectRatio * height;
		if (width > w)
			height = width / aspectRatio;
		else
			width = w;
	}

	ComPtr<ID3D11Texture2D> dstTex;
	ComPtr<ID3D11RenderTargetView> dstRenderTarget;
	createTexAndRenderTarget(dstTex, dstRenderTarget, width, height);

	ID3D11ShaderResourceView *nullResView = nullptr;
	deviceContext->PSSetShaderResources(0, 1, &nullResView);
	deviceContext->OMSetRenderTargets(1, &dstRenderTarget.get(), nullptr);
	D3D11_VIEWPORT vp{};
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.f;
	vp.MaxDepth = 1.f;
	deviceContext->RSSetViewports(1, &vp);
	const D3D11_RECT r = { 0, 0, (LONG)width, (LONG)height };
	deviceContext->RSSetScissorRects(1, &r);
	deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
	deviceContext->GSSetShader(nullptr, nullptr, 0);
	deviceContext->HSSetShader(nullptr, nullptr, 0);
	deviceContext->DSSetShader(nullptr, nullptr, 0);
	deviceContext->CSSetShader(nullptr, nullptr, 0);

	quad->draw(fbTextureView, samplers->getSampler(true), nullptr, -1.f, -1.f, 2.f, 2.f, config::Rotate90);

#ifndef LIBRETRO
	deviceContext->OMSetRenderTargets(1, &DX11Context::Instance()->getRenderTarget().get(), nullptr);
#else
	ID3D11RenderTargetView *nullView = nullptr;
	deviceContext->OMSetRenderTargets(1, &nullView, nullptr);
#endif
	D3D11_TEXTURE2D_DESC desc;
	dstTex->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	ComPtr<ID3D11Texture2D> stagingTex;
	HRESULT hr = device->CreateTexture2D(&desc, nullptr, &stagingTex.get());
	if (FAILED(hr))
	{
		WARN_LOG(RENDERER, "Staging screenshot texture creation failed");
		return false;
	}
	deviceContext->CopyResource(stagingTex, dstTex);

	D3D11_MAPPED_SUBRESOURCE mappedSubres;
	hr = deviceContext->Map(stagingTex, 0, D3D11_MAP_READ, 0, &mappedSubres);
	if (FAILED(hr))
	{
		WARN_LOG(RENDERER, "Failed to map staging screenshot texture");
		return false;
	}
	const u8* const src = (const u8 *)mappedSubres.pData;
	for (int y = 0; y < height; y++)
	{
		const u8 *p = src + y * mappedSubres.RowPitch;
		for (int x = 0; x < width; x++, p += 4)
		{
			data.push_back(p[2]);
			data.push_back(p[1]);
			data.push_back(p[0]);
		}
	}
	deviceContext->Unmap(stagingTex, 0);

	return true;
}

void DX11Renderer::renderVideoRouting()
{
#ifdef VIDEO_ROUTING
	if (config::VideoRouting)
	{
		extern void os_VideoRoutingPublishFrameTexture(ID3D11Texture2D* pTexture);
		
		ID3D11RenderTargetView* pRenderTargetView = DX11Context::Instance()->getRenderTarget().get();

		// Backbuffer texture would be different after resizing, fetching new address everytime
		ID3D11Resource* pResource = nullptr;
		pRenderTargetView->GetResource(&pResource);
		ID3D11Texture2D* backBufferTexture = nullptr;
		pResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&backBufferTexture);		
		
		if (config::VideoRoutingScale)
		{
			D3D11_TEXTURE2D_DESC bbDesc = {};
			backBufferTexture->GetDesc(&bbDesc);
			D3D11_TEXTURE2D_DESC vrsDesc = {};
			if (vrStagingTexture)
				vrStagingTexture->GetDesc(&vrsDesc);

			// Window resized?
			if (!vrStagingTexture || bbDesc.Width != vrsDesc.Width || bbDesc.Height != vrsDesc.Height)
			{
				vrStagingTexture.reset();
				vrStagingTextureSRV.reset();

				D3D11_TEXTURE2D_DESC srvDesc = bbDesc;
				srvDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				srvDesc.Usage = D3D11_USAGE_DEFAULT;
				device->CreateTexture2D(&srvDesc, nullptr, &vrStagingTexture.get());

				D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
				viewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
				viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				viewDesc.Texture2D.MipLevels = 1;
				
				device->CreateShaderResourceView(vrStagingTexture.get(), &viewDesc, &vrStagingTextureSRV.get());
			}

			// Scale down value changed?
			D3D11_TEXTURE2D_DESC vrscDesc = {};
			if (vrScaledTexture)
				vrScaledTexture->GetDesc(&vrscDesc);
			int targetWidth = config::VideoRoutingVRes * settings.display.width / settings.display.height;
			if (!vrScaledTexture || (int)vrscDesc.Height != config::VideoRoutingVRes)
			{

				vrScaledTexture.reset();
				vrScaledRenderTarget.reset();
				createTexAndRenderTarget(vrScaledTexture, vrScaledRenderTarget, targetWidth, config::VideoRoutingVRes);
			}
			D3D11_VIEWPORT scaledViewPort{};
			scaledViewPort.Width = targetWidth;
			scaledViewPort.Height = config::VideoRoutingVRes;
			scaledViewPort.MinDepth = 0.f;
			scaledViewPort.MaxDepth = 1.f;

			deviceContext->OMSetRenderTargets(1, &vrScaledRenderTarget.get(), nullptr);
			deviceContext->RSSetViewports(1, &scaledViewPort);
			deviceContext->CopyResource(vrStagingTexture.get(), backBufferTexture);
			quad->draw(vrStagingTextureSRV, samplers->getSampler(true));
			os_VideoRoutingPublishFrameTexture(vrScaledTexture);

			deviceContext->OMSetRenderTargets(1, &DX11Context::Instance()->getRenderTarget().get(), nullptr);

		} else {
			os_VideoRoutingPublishFrameTexture(backBufferTexture);
		}

		backBufferTexture->Release();
		pResource->Release();
	}
	else
	{
		os_VideoRoutingTermDX();
	}
#endif
}

Renderer *rend_DirectX11()
{
	return new DX11Renderer();
}
