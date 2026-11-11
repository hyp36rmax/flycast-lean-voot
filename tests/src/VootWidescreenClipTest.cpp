#include "gtest/gtest.h"
#include "rend/transform_matrix.h"
#include "rend/voot_visibility.h"
#include "hw/pvr/pvr_regs.h"
#include <array>
#include <cstring>

class VootWidescreenClipTest : public ::testing::Test
{
protected:
	bool wide = config::Widescreen, super = config::SuperWidescreen;
	bool rotate = config::Rotate90, framebuffer = config::EmulateFramebuffer;
	int displayWidth = settings.display.width, displayHeight = settings.display.height;
	u32 readControl = FB_R_CTRL.full, spgControl = SPG_CONTROL.full;
	rend_context context{};
	void SetUp() override
	{
		config::Widescreen = true; config::SuperWidescreen = false;
		config::Rotate90 = false; config::EmulateFramebuffer = false;
		FB_R_CTRL.full = 0; FB_R_CTRL.vclk_div = 1; SPG_CONTROL.full = 0;
		context.globClip = {640, 480};
		context.fbClip = {{0, 0}, {640, 480}};
		context.tileClip = context.fbClip;
		context.scaler_ctl.full = 0; context.scaler_ctl.vscalefactor = 1024;
		SetOutput(1920, 1080);
	}
	void TearDown() override
	{
		config::Widescreen = wide; config::SuperWidescreen = super;
		config::Rotate90 = rotate; config::EmulateFramebuffer = framebuffer;
		settings.display.width = displayWidth; settings.display.height = displayHeight;
		FB_R_CTRL.full = readControl; SPG_CONTROL.full = spgControl;
	}
	void SetOutput(int width, int height)
	{
		settings.display.width = width; settings.display.height = height;
		context.framebufferWidth = width; context.framebufferHeight = height;
	}
	TransformMatrix Matrix() { return TransformMatrix(context, context.framebufferWidth, context.framebufferHeight, true); }
	static std::array<Vertex, 4> Background()
	{
		std::array<Vertex, 4> vertices{};
		vertices[0].x = vertices[2].x = -256;
		vertices[1].x = vertices[3].x = 896;
		vertices[2].y = vertices[3].y = 480;
		for (auto& vertex : vertices) { vertex.z = 0.001f; vertex.u = 0.25f; vertex.v = 0.75f; }
		return vertices;
	}
	static void Same(const Rect& a, const Rect& b)
	{
		EXPECT_EQ(a.origin.x, b.origin.x); EXPECT_EQ(a.origin.y, b.origin.y);
		EXPECT_EQ(a.size.x, b.size.x); EXPECT_EQ(a.size.y, b.size.y);
	}
	static std::array<Vertex, 4> TitleQuad()
	{
		std::array<Vertex, 4> vertices{};
		for (int i = 0; i < 4; i++) {
			vertices[i].x = (i & 1) ? 1024.f : 0.f;
			vertices[i].y = (i & 2) ? 496.f : -16.f;
			vertices[i].z = 10;
			vertices[i].u = (i & 1) ? 1023.5f / 1024 : 0.5f / 1024;
			vertices[i].v = (i & 2) ? 511.5f / 512 : 0.5f / 512;
			memset(vertices[i].col, 255, sizeof(vertices[i].col));
		}
		return vertices;
	}
};

TEST_F(VootWidescreenClipTest, DefaultOff)
{
	EXPECT_FALSE(config::VootWidescreenClipping.get());
}

TEST_F(VootWidescreenClipTest, ExpandsHorizontalClipWithoutChangingVerticalClip)
{
	auto matrix = Matrix(); Rect original{}, expanded{};
	EXPECT_EQ(matrix.getTileClip(0x201a04c0, original), TileClipping::Outside);
	matrix.getTileClip(0x201a04c0, expanded, true);
	EXPECT_EQ(original.origin.x, 240); EXPECT_EQ(original.size.x, 1440);
	EXPECT_EQ(expanded.origin.x, 0); EXPECT_EQ(expanded.size.x, 1920);
	EXPECT_EQ(expanded.origin.y, original.origin.y);
	EXPECT_EQ(expanded.size.y, original.size.y); EXPECT_EQ(expanded.size.y, 1008);
}

