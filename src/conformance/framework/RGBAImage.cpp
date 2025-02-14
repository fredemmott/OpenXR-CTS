// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "RGBAImage.h"

#include "utilities/colors.h"
#include "conformance_framework.h"
#include "report.h"

#ifdef XR_USE_PLATFORM_ANDROID
#include "common/unique_asset.h"

#include <android/asset_manager.h>
#endif

// Only one compilation unit can have the STB implementations.
#define STB_IMAGE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "stb/stb_truetype.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace
{
    // Convert R32G32B32A_FLOAT to R8G8B8A8_UNORM.
    Conformance::RGBA8Color AsRGBA(float r, float g, float b, float a)
    {
        return {{(uint8_t)(255 * r), (uint8_t)(255 * g), (uint8_t)(255 * b), (uint8_t)(255 * a)}};
    };

    // Cached TrueType font baked as glyphs.
    struct BakedFont
    {
        BakedFont(int pixelHeight)
        {
            const char* FontFileName = "SourceCodePro-Regular.otf";

#ifdef XR_USE_PLATFORM_ANDROID
            AAssetManager* assetManager = (AAssetManager*)Conformance_Android_Get_Asset_Manager();
            UniqueAsset asset(AAssetManager_open(assetManager, FontFileName, AASSET_MODE_BUFFER));

            if (!asset) {
                throw std::runtime_error((std::string("Unable to open font ") + FontFileName).c_str());
            }

            size_t length = AAsset_getLength(asset.get());
            const uint8_t* buf = (const uint8_t*)AAsset_getBuffer(asset.get());
            if (!buf) {
                throw std::runtime_error((std::string("Unable to open font ") + FontFileName).c_str());
            }
            std::vector<uint8_t> fontData(buf, buf + length);
#else
            std::ifstream file;
            file.open(FontFileName, std::ios::in | std::ios::binary);
            if (!file) {
                throw std::runtime_error((std::string("Unable to open font ") + FontFileName).c_str());
            }

            file.seekg(0, std::ios::end);
            std::vector<uint8_t> fontData((uint32_t)file.tellg());
            file.seekg(0, std::ios::beg);

            file.read(reinterpret_cast<char*>(fontData.data()), fontData.size());
#endif
            // This is just a starting size.
            m_bitmapWidth = 1024;
            m_bitmapHeight = 64;

        retry:
            glyphBitmap.resize(m_bitmapWidth * m_bitmapHeight);

            int res = stbtt_BakeFontBitmap(fontData.data(), 0, (float)pixelHeight, glyphBitmap.data(), m_bitmapWidth, m_bitmapHeight,
                                           StartChar, (int)m_bakedChars.size(), m_bakedChars.data());
            if (res == 0) {
                throw std::runtime_error((std::string("Unable to parse font") + FontFileName).c_str());
            }
            else if (res < 0) {
                // Bitmap was not big enough to fit so double size and try again.
                m_bitmapHeight *= 2;
                goto retry;
            }
        }

        static std::shared_ptr<const BakedFont> GetOrCreate(int pixelHeight)
        {
            std::unordered_map<int, std::shared_ptr<BakedFont>> s_bakedFonts;
            auto it = s_bakedFonts.find(pixelHeight);
            if (it == s_bakedFonts.end()) {
                std::shared_ptr<BakedFont> font = std::make_shared<BakedFont>(pixelHeight);
                s_bakedFonts.insert({pixelHeight, font});
                return font;
            }

            return it->second;
        }

        const stbtt_bakedchar& GetBakedChar(char c) const
        {
            const char safeChar = (c < StartChar || c > EndChar) ? '_' : c;
            return m_bakedChars[safeChar - StartChar];
        }

        const uint8_t* GetBakedCharRow(const stbtt_bakedchar& bc, int charY) const
        {
            return glyphBitmap.data() + ((charY + bc.y0) * m_bitmapWidth);
        }

    private:
        static constexpr int StartChar = ' ';  // 32
        static constexpr int EndChar = '~';    // 126

        std::vector<uint8_t> glyphBitmap;  // Glyphs are single channel
        std::array<stbtt_bakedchar, EndChar - StartChar + 1> m_bakedChars;
        int m_bitmapWidth;
        int m_bitmapHeight;
    };
}  // namespace

namespace Conformance
{
    RGBAImage::RGBAImage(int width, int height) : width(width), height(height)
    {
        pixels.resize(width * height);
    }

