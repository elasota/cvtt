//-------------------------------------------------------------------------------------
// DirectXTexCompress.cpp
//  
// DirectX Texture Library - Texture compression
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//-------------------------------------------------------------------------------------

#include "directxtexp.h"

#ifdef _OPENMP
#include <omp.h>
#pragma warning(disable : 4616 6993)
#endif

#include "bc.h"

using namespace DirectX;

namespace
{
    inline DWORD GetBCFlags(_In_ DWORD compress)
    {
        static_assert(static_cast<int>(TEX_COMPRESS_RGB_DITHER) == static_cast<int>(BC_FLAGS_DITHER_RGB), "TEX_COMPRESS_* flags should match BC_FLAGS_*");
        static_assert(static_cast<int>(TEX_COMPRESS_A_DITHER) == static_cast<int>(BC_FLAGS_DITHER_A), "TEX_COMPRESS_* flags should match BC_FLAGS_*");
        static_assert(static_cast<int>(TEX_COMPRESS_DITHER) == static_cast<int>(BC_FLAGS_DITHER_RGB | BC_FLAGS_DITHER_A), "TEX_COMPRESS_* flags should match BC_FLAGS_*");
        static_assert(static_cast<int>(TEX_COMPRESS_UNIFORM) == static_cast<int>(BC_FLAGS_UNIFORM), "TEX_COMPRESS_* flags should match BC_FLAGS_*");
        return (compress & (BC_FLAGS_DITHER_RGB | BC_FLAGS_DITHER_A | BC_FLAGS_UNIFORM));
    }

    inline DWORD GetSRGBFlags(_In_ DWORD compress)
    {
        static_assert(static_cast<int>(TEX_COMPRESS_SRGB_IN) == static_cast<int>(TEX_FILTER_SRGB_IN), "TEX_COMPRESS_SRGB* should match TEX_FILTER_SRGB*");
        static_assert(static_cast<int>(TEX_COMPRESS_SRGB_OUT) == static_cast<int>(TEX_FILTER_SRGB_OUT), "TEX_COMPRESS_SRGB* should match TEX_FILTER_SRGB*");
        static_assert(static_cast<int>(TEX_COMPRESS_SRGB) == static_cast<int>(TEX_FILTER_SRGB), "TEX_COMPRESS_SRGB* should match TEX_FILTER_SRGB*");
        return (compress & TEX_COMPRESS_SRGB);
    }

    inline bool DetermineEncoderSettings(_In_ DXGI_FORMAT format, _Out_ BC_ENCODE& pfEncode, _Out_ BC_CONFIGURE& pfConfigure, _Out_ size_t& blocksize, _Out_ DWORD& cflags, _Out_ int& nBlocksPerChunk)
    {
        pfConfigure = D3DXConfigureParallel;

        switch (format)
        {
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:    pfEncode = D3DXEncodeBC1Parallel;   blocksize = 8;   cflags = 0; nBlocksPerChunk = NUM_PARALLEL_BLOCKS; break;
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:    pfEncode = D3DXEncodeBC2Parallel;   blocksize = 16;  cflags = 0; nBlocksPerChunk = NUM_PARALLEL_BLOCKS; break;
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:    pfEncode = D3DXEncodeBC3Parallel;   blocksize = 16;  cflags = 0; nBlocksPerChunk = NUM_PARALLEL_BLOCKS; break;
        case DXGI_FORMAT_BC4_UNORM:         pfEncode = D3DXEncodeBC4UParallel;  blocksize = 8;   cflags = TEX_FILTER_RGB_COPY_RED; nBlocksPerChunk = NUM_PARALLEL_BLOCKS; break;
        case DXGI_FORMAT_BC4_SNORM:         pfEncode = D3DXEncodeBC4SParallel;  blocksize = 8;   cflags = TEX_FILTER_RGB_COPY_RED; nBlocksPerChunk = NUM_PARALLEL_BLOCKS; break;
        case DXGI_FORMAT_BC5_UNORM:         pfEncode = D3DXEncodeBC5UParallel;  blocksize = 16;  cflags = TEX_FILTER_RGB_COPY_RED | TEX_FILTER_RGB_COPY_GREEN; nBlocksPerChunk = NUM_PARALLEL_BLOCKS; break;
        case DXGI_FORMAT_BC5_SNORM:         pfEncode = D3DXEncodeBC5SParallel;  blocksize = 16;  cflags = TEX_FILTER_RGB_COPY_RED | TEX_FILTER_RGB_COPY_GREEN; nBlocksPerChunk = NUM_PARALLEL_BLOCKS; break;
        case DXGI_FORMAT_BC6H_UF16:         pfEncode = D3DXEncodeBC6HUParallel; blocksize = 16;  cflags = 0; nBlocksPerChunk = NUM_PARALLEL_BLOCKS; break;
        case DXGI_FORMAT_BC6H_SF16:         pfEncode = D3DXEncodeBC6HSParallel; blocksize = 16;  cflags = 0; nBlocksPerChunk = NUM_PARALLEL_BLOCKS; break;
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:    pfEncode = D3DXEncodeBC7Parallel; pfConfigure = D3DXConfigureBC7Parallel;  blocksize = 16; cflags = 0; nBlocksPerChunk = NUM_PARALLEL_BLOCKS; break;
        default:                            pfEncode = nullptr;         blocksize = 0;   cflags = 0; nBlocksPerChunk = 1; return false;
        }

        return true;
    }