TEST_F(VootWidescreenClipTest, PartialAndInverseClipsRemainUnchanged)
{
	auto matrix = Matrix();
	for (u32 raw : {0x201a04c1u, 0x201a0480u, 0x301a04c0u, 0u})
	{
		Rect original{}, expanded{};
		EXPECT_EQ(matrix.getTileClip(raw, original), matrix.getTileClip(raw, expanded, true));
		Same(original, expanded);
	}
}

TEST_F(VootWidescreenClipTest, RenderToTextureRemainsUnchanged)
{
	context.isRTT = true;
	auto matrix = Matrix(); Rect original{}, expanded{};
	matrix.getTileClip(0x201a04c0, original); matrix.getTileClip(0x201a04c0, expanded, true);
	Same(original, expanded);
}

TEST_F(VootWidescreenClipTest, OriginalFourByThreeRemainsUnchanged)
{
	config::Widescreen = false; SetOutput(1440, 1080);
	auto matrix = Matrix(); Rect original{}, expanded{};
	matrix.getTileClip(0x201a04c0, original); matrix.getTileClip(0x201a04c0, expanded, true);
	Same(original, expanded);
	EXPECT_FLOAT_EQ(matrix.GetViewportMatrix()[0][0], 2.25f);
	EXPECT_FLOAT_EQ(matrix.GetViewportMatrix()[1][1], 2.25f);
}

TEST_F(VootWidescreenClipTest, UsesOneHeightDerivedProjectionAcrossTargetDimensions)
{
	config::SuperWidescreen = true;
	for (auto size : {glm::ivec2(1920,1080), {2560,1080}, {3840,1080}, {2560,1440}, {3440,1440}, {5120,1440}})
	{
		SetOutput(size.x, size.y); auto matrix = Matrix();
		const auto& viewport = matrix.GetViewportMatrix();
		EXPECT_NEAR(viewport[0][0], size.y / 480.f, 0.001f);
		EXPECT_NEAR(viewport[1][1], size.y / 480.f, 0.001f);
		auto center = viewport * glm::vec4(320, 240, 0, 1);
		EXPECT_NEAR(center.x, size.x / 2.f, 0.001f); EXPECT_NEAR(center.y, size.y / 2.f, 0.001f);
		Rect original{}, expanded{};
		matrix.getTileClip(0x201a04c0, original); matrix.getTileClip(0x201a04c0, expanded, true);
		EXPECT_EQ(expanded.origin.x, 0); EXPECT_EQ(expanded.size.x, size.x);
		EXPECT_EQ(expanded.origin.y, original.origin.y); EXPECT_EQ(expanded.size.y, original.size.y);
	}
}

TEST_F(VootWidescreenClipTest, SixteenByNinePresetsPreserveHeightDerivedClipAndProjection)
{
	for (auto size : {glm::ivec2(1280,720), {1920,1080}, {2560,1440}, {3840,2160}})
	{
		SetOutput(size.x, size.y); auto matrix = Matrix();
		const auto& viewport = matrix.GetViewportMatrix();
		EXPECT_NEAR(viewport[0][0], size.y / 480.f, 0.001f);
		EXPECT_NEAR(viewport[1][1], size.y / 480.f, 0.001f);
		auto center = viewport * glm::vec4(320, 240, 0, 1);
		EXPECT_NEAR(center.x, size.x / 2.f, 0.001f);
		EXPECT_NEAR(center.y, size.y / 2.f, 0.001f);
		Rect original{}, expanded{};
		matrix.getTileClip(0x201a04c0, original);
		matrix.getTileClip(0x201a04c0, expanded, true);
		EXPECT_EQ(original.origin.x, size.x / 8);
		EXPECT_EQ(original.size.x, size.x * 3 / 4);
		EXPECT_EQ(expanded.origin.x, 0); EXPECT_EQ(expanded.size.x, size.x);
		EXPECT_EQ(expanded.origin.y, original.origin.y);
		EXPECT_EQ(expanded.size.y, original.size.y);
		EXPECT_EQ(expanded.size.y, size.y * 14 / 15);
		for (u32 raw : {0x201a04c1u, 0x301a04c0u}) {
			matrix.getTileClip(raw, original); matrix.getTileClip(raw, expanded, true);
			Same(original, expanded);
		}
	}
}

TEST_F(VootWidescreenClipTest, OriginalFourByThreeRemainsUnchangedAcrossPresetHeights)
{
	config::Widescreen = false;
	for (auto size : {glm::ivec2(960,720), {1440,1080}, {1920,1440}, {2880,2160}})
	{
		SetOutput(size.x, size.y); auto matrix = Matrix(); Rect original{}, expanded{};
		matrix.getTileClip(0x201a04c0, original); matrix.getTileClip(0x201a04c0, expanded, true);
		Same(original, expanded);
		EXPECT_NEAR(matrix.GetViewportMatrix()[0][0], size.y / 480.f, 0.001f);
		EXPECT_NEAR(matrix.GetViewportMatrix()[1][1], size.y / 480.f, 0.001f);
	}
}

TEST_F(VootWidescreenClipTest, BackgroundCoversActualWideFrameWithoutMovingOtherAttributes)
{
	config::SuperWidescreen = true; SetOutput(3840, 1080);
	auto matrix = Matrix(); auto vertices = Background(); const auto original = vertices;
	ASSERT_TRUE(matrix.expandBackground(vertices.data()));
	const auto& viewport = matrix.GetViewportMatrix();
	EXPECT_NEAR((viewport * glm::vec4(vertices[0].x, 0, 0, 1)).x, 0, 0.001f);
	EXPECT_NEAR((viewport * glm::vec4(vertices[1].x, 0, 0, 1)).x, 3840, 0.001f);
	for (size_t i = 0; i < vertices.size(); i++)
	{
		Vertex expected = original[i]; expected.x = vertices[i].x;
		EXPECT_EQ(memcmp(&expected, &vertices[i], sizeof(Vertex)), 0);
	}
}

TEST_F(VootWidescreenClipTest, ExistingBackgroundAlreadyCoversFourByThreeSixteenAndTwentyOne)
{
	config::SuperWidescreen = true;
	for (int width : {1440, 1920, 2560})
	{
		config::Widescreen = width != 1440; SetOutput(width, 1080);
		auto matrix = Matrix(); auto vertices = Background(); const auto original = vertices;
		EXPECT_FALSE(matrix.expandBackground(vertices.data()));
		EXPECT_EQ(memcmp(vertices.data(), original.data(), sizeof(vertices)), 0);
	}
}

TEST_F(VootWidescreenClipTest, BackgroundSkipsRenderToTextureAndNonuniformShading)
{
	config::SuperWidescreen = true; SetOutput(3840, 1080); context.isRTT = true;
	auto vertices = Background(); auto original = vertices;
	EXPECT_FALSE(Matrix().expandBackground(vertices.data()));
	EXPECT_EQ(memcmp(vertices.data(), original.data(), sizeof(vertices)), 0);
	context.isRTT = false; vertices[1].col[0] = 255; original = vertices;
	EXPECT_FALSE(Matrix().expandBackground(vertices.data()));
	EXPECT_EQ(memcmp(vertices.data(), original.data(), sizeof(vertices)), 0);
}

TEST_F(VootWidescreenClipTest, BackgroundExpansionIsIdempotent)
{
	config::SuperWidescreen = true; SetOutput(3840, 1080);
	auto matrix = Matrix(); auto vertices = Background();
	ASSERT_TRUE(matrix.expandBackground(vertices.data()));
	const auto expanded = vertices;
	EXPECT_FALSE(matrix.expandBackground(vertices.data()));
	EXPECT_EQ(memcmp(vertices.data(), expanded.data(), sizeof(vertices)), 0);
}

TEST_F(VootWidescreenClipTest, TitleSidesUseActualFrameAndRetainOriginalVerticalClip)
{
	config::SuperWidescreen = true;
	for (int width : {1920, 2560, 3840}) {
		SetOutput(width, 1080); auto matrix = Matrix();
		auto title = TitleQuad(); const auto original = title;
		std::array<Vertex, 8> sides{};
		ASSERT_TRUE(matrix.getVootTitleSideFill(title.data(), sides.data()));
		const auto& vp = matrix.GetViewportMatrix();
		EXPECT_NEAR((vp * glm::vec4(sides[0].x, 0, 0, 1)).x, 0, 0.001f);
		EXPECT_NEAR((vp * glm::vec4(sides[5].x, 0, 0, 1)).x, width, 0.001f);
		EXPECT_FLOAT_EQ(sides[1].x, 0); EXPECT_FLOAT_EQ(sides[4].x, 640);
		for (size_t i = 0; i < sides.size(); i++) {
			EXPECT_FLOAT_EQ(sides[i].y, (i & 2) ? 448.f : 0.f);
			EXPECT_FLOAT_EQ(sides[i].u, 0.5f / 1024);
			EXPECT_FLOAT_EQ(sides[i].v, 0.5f / 512);
			EXPECT_FLOAT_EQ(sides[i].z, 10);
		}
		EXPECT_EQ(memcmp(title.data(), original.data(), sizeof(title)), 0);
	}
}