    /* static */ RGBAImage RGBAImage::Load(const char* path)
    {
        constexpr int RequiredComponents = 4;  // RGBA

        int width, height;

#ifdef XR_USE_PLATFORM_ANDROID
        stbi_uc* uc = nullptr;
        {
            AAssetManager* assetManager = (AAssetManager*)Conformance_Android_Get_Asset_Manager();
            UniqueAsset asset(AAssetManager_open(assetManager, path, AASSET_MODE_BUFFER));

            if (!asset) {
                throw std::runtime_error((std::string("Unable to load asset ") + path).c_str());
            }

            size_t length = AAsset_getLength(asset.get());

            auto buf = AAsset_getBuffer(asset.get());

            if (!buf) {
                throw std::runtime_error((std::string("Unable to load asset ") + path).c_str());
            }

            uc = stbi_load_from_memory((const stbi_uc*)buf, length, &width, &height, nullptr, RequiredComponents);
        }
#else

        stbi_uc* const uc = stbi_load(path, &width, &height, nullptr, RequiredComponents);
#endif
        if (uc == nullptr) {
            throw std::runtime_error((std::string("Unable to load file ") + path).c_str());
        }

        RGBAImage image(width, height);
        memcpy(image.pixels.data(), uc, width * height * RequiredComponents);

        stbi_image_free(uc);

        // Images loaded from files are assumed to be SRGB
        image.isSrgb = true;

        return image;
    }

    void RGBAImage::PutText(const XrRect2Di& rect, const char* text, int pixelHeight, XrColor4f color, WordWrap wordWrap)
    {
        const std::shared_ptr<const BakedFont> font = BakedFont::GetOrCreate(pixelHeight);
        if (!font)
            return;

        float xadvance = (float)rect.offset.x;
        int yadvance =
            rect.offset.y + (int)(pixelHeight * 0.8f);  // Adjust down because glyphs are relative to the font baseline. This is hacky.

        const char* const fullText = text;

        // Loop through each character and copy over the chracters' glyphs.
        for (; *text; text++) {
            if (*text == '\n') {
                xadvance = (float)rect.offset.x;
                yadvance += pixelHeight;
                continue;
            }

            // Word wrap.
            {
                float remainingWordWidth = 0;
                for (const char* w = text; *w > ' '; w++) {
                    const stbtt_bakedchar& bakedChar = font->GetBakedChar(*text);
                    remainingWordWidth += bakedChar.xadvance;
                }

                // Wrap to new line if there isn't enough room for this word.
                if (xadvance + remainingWordWidth > rect.offset.x + rect.extent.width) {
                    // But only if the word isn't longer than the destination.
                    if (remainingWordWidth <= (rect.extent.width - rect.offset.x)) {
                        if (wordWrap == WordWrap::Enabled) {
                            xadvance = (float)rect.offset.x;
                            yadvance += pixelHeight;
                        }
                        else {
                            ReportConsoleOnlyF("CTS dev warning: Would have wrapped this text but told to disable word wrap! Text: %s",
                                               fullText);
                        }
                    }
                }
            }

            const stbtt_bakedchar& bakedChar = font->GetBakedChar(*text);
            const int characterWidth = (int)(bakedChar.x1 - bakedChar.x0);
            const int characterHeight = (int)(bakedChar.y1 - bakedChar.y0);

            if ((xadvance + characterWidth) > (rect.offset.x + rect.extent.width)) {
                if (wordWrap == WordWrap::Enabled) {

                    // Wrap to new line if there isn't enough room for this char.
                    xadvance = (float)rect.offset.x;
                    yadvance += pixelHeight;
                }
                else {
                    ReportConsoleOnlyF("CTS dev warning: Would have wrapped this text but told to disable word wrap! Text: %s", fullText);
                }
            }

            // For each row of the glyph bitmap
            for (int cy = 0; cy < characterHeight; cy++) {
                // Compute the destination row in the image.
                const int destY = yadvance + cy + (int)bakedChar.yoff;
                if (destY < 0 || destY >= height || destY < rect.offset.y || destY >= rect.offset.y + rect.extent.height) {
                    continue;  // Don't bother copying if out of bounds.
                }

                // Get a pointer to the src and dest row.
                const uint8_t* const srcGlyphRow = font->GetBakedCharRow(bakedChar, cy);
                RGBA8Color* const destImageRow = pixels.data() + (destY * width);

                for (int cx = 0; cx < characterWidth; cx++) {
                    const int destX = (int)std::lround(bakedChar.xoff + xadvance) + cx;
                    if (destX < 0 || destX >= width || destX < rect.offset.x || destX >= rect.offset.x + rect.extent.width) {
                        continue;  // Don't bother copying if out of bounds.
                    }

                    // Glyphs are 0-255 intensity.
                    const uint8_t srcGlyphPixel = srcGlyphRow[cx + bakedChar.x0];

                    // Do blending (assuming premultiplication).
                    RGBA8Color pixel = destImageRow[destX];
                    pixel.Channels.R = (uint8_t)(srcGlyphPixel * color.r) + (pixel.Channels.R * (255 - srcGlyphPixel) / 255);
                    pixel.Channels.G = (uint8_t)(srcGlyphPixel * color.g) + (pixel.Channels.G * (255 - srcGlyphPixel) / 255);
                    pixel.Channels.B = (uint8_t)(srcGlyphPixel * color.b) + (pixel.Channels.B * (255 - srcGlyphPixel) / 255);
                    pixel.Channels.A = (uint8_t)(srcGlyphPixel * color.a) + (pixel.Channels.A * (255 - srcGlyphPixel) / 255);
                    destImageRow[destX] = pixel;
                }
            }

            xadvance += bakedChar.xadvance;
        }
    }

    void RGBAImage::DrawRect(int x, int y, int w, int h, XrColor4f color)
    {
        if (x + w > width || y + h > height) {
            throw std::out_of_range("Rectangle out of bounds");
        }

        const RGBA8Color color32 = AsRGBA(color.r, color.g, color.b, color.a);
        for (int row = 0; row < h; row++) {
            RGBA8Color* start = pixels.data() + ((row + y) * width) + x;
            for (int col = 0; col < w; col++) {
                *(start + col) = color32;
            }
        }
    }

    void RGBAImage::DrawRectBorder(int x, int y, int w, int h, int thickness, XrColor4f color)
    {
        if (x < 0 || y < 0 || w < 0 || h < 0 || x + w > width || y + h > height) {
            throw std::out_of_range("Rectangle out of bounds");
        }

        const RGBA8Color color32 = AsRGBA(color.r, color.g, color.b, color.a);
        for (int row = 0; row < h; row++) {
            RGBA8Color* start = pixels.data() + ((row + y) * width) + x;
            if (row < thickness || row >= h - thickness) {
                for (int col = 0; col < w; col++) {
                    *(start + col) = color32;
                }
            }
            else {
                int leftBorderEnd = std::min(thickness, w);
                for (int col = 0; col < leftBorderEnd; col++) {
                    *(start + col) = color32;
                }

                int rightBorderBegin = std::max(w - thickness, 0);
                for (int col = rightBorderBegin; col < w; col++) {
                    *(start + col) = color32;
                }
            }
        }
    }

    void RGBAImage::ConvertToSRGB()
    {
        for (RGBA8Color& pixel : pixels) {
            pixel.Channels.R = (uint8_t)(ColorUtils::ToSRGB((double)pixel.Channels.R / 255.0) * 255.0);
            pixel.Channels.G = (uint8_t)(ColorUtils::ToSRGB((double)pixel.Channels.G / 255.0) * 255.0);
            pixel.Channels.B = (uint8_t)(ColorUtils::ToSRGB((double)pixel.Channels.B / 255.0) * 255.0);
        }
    }

    void RGBAImage::CopyWithStride(uint8_t* data, uint32_t rowPitch, uint32_t offset) const
    {
        Conformance::CopyWithStride(reinterpret_cast<const uint8_t*>(pixels.data()), data + offset, width * sizeof(RGBA8Color), height,
                                    rowPitch);
    }

    void RGBAImageCache::Init()
    {
        if (!m_cacheMutex) {
            m_cacheMutex = std::make_unique<std::mutex>();
        }
    }

    std::shared_ptr<RGBAImage> RGBAImageCache::Load(const char* path)
    {
        if (!IsValid()) {
            throw std::logic_error("RGBAImageCache accessed before initialization");
        }

        // Check cache to see if this image already exists.
        {
            std::lock_guard<std::mutex> guard(*m_cacheMutex);
            auto imageIt = m_imageCache.find(path);
            if (imageIt != m_imageCache.end()) {
                return imageIt->second;
            }
        }

        ReportConsoleOnlyF("Loading and caching image: %s", path);

        auto image = std::make_shared<RGBAImage>(RGBAImage::Load(path));

        std::lock_guard<std::mutex> guard(*m_cacheMutex);
        // If the key already exists then the existing image will be returned.
        return m_imageCache.emplace(path, image).first->second;
    }

    void CopyWithStride(const uint8_t* source, uint8_t* dest, uint32_t rowSize, uint32_t rows, uint32_t rowPitch)
    {
        for (size_t row = 0; row < rows; ++row) {
            uint8_t* rowPtr = &dest[row * rowPitch];
            memcpy(rowPtr, &source[row * rowSize], rowSize);
        }
    }

}  // namespace Conformance
