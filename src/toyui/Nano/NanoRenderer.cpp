//  Copyright (c) 2016 Hugo Amiard hugo.amiard@laposte.net
//  This software is provided 'as-is' under the zlib License, see the LICENSE.txt file.
//  This notice and the license may not be removed or altered from any source distribution.

#include <toyui/Config.h>
#include <toyui/Nano/NanoRenderer.h>

#include <toyobj/Util/Colour.h>

#include <toyui/Frame/Frame.h>
#include <toyui/Frame/Stripe.h>
#include <toyui/Frame/Layer.h>

#include <toyui/Widget/Widget.h>

#include <toyui/ImageAtlas.h>
#include <toyui/UiWindow.h>

#include <nanovg.h>

#include <cmath>

namespace toy
{

	inline float clamp(float v, float mn, float mx)
	{
		return (v > mx) ? mx : (v < mn) ? mn : v;
	}

	NVGcolor nvgColour(const Colour& colour)
	{
		return nvgRGBAf(colour.r(), colour.g(), colour.b(), colour.a());
	}

	NVGcolor nvgOffsetColour(const Colour& colour, float delta)
	{
		float offset = delta / 255.0f;
		return nvgRGBAf(	clamp(colour.r() + offset, 0, 1),
							clamp(colour.g() + offset, 0, 1),
							clamp(colour.b() + offset, 0, 1),
							colour.a());
	}

	NanoRenderer::NanoRenderer(const string& resourcePath)
		: Renderer(resourcePath)
		, m_ctx(nullptr)
	{}

	NanoRenderer::~NanoRenderer()
	{
		//if(m_ctx)
			//this->releaseContext();
	}

	unique_ptr<RenderTarget> NanoRenderer::createRenderTarget(MasterLayer& masterLayer)
	{
		return make_unique<RenderTarget>(*this, masterLayer, false);
	}

	void NanoRenderer::loadFont()
	{
		string fontPath = m_resourcePath + "interface/fonts/DejaVuSans.ttf";
		nvgCreateFont(m_ctx, "dejavu", fontPath.c_str());
		nvgFontSize(m_ctx, 14.0f);
		nvgFontFace(m_ctx, "dejavu");
	}

	void NanoRenderer::loadImageRGBA(Image& image, const unsigned char* data)
	{
		image.d_index = nvgCreateImageRGBA(m_ctx, image.d_width, image.d_height, 0, data);
	}

	void NanoRenderer::loadImage(Image& image)
	{
		image.d_index = nvgCreateImage(m_ctx, image.d_path.c_str(), image.d_tile ? (NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY) : 0);
	}

	void NanoRenderer::unloadImage(Image& image)
	{
		nvgDeleteImage(m_ctx, image.d_index);
		image.d_index = 0;
	}

	void NanoRenderer::render(RenderTarget& target)
	{
		m_debugBatch = 0;
		Stencil::s_debugBatch = 0;
		static int prevBatch = 0;

		float pixelRatio = 1.f;
		nvgBeginFrame(m_ctx, target.layer().width(), target.layer().height(), pixelRatio);

		if(target.layer().dirty() < Frame::DIRTY_MAPPING)
		{
			target.layer().widget()->render(*this, false);

#ifdef TOYUI_DRAW_CACHE
			void* layerCache = nullptr;
			this->layerCache(target.layer(), layerCache);
			this->drawLayer(layerCache, 0.f, 0.f, 1.f);

			for(Layer* layer : target.layer().layers())
				if(layer->visible())
				{
					this->layerCache(*layer, layerCache);
					this->drawLayer(layerCache, 0.f, 0.f, 1.f);
				}
#endif
		}

		if(Stencil::s_debugBatch > 1 && Stencil::s_debugBatch != prevBatch)
		{
			prevBatch = Stencil::s_debugBatch;
			//printf("Render Frame : %i frames redrawn\n", Stencil::s_debugBatch);
		}

		nvgEndFrame(m_ctx);
	}

	bool NanoRenderer::clipTest(const BoxFloat& rect)
	{
		BoxFloat scissor;
		nvgCurrentScissor(m_ctx, scissor.pointer());

		if(scissor.x1() < 0.f || scissor.y1() < 0.f)
			return false;

		return !rect.intersects(scissor);
	}

	void NanoRenderer::clipRect(const BoxFloat& rect)
	{
		nvgIntersectScissor(m_ctx, rect.x(), rect.y(), rect.w(), rect.h());
	}

	void NanoRenderer::unclipRect()
	{
		nvgResetScissor(m_ctx);
	}

	void NanoRenderer::pathLine(float x1, float y1, float x2, float y2)
	{
		nvgBeginPath(m_ctx);
		nvgMoveTo(m_ctx, x1, y1);
		nvgLineTo(m_ctx, x2, y2);
	}

	void NanoRenderer::pathBezier(float x1, float y1, float c1x, float c1y, float c2x, float c2y, float x2, float y2)
	{
		nvgBeginPath(m_ctx);
		nvgMoveTo(m_ctx, x1, y1);
		nvgBezierTo(m_ctx, c1x, c1y, c2x, c2y, x2, y2);
	}

	void NanoRenderer::pathRect(const BoxFloat& rect, const BoxFloat& corners, float border)
	{
		float halfborder = border * 0.5f;

		// Path
		if(corners.null())
			nvgRect(m_ctx, rect.x() + halfborder, rect.y() + halfborder, rect.w() - border, rect.h() - border);
		else
			nvgRoundedRectVarying(m_ctx, rect.x() + halfborder, rect.y() + halfborder, rect.w() - border, rect.h() - border, corners.xx(), corners.xy(), corners.yx(), corners.yy());
	}

	void NanoRenderer::drawShadow(const BoxFloat& rect, const BoxFloat& corners, const Shadow& shadow)
	{
		NVGpaint shadowPaint = nvgBoxGradient(m_ctx, rect.x() + shadow.d_xpos - shadow.d_spread, rect.y() + shadow.d_ypos - shadow.d_spread, rect.w() + shadow.d_spread * 2.f, rect.h() + shadow.d_spread * 2.f, corners.xy() + shadow.d_spread, shadow.d_blur, nvgRGBA(0, 0, 0, 128), nvgRGBA(0, 0, 0, 0));
		nvgBeginPath(m_ctx);
		nvgRect(m_ctx, rect.x() + shadow.d_xpos - shadow.d_radius, rect.y() + shadow.d_ypos - shadow.d_radius, rect.w() + shadow.d_radius * 2.f, rect.h() + shadow.d_radius * 2.f);
		if(corners.null())
			nvgRect(m_ctx, rect.x(), rect.y(), rect.w(), rect.h());
		else
			nvgRoundedRectVarying(m_ctx, rect.x(), rect.y(), rect.w(), rect.h(), corners.xx(), corners.xy(), corners.yx(), corners.yy());
		nvgPathWinding(m_ctx, NVG_HOLE);
		nvgFillPaint(m_ctx, shadowPaint);
		nvgFill(m_ctx);
	}

	void NanoRenderer::drawRect(const BoxFloat& rect, const BoxFloat& corners, InkStyle& skin)
	{
		float border = skin.borderWidth().x0();

		nvgBeginPath(m_ctx);
		this->pathRect(rect, corners, border);

		// Fill
		if(skin.backgroundColour().a() > 0.f)
			this->fill(skin, rect);

		// Border
		if(border > 0.f)
			this->stroke(skin);
	}

	void NanoRenderer::debugRect(const BoxFloat& rect, const Colour& colour)
	{
		//nvgSave(m_ctx);
		//nvgResetScissor(m_ctx);

		static InkStyle debugStyle;
		debugStyle.m_borderWidth = 1.f;
		debugStyle.m_borderColour = colour;

		this->drawRect(rect, BoxFloat(), debugStyle);

		//nvgRestore(m_ctx);
	}

	void NanoRenderer::fill(InkStyle& skin, const BoxFloat& rect)
	{
		if(skin.linearGradient().null())
		{
			nvgFillColor(m_ctx, nvgColour(skin.m_backgroundColour));
		}
		else
		{
			NVGcolor first = nvgOffsetColour(skin.backgroundColour(), skin.linearGradient().x());
			NVGcolor second = nvgOffsetColour(skin.backgroundColour(), skin.linearGradient().y());
			if(skin.linearGradientDim() == DIM_X)
				nvgFillPaint(m_ctx, nvgLinearGradient(m_ctx, rect.x(), rect.y(), rect.x() + rect.w(), rect.y(), first, second));
			else
				nvgFillPaint(m_ctx, nvgLinearGradient(m_ctx, rect.x(), rect.y(), rect.x(), rect.y() + rect.h(), first, second));
		}
		nvgFill(m_ctx);
	}