TEST_F(VootWidescreenClipTest, TitleSidesRejectOriginalAspectRttAndUnrecognizedQuads)
{
	auto title = TitleQuad(); std::array<Vertex, 8> sides{};
	config::Widescreen = false; SetOutput(1440, 1080);
	EXPECT_FALSE(Matrix().getVootTitleSideFill(title.data(), sides.data()));
	config::Widescreen = true; config::SuperWidescreen = true; SetOutput(3840, 1080);
	context.isRTT = true;
	EXPECT_FALSE(Matrix().getVootTitleSideFill(title.data(), sides.data()));
	context.isRTT = false; title[1].u = 0.5f;
	EXPECT_FALSE(Matrix().getVootTitleSideFill(title.data(), sides.data()));
	title = TitleQuad(); title[2].col[0] = 128;
	EXPECT_FALSE(Matrix().getVootTitleSideFill(title.data(), sides.data()));
	title = TitleQuad(); title[3].z = 1;
	EXPECT_FALSE(Matrix().getVootTitleSideFill(title.data(), sides.data()));
}

TEST_F(VootWidescreenClipTest, TitleSidesPreserveUniformTintAndAlpha)
{
	config::SuperWidescreen = true; SetOutput(3840, 1080);
	auto title = TitleQuad();
	for (auto& vertex : title) { vertex.col[0] = 64; vertex.col[3] = 128; }
	std::array<Vertex, 8> sides{};
	ASSERT_TRUE(Matrix().getVootTitleSideFill(title.data(), sides.data()));
	for (const auto& vertex : sides)
		EXPECT_EQ(memcmp(vertex.col, title[0].col, sizeof(vertex.col)), 0);
}

#include <chrono>
#include <cstdio>
TEST_F(VootWidescreenClipTest, DISABLED_ClipMicrobenchmark)
{
    config::SuperWidescreen = true;
    for (int width : {1920, 2560, 3840}) {
        SetOutput(width, 1080);
        auto matrix = Matrix();
        for (bool expand : {false, true}) {
            for (int sample = 0; sample < 7; ++sample) {
                long long checksum = 0;
                const auto start = std::chrono::steady_clock::now();
                for (int i = 0; i < 1000000; ++i) {
                    Rect rect{};
                    const u32 raw = (i & 1) ? 0x201a04c0u : 0x201a04c1u;
                    matrix.getTileClip(raw, rect, expand);
                    checksum += rect.origin.x + rect.origin.y + rect.size.x + rect.size.y;
                }
                const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count() / 1000000;
                std::printf("CLIP_BENCH width=%d expand=%d sample=%d ns=%.3f checksum=%lld\n", width, expand, sample, ns, checksum);
            }
        }
    }
}
static TileClipping BaselineClip(TransformMatrix& matrix, const rend_context* renderingContext, u32 val, Rect& clipRect, bool expandFullWidth)
{
	if (!config::Clipping)
		return TileClipping::Off;

	u32 clipmode = val >> 28;
	if (clipmode < 2)
		return TileClipping::Off;	//always passes

	TileClipping tileClippingMode;
	if (clipmode & 1)
		tileClippingMode = TileClipping::Inside;   //render stuff outside the region
	else
		tileClippingMode = TileClipping::Outside;  //render stuff inside the region

	float csx = (float)(val & 63);
	float cex = (float)((val >> 6) & 63);
	float csy = (float)((val >> 12) & 31);
	float cey = (float)((val >> 17) & 31);
	csx = csx * 32;
	cex = (cex + 1) * 32;
	csy = csy * 32;
	cey = (cey + 1) * 32;

	if (csx == 0 && csy == 0 && cex >= renderingContext->globClip.x && cey >= renderingContext->globClip.y)
		return TileClipping::Off;

	glm::vec4 clip_start(csx, csy, 0, 1);
	glm::vec4 clip_end(cex, cey, 0, 1);
	clip_start = matrix.GetViewportMatrix() * clip_start;
	clip_end = matrix.GetViewportMatrix() * clip_end;
	clipRect = { glm::vec2(clip_start), glm::vec2(clip_end) - glm::vec2(clip_start) };
	clipRect.size.x++;
	clipRect.size.y = std::abs(clipRect.size.y) + 1;
	if (tileClippingMode == TileClipping::Outside)
	{
		Rect base = matrix.getBaseScissor();
		clipRect = intersect(clipRect, base);
	}
	csx = clip_start[0];
	csy = clip_start[1];
	cey = clip_end[1];
	cex = clip_end[0];
	clipRect = {
		glm::ivec2(std::max(0, (int)std::round(csx)), std::max(0, (int)std::round(std::min(csy, cey)))),
		glm::ivec2(std::max(0, (int)std::round(cex - csx)), std::max(0, (int)std::round(std::abs(cey - csy))))
	};
	if (expandFullWidth && !renderingContext->isRTT && tileClippingMode == TileClipping::Outside
			&& (val & 63) == 0 && (((val >> 6) & 63) + 1) * 32 >= (u32)renderingContext->globClip.x)
	{
		// A vertically restricted full-width clip must not reintroduce a 4:3 horizontal crop.
		// Keep partial-width/inverse clips and every vertical boundary unchanged.
		Rect base = matrix.getBaseScissor();
		clipRect.origin.x = base.origin.x;
		clipRect.size.x = base.size.x;
	}
	return tileClippingMode;
}


