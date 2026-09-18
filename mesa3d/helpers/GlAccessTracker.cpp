#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"


#include <set>
#include <queue>

using namespace llvm;

namespace {

// Utility: Check if function directly calls another function
bool callsFunction(Function &F, Function *targetFn) {
    for (auto &BB : F) {
        for (auto &I : BB) {
            if (auto *call = dyn_cast<CallBase>(&I)) {
                if (Function *callee = call->getCalledFunction()) {
                    if (callee == targetFn)
                        return true;
                }
            }
        }
    }
    return false;
}

// Utility: Check if a specific field of a struct type is accessed in a function
bool accessesStructField(Function &F, StructType *targetStruct, unsigned fieldIndex) {
    for (auto &BB : F) {
        for (auto &I : BB) {
            if (auto *GEP = dyn_cast<GetElementPtrInst>(&I)) {
                if (GEP->getNumIndices() < 2)
                    continue;
                if (GEP->getSourceElementType() != targetStruct)
                    continue;

                auto *index = dyn_cast<ConstantInt>(GEP->getOperand(2));
                if (index && index->getZExtValue() == fieldIndex) {
                    return true;
                }
            }
        }
    }
    return false;
}
static llvm::cl::opt<std::string> StartFnName(
    "start-fn", 
    llvm::cl::desc("Name of the function to start from"), 
    llvm::cl::value_desc("function"), 
    llvm::cl::init("_mesa_UseProgram")); // default value

// The main pass
struct GlAccessTrackerPass : PassInfoMixin<GlAccessTrackerPass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
        Function *startFn = M.getFunction(StartFnName);
        if (!startFn) {
            errs() << "Error: start function '" << StartFnName << "' not found!\n";
            return PreservedAnalyses::all();
        }

        Function *intelIoctlFn = M.getFunction("ioctl");
        if (!intelIoctlFn) {
            errs() << "Warning: intel_ioctl function not found!\n";
        }

        StructType *glCtxTy = StructType::getTypeByName(M.getContext(), "struct.gl_context");
        if (!glCtxTy) {
            errs() << "Warning: struct.gl_context not found!\n";
        }

        StructType *pipeConCtxTy = StructType::getTypeByName(M.getContext(), "struct.pipe_context");
        if (!pipeConCtxTy) {
            errs() << "Warning: struct.pipe_context not found!\n";
        }

        StructType *pipeScrCtxTy = StructType::getTypeByName(M.getContext(), "struct.pipe_screen");
        if (!pipeScrCtxTy) {
            errs() << "Warning: struct.pipe_screen not found!\n";
        }

        std::set<Function *> visited;
        std::queue<std::pair<Function *, int>> worklist;

        visited.insert(startFn);
        worklist.emplace(startFn, 0);

        while (!worklist.empty()) {
            auto [F, depth] = worklist.front();
            worklist.pop();

            // errs() << "Depth " << depth << ": " << F->getName() << "\n";

            // Check if this function directly calls intel_ioctl
            if (intelIoctlFn && callsFunction(*F, intelIoctlFn)) {
                errs() << "  ==> calls intel_ioctl!\n";
            }

            if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 7)) {
    errs() << "  ==> accesses destroy\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 9)) {
    errs() << "  ==> accesses draw_vertex_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 10)) {
    errs() << "  ==> accesses render_condition\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 11)) {
    errs() << "  ==> accesses render_condition_mem\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 12)) {
    errs() << "  ==> accesses create_query\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 13)) {
    errs() << "  ==> accesses create_batch_query\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 14)) {
    errs() << "  ==> accesses destroy_query\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 15)) {
    errs() << "  ==> accesses begin_query\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 16)) {
    errs() << "  ==> accesses end_query\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 17)) {
    errs() << "  ==> accesses get_query_result\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 18)) {
    errs() << "  ==> accesses get_query_result_resource\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 19)) {
    errs() << "  ==> accesses set_active_query_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 20)) {
    errs() << "  ==> accesses init_intel_perf_query_info\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 21)) {
    errs() << "  ==> accesses get_intel_perf_query_info\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 22)) {
    errs() << "  ==> accesses get_intel_perf_query_counter_info\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 23)) {
    errs() << "  ==> accesses new_intel_perf_query_obj\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 24)) {
    errs() << "  ==> accesses begin_intel_perf_query\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 25)) {
    errs() << "  ==> accesses end_intel_perf_query\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 26)) {
    errs() << "  ==> accesses delete_intel_perf_query\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 27)) {
    errs() << "  ==> accesses wait_intel_perf_query\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 28)) {
    errs() << "  ==> accesses is_intel_perf_query_ready\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 29)) {
    errs() << "  ==> accesses get_intel_perf_query_data\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 30)) {
    errs() << "  ==> accesses link_shader\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 31)) {
    errs() << "  ==> accesses create_blend_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 32)) {
    errs() << "  ==> accesses bind_blend_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 33)) {
    errs() << "  ==> accesses delete_blend_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 34)) {
    errs() << "  ==> accesses create_sampler_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 35)) {
    errs() << "  ==> accesses bind_sampler_states\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 36)) {
    errs() << "  ==> accesses delete_sampler_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 37)) {
    errs() << "  ==> accesses create_rasterizer_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 38)) {
    errs() << "  ==> accesses bind_rasterizer_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 39)) {
    errs() << "  ==> accesses delete_rasterizer_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 40)) {
    errs() << "  ==> accesses create_depth_stencil_alpha_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 41)) {
    errs() << "  ==> accesses bind_depth_stencil_alpha_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 42)) {
    errs() << "  ==> accesses delete_depth_stencil_alpha_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 43)) {
    errs() << "  ==> accesses create_fs_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 44)) {
    errs() << "  ==> accesses bind_fs_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 45)) {
    errs() << "  ==> accesses delete_fs_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 46)) {
    errs() << "  ==> accesses create_vs_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 47)) {
    errs() << "  ==> accesses bind_vs_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 48)) {
    errs() << "  ==> accesses delete_vs_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 49)) {
    errs() << "  ==> accesses create_gs_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 50)) {
    errs() << "  ==> accesses bind_gs_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 51)) {
    errs() << "  ==> accesses delete_gs_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 52)) {
    errs() << "  ==> accesses create_tcs_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 53)) {
    errs() << "  ==> accesses bind_tcs_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 54)) {
    errs() << "  ==> accesses delete_tcs_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 55)) {
    errs() << "  ==> accesses create_tes_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 56)) {
    errs() << "  ==> accesses bind_tes_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 57)) {
    errs() << "  ==> accesses delete_tes_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 58)) {
    errs() << "  ==> accesses create_vertex_elements_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 59)) {
    errs() << "  ==> accesses bind_vertex_elements_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 60)) {
    errs() << "  ==> accesses delete_vertex_elements_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 61)) {
    errs() << "  ==> accesses create_ts_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 62)) {
    errs() << "  ==> accesses bind_ts_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 63)) {
    errs() << "  ==> accesses delete_ts_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 64)) {
    errs() << "  ==> accesses create_ms_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 65)) {
    errs() << "  ==> accesses bind_ms_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 66)) {
    errs() << "  ==> accesses delete_ms_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 67)) {
    errs() << "  ==> accesses set_blend_color\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 68)) {
    errs() << "  ==> accesses set_stencil_ref\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 69)) {
    errs() << "  ==> accesses set_sample_mask\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 70)) {
    errs() << "  ==> accesses set_min_samples\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 71)) {
    errs() << "  ==> accesses set_clip_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 72)) {
    errs() << "  ==> accesses set_constant_buffer\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 73)) {
    errs() << "  ==> accesses set_inlinable_constants\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 74)) {
    errs() << "  ==> accesses set_framebuffer_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 75)) {
    errs() << "  ==> accesses set_sample_locations\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 76)) {
    errs() << "  ==> accesses set_polygon_stipple\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 77)) {
    errs() << "  ==> accesses set_scissor_states\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 78)) {
    errs() << "  ==> accesses set_window_rectangles\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 79)) {
    errs() << "  ==> accesses set_viewport_states\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 80)) {
    errs() << "  ==> accesses set_sampler_views\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 81)) {
    errs() << "  ==> accesses set_tess_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 82)) {
    errs() << "  ==> accesses set_patch_vertices\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 83)) {
    errs() << "  ==> accesses set_debug_callback\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 84)) {
    errs() << "  ==> accesses set_shader_buffers\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 85)) {
    errs() << "  ==> accesses set_hw_atomic_buffers\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 86)) {
    errs() << "  ==> accesses set_shader_images\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 87)) {
    errs() << "  ==> accesses set_vertex_buffers\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 88)) {
    errs() << "  ==> accesses create_stream_output_target\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 89)) {
    errs() << "  ==> accesses stream_output_target_destroy\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 90)) {
    errs() << "  ==> accesses set_stream_output_targets\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 91)) {
    errs() << "  ==> accesses stream_output_target_offset\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 92)) {
    errs() << "  ==> accesses set_frontend_noop\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 93)) {
    errs() << "  ==> accesses resource_copy_region\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 94)) {
    errs() << "  ==> accesses blit\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 95)) {
    errs() << "  ==> accesses clear\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 96)) {
    errs() << "  ==> accesses clear_render_target\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 97)) {
    errs() << "  ==> accesses clear_depth_stencil\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 98)) {
    errs() << "  ==> accesses clear_texture\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 99)) {
    errs() << "  ==> accesses clear_buffer\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 100)) {
    errs() << "  ==> accesses evaluate_depth_buffer\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 101)) {
    errs() << "  ==> accesses flush\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 102)) {
    errs() << "  ==> accesses create_fence_fd\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 103)) {
    errs() << "  ==> accesses fence_server_sync\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 104)) {
    errs() << "  ==> accesses fence_server_signal\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 105)) {
    errs() << "  ==> accesses create_sampler_view\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 106)) {
    errs() << "  ==> accesses sampler_view_destroy\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 107)) {
    errs() << "  ==> accesses sampler_view_release\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 108)) {
    errs() << "  ==> accesses create_surface\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 109)) {
    errs() << "  ==> accesses surface_destroy\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 110)) {
    errs() << "  ==> accesses buffer_map\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 111)) {
    errs() << "  ==> accesses transfer_flush_region\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 112)) {
    errs() << "  ==> accesses buffer_unmap\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 113)) {
    errs() << "  ==> accesses texture_map\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 114)) {
    errs() << "  ==> accesses texture_unmap\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 115)) {
    errs() << "  ==> accesses buffer_subdata\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 116)) {
    errs() << "  ==> accesses texture_subdata\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 117)) {
    errs() << "  ==> accesses texture_barrier\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 118)) {
    errs() << "  ==> accesses memory_barrier\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 119)) {
    errs() << "  ==> accesses resource_commit\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 120)) {
    errs() << "  ==> accesses create_video_codec\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 121)) {
    errs() << "  ==> accesses create_video_buffer\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 122)) {
    errs() << "  ==> accesses create_compute_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 123)) {
    errs() << "  ==> accesses bind_compute_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 124)) {
    errs() << "  ==> accesses delete_compute_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 125)) {
    errs() << "  ==> accesses get_compute_state_info\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 126)) {
    errs() << "  ==> accesses get_compute_state_subgroup_size\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 127)) {
    errs() << "  ==> accesses set_global_binding\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 128)) {
    errs() << "  ==> accesses launch_grid\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 129)) {
    errs() << "  ==> accesses draw_mesh_tasks\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 130)) {
    errs() << "  ==> accesses svm_migrate\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 131)) {
    errs() << "  ==> accesses get_sample_position\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 132)) {
    errs() << "  ==> accesses get_timestamp\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 133)) {
    errs() << "  ==> accesses flush_resource\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 134)) {
    errs() << "  ==> accesses invalidate_resource\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 135)) {
    errs() << "  ==> accesses get_device_reset_status\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 136)) {
    errs() << "  ==> accesses set_device_reset_callback\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 137)) {
    errs() << "  ==> accesses dump_debug_state\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 138)) {
    errs() << "  ==> accesses set_log_context\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 139)) {
    errs() << "  ==> accesses emit_string_marker\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 140)) {
    errs() << "  ==> accesses generate_mipmap\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 141)) {
    errs() << "  ==> accesses create_texture_handle\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 142)) {
    errs() << "  ==> accesses delete_texture_handle\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 143)) {
    errs() << "  ==> accesses make_texture_handle_resident\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 144)) {
    errs() << "  ==> accesses create_image_handle\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 145)) {
    errs() << "  ==> accesses delete_image_handle\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 146)) {
    errs() << "  ==> accesses make_image_handle_resident\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 147)) {
    errs() << "  ==> accesses callback\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 148)) {
    errs() << "  ==> accesses set_context_param\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 149)) {
    errs() << "  ==> accesses create_video_buffer_with_modifiers\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 150)) {
    errs() << "  ==> accesses video_buffer_from_handle\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 151)) {
    errs() << "  ==> accesses ml_subgraph_create\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 152)) {
    errs() << "  ==> accesses ml_subgraph_invoke\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 153)) {
    errs() << "  ==> accesses ml_subgraph_read_output\n";
}

if (pipeConCtxTy && accessesStructField(*F, pipeConCtxTy, 154)) {
    errs() << "  ==> accesses ml_subgraph_destroy\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 5)) {
    errs() << "  ==> accesses get_screen_fd\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 8)) {
    errs() << "  ==> accesses destroy\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 9)) {
    errs() << "  ==> accesses get_name\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 10)) {
    errs() << "  ==> accesses get_vendor\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 11)) {
    errs() << "  ==> accesses get_device_vendor\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 12)) {
    errs() << "  ==> accesses get_cl_cts_version\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 13)) {
    errs() << "  ==> accesses get_video_param\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 14)) {
    errs() << "  ==> accesses get_sample_pixel_grid\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 15)) {
    errs() << "  ==> accesses get_timestamp\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 16)) {
    errs() << "  ==> accesses get_canonical_format\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 17)) {
    errs() << "  ==> accesses context_create\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 18)) {
    errs() << "  ==> accesses is_compute_copy_faster\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 19)) {
    errs() << "  ==> accesses is_format_supported\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 20)) {
    errs() << "  ==> accesses is_video_format_supported\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 21)) {
    errs() << "  ==> accesses can_create_resource\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 22)) {
    errs() << "  ==> accesses resource_create\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 23)) {
    errs() << "  ==> accesses resource_create_drawable\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 24)) {
    errs() << "  ==> accesses resource_create_front\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 25)) {
    errs() << "  ==> accesses resource_from_handle\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 26)) {
    errs() << "  ==> accesses resource_from_user_memory\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 27)) {
    errs() << "  ==> accesses check_resource_capability\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 28)) {
    errs() << "  ==> accesses resource_get_handle\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 29)) {
    errs() << "  ==> accesses resource_get_param\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 30)) {
    errs() << "  ==> accesses resource_get_info\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 31)) {
    errs() << "  ==> accesses resource_changed\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 32)) {
    errs() << "  ==> accesses resource_destroy\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 33)) {
    errs() << "  ==> accesses flush_frontbuffer\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 34)) {
    errs() << "  ==> accesses fence_reference\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 35)) {
    errs() << "  ==> accesses fence_finish\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 36)) {
    errs() << "  ==> accesses fence_get_fd\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 37)) {
    errs() << "  ==> accesses fence_get_win32_handle\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 38)) {
    errs() << "  ==> accesses create_fence_win32\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 39)) {
    errs() << "  ==> accesses get_driver_query_info\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 40)) {
    errs() << "  ==> accesses get_driver_query_group_info\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 41)) {
    errs() << "  ==> accesses query_memory_info\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 42)) {
    errs() << "  ==> accesses get_compiler_options\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 43)) {
    errs() << "  ==> accesses get_disk_shader_cache\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 44)) {
    errs() << "  ==> accesses resource_create_with_modifiers\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 45)) {
    errs() << "  ==> accesses query_dmabuf_modifiers\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 46)) {
    errs() << "  ==> accesses memobj_create_from_handle\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 47)) {
    errs() << "  ==> accesses memobj_destroy\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 48)) {
    errs() << "  ==> accesses resource_from_memobj\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 49)) {
    errs() << "  ==> accesses get_driver_uuid\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 50)) {
    errs() << "  ==> accesses get_device_uuid\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 51)) {
    errs() << "  ==> accesses get_device_luid\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 52)) {
    errs() << "  ==> accesses get_device_node_mask\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 53)) {
    errs() << "  ==> accesses set_max_shader_compiler_threads\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 54)) {
    errs() << "  ==> accesses is_parallel_shader_compilation_finished\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 55)) {
    errs() << "  ==> accesses driver_thread_add_job\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 56)) {
    errs() << "  ==> accesses set_damage_region\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 57)) {
    errs() << "  ==> accesses finalize_nir\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 58)) {
    errs() << "  ==> accesses resource_create_unbacked\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 59)) {
    errs() << "  ==> accesses allocate_memory\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 60)) {
    errs() << "  ==> accesses free_memory\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 61)) {
    errs() << "  ==> accesses allocate_memory_fd\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 62)) {
    errs() << "  ==> accesses import_memory_fd\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 63)) {
    errs() << "  ==> accesses free_memory_fd\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 64)) {
    errs() << "  ==> accesses resource_bind_backing\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 65)) {
    errs() << "  ==> accesses map_memory\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 66)) {
    errs() << "  ==> accesses unmap_memory\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 67)) {
    errs() << "  ==> accesses is_dmabuf_modifier_supported\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 68)) {
    errs() << "  ==> accesses get_dmabuf_modifier_planes\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 69)) {
    errs() << "  ==> accesses get_sparse_texture_virtual_page_size\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 72)) {
    errs() << "  ==> accesses set_fence_timeline_value\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 73)) {
    errs() << "  ==> accesses interop_query_device_info\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 74)) {
    errs() << "  ==> accesses interop_export_object\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 75)) {
    errs() << "  ==> accesses query_compression_rates\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 76)) {
    errs() << "  ==> accesses query_compression_modifiers\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 77)) {
    errs() << "  ==> accesses is_video_target_buffer_supported\n";
}

if (pipeScrCtxTy && accessesStructField(*F, pipeScrCtxTy, 78)) {
    errs() << "  ==> accesses get_driver_pipe_screen\n";
}


            // Check for access to a specific struct field (e.g., field index 114 in pipe_context)
            // if (glCtxTy && accessesStructField(*F, glCtxTy, 131)) {
            //     errs() << "  ==> accesses vbo_context\n";
            // }

            // if (glCtxTy && accessesStructField(*F, glCtxTy, 132)) {
            //     errs() << "  ==> accesses st_context\n";
            // }

            // if (glCtxTy && accessesStructField(*F, glCtxTy, 133)) {
            //     errs() << "  ==> accesses pipe_screen\n";
            // }

            // if (glCtxTy && accessesStructField(*F, glCtxTy, 134)) {
            //     errs() << "  ==> accesses pipe_context\n";
            // }

            // if (glCtxTy && accessesStructField(*F, glCtxTy, 135)) {
            //     errs() << "  ==> accesses st_config_options\n";
            // }

            // if (glCtxTy && accessesStructField(*F, glCtxTy, 136)) {
            //     errs() << "  ==> accesses cso_context\n";
            // }
            // Traverse to callees
            for (auto &BB : *F) {
                for (auto &I : BB) {
                    if (auto *call = dyn_cast<CallBase>(&I)) {
                        Function *callee = call->getCalledFunction();
                        if (callee && !callee->isDeclaration() && visited.insert(callee).second) {
                            worklist.emplace(callee, depth + 1);
                        }
                    }
                }
            }
        }

        return PreservedAnalyses::all();
    }
};

} // namespace

// Registration boilerplate
extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "GlAccessTracker", LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "gl-access-tracker") {
                        MPM.addPass(GlAccessTrackerPass());
                        return true;
                    }
                    return false;
                });
        }};
}
