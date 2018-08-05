#pragma once
#ifndef __CVTT_CONVECTION_KERNELS__
#define __CVTT_CONVECTION_KERNELS__

#include <stdint.h>

namespace CVTT
{
    namespace Flags
    {
        // Enable partitioned modes in BC7 encoding (slower, better quality)
        const uint32_t BC7_EnablePartitioning   = 0x001;

        // Enable 3-partition modes in BC7 encoding (slower, better quality, requires BC7_EnablePartitioning)
        const uint32_t BC7_Enable3Subsets       = 0x002;

        // Enable dual-plane modes in BC7 encoding (slower, better quality)
        const uint32_t BC7_EnableDualPlane      = 0x004;

        // Use fast indexing in BC7 encoding (about 2x faster, slightly worse quality)
        const uint32_t BC7_FastIndexing         = 0x008;

        // Try precomputed single-color lookups where applicable (slightly slower, small quality increase on specific blocks)
        const uint32_t BC7_TrySingleColor       = 0x010;

        // Use fast indexing in HDR formats (faster, worse quality)
        const uint32_t BC6H_FastIndexing        = 0x020;

        // Exhaustive search RGB orderings when encoding BC1-BC3 (much slower, better quality)
        const uint32_t S3TC_Exhaustive          = 0x040;

        // Penalize distant endpoints, improving quality on inaccurate GPU decoders
        const uint32_t S3TC_Paranoid            = 0x080;

        // Uniform color channel importance
        const uint32_t Uniform                  = 0x100;

        // Misc useful default flag combinations
        const uint32_t Fastest = (BC6H_FastIndexing | S3TC_Paranoid);
        const uint32_t Faster = (BC7_EnableDualPlane | BC6H_FastIndexing | S3TC_Paranoid);
        const uint32_t Fast = (BC7_EnablePartitioning | BC7_EnableDualPlane | BC7_FastIndexing | S3TC_Paranoid);
        const uint32_t Default = (BC7_EnablePartitioning | BC7_EnableDualPlane | BC7_Enable3Subsets | BC7_FastIndexing | S3TC_Paranoid);
        const uint32_t Better = (BC7_EnablePartitioning | BC7_EnableDualPlane | BC7_Enable3Subsets | S3TC_Paranoid | S3TC_Exhaustive);
        const uint32_t Ultra = (BC7_EnablePartitioning | BC7_EnableDualPlane | BC7_Enable3Subsets | BC7_TrySingleColor | S3TC_Paranoid | S3TC_Exhaustive);
    }

    const unsigned int NumParallelBlocks = 8;

    struct Options
    {
        uint32_t flags;         // Bitmask of CVTT::Flags values
        float threshold;        // Alpha test threshold for BC1
        float redWeight;        // Red channel importance
        float greenWeight;      // Green channel importance
        float blueWeight;       // Blue channel importance
        float alphaWeight;      // Alpha channel importance

        int refineRoundsBC7;    // Number of refine rounds for BC7
        int refineRoundsBC6H;   // Number of refine rounds for BC6H (max 3)
        int refineRoundsIIC;    // Number of refine rounds for independent interpolated channels (BC3 alpha, BC4, BC5)
        int refineRoundsS3TC;   // Number of refine rounds for S3TC RGB

        int seedPoints;         // Number of seed points (min 1, max 4)

        Options()
            : flags(Flags::Default)
            , threshold(0.5f)
            , redWeight(0.2125f / 0.7154f)
            , greenWeight(1.0f)
            , blueWeight(0.0721f / 0.7154f)
            , alphaWeight(1.0f)
            , refineRoundsBC7(2)
            , refineRoundsBC6H(3)
            , refineRoundsIIC(8)
            , refineRoundsS3TC(2)
            , seedPoints(4)
        {
        }
    };

    // RGBA input block for unsigned 8-bit formats
    struct InputBlockU8
    {
        uint8_t m_pixels[16][4];
    };

    // RGBA input block for signed 8-bit formats
    struct InputBlockS8
    {
        int8_t m_pixels[16][4];
    };

    // RGBA input block for half-precision float formats (bit-cast to int16_t)
    struct InputBlockF16
    {
        int16_t m_pixels[16][4];
    };

    namespace Kernels
    {
        // NOTE: All functions accept and output NumParallelBlocks blocks at once
        void EncodeBC1(uint8_t *pBC, const InputBlockU8 *pBlocks, const Options &options);
        void EncodeBC2(uint8_t *pBC, const InputBlockU8 *pBlocks, const Options &options);
        void EncodeBC3(uint8_t *pBC, const InputBlockU8 *pBlocks, const Options &options);
        void EncodeBC4U(uint8_t *pBC, const InputBlockU8 *pBlocks, const Options &options);
        void EncodeBC4S(uint8_t *pBC, const InputBlockS8 *pBlocks, const Options &options);
        void EncodeBC5U(uint8_t *pBC, const InputBlockU8 *pBlocks, const Options &options);
        void EncodeBC5S(uint8_t *pBC, const InputBlockS8 *pBlocks, const Options &options);
        void EncodeBC6HU(uint8_t *pBC, const InputBlockF16 *pBlocks, const Options &options);
        void EncodeBC6HS(uint8_t *pBC, const InputBlockF16 *pBlocks, const Options &options);
        void EncodeBC7(uint8_t *pBC, const InputBlockU8 *pBlocks, const Options &options);
    }
}

#endif