	void NanoRenderer::stroke(InkStyle& skin)
	{
		float border = skin.borderWidth().x0();

		nvgStrokeWidth(m_ctx, border);
		nvgStrokeColor(m_ctx, nvgColour(skin.borderColour()));
		nvgStroke(m_ctx);
	}

	void NanoRenderer::drawImage(int image, const BoxFloat& rect, const BoxFloat& imageRect)
	{
		NVGpaint imgPaint = nvgImagePattern(m_ctx, imageRect.x(), imageRect.y(), imageRect.w(), imageRect.h(), 0.0f / 180.0f*NVG_PI, image, 1.f);
		nvgBeginPath(m_ctx);
		nvgRect(m_ctx, rect.x(), rect.y(), rect.w(), rect.h());
		nvgFillPaint(m_ctx, imgPaint);
		nvgFill(m_ctx);
	}

	void NanoRenderer::drawImage(const Image& image, const BoxFloat& rect)
	{
		if(image.d_atlas)
		{
			Image& atlas = image.d_atlas->image();
			BoxFloat imageRect(rect.x() - image.d_left, rect.y() - image.d_top, float(atlas.d_width), float(atlas.d_height));
			this->drawImage(atlas.d_index, rect, imageRect);
		}
		else
		{
			this->drawImage(image.d_index, rect, rect);
		}
	}

	void NanoRenderer::drawImageStretch(const Image& image, const BoxFloat& rect, float xstretch, float ystretch)
	{
		if(image.d_atlas)
		{
			Image& atlas = image.d_atlas->image();
			BoxFloat imageRect(rect.x() - image.d_left * xstretch, rect.y() - image.d_top * ystretch, atlas.d_width * xstretch, atlas.d_height * ystretch);
			this->drawImage(atlas.d_index, rect, imageRect);
		}
		else
		{
			BoxFloat imageRect(rect.x(), rect.y(), image.d_width * xstretch, image.d_height * ystretch);
			this->drawImage(image.d_index, rect, imageRect);
		}
	}


	void NanoRenderer::setupText(InkStyle& skin)
	{
		NVGalign alignH = NVG_ALIGN_LEFT;
		if(skin.align()[DIM_X] == CENTER)
			alignH = NVG_ALIGN_CENTER;
		else if(skin.align()[DIM_X] == RIGHT)
			alignH = NVG_ALIGN_RIGHT;

		nvgFontSize(m_ctx, skin.textSize());
		nvgFontFace(m_ctx, skin.textFont().c_str());
		nvgTextAlign(m_ctx, alignH | NVG_ALIGN_TOP);

		m_lineHeight = 0.f;
		nvgTextMetrics(m_ctx, nullptr, nullptr, &m_lineHeight);
	}

	void NanoRenderer::fillText(const string& text, const BoxFloat& rect, InkStyle& skin, TextRow& row)
	{
		this->setupText(skin);

		row.start = text.c_str();
		row.end = text.c_str() + text.size();
		row.rect.assign(rect.x(), rect.y(), this->textSize(text, DIM_X, skin), m_lineHeight);

		this->breakTextLine(rect, row);
	}


	void NanoRenderer::breakTextWidth(const char* first, const char* end, const BoxFloat& rect, InkStyle& skin, TextRow& row)
	{
		UNUSED(skin);

		NVGtextRow nvgTextRow;
		nvgTextBreakLines(m_ctx, first, end, rect.w(), &nvgTextRow, 1);

		row.start = nvgTextRow.start;
		row.end = nvgTextRow.end;
		row.rect.assign(rect.x(), rect.y(), nvgTextRow.width, m_lineHeight);

		if(row.start != row.end)
			this->breakTextLine(rect, row);
	}

	void NanoRenderer::breakTextReturns(const char* first, const char* end, const BoxFloat& rect, InkStyle& skin, TextRow& row)
	{
		const char* iter = first;
		
		do
			++iter;
		while(*iter != '\n' && iter < end);

		row.start = first;
		row.end = iter;
		row.rect.assign(rect.x(), rect.y(), this->textSize(string(first, iter - first), DIM_X, skin), m_lineHeight);

		this->breakTextLine(rect, row);
	}

	void NanoRenderer::breakText(const string& text, const DimFloat& space, InkStyle& skin, std::vector<TextRow>& textRows)
	{
		this->setupText(skin);

		const char* first = text.c_str();
		const char* end = first + text.size();

		textRows.clear();
	
		if(!skin.textBreak())
		{
			textRows.resize(1);

			BoxFloat rect(0.f, 0.f, space.x(), m_lineHeight);
			this->fillText(text, rect, skin, textRows[0]);
			return;
		}

		while(first < end)
		{
			size_t index = textRows.size();
			textRows.resize(index + 1);
			TextRow& row = textRows.back();

			BoxFloat rect(0.f, index * m_lineHeight, space.x(), 0.f);
			if(skin.textWrap())
				this->breakTextWidth(first, end, rect, skin, row);
			else
				this->breakTextReturns(first, end, rect, skin, row);

			row.startIndex = row.start - text.c_str();
			row.endIndex = row.end - text.c_str();
			first = row.end + 1;
		}
	}

	void NanoRenderer::breakTextLine(const BoxFloat& rect, TextRow& textRow)
	{
		size_t numGlyphs = textRow.end - textRow.start;
		std::vector<NVGglyphPosition> positions;
		positions.resize(numGlyphs);
		textRow.glyphs.resize(numGlyphs);

		nvgTextGlyphPositions(m_ctx, rect.x(), rect.y(), textRow.start, textRow.end, &positions.front(), positions.size());

		for(size_t i = 0; i < positions.size(); ++i)
		{
			NVGglyphPosition& glyph = positions[i];
			TextGlyph& out = textRow.glyphs[i];
			out.position = textRow.start + i;
			out.rect.assign(glyph.minx, textRow.rect.y(), glyph.maxx - glyph.minx, textRow.rect.h());
		}
	}

	void NanoRenderer::drawText(float x, float y, const char* start, const char* end, InkStyle& skin)
	{
		this->setupText(skin);

		nvgFillColor(m_ctx, nvgColour(skin.m_textColour));
		nvgText(m_ctx, x, y, start, end);
	}

	void NanoRenderer::beginTarget()
	{
		nvgSave(m_ctx);
		nvgResetTransform(m_ctx);
		nvgResetScissor(m_ctx);
	}

	void NanoRenderer::endTarget()
	{
		nvgRestore(m_ctx);
	}

#ifdef TOYUI_DRAW_CACHE
	void NanoRenderer::layerCache(Layer& layer, void*& cache)
	{
		if(m_layers.find(&layer) == m_layers.end())
			m_layers[&layer] = nvgCreateDisplayList(-1);

		cache = m_layers[&layer];
	}

	void NanoRenderer::drawLayer(void* layerCache, float x, float y, float scale)
	{
		nvgSave(m_ctx);
		nvgTranslate(m_ctx, x, y);
		nvgScale(m_ctx, scale, scale);
		nvgDrawDisplayList(m_ctx, (NVGdisplayList*)layerCache);
		nvgRestore(m_ctx);
	}

	void NanoRenderer::clearLayer(void* layerCache)
	{
		nvgResetDisplayList((NVGdisplayList*)layerCache);
		//nvgResetScissor(m_ctx);
	}

	void NanoRenderer::beginUpdate(void* layerCache, float x, float y, float scale)
	{
		nvgBindDisplayList(m_ctx, (NVGdisplayList*)layerCache);
		nvgSave(m_ctx);
		nvgTranslate(m_ctx, x, y);
		nvgScale(m_ctx, scale, scale);

		++m_debugBatch;
	}

	void NanoRenderer::endUpdate()
	{
		nvgRestore(m_ctx);
		nvgBindDisplayList(m_ctx, nullptr);
	}

#else
	void NanoRenderer::beginUpdate(float x, float y)
	{
		nvgSave(m_ctx);
		nvgTranslate(m_ctx, x, y);
	}

	void NanoRenderer::endUpdate()
	{
		nvgRestore(m_ctx);
	}
#endif

	float NanoRenderer::textLineHeight(InkStyle& skin)
	{
		this->setupText(skin);
		return m_lineHeight;
	}

	float NanoRenderer::textSize(const string& text, Dimension dim, InkStyle& skin)
	{
		this->setupText(skin);

		float bounds[4];
		nvgTextBounds(m_ctx, 0.f, 0.f, text.c_str(), nullptr, bounds);

		return dim == DIM_X ? bounds[2] - bounds[0] : m_lineHeight;
	}
}
