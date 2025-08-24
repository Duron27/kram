// kram - Copyright 2020-2025 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

//#include <string>
//#include <vector>

#include "KTXImage.h" // for MyMTLTextureType
//#include "KramConfig.h"
#include "KramImageInfo.h"
#include "KramMipper.h"

namespace kram {

using namespace STL_NAMESPACE;
using namespace SIMD_NAMESPACE;

class Mipper;
class KTXHeader;
class TextureData;

enum ImageResizeFilter {
    kImageResizeFilterPoint,
    //kImageResizeFilterLinear,
    //kImageResizeFilterLanczos3, Mitchell, Kaiser, etc,
};

//---------------------------

struct MipConstructData;

// TODO: this can only hold one level of mips, so custom mips aren't possible.
// Mipmap generation is all in-place to this storage.
// Multiple chunks are possible in strip or grid form.
class Image {
public:
    Image();

    // these 3 calls for Encode
    bool loadImageFromPixels(const vector<Color>& pixels,
                             int32_t width, int32_t height,
                             bool hasColor, bool hasAlpha);

    // set state off png blocks
    void setSrgbState(bool isSrgb, bool hasSrgbBlock, bool hasNonSrgbBlocks);
    void setBackgroundState(bool hasBlackBackground) { _hasBlackBackground = hasBlackBackground; }

    // convert mip level of explicit format to single-image
    bool loadImageFromKTX(const KTXImage& image, uint32_t mipNumber = 0);

    // convert mip level of explicit format to single-image thumbnail
    bool loadThumbnailFromKTX(const KTXImage& image, uint32_t mipNumber);

    // this is only for 2d images
    bool resizeImage(int32_t wResize, int32_t hResize, bool resizePow2, ImageResizeFilter filter = kImageResizeFilterPoint);
    
    // flip image vertically (swaps top and bottom rows)
    void flipVertical();

    // this is width and height of the strip/grid, chunks may be copied out of this
    int32_t width() const { return _width; }
    int32_t height() const { return _height; }

    const vector<Color>& pixels() const { return _pixels; }
    const vector<float4>& pixelsFloat() const { return _pixelsFloat; }

    // content analysis
    bool hasColor() const { return _hasColor; }
    bool hasAlpha() const { return _hasAlpha; }

    // only for png files, detects ICCP/CHRM/GAMA blocks vs. sRGB block
    // so that these can be stripped by fixup -srgb
    bool isSrgb() const { return _isSrgb; }
    bool hasSrgbBlock() const { return _hasSrgbBlock; }
    bool hasNonSrgbBlocks() const { return _hasNonSrgbBlocks; }

    bool hasBlackBackground() const { return _hasBlackBackground; }

    // if converted a KTX/2 image to Image, then this field will be non-zero
    uint32_t chunksY() const { return _chunksY; }
    void setChunksY(uint32_t chunksY) { _chunksY = chunksY; }

private:
    // convert r/rg/rgb to rgba, 16f -> 32f
    bool convertToFourChannel(const KTXImage& image, uint32_t mipNumber);

    // converts all to rgba8unorm
    bool convertToFourChannelForThumbnail(const KTXImage& image, uint32_t mipNumber);

private:
    // pixel size of image
    int32_t _width = 0;
    int32_t _height = 0;

    // this is whether png/ktx source image  format was L or LA or A or RGB
    // if unknown then set to true, and the pixel walk will set to false
    bool _hasColor = true;
    bool _hasAlpha = true;

    // track to fix incorrect sRGB state from Figma/Photoshop on PNG files
    bool _isSrgb = false;
    bool _hasNonSrgbBlocks = false;
    bool _hasSrgbBlock = false;

    // track to fix Apple Finder previews that are always white background
    bool _hasBlackBackground = false;

    // this is the entire strip data, float version can be passed for HDR
    // sources always 4 channels RGBA for 8 and 32f data.  16f promoted to 32f.
    vector<Color> _pixels;
    //vector<half4> _pixelsHalf; // TODO: add support to import fp16
    vector<float4> _pixelsFloat;

    uint32_t _chunksY = 0;
};

class KramDecoderParams {
public:
    TexEncoder decoder = kTexEncoderUnknown; // will pick best available from format
    bool isVerbose = false;
    string swizzleText;
};

// The decoder can decode an entire KTX/KTX2 into RGBA8u/16F/32F data.
// This is useful on platforms to display formats unsupported by the gpu, but the expanded pixels
// can take up much more memory.
class KramDecoder {
public:
    bool decode(const KTXImage& image, FILE* dstFile, const KramDecoderParams& params) const;

    bool decode(const KTXImage& image, KTXImage& dstImage, const KramDecoderParams& params) const;

    bool decodeBlocks(
        int32_t w, int32_t h,
        const uint8_t* blockData, uint32_t numBlocks, MyMTLPixelFormat blockFormat,
        vector<uint8_t>& dstPixels, // currently Color
        const KramDecoderParams& params) const;

private:
    bool decodeImpl(const KTXImage& srcImage, FILE* dstFile, KTXImage& dstImage, const KramDecoderParams& params) const;
};

// The encoder takes a single-mip image, and in-place encodes mips and applies other
// requested operations from ImageInfo as it writes those mips.   Note that KTX2 must
// accumulate all mips if compressed so that offsets of where to write data are known.
class KramEncoder {
public:
    // encode/ecode to a file
    bool encode(ImageInfo& info, Image& singleImage, FILE* dstFile) const;

    // encode/decode to a memory block
    bool encode(ImageInfo& info, Image& singleImage, KTXImage& dstImage) const;

    // can save out to ktx1 directly, if say imported from dds
    bool saveKTX1(const KTXImage& image, FILE* dstFile) const;

    // can save out to ktx2 directly, this can supercompress mips
    bool saveKTX2(const KTXImage& srcImage, const KTX2Compressor& compressor, FILE* dstFile) const;

private:
    bool encodeImpl(ImageInfo& info, Image& singleImage, FILE* dstFile, KTXImage& dstImage) const;

    // compute how big mips will be
    void computeMipStorage(const KTXImage& image, int32_t& w, int32_t& h, int32_t& numSkippedMips,
                           bool doMipmaps, int32_t mipMinSize, int32_t mipMaxSize,
                           vector<KTXImageLevel>& dstMipLevels) const;

    // ugh, reduce the params into this
    bool compressMipLevel(const ImageInfo& info, KTXImage& image,
                          ImageData& mipImage, TextureData& outputTexture,
                          int32_t mipStorageSize) const;

    // can pass in which channels to average
    void averageChannelsInBlock(const char* averageChannels,
                                const KTXImage& image, ImageData& srcImage) const;

    bool createMipsFromChunks(ImageInfo& info,
                              Image& singleImage,
                              MipConstructData& data,
                              FILE* dstFile, KTXImage& dstImage) const;

    bool writeKTX1FileOrImage(
        ImageInfo& info,
        Image& singleImage,
        MipConstructData& mipConstructData,
        FILE* dstFile, KTXImage& dstImage) const;

    void addBaseProps(const ImageInfo& info, KTXImage& dstImage) const;
};

} // namespace kram