    //-------------------------------------------------------------------------------------
    HRESULT CompressBC(
        const Image& image,
        const Image& result,
        DWORD srgb,
        const TexCompressOptions &options)
    {
        if (!image.pixels || !result.pixels)
            return E_POINTER;

        assert(image.width == result.width);
        assert(image.height == result.height);

        const DXGI_FORMAT format = image.format;
        size_t sbpp = BitsPerPixel(format);
        if (!sbpp)
            return E_FAIL;

        if (sbpp < 8)
        {
            // We don't support compressing from monochrome (DXGI_FORMAT_R1_UNORM)
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        // Round to bytes
        sbpp = (sbpp + 7) / 8;

        uint8_t *pDest = result.pixels;

        // Determine BC format encoder
        BC_ENCODE pfEncode;
        BC_CONFIGURE pfConfigure;
        size_t blocksize;
        DWORD cflags;
        int nBlocksPerChunk = 0;
        if (!DetermineEncoderSettings(result.format, pfEncode, pfConfigure, blocksize, cflags, nBlocksPerChunk))
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

        TexCompressConfiguration *config = pfConfigure(options);
        if (!config)
            return E_OUTOFMEMORY;

        __declspec(align(16)) XMVECTOR tempBlocks[16 * MAX_PARALLEL_BLOCKS];
        const uint8_t *pSrc = image.pixels;
        const uint8_t *pEnd = image.pixels + image.slicePitch;
        const size_t rowPitch = image.rowPitch;
        for (size_t h = 0; h < image.height; h += 4)
        {
            const uint8_t *sptr = pSrc;
            uint8_t* dptr = pDest;
            size_t ph = std::min<size_t>(4, image.height - h);
            size_t w = 0;

            int nQueuedBlocks = 0;
            for (size_t count = 0; (count < result.rowPitch) && (w < image.width); count += blocksize, w += 4)
            {
                XMVECTOR *temp = tempBlocks + nQueuedBlocks * 16;

                size_t pw = std::min<size_t>(4, image.width - w);
                assert(pw > 0 && ph > 0);

                ptrdiff_t bytesLeft = pEnd - sptr;
                assert(bytesLeft > 0);
                size_t bytesToRead = std::min<size_t>(rowPitch, bytesLeft);
                if (!_LoadScanline(&temp[0], pw, sptr, bytesToRead, format))
                    return E_FAIL;

                if (ph > 1)
                {
                    bytesToRead = std::min<size_t>(rowPitch, bytesLeft - rowPitch);
                    if (!_LoadScanline(&temp[4], pw, sptr + rowPitch, bytesToRead, format))
                        return E_FAIL;

                    if (ph > 2)
                    {
                        bytesToRead = std::min<size_t>(rowPitch, bytesLeft - rowPitch * 2);
                        if (!_LoadScanline(&temp[8], pw, sptr + rowPitch * 2, bytesToRead, format))
                            return E_FAIL;

                        if (ph > 3)
                        {
                            bytesToRead = std::min<size_t>(rowPitch, bytesLeft - rowPitch * 3);
                            if (!_LoadScanline(&temp[12], pw, sptr + rowPitch * 3, bytesToRead, format))
                                return E_FAIL;
                        }
                    }
                }

                if (pw != 4 || ph != 4)
                {
                    // Replicate pixels for partial block
                    static const size_t uSrc[] = { 0, 0, 0, 1 };

                    if (pw < 4)
                    {
                        for (size_t t = 0; t < ph && t < 4; ++t)
                        {
                            for (size_t s = pw; s < 4; ++s)
                            {
#pragma prefast(suppress: 26000, "PREFAST false positive")
                                temp[(t << 2) | s] = temp[(t << 2) | uSrc[s]];
                            }
                        }
                    }

                    if (ph < 4)
                    {
                        for (size_t t = ph; t < 4; ++t)
                        {
                            for (size_t s = 0; s < 4; ++s)
                            {
#pragma prefast(suppress: 26000, "PREFAST false positive")
                                temp[(t << 2) | s] = temp[(uSrc[t] << 2) | s];
                            }
                        }
                    }
                }

                _ConvertScanline(temp, 16, result.format, format, cflags | srgb);

                nQueuedBlocks++;

                if (nQueuedBlocks == nBlocksPerChunk)
                {
                    assert(pfEncode);
                    pfEncode(dptr, tempBlocks, *config);

                    dptr += blocksize * nBlocksPerChunk;
                    nQueuedBlocks = 0;
                }

                sptr += sbpp * 4;
            }

            if (nQueuedBlocks != 0)
            {
                uint8_t scratch[MAX_BLOCK_SIZE * MAX_PARALLEL_BLOCKS];

                for (int i = nQueuedBlocks; i < nBlocksPerChunk; i++)
                    for (int element = 0; element < NUM_PIXELS_PER_BLOCK; element++)
                        tempBlocks[i * NUM_PIXELS_PER_BLOCK + element] = XMVectorSet(0.f, 0.f, 0.f, 0.f);

                assert(pfEncode);
                pfEncode(scratch, tempBlocks, *config);

                memcpy(dptr, scratch, blocksize * nQueuedBlocks);
                dptr += blocksize * nQueuedBlocks;
                nQueuedBlocks = 0;
            }

            pSrc += rowPitch * 4;
            pDest += result.rowPitch;
        }

        config->Release();

        return S_OK;
    }


    //-------------------------------------------------------------------------------------
#ifdef _OPENMP
    HRESULT CompressBC_Parallel(
        const Image& image,
        const Image& result,
        DWORD srgb,
        const TexCompressOptions &options)
    {
        if (!image.pixels || !result.pixels)
            return E_POINTER;

        assert(image.width == result.width);
        assert(image.height == result.height);

        const DXGI_FORMAT format = image.format;
        size_t sbpp = BitsPerPixel(format);
        if (!sbpp)
            return E_FAIL;

        if (sbpp < 8)
        {
            // We don't support compressing from monochrome (DXGI_FORMAT_R1_UNORM)
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        // Round to bytes
        sbpp = (sbpp + 7) / 8;

        const uint8_t *pEnd = image.pixels + image.slicePitch;

        // Determine BC format encoder
        BC_ENCODE pfEncode;
        BC_CONFIGURE pfConfigure;
        size_t blocksize;
        DWORD cflags;
        int nBlocksPerChunk;
        if (!DetermineEncoderSettings(result.format, pfEncode, pfConfigure, blocksize, cflags, nBlocksPerChunk))
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

        TexCompressConfiguration *config = pfConfigure(options);
        if (!config)
            return E_OUTOFMEMORY;

        // Refactored version of loop to support parallel independance
        const size_t nBlocks = std::max<size_t>(1, (image.width + 3) / 4) * std::max<size_t>(1, (image.height + 3) / 4);

        bool fail = false;

#pragma omp parallel for
        for (int nbBase = 0; nbBase < static_cast<int>(nBlocks); nbBase += nBlocksPerChunk)
        {
            __declspec(align(16)) XMVECTOR tempBlocks[16 * MAX_PARALLEL_BLOCKS];

            int numProcessableBlocks = std::min<int>(static_cast<int>(nBlocks) - nbBase, nBlocksPerChunk);

            for (int subBlock = 0; subBlock < numProcessableBlocks; subBlock++)
            {
                XMVECTOR *temp = tempBlocks + subBlock * NUM_PIXELS_PER_BLOCK;
                int nb = nbBase + subBlock;
                if (nb >= static_cast<int>(nBlocks))
                {
                    for (int i = 0; i < 16; i++)
                        temp[i] = XMVectorSet(0.f, 0.f, 0.f, 0.f);
                    continue;
                }

                int nbWidth = std::max<int>(1, int((image.width + 3) / 4));

                int y = nb / nbWidth;
                int x = (nb - (y*nbWidth)) * 4;
                y *= 4;

                assert((x >= 0) && (x < int(image.width)));
                assert((y >= 0) && (y < int(image.height)));

                size_t rowPitch = image.rowPitch;
                const uint8_t *pSrc = image.pixels + (y*rowPitch) + (x*sbpp);

                size_t ph = std::min<size_t>(4, image.height - y);
                size_t pw = std::min<size_t>(4, image.width - x);
                assert(pw > 0 && ph > 0);

                ptrdiff_t bytesLeft = pEnd - pSrc;
                assert(bytesLeft > 0);
                size_t bytesToRead = std::min<size_t>(rowPitch, bytesLeft);

                if (!_LoadScanline(&temp[0], pw, pSrc, bytesToRead, format))
                    fail = true;

                if (ph > 1)
                {
                    bytesToRead = std::min<size_t>(rowPitch, bytesLeft - rowPitch);
                    if (!_LoadScanline(&temp[4], pw, pSrc + rowPitch, bytesToRead, format))
                        fail = true;

                    if (ph > 2)
                    {
                        bytesToRead = std::min<size_t>(rowPitch, bytesLeft - rowPitch * 2);
                        if (!_LoadScanline(&temp[8], pw, pSrc + rowPitch * 2, bytesToRead, format))
                            fail = true;

                        if (ph > 3)
                        {
                            bytesToRead = std::min<size_t>(rowPitch, bytesLeft - rowPitch * 3);
                            if (!_LoadScanline(&temp[12], pw, pSrc + rowPitch * 3, bytesToRead, format))
                                fail = true;
                        }
                    }
                }

                if (pw != 4 || ph != 4)
                {
                    // Replicate pixels for partial block
                    static const size_t uSrc[] = { 0, 0, 0, 1 };

                    if (pw < 4)
                    {
                        for (size_t t = 0; t < ph && t < 4; ++t)
                        {
                            for (size_t s = pw; s < 4; ++s)
                            {
                                temp[(t << 2) | s] = temp[(t << 2) | uSrc[s]];
                            }
                        }
                    }

                    if (ph < 4)
                    {
                        for (size_t t = ph; t < 4; ++t)
                        {
                            for (size_t s = 0; s < 4; ++s)
                            {
                                temp[(t << 2) | s] = temp[(uSrc[t] << 2) | s];
                            }
                        }
                    }
                }

                _ConvertScanline(temp, 16, result.format, format, cflags | srgb);
            }

            for (int fillBlock = numProcessableBlocks; fillBlock < nBlocksPerChunk; fillBlock++)
            {
                for (int element = 0; element < NUM_PIXELS_PER_BLOCK; element++)
                    tempBlocks[fillBlock * NUM_PIXELS_PER_BLOCK + element] = XMVectorSet(0.f, 0.f, 0.f, 0.f);
            }

            uint8_t *pDest = result.pixels + (nbBase*blocksize);

            if (numProcessableBlocks == nBlocksPerChunk)
            {
                assert(pfEncode);
                pfEncode(pDest, tempBlocks, *config);
            }
            else
            {
                uint8_t scratch[MAX_BLOCK_SIZE * MAX_PARALLEL_BLOCKS];

                assert(pfEncode);
                pfEncode(scratch, tempBlocks, *config);

                memcpy(pDest, scratch, numProcessableBlocks * blocksize);
            }
        }

        config->Release();

        return (fail) ? E_FAIL : S_OK;
    }
#endif // _OPENMP


    //-------------------------------------------------------------------------------------
    DXGI_FORMAT DefaultDecompress(_In_ DXGI_FORMAT format)
    {
        switch (format)
        {
        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC7_TYPELESS:
        case DXGI_FORMAT_BC7_UNORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM;

        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
            return DXGI_FORMAT_R8_UNORM;

        case DXGI_FORMAT_BC4_SNORM:
            return DXGI_FORMAT_R8_SNORM;

        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_UNORM:
            return DXGI_FORMAT_R8G8_UNORM;

        case DXGI_FORMAT_BC5_SNORM:
            return DXGI_FORMAT_R8G8_SNORM;

        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
            // We could use DXGI_FORMAT_R32G32B32_FLOAT here since BC6H is always Alpha 1.0,
            // but this format is more supported by viewers
            return DXGI_FORMAT_R32G32B32A32_FLOAT;

        default:
            return DXGI_FORMAT_UNKNOWN;
        }
    }


    //-------------------------------------------------------------------------------------
    HRESULT DecompressBC(_In_ const Image& cImage, _In_ const Image& result)
    {
        if (!cImage.pixels || !result.pixels)
            return E_POINTER;

        assert(cImage.width == result.width);
        assert(cImage.height == result.height);

        const DXGI_FORMAT format = result.format;
        size_t dbpp = BitsPerPixel(format);
        if (!dbpp)
            return E_FAIL;

        if (dbpp < 8)
        {
            // We don't support decompressing to monochrome (DXGI_FORMAT_R1_UNORM)
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        // Round to bytes
        dbpp = (dbpp + 7) / 8;

        uint8_t *pDest = result.pixels;
        if (!pDest)
            return E_POINTER;

        // Promote "typeless" BC formats
        DXGI_FORMAT cformat;
        switch (cImage.format)
        {
        case DXGI_FORMAT_BC1_TYPELESS:  cformat = DXGI_FORMAT_BC1_UNORM; break;
        case DXGI_FORMAT_BC2_TYPELESS:  cformat = DXGI_FORMAT_BC2_UNORM; break;
        case DXGI_FORMAT_BC3_TYPELESS:  cformat = DXGI_FORMAT_BC3_UNORM; break;
        case DXGI_FORMAT_BC4_TYPELESS:  cformat = DXGI_FORMAT_BC4_UNORM; break;
        case DXGI_FORMAT_BC5_TYPELESS:  cformat = DXGI_FORMAT_BC5_UNORM; break;
        case DXGI_FORMAT_BC6H_TYPELESS: cformat = DXGI_FORMAT_BC6H_UF16; break;
        case DXGI_FORMAT_BC7_TYPELESS:  cformat = DXGI_FORMAT_BC7_UNORM; break;
        default:                        cformat = cImage.format;         break;
        }

        // Determine BC format decoder
        BC_DECODE pfDecode;
        size_t sbpp;
        switch (cformat)
        {
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:    pfDecode = D3DXDecodeBC1;   sbpp = 8;   break;
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:    pfDecode = D3DXDecodeBC2;   sbpp = 16;  break;
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:    pfDecode = D3DXDecodeBC3;   sbpp = 16;  break;
        case DXGI_FORMAT_BC4_UNORM:         pfDecode = D3DXDecodeBC4U;  sbpp = 8;   break;
        case DXGI_FORMAT_BC4_SNORM:         pfDecode = D3DXDecodeBC4S;  sbpp = 8;   break;
        case DXGI_FORMAT_BC5_UNORM:         pfDecode = D3DXDecodeBC5U;  sbpp = 16;  break;
        case DXGI_FORMAT_BC5_SNORM:         pfDecode = D3DXDecodeBC5S;  sbpp = 16;  break;
        case DXGI_FORMAT_BC6H_UF16:         pfDecode = D3DXDecodeBC6HU; sbpp = 16;  break;
        case DXGI_FORMAT_BC6H_SF16:         pfDecode = D3DXDecodeBC6HS; sbpp = 16;  break;
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:    pfDecode = D3DXDecodeBC7;   sbpp = 16;  break;
        default:
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        __declspec(align(16)) XMVECTOR temp[16];
        const uint8_t *pSrc = cImage.pixels;
        const size_t rowPitch = result.rowPitch;
        for (size_t h = 0; h < cImage.height; h += 4)
        {
            const uint8_t *sptr = pSrc;
            uint8_t* dptr = pDest;
            size_t ph = std::min<size_t>(4, cImage.height - h);
            size_t w = 0;
            for (size_t count = 0; (count < cImage.rowPitch) && (w < cImage.width); count += sbpp, w += 4)
            {
                pfDecode(temp, sptr);
                _ConvertScanline(temp, 16, format, cformat, 0);

                size_t pw = std::min<size_t>(4, cImage.width - w);
                assert(pw > 0 && ph > 0);

                if (!_StoreScanline(dptr, rowPitch, format, &temp[0], pw))
                    return E_FAIL;

                if (ph > 1)
                {
                    if (!_StoreScanline(dptr + rowPitch, rowPitch, format, &temp[4], pw))
                        return E_FAIL;

                    if (ph > 2)
                    {
                        if (!_StoreScanline(dptr + rowPitch * 2, rowPitch, format, &temp[8], pw))
                            return E_FAIL;

                        if (ph > 3)
                        {
                            if (!_StoreScanline(dptr + rowPitch * 3, rowPitch, format, &temp[12], pw))
                                return E_FAIL;
                        }
                    }
                }

                sptr += sbpp;
                dptr += dbpp * 4;
            }

            pSrc += cImage.rowPitch;
            pDest += rowPitch * 4;
        }

        return S_OK;
    }
}

//-------------------------------------------------------------------------------------
namespace DirectX
{
    bool _IsAlphaAllOpaqueBC(_In_ const Image& cImage)
    {
        if (!cImage.pixels)
            return false;

        // Promote "typeless" BC formats
        DXGI_FORMAT cformat;
        switch (cImage.format)
        {
        case DXGI_FORMAT_BC1_TYPELESS:  cformat = DXGI_FORMAT_BC1_UNORM; break;
        case DXGI_FORMAT_BC2_TYPELESS:  cformat = DXGI_FORMAT_BC2_UNORM; break;
        case DXGI_FORMAT_BC3_TYPELESS:  cformat = DXGI_FORMAT_BC3_UNORM; break;
        case DXGI_FORMAT_BC7_TYPELESS:  cformat = DXGI_FORMAT_BC7_UNORM; break;
        default:                        cformat = cImage.format;         break;
        }

        // Determine BC format decoder
        BC_DECODE pfDecode;
        size_t sbpp;
        switch (cformat)
        {
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:    pfDecode = D3DXDecodeBC1;   sbpp = 8;   break;
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:    pfDecode = D3DXDecodeBC2;   sbpp = 16;  break;
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:    pfDecode = D3DXDecodeBC3;   sbpp = 16;  break;
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:    pfDecode = D3DXDecodeBC7;   sbpp = 16;  break;
        default:
            // BC4, BC5, and BC6 don't have alpha channels
            return false;
        }

        // Scan blocks for non-opaque alpha
        static const XMVECTORF32 threshold = { { { 0.99f, 0.99f, 0.99f, 0.99f } } };

        __declspec(align(16)) XMVECTOR temp[16];
        const uint8_t *pPixels = cImage.pixels;
        for (size_t h = 0; h < cImage.height; h += 4)
        {
            const uint8_t *ptr = pPixels;
            size_t ph = std::min<size_t>(4, cImage.height - h);
            size_t w = 0;
            for (size_t count = 0; (count < cImage.rowPitch) && (w < cImage.width); count += sbpp, w += 4)
            {
                pfDecode(temp, ptr);

                size_t pw = std::min<size_t>(4, cImage.width - w);
                assert(pw > 0 && ph > 0);

                if (pw == 4 && ph == 4)
                {
                    // Full blocks
                    for (size_t j = 0; j < 16; ++j)
                    {
                        XMVECTOR alpha = XMVectorSplatW(temp[j]);
                        if (XMVector4Less(alpha, threshold))
                            return false;
                    }
                }
                else
                {
                    // Handle partial blocks
                    for (size_t y = 0; y < ph; ++y)
                    {
                        for (size_t x = 0; x < pw; ++x)
                        {
                            XMVECTOR alpha = XMVectorSplatW(temp[y * 4 + x]);
                            if (XMVector4Less(alpha, threshold))
                                return false;
                        }
                    }
                }

                ptr += sbpp;
            }

            pPixels += cImage.rowPitch;
        }

        return true;
    }
};


//=====================================================================================
// Entry-points
//=====================================================================================

//-------------------------------------------------------------------------------------
// Compression
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::CompressEx(
    const Image& srcImage,
    DXGI_FORMAT format,
    const TexCompressOptions &options,
    ScratchImage& image)
{
    if (IsCompressed(srcImage.format) || !IsCompressed(format))
        return E_INVALIDARG;

    if (IsTypeless(format)
        || IsTypeless(srcImage.format) || IsPlanar(srcImage.format) || IsPalettized(srcImage.format))
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

    // Create compressed image
    HRESULT hr = image.Initialize2D(format, srcImage.width, srcImage.height, 1, 1);
    if (FAILED(hr))
        return hr;

    const Image *img = image.GetImage(0, 0, 0);
    if (!img)
    {
        image.Release();
        return E_POINTER;
    }

    // Compress single image
    if (options.flags & TEX_COMPRESS_PARALLEL)
    {
#ifndef _OPENMP
        return E_NOTIMPL;
#else
        hr = CompressBC_Parallel(srcImage, *img, GetSRGBFlags(options.flags), options);
#endif // _OPENMP
    }
    else
    {
        hr = CompressBC(srcImage, *img, GetSRGBFlags(options.flags), options);
    }

    if (FAILED(hr))
        image.Release();

    return hr;
}

_Use_decl_annotations_
HRESULT DirectX::Compress(
    const Image& srcImage,
    DXGI_FORMAT format,
    DWORD compress,
    float threshold,
    ScratchImage& image)
{
    TexCompressOptions options;

    options.threshold = threshold;
    options.flags = compress;
    return CompressEx(srcImage, format, options, image);
}

_Use_decl_annotations_
HRESULT DirectX::CompressEx(
    const Image* srcImages,
    size_t nimages,
    const TexMetadata& metadata,
    DXGI_FORMAT format,
    const TexCompressOptions &options,
    ScratchImage& cImages)
{
    if (!srcImages || !nimages)
        return E_INVALIDARG;

    if (IsCompressed(metadata.format) || !IsCompressed(format))
        return E_INVALIDARG;

    if (IsTypeless(format)
        || IsTypeless(metadata.format) || IsPlanar(metadata.format) || IsPalettized(metadata.format))
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

    cImages.Release();

    TexMetadata mdata2 = metadata;
    mdata2.format = format;
    HRESULT hr = cImages.Initialize(mdata2);
    if (FAILED(hr))
        return hr;

    if (nimages != cImages.GetImageCount())
    {
        cImages.Release();
        return E_FAIL;
    }

    const Image* dest = cImages.GetImages();
    if (!dest)
    {
        cImages.Release();
        return E_POINTER;
    }

    for (size_t index = 0; index < nimages; ++index)
    {
        assert(dest[index].format == format);

        const Image& src = srcImages[index];

        if (src.width != dest[index].width || src.height != dest[index].height)
        {
            cImages.Release();
            return E_FAIL;
        }

        if ((options.flags & TEX_COMPRESS_PARALLEL))
        {
#ifndef _OPENMP
            return E_NOTIMPL;
#else
            if (options.flags & TEX_COMPRESS_PARALLEL)
            {
                hr = CompressBC_Parallel(src, dest[index], GetSRGBFlags(options.flags), options);
                if (FAILED(hr))
                {
                    cImages.Release();
                    return  hr;
                }
            }
#endif // _OPENMP
        }
        else
        {
            hr = CompressBC(src, dest[index], GetSRGBFlags(options.flags), options);
            if (FAILED(hr))
            {
                cImages.Release();
                return hr;
            }
        }
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT DirectX::Compress(
    const Image* srcImages,
    size_t nimages,
    const TexMetadata& metadata,
    DXGI_FORMAT format,
    DWORD compress,
    float threshold,
    ScratchImage& cImages)
{
    TexCompressOptions options;

    options.threshold = threshold;
    options.flags = compress;
    return CompressEx(srcImages, nimages, metadata, format, options, cImages);
}


//-------------------------------------------------------------------------------------
// Decompression
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::Decompress(
    const Image& cImage,
    DXGI_FORMAT format,
    ScratchImage& image)
{
    if (!IsCompressed(cImage.format) || IsCompressed(format))
        return E_INVALIDARG;

    if (format == DXGI_FORMAT_UNKNOWN)
    {
        // Pick a default decompressed format based on BC input format
        format = DefaultDecompress(cImage.format);
        if (format == DXGI_FORMAT_UNKNOWN)
        {
            // Input is not a compressed format
            return E_INVALIDARG;
        }
    }
    else
    {
        if (!IsValid(format))
            return E_INVALIDARG;

        if (IsTypeless(format) || IsPlanar(format) || IsPalettized(format))
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    // Create decompressed image
    HRESULT hr = image.Initialize2D(format, cImage.width, cImage.height, 1, 1);
    if (FAILED(hr))
        return hr;

    const Image *img = image.GetImage(0, 0, 0);
    if (!img)
    {
        image.Release();
        return E_POINTER;
    }

    // Decompress single image
    hr = DecompressBC(cImage, *img);
    if (FAILED(hr))
        image.Release();

    return hr;
}

_Use_decl_annotations_
HRESULT DirectX::Decompress(
    const Image* cImages,
    size_t nimages,
    const TexMetadata& metadata,
    DXGI_FORMAT format,
    ScratchImage& images)
{
    if (!cImages || !nimages)
        return E_INVALIDARG;

    if (!IsCompressed(metadata.format) || IsCompressed(format))
        return E_INVALIDARG;

    if (format == DXGI_FORMAT_UNKNOWN)
    {
        // Pick a default decompressed format based on BC input format
        format = DefaultDecompress(cImages[0].format);
        if (format == DXGI_FORMAT_UNKNOWN)
        {
            // Input is not a compressed format
            return E_FAIL;
        }
    }
    else
    {
        if (!IsValid(format))
            return E_INVALIDARG;

        if (IsTypeless(format) || IsPlanar(format) || IsPalettized(format))
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    images.Release();

    TexMetadata mdata2 = metadata;
    mdata2.format = format;
    HRESULT hr = images.Initialize(mdata2);
    if (FAILED(hr))
        return hr;

    if (nimages != images.GetImageCount())
    {
        images.Release();
        return E_FAIL;
    }

    const Image* dest = images.GetImages();
    if (!dest)
    {
        images.Release();
        return E_POINTER;
    }

    for (size_t index = 0; index < nimages; ++index)
    {
        assert(dest[index].format == format);

        const Image& src = cImages[index];
        if (!IsCompressed(src.format))
        {
            images.Release();
            return E_FAIL;
        }

        if (src.width != dest[index].width || src.height != dest[index].height)
        {
            images.Release();
            return E_FAIL;
        }

        hr = DecompressBC(src, dest[index]);
        if (FAILED(hr))
        {
            images.Release();
            return hr;
        }
    }

    return S_OK;
}