TEST_F(VootWidescreenClipTest, DifferentialClipCorpus)
{
    unsigned cases = 0;
    for (bool wideMode : {false, true}) {
        config::Widescreen = wideMode; config::SuperWidescreen = true;
        for (int width : {1440, 1920, 2560, 3840}) {
            SetOutput(width, 1080);
            for (bool rtt : {false, true}) {
                context.isRTT = rtt;
                auto matrix = Matrix();
                u32 seed = 0xdecafbad;
                for (int i = 0; i < 10000; ++i) {
                    seed = seed * 1664525u + 1013904223u;
                    for (bool expand : {false, true}) {
                        Rect expected{}, actual{};
                        EXPECT_EQ(BaselineClip(matrix, &context, seed, expected, expand), matrix.getTileClip(seed, actual, expand));
                        Same(expected, actual);
                        ++cases;
                    }
                }
            }
        }
    }
    std::printf("DIFFERENTIAL_CLIP_CASES=%u\n", cases);
}


TEST_F(VootWidescreenClipTest, RoundedInternalTargetPreservesVerticalScaleAndCenter)
{
    config::SuperWidescreen = true;
    SetOutput(2560, 1080);
    context.framebufferWidth = 2276;
    context.framebufferHeight = 960;
    auto matrix = Matrix();
    const auto& vp = matrix.GetViewportMatrix();
    EXPECT_NEAR(vp[0][0], 2.f, 0.000001f);
    EXPECT_NEAR(vp[1][1], 2.f, 0.000001f);
    const auto center = vp * glm::vec4(320, 240, 0, 1);
    EXPECT_NEAR(center.x, 1138.f, 0.001f);
    EXPECT_NEAR(center.y, 480.f, 0.001f);
}


TEST_F(VootWidescreenClipTest, GuestVisibilityUsesActualViewportAndPreservesFallbacks)
{
    const auto game = settings.content.gameId;
    const bool clipping = config::VootWidescreenClipping;
    settings.content.gameId = "VIRTUAL-ON ORATORIO TANGRAM";
    config::VootWidescreenClipping = true;
    config::SuperWidescreen = true;
    SetOutput(3840,1080); Matrix();
    EXPECT_NEAR(voot_visibility::horizontalExtra.load(), 533.333333f, .001f);
    SetOutput(2560,1080); Matrix();
    EXPECT_NEAR(voot_visibility::horizontalExtra.load(), 248.888889f, .001f);
    SetOutput(1920,1080); Matrix();
    EXPECT_NEAR(voot_visibility::horizontalExtra.load(), 106.666667f, .001f);
    SetOutput(1440,1080); Matrix();
    EXPECT_FLOAT_EQ(voot_visibility::horizontalExtra.load(), 0.f);
    SetOutput(3840,1080); config::VootWidescreenClipping = false; Matrix();
    EXPECT_FLOAT_EQ(voot_visibility::horizontalExtra.load(), 0.f);
    config::VootWidescreenClipping = true; config::Widescreen = false; Matrix();
    EXPECT_FLOAT_EQ(voot_visibility::horizontalExtra.load(), 0.f);
    config::Widescreen = true; settings.content.gameId = "OTHER"; Matrix();
    EXPECT_FLOAT_EQ(voot_visibility::horizontalExtra.load(), 0.f);
    settings.content.gameId = game; config::VootWidescreenClipping = clipping;
}