/*******************************************************************************
 *
 * MIT License
 *
 * Copyright (c) 2019 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/
#include <cstddef>
#include "miopen/solver.hpp"
#include "miopen/handle.hpp"
#include <miopen/generic_search.hpp>
#include "implicitgemm_util.hpp"

namespace miopen {
namespace solver {

bool ConvHipImplicitGemmBwdDataV1R1::IsApplicable(const ConvolutionContext& ctx) const
{
    if(!ctx.direction.IsBackwardData())
        return false;

    if(!ctx.Is2d())
        return false;

    if(!ctx.IsFp32())
        return false;

    if(ctx.group_counts != 1)
        return false;

    std::size_t n  = ConvolutionContextInterpreter::GetBatchN(ctx);
    std::size_t k  = ConvolutionContextInterpreter::GetOutputChannelK(ctx);
    std::size_t c  = ConvolutionContextInterpreter::GetInputChannelC(ctx);
    std::size_t y  = ConvolutionContextInterpreter::GetFilterHeightY(ctx);
    std::size_t x  = ConvolutionContextInterpreter::GetFilterWidthX(ctx);
    std::size_t ho = ConvolutionContextInterpreter::GetOutputHeightHo(ctx);
    std::size_t wo = ConvolutionContextInterpreter::GetOutputWidthWo(ctx);

    return (n * ho * wo) % 128 == 0 && (c * y * x) % 128 == 0 && k % 8 == 0;
}

ConvSolution ConvHipImplicitGemmBwdDataV1R1::GetSolution(const ConvolutionContext& ctx) const
{
    ConvSolution result;
    KernelInfo construction_parameters;

    std::size_t n               = ConvolutionContextInterpreter::GetBatchN(ctx);
    std::size_t k               = ConvolutionContextInterpreter::GetOutputChannelK(ctx);
    std::size_t c               = ConvolutionContextInterpreter::GetInputChannelC(ctx);
    std::size_t hi              = ConvolutionContextInterpreter::GetInputHeightHi(ctx);
    std::size_t wi              = ConvolutionContextInterpreter::GetInputWidthWi(ctx);
    std::size_t ho              = ConvolutionContextInterpreter::GetOutputHeightHo(ctx);
    std::size_t wo              = ConvolutionContextInterpreter::GetOutputWidthWo(ctx);
    std::size_t y               = ConvolutionContextInterpreter::GetFilterHeightY(ctx);
    std::size_t x               = ConvolutionContextInterpreter::GetFilterWidthX(ctx);
    std::size_t conv_stride_h   = ConvolutionContextInterpreter::GetConvolutionStrideH(ctx);
    std::size_t conv_stride_w   = ConvolutionContextInterpreter::GetConvolutionStrideW(ctx);
    std::size_t conv_dilation_h = ConvolutionContextInterpreter::GetConvolutionDilationH(ctx);
    std::size_t conv_dilation_w = ConvolutionContextInterpreter::GetConvolutionDilationW(ctx);
    std::size_t in_left_pad_h   = ConvolutionContextInterpreter::GetInputLeftPadH(ctx);
    std::size_t in_left_pad_w   = ConvolutionContextInterpreter::GetInputLeftPadW(ctx);
    std::size_t in_right_pad_h  = ConvolutionContextInterpreter::GetAdjustedInputRightPadH(ctx);
    std::size_t in_right_pad_w  = ConvolutionContextInterpreter::GetAdjustedInputRightPadW(ctx);

    std::size_t gemm_m = (static_cast<std::size_t>(c) * y * x);
    std::size_t gemm_n = (static_cast<std::size_t>(n) * ho * wo);

    std::size_t gemm_m_per_block = 128;
    std::size_t gemm_n_per_block = 128;

    const int block_size = 256;

    std::size_t grid_size = (gemm_m / gemm_m_per_block) * (gemm_n / gemm_n_per_block);

    std::size_t lkl_wk0 = block_size;
    std::size_t lkl_wk1 = 1;
    std::size_t lkl_wk2 = 1;

    construction_parameters.l_wk.push_back(lkl_wk0);
    construction_parameters.l_wk.push_back(lkl_wk1);
    construction_parameters.l_wk.push_back(lkl_wk2);

    std::size_t gbl_wk0 = lkl_wk0 * grid_size;
    std::size_t gbl_wk1 = 1;
    std::size_t gbl_wk2 = 1;

    construction_parameters.g_wk.push_back(gbl_wk0);
    construction_parameters.g_wk.push_back(gbl_wk1);
    construction_parameters.g_wk.push_back(gbl_wk2);

    construction_parameters.kernel_file =
        "gridwise_convolution_backward_data_implicit_gemm_v1r1_nchw_kcyx_nkhw.cpp";

    construction_parameters.kernel_name =
        "gridwise_convolution_backward_data_implicit_gemm_v1r1_nchw_kcyx_nkhw";

    // clang-format off
    construction_parameters.comp_options = 
        std::string(" -std=c++14 ") +
        std::string(" -DCK_PARAM_PROBLEM_N=") + std::to_string(n) +
        std::string(" -DCK_PARAM_PROBLEM_K=") + std::to_string(k) +
        std::string(" -DCK_PARAM_PROBLEM_C=") + std::to_string(c) +
        std::string(" -DCK_PARAM_PROBLEM_HI=") + std::to_string(hi) +
        std::string(" -DCK_PARAM_PROBLEM_WI=") + std::to_string(wi) +
        std::string(" -DCK_PARAM_PROBLEM_HO=") + std::to_string(ho) +
        std::string(" -DCK_PARAM_PROBLEM_WO=") + std::to_string(wo) +
        std::string(" -DCK_PARAM_PROBLEM_Y=") + std::to_string(y) +
        std::string(" -DCK_PARAM_PROBLEM_X=") + std::to_string(x) +
        std::string(" -DCK_PARAM_PROBLEM_CONV_STRIDE_H=") + std::to_string(conv_stride_h) +
        std::string(" -DCK_PARAM_PROBLEM_CONV_STRIDE_W=") + std::to_string(conv_stride_w) +
        std::string(" -DCK_PARAM_PROBLEM_CONV_DILATION_H=") + std::to_string(conv_dilation_h) +
        std::string(" -DCK_PARAM_PROBLEM_CONV_DILATION_W=") + std::to_string(conv_dilation_w) +
        std::string(" -DCK_PARAM_PROBLEM_IN_LEFT_PAD_H=") + std::to_string(in_left_pad_h) +
        std::string(" -DCK_PARAM_PROBLEM_IN_LEFT_PAD_W=") + std::to_string(in_left_pad_w) +
        std::string(" -DCK_PARAM_PROBLEM_IN_RIGHT_PAD_H=") + std::to_string(in_right_pad_h) +
        std::string(" -DCK_PARAM_PROBLEM_IN_RIGHT_PAD_W=") + std::to_string(in_right_pad_w) +
        std::string(" -DCK_PARAM_TUNABLE_BLOCK_SIZE=") + std::to_string(block_size) +
        std::string(" -DCK_PARAM_TUNABLE_GEMM_M_PER_BLOCK=") + std::to_string(gemm_m_per_block) +
        std::string(" -DCK_PARAM_TUNABLE_GEMM_N_PER_BLOCK=") + std::to_string(gemm_n_per_block) +
        std::string(" -DCK_PARAM_TUNABLE_GEMM_K_PER_BLOCK=") + std::to_string(8) +
        std::string(" -DCK_PARAM_TUNABLE_GEMM_M_PER_THREAD_SUB_C=") + std::to_string(4) +
        std::string(" -DCK_PARAM_TUNABLE_GEMM_N_PER_THREAD_SUB_C=") + std::to_string(4) +
        std::string(" -DCK_PARAM_TUNABLE_GEMM_M_LEVEL0_CLUSTER=") + std::to_string(4) +
        std::string(" -DCK_PARAM_TUNABLE_GEMM_N_LEVEL0_CLUSTER=") + std::to_string(4) +
        std::string(" -DCK_PARAM_TUNABLE_GEMM_M_LEVEL1_CLUSTER=") + std::to_string(4) +
        std::string(" -DCK_PARAM_TUNABLE_GEMM_N_LEVEL1_CLUSTER=") + std::to_string(4) +
        std::string(" -DCK_PARAM_TUNABLE_GEMM_A_BLOCK_COPY_CLUSTER_LENGTHS_GEMM_K=") + std::to_string(8) +
        std::string(" -DCK_PARAM_TUNABLE_GEMM_A_BLOCK_COPY_CLUSTER_LENGTHS_GEMM_M=") + std::to_string(32) +
        std::string(" -DCK_PARAM_TUNABLE_GEMM_A_BLOCK_COPY_SRC_DATA_PER_READ_GEMM_M=") + std::to_string(4) +
        std::string(" -DCK_PARAM_TUNABLE_GEMM_A_BLOCK_COPY_DST_DATA_PER_WRITE_GEMM_M=") + std::to_string(4) +
        std::string(" -DCK_PARAM_TUNABLE_GEMM_B_BLOCK_COPY_CLUSTER_LENGTHS_GEMM_K=") + std::to_string(2) +
        std::string(" -DCK_PARAM_TUNABLE_GEMM_B_BLOCK_COPY_CLUSTER_LENGTHS_GEMM_N=") + std::to_string(128) +
        std::string(" -DCK_PARAM_TUNABLE_GEMM_B_BLOCK_COPY_SRC_DATA_PER_READ_GEMM_N=") + std::to_string(1) +
        std::string(" -DCK_PARAM_TUNABLE_GEMM_B_BLOCK_COPY_DST_DATA_PER_WRITE_GEMM_N=") + std::to_string(1) +
        std::string(" -DCK_PARAM_TUNABLE_GEMM_C_THREAD_COPY_DST_DATA_PER_WRITE_GEMM_N1=") + std::to_string(1) +
        std::string(" -DCK_PARAM_DEPENDENT_GRID_SIZE=") + std::to_string(grid_size) +
        std::string(" -DCK_THREADWISE_GEMM_USE_AMD_INLINE_ASM=") + (use_amd_inline_asm(ctx) ? '1' : '0') +
        std::string(" -DCK_USE_AMD_BUFFER_ATOMIC_ADD=") + (support_amd_buffer_atomic_add(ctx) ? '1' : '0') +
        std::string(" -D__HIP_PLATFORM_HCC__=1") +
        ctx.general_compile_options;
    // clang-format on

    result.construction_params.push_back(construction_parameters);
    return result;
}

} // namespace solver
} // namespace miopen