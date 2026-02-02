// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstdlib>
#include <sys/stat.h>
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/maxwell/control_flow.h"
#include "shader_recompiler/frontend/maxwell/translate_program.h"
#include "shader_recompiler/host_translate_info.h"
#include "shader_recompiler/object_pool.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/runtime_info.h"

class FileEnvironment final : public Shader::Environment {
public:
    FileEnvironment() = default;
    ~FileEnvironment() override = default;
    FileEnvironment& operator=(FileEnvironment&&) noexcept = default;
    FileEnvironment(FileEnvironment&&) noexcept = default;
    FileEnvironment& operator=(const FileEnvironment&) = delete;
    FileEnvironment(const FileEnvironment&) = delete;
    void Deserialize(std::ifstream& file);
    [[nodiscard]] u64 ReadInstruction(u32 address) override;
    [[nodiscard]] u32 ReadCbufValue(u32 cbuf_index, u32 cbuf_offset) override;
    [[nodiscard]] Shader::TextureType ReadTextureType(u32 handle) override;
    [[nodiscard]] Shader::TexturePixelFormat ReadTexturePixelFormat(u32 handle) override;
    [[nodiscard]] bool IsTexturePixelFormatInteger(u32 handle) override;
    [[nodiscard]] u32 ReadViewportTransformState() override;
    [[nodiscard]] u32 LocalMemorySize() const override;
    [[nodiscard]] u32 SharedMemorySize() const override;
    [[nodiscard]] u32 TextureBoundBuffer() const override;
    [[nodiscard]] std::array<u32, 3> WorkgroupSize() const override;
    [[nodiscard]] std::optional<Shader::ReplaceConstant> GetReplaceConstBuffer(u32 bank, u32 offset) override;
    [[nodiscard]] bool HasHLEMacroState() const override {
        return cbuf_replacements.size() != 0;
    }
    void Dump(u64 pipeline_hash, u64 shader_hash) override;

    std::vector<u64> code;
    std::unordered_map<u32, Shader::TextureType> texture_types;
    std::unordered_map<u32, Shader::TexturePixelFormat> texture_pixel_formats;
    std::unordered_map<u64, u32> cbuf_values;
    std::unordered_map<u64, Shader::ReplaceConstant> cbuf_replacements;
    std::array<u32, 3> workgroup_size{};
    u32 local_memory_size{};
    u32 shared_memory_size{};
    u32 texture_bound{};
    u32 read_lowest{};
    u32 read_highest{};
    u32 initial_offset{};
    u32 viewport_transform_state = 1;
};

void FileEnvironment::Deserialize(std::ifstream& file) {}

void FileEnvironment::Dump(u64 pipeline_hash, u64 shader_hash) {
    //DumpImpl(pipeline_hash, shader_hash, code, read_highest, read_lowest, initial_offset, stage);
}

u64 FileEnvironment::ReadInstruction(u32 address) {
    if (address < read_lowest || address > read_highest) {
        std::printf("cant read %08x\n", address);
        std::abort();
    }
    return code[(address - read_lowest) / sizeof(u64)];
}

u32 FileEnvironment::ReadCbufValue(u32 cbuf_index, u32 cbuf_offset) {
    return 0;
}

Shader::TextureType FileEnvironment::ReadTextureType(u32 handle) {
    auto const it{texture_types.find(handle)};
    return it->second;
}

Shader::TexturePixelFormat FileEnvironment::ReadTexturePixelFormat(u32 handle) {
    auto const it{texture_pixel_formats.find(handle)};
    return it->second;
}

bool FileEnvironment::IsTexturePixelFormatInteger(u32 handle) {
    return true;
}

u32 FileEnvironment::ReadViewportTransformState() {
    return viewport_transform_state;
}

u32 FileEnvironment::LocalMemorySize() const {
    return local_memory_size;
}

u32 FileEnvironment::SharedMemorySize() const {
    return shared_memory_size;
}

u32 FileEnvironment::TextureBoundBuffer() const {
    return texture_bound;
}

std::array<u32, 3> FileEnvironment::WorkgroupSize() const {
    return workgroup_size;
}

std::optional<Shader::ReplaceConstant> FileEnvironment::GetReplaceConstBuffer(u32 bank, u32 offset) {
    auto const it = cbuf_replacements.find((u64(bank) << 32) | u64(offset));
    return it != cbuf_replacements.end() ? std::optional{it->second} : std::nullopt;
}

int ShaderRecompilerImpl(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: %s [input file] [-n]\n"
            "RAW SPIRV CODE WILL BE SENT TO STDOUT!\n", argv[0]);
        return EXIT_FAILURE;
    }

    size_t cfg_offset = 0;

    Shader::ObjectPool<Shader::IR::Inst> inst_pool;
    Shader::ObjectPool<Shader::IR::Block> block_pool;
    Shader::ObjectPool<Shader::Maxwell::Flow::Block> cfg_blocks;
    FileEnvironment env;

    FILE *fp = fopen(argv[1], "rb");
    if (fp != NULL) {
        struct stat st;
        fstat(fileno(fp), &st);
        auto const words = (st.st_size / sizeof(u64));
        env.code.resize(words + 1);
        fread(env.code.data(), sizeof(u64), words, fp);
        fclose(fp);
    }

    env.read_highest = env.read_lowest + env.code.size() * sizeof(u64);

    Shader::Maxwell::Flow::CFG cfg(env, cfg_blocks, cfg_offset);

    Shader::HostTranslateInfo host_info;
    host_info.support_float64 = true;
    host_info.support_float16 = true;
    host_info.support_int64 = true;
    host_info.needs_demote_reorder = true;
    host_info.support_snorm_render_buffer = true;
    host_info.support_viewport_index_layer = true;
    host_info.support_geometry_shader_passthrough = true;
    host_info.support_conditional_barrier = true;
    host_info.min_ssbo_alignment = 0;
    auto program = Shader::Maxwell::TranslateProgram(inst_pool, block_pool, env, cfg, host_info);

    // IR::Program TranslateProgram(ObjectPool<IR::Inst>& inst_pool, ObjectPool<IR::Block>& block_pool,
    //                              Environment& env, Flow::CFG& cfg, const HostTranslateInfo& host_info)
    // std::vector<u32> EmitSPIRV(const Profile& profile, const RuntimeInfo& runtime_info,
    //                            IR::Program& program, Bindings& bindings, bool optimize)
    Shader::Profile profile{};
    Shader::RuntimeInfo runtime_info;
    auto const spirv_pgm = Shader::Backend::SPIRV::EmitSPIRV(profile, program, true);
    fwrite(spirv_pgm.data(), sizeof(u64), spirv_pgm.size(), stdout);

    return EXIT_SUCCESS;
}
