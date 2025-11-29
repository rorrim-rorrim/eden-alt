// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <boost/container/small_vector.hpp>
#include <boost/container/static_vector.hpp>

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"
#include "shader_recompiler/backend/spirv/texture_helpers.h"
#include "shader_recompiler/frontend/ir/modifiers.h"

namespace Shader::Backend::SPIRV {
namespace {

TextureType GetEffectiveType(const EmitContext& ctx, TextureType type) {
    return EffectiveTextureType(ctx.profile, type);
}

bool HasLayerComponent(TextureType type) {
    switch (type) {
    case TextureType::ColorArray1D:
    case TextureType::ColorArray2D:
    case TextureType::ColorArrayCube:
        return true;
    default:
        return false;
    }
}

u32 BaseDimension(TextureType type) {
    switch (type) {
    case TextureType::Color1D:
    case TextureType::ColorArray1D:
        return 1;
    case TextureType::Color2D:
    case TextureType::Color2DRect:
    case TextureType::ColorArray2D:
    case TextureType::ColorCube:
    case TextureType::ColorArrayCube:
        return 2;
    case TextureType::Color3D:
        return 3;
    default:
        return 0;
    }
}

bool IsFloatCoordType(IR::Type type) {
    switch (type) {
    case IR::Type::F32:
    case IR::Type::F32x2:
    case IR::Type::F32x3:
    case IR::Type::F32x4:
        return true;
    case IR::Type::U32:
    case IR::Type::U32x2:
    case IR::Type::U32x3:
    case IR::Type::U32x4:
        return false;
    default:
        throw InvalidArgument("Unsupported coordinate type {}", type);
    }
}

u32 NumComponents(IR::Type type) {
    switch (type) {
    case IR::Type::F32:
    case IR::Type::U32:
        return 1;
    case IR::Type::F32x2:
    case IR::Type::U32x2:
        return 2;
    case IR::Type::F32x3:
    case IR::Type::U32x3:
        return 3;
    case IR::Type::F32x4:
    case IR::Type::U32x4:
        return 4;
    default:
        throw InvalidArgument("Unsupported type for coordinate promotion {}", type);
    }
}

Id ExtractComponent(EmitContext& ctx, Id value, IR::Type type, u32 index, bool is_float) {
    if (NumComponents(type) == 1) {
        return value;
    }
    const Id scalar_type{is_float ? ctx.F32[1] : ctx.U32[1]};
    return ctx.OpCompositeExtract(scalar_type, value, index);
}

Id PromoteCoordinate(EmitContext& ctx, IR::Inst* inst, const IR::TextureInstInfo& info,
                     Id coords) {
    if (!Needs1DPromotion(ctx.profile, info.type)) {
        return coords;
    }
    const TextureType effective_type{GetEffectiveType(ctx, info.type)};
    const bool has_layer{HasLayerComponent(effective_type)};
    const u32 target_base_dim{BaseDimension(effective_type)};
    if (target_base_dim == 0) {
        return coords;
    }
    const IR::Type coord_type{inst->Arg(1).Type()};
    const bool is_float{IsFloatCoordType(coord_type)};
    const Id zero{is_float ? ctx.f32_zero_value : ctx.u32_zero_value};
    const u32 total_components{NumComponents(coord_type)};
    const u32 original_base_dim{total_components - (has_layer ? 1u : 0u)};

    boost::container::small_vector<Id, 5> components;
    components.reserve(target_base_dim + (has_layer ? 1u : 0u));
    for (u32 i = 0; i < original_base_dim; ++i) {
        components.push_back(ExtractComponent(ctx, coords, coord_type, i, is_float));
    }
    if (components.empty()) {
        components.push_back(coords);
    }
    while (components.size() < target_base_dim) {
        components.push_back(zero);
    }
    if (has_layer) {
        const u32 layer_index{total_components == 1 ? 0u : total_components - 1u};
        components.push_back(ExtractComponent(ctx, coords, coord_type, layer_index, is_float));
    }
    const Id vector_type{is_float ? ctx.F32[components.size()] : ctx.U32[components.size()]};
    return ctx.OpCompositeConstruct(vector_type, std::span{components.data(), components.size()});
}

Id PromoteOffset(EmitContext& ctx, const IR::Value& offset, Id offset_id,
                 TextureType effective_type) {
    const u32 required_components{BaseDimension(effective_type)};
    if (required_components <= 1 || offset.IsEmpty()) {
        return offset_id;
    }
    const IR::Type offset_type{offset.Type()};
    const u32 existing_components{NumComponents(offset_type)};
    if (existing_components >= required_components) {
        return offset_id;
    }
    boost::container::small_vector<Id, 3> components;
    components.reserve(required_components);
    if (existing_components == 1) {
        components.push_back(offset_id);
    } else {
        for (u32 i = 0; i < existing_components; ++i) {
            components.push_back(ctx.OpCompositeExtract(ctx.S32[1], offset_id, i));
        }
    }
    while (components.size() < required_components) {
        components.push_back(ctx.SConst(0));
    }
    return ctx.OpCompositeConstruct(ctx.S32[required_components],
                                    std::span{components.data(), components.size()});
}

u32 ExpectedDerivativeComponents(TextureType type) {
    return BaseDimension(type);
}

class ImageOperands {
public:
    [[maybe_unused]] static constexpr bool ImageSampleOffsetAllowed = false;
    [[maybe_unused]] static constexpr bool ImageGatherOffsetAllowed = true;
    [[maybe_unused]] static constexpr bool ImageFetchOffsetAllowed = false;
    [[maybe_unused]] static constexpr bool ImageGradientOffsetAllowed = false;

    explicit ImageOperands(EmitContext& ctx, TextureType type, bool promote,
                           bool has_bias, bool has_lod, bool has_lod_clamp, Id lod,
                           const IR::Value& offset)
        : texture_type{type}, needs_promotion{promote} {
        if (has_bias) {
            const Id bias{has_lod_clamp ? ctx.OpCompositeExtract(ctx.F32[1], lod, 0) : lod};
            Add(spv::ImageOperandsMask::Bias, bias);
        }
        if (has_lod) {
            const Id lod_value{has_lod_clamp ? ctx.OpCompositeExtract(ctx.F32[1], lod, 0) : lod};
            Add(spv::ImageOperandsMask::Lod, lod_value);
        }
        AddOffset(ctx, offset, ImageSampleOffsetAllowed);
        if (has_lod_clamp) {
            const Id lod_clamp{has_bias ? ctx.OpCompositeExtract(ctx.F32[1], lod, 1) : lod};
            Add(spv::ImageOperandsMask::MinLod, lod_clamp);
        }
    }

    explicit ImageOperands(EmitContext& ctx, TextureType type, bool promote,
                           const IR::Value& offset, const IR::Value& offset2)
        : texture_type{type}, needs_promotion{promote} {
        if (offset2.IsEmpty()) {
            AddOffset(ctx, offset, ImageGatherOffsetAllowed);
            return;
        }
        const std::array values{offset.InstRecursive(), offset2.InstRecursive()};
        if (!values[0]->AreAllArgsImmediates() || !values[1]->AreAllArgsImmediates()) {
            LOG_WARNING(Shader_SPIRV, "Not all arguments in PTP are immediate, ignoring");
            return;
        }
        const IR::Opcode opcode{values[0]->GetOpcode()};
        if (opcode != values[1]->GetOpcode() || opcode != IR::Opcode::CompositeConstructU32x4) {
            throw LogicError("Invalid PTP arguments");
        }
        auto read{[&](unsigned int a, unsigned int b) { return values[a]->Arg(b).U32(); }};

        const Id offsets{ctx.ConstantComposite(
            ctx.TypeArray(ctx.U32[2], ctx.Const(4U)), ctx.Const(read(0, 0), read(0, 1)),
            ctx.Const(read(0, 2), read(0, 3)), ctx.Const(read(1, 0), read(1, 1)),
            ctx.Const(read(1, 2), read(1, 3)))};
        Add(spv::ImageOperandsMask::ConstOffsets, offsets);
    }

    explicit ImageOperands(Id lod, Id ms) {
        if (Sirit::ValidId(lod)) {
            Add(spv::ImageOperandsMask::Lod, lod);
        }
        if (Sirit::ValidId(ms)) {
            Add(spv::ImageOperandsMask::Sample, ms);
        }
    }

    explicit ImageOperands(EmitContext& ctx, TextureType type, bool promote, bool has_lod_clamp,
                           Id derivatives, u32 num_derivatives, const IR::Value& offset,
                           Id lod_clamp)
        : texture_type{type}, needs_promotion{promote} {
        if (!Sirit::ValidId(derivatives)) {
            throw LogicError("Derivatives must be present");
        }
        boost::container::static_vector<Id, 3> deriv_x_accum;
        boost::container::static_vector<Id, 3> deriv_y_accum;
        for (u32 i = 0; i < num_derivatives; ++i) {
            deriv_x_accum.push_back(ctx.OpCompositeExtract(ctx.F32[1], derivatives, i * 2));
            deriv_y_accum.push_back(ctx.OpCompositeExtract(ctx.F32[1], derivatives, i * 2 + 1));
        }
        const u32 expected_components{needs_promotion ? ExpectedDerivativeComponents(texture_type)
                                                      : num_derivatives};
        while (needs_promotion && deriv_x_accum.size() < expected_components) {
            deriv_x_accum.push_back(ctx.f32_zero_value);
            deriv_y_accum.push_back(ctx.f32_zero_value);
        }
        const Id derivatives_X{ctx.OpCompositeConstruct(
            ctx.F32[expected_components], std::span{deriv_x_accum.data(), deriv_x_accum.size()})};
        const Id derivatives_Y{ctx.OpCompositeConstruct(
            ctx.F32[expected_components], std::span{deriv_y_accum.data(), deriv_y_accum.size()})};
        Add(spv::ImageOperandsMask::Grad, derivatives_X, derivatives_Y);
        AddOffset(ctx, offset, ImageGradientOffsetAllowed);
        if (has_lod_clamp) {
            Add(spv::ImageOperandsMask::MinLod, lod_clamp);
        }
    }

    explicit ImageOperands(EmitContext& ctx, TextureType type, bool promote, bool has_lod_clamp,
                           Id derivatives_1, Id derivatives_2, const IR::Value& offset,
                           Id lod_clamp)
        : texture_type{type}, needs_promotion{promote} {
        if (!Sirit::ValidId(derivatives_1) || !Sirit::ValidId(derivatives_2)) {
            throw LogicError("Derivatives must be present");
        }
        boost::container::static_vector<Id, 3> deriv_1_accum{
            ctx.OpCompositeExtract(ctx.F32[1], derivatives_1, 0),
            ctx.OpCompositeExtract(ctx.F32[1], derivatives_1, 2),
            ctx.OpCompositeExtract(ctx.F32[1], derivatives_2, 0),
        };
        boost::container::static_vector<Id, 3> deriv_2_accum{
            ctx.OpCompositeExtract(ctx.F32[1], derivatives_1, 1),
            ctx.OpCompositeExtract(ctx.F32[1], derivatives_1, 3),
            ctx.OpCompositeExtract(ctx.F32[1], derivatives_2, 1),
        };
        const Id derivatives_id1{ctx.OpCompositeConstruct(
            ctx.F32[3], std::span{deriv_1_accum.data(), deriv_1_accum.size()})};
        const Id derivatives_id2{ctx.OpCompositeConstruct(
            ctx.F32[3], std::span{deriv_2_accum.data(), deriv_2_accum.size()})};
        Add(spv::ImageOperandsMask::Grad, derivatives_id1, derivatives_id2);
        AddOffset(ctx, offset, ImageGradientOffsetAllowed);
        if (has_lod_clamp) {
            Add(spv::ImageOperandsMask::MinLod, lod_clamp);
        }
    }

    std::span<const Id> Span() const noexcept {
        return std::span{operands.data(), operands.size()};
    }

    std::optional<spv::ImageOperandsMask> MaskOptional() const noexcept {
        return mask != spv::ImageOperandsMask{} ? std::make_optional(mask) : std::nullopt;
    }

    spv::ImageOperandsMask Mask() const noexcept {
        return mask;
    }

private:
    void AddOffset(EmitContext& ctx, const IR::Value& offset, bool runtime_offset_allowed) {
        if (offset.IsEmpty()) {
            return;
        }
        const u32 required_components{BaseDimension(texture_type)};
        const bool promote_offset{needs_promotion && required_components > 1};

        auto build_const_offset{[&](const boost::container::small_vector<s32, 3>& values) -> Id {
            switch (values.size()) {
            case 1:
                return ctx.SConst(values[0]);
            case 2:
                return ctx.SConst(values[0], values[1]);
            case 3:
                return ctx.SConst(values[0], values[1], values[2]);
            default:
                throw LogicError("Unsupported constant offset component count {}",
                                 values.size());
            }
        }};

        auto pad_components{[&](boost::container::small_vector<s32, 3>& components) {
            while (promote_offset && components.size() < required_components) {
                components.push_back(0);
            }
        }};

        if (offset.IsImmediate()) {
            boost::container::small_vector<s32, 3> components{
                static_cast<s32>(offset.U32())};
            pad_components(components);
            Add(spv::ImageOperandsMask::ConstOffset, build_const_offset(components));
            return;
        }
        IR::Inst* const inst{offset.InstRecursive()};
        if (inst->AreAllArgsImmediates()) {
            switch (inst->GetOpcode()) {
            case IR::Opcode::CompositeConstructU32x2:
            case IR::Opcode::CompositeConstructU32x3:
            case IR::Opcode::CompositeConstructU32x4: {
                boost::container::small_vector<s32, 3> components;
                for (u32 i = 0; i < inst->NumArgs(); ++i) {
                    components.push_back(static_cast<s32>(inst->Arg(i).U32()));
                }
                pad_components(components);
                Add(spv::ImageOperandsMask::ConstOffset, build_const_offset(components));
                return;
            }
            default:
                break;
            }
        }
        if (!runtime_offset_allowed) {
            return;
        }
        Id offset_id{ctx.Def(offset)};
        if (promote_offset) {
            offset_id = PromoteOffset(ctx, offset, offset_id, texture_type);
        }
        Add(spv::ImageOperandsMask::Offset, offset_id);
    }

    void Add(spv::ImageOperandsMask new_mask, Id value) {
        mask = static_cast<spv::ImageOperandsMask>(static_cast<unsigned>(mask) |
                                                   static_cast<unsigned>(new_mask));
        operands.push_back(value);
    }

    void Add(spv::ImageOperandsMask new_mask, Id value_1, Id value_2) {
        mask = static_cast<spv::ImageOperandsMask>(static_cast<unsigned>(mask) |
                                                   static_cast<unsigned>(new_mask));
        operands.push_back(value_1);
        operands.push_back(value_2);
    }

    TextureType texture_type{TextureType::Color2D};
    bool needs_promotion{};
    boost::container::static_vector<Id, 4> operands;
    spv::ImageOperandsMask mask{};
};

Id Texture(EmitContext& ctx, IR::TextureInstInfo info, [[maybe_unused]] const IR::Value& index) {
    const TextureDefinition& def{ctx.textures.at(info.descriptor_index)};
    if (def.count > 1) {
        const Id pointer{ctx.OpAccessChain(def.pointer_type, def.id, ctx.Def(index))};
        return ctx.OpLoad(def.sampled_type, pointer);
    } else {
        return ctx.OpLoad(def.sampled_type, def.id);
    }
}

Id TextureColorResultType(EmitContext& ctx, const TextureDefinition& def) {
    switch (def.component_type) {
    case SamplerComponentType::Float:
    case SamplerComponentType::Depth:
        return ctx.F32[4];
    case SamplerComponentType::Sint:
        return ctx.S32[4];
    case SamplerComponentType::Stencil:
        return ctx.U32[4];
    case SamplerComponentType::Uint:
        return ctx.U32[4];
    }
    throw InvalidArgument("Invalid sampler component type {}", def.component_type);
}

Id TextureSampleResultToFloat(EmitContext& ctx, const TextureDefinition& def, Id color) {
    switch (def.component_type) {
    case SamplerComponentType::Float:
    case SamplerComponentType::Depth:
        return color;
    case SamplerComponentType::Sint:
        return ctx.OpConvertSToF(ctx.F32[4], color);
    case SamplerComponentType::Stencil:
        {
            const Id converted{ctx.OpConvertUToF(ctx.F32[4], color)};
            const Id inv255{ctx.Const(1.0f / 255.0f)};
            const Id scale{ctx.ConstantComposite(ctx.F32[4], inv255, inv255, inv255, inv255)};
            return ctx.OpFMul(ctx.F32[4], converted, scale);
        }
    case SamplerComponentType::Uint:
        return ctx.OpConvertUToF(ctx.F32[4], color);
    }
    throw InvalidArgument("Invalid sampler component type {}", def.component_type);
}

Id TextureImage(EmitContext& ctx, IR::TextureInstInfo info, const IR::Value& index) {
    if (!index.IsImmediate() || index.U32() != 0) {
        throw NotImplementedException("Indirect image indexing");
    }
    if (info.type == TextureType::Buffer) {
        const TextureBufferDefinition& def{ctx.texture_buffers.at(info.descriptor_index)};
        if (def.count > 1) {
            throw NotImplementedException("Indirect texture sample");
        }
        return ctx.OpLoad(ctx.image_buffer_type, def.id);
    } else {
        const TextureDefinition& def{ctx.textures.at(info.descriptor_index)};
        if (def.count > 1) {
            throw NotImplementedException("Indirect texture sample");
        }
        return ctx.OpImage(def.image_type, ctx.OpLoad(def.sampled_type, def.id));
    }
}

std::pair<Id, bool> Image(EmitContext& ctx, const IR::Value& index, IR::TextureInstInfo info) {
    if (!index.IsImmediate() || index.U32() != 0) {
        throw NotImplementedException("Indirect image indexing");
    }
    if (info.type == TextureType::Buffer) {
        const ImageBufferDefinition def{ctx.image_buffers.at(info.descriptor_index)};
        return {ctx.OpLoad(def.image_type, def.id), def.is_integer};
    } else {
        const ImageDefinition def{ctx.images.at(info.descriptor_index)};
        return {ctx.OpLoad(def.image_type, def.id), def.is_integer};
    }
}

bool IsTextureMsaa(EmitContext& ctx, const IR::TextureInstInfo& info) {
    if (info.type == TextureType::Buffer) {
        return false;
    }
    return ctx.textures.at(info.descriptor_index).is_multisample;
}

Id Decorate(EmitContext& ctx, IR::Inst* inst, Id sample) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    if (info.relaxed_precision != 0) {
        ctx.Decorate(sample, spv::Decoration::RelaxedPrecision);
    }
    return sample;
}

template <typename MethodPtrType, typename... Args>
Id Emit(MethodPtrType sparse_ptr, MethodPtrType non_sparse_ptr, EmitContext& ctx, IR::Inst* inst,
        Id result_type, Args&&... args) {
    IR::Inst* const sparse{inst->GetAssociatedPseudoOperation(IR::Opcode::GetSparseFromOp)};
    if (!sparse) {
        return Decorate(ctx, inst, (ctx.*non_sparse_ptr)(result_type, std::forward<Args>(args)...));
    }
    const Id struct_type{ctx.TypeStruct(ctx.U32[1], result_type)};
    const Id sample{(ctx.*sparse_ptr)(struct_type, std::forward<Args>(args)...)};
    const Id resident_code{ctx.OpCompositeExtract(ctx.U32[1], sample, 0U)};
    sparse->SetDefinition(ctx.OpImageSparseTexelsResident(ctx.U1, resident_code));
    sparse->Invalidate();
    Decorate(ctx, inst, sample);
    return ctx.OpCompositeExtract(result_type, sample, 1U);
}

Id IsScaled(EmitContext& ctx, const IR::Value& index, Id member_index, u32 base_index) {
    const Id push_constant_u32{ctx.TypePointer(spv::StorageClass::PushConstant, ctx.U32[1])};
    Id bit{};
    if (index.IsImmediate()) {
        // Use BitwiseAnd instead of BitfieldExtract for better codegen on Nvidia OpenGL.
        // LOP32I.NZ is used to set the predicate rather than BFE+ISETP.
        const u32 index_value{index.U32() + base_index};
        const Id word_index{ctx.Const(index_value / 32)};
        const Id bit_index_mask{ctx.Const(1u << (index_value % 32))};
        const Id pointer{ctx.OpAccessChain(push_constant_u32, ctx.rescaling_push_constants,
                                           member_index, word_index)};
        const Id word{ctx.OpLoad(ctx.U32[1], pointer)};
        bit = ctx.OpBitwiseAnd(ctx.U32[1], word, bit_index_mask);
    } else {
        Id index_value{ctx.Def(index)};
        if (base_index != 0) {
            index_value = ctx.OpIAdd(ctx.U32[1], index_value, ctx.Const(base_index));
        }
        const Id bit_index{ctx.OpBitwiseAnd(ctx.U32[1], index_value, ctx.Const(31u))};
        bit = ctx.OpBitFieldUExtract(ctx.U32[1], index_value, bit_index, ctx.Const(1u));
    }
    return ctx.OpINotEqual(ctx.U1, bit, ctx.u32_zero_value);
}

Id BitTest(EmitContext& ctx, Id mask, Id bit) {
    const Id shifted{ctx.OpShiftRightLogical(ctx.U32[1], mask, bit)};
    const Id bit_value{ctx.OpBitwiseAnd(ctx.U32[1], shifted, ctx.Const(1u))};
    return ctx.OpINotEqual(ctx.U1, bit_value, ctx.u32_zero_value);
}

Id ImageGatherSubpixelOffset(EmitContext& ctx, TextureType type, Id texture, Id coords) {
    // Apply a subpixel offset of 1/512 the texel size of the texture to ensure same rounding on
    // AMD hardware as on Maxwell or other Nvidia architectures.
    const auto calculate_coords{[&](size_t dim) {
        const Id nudge{ctx.Const(0x1p-9f)};
        const Id image_size{ctx.OpImageQuerySizeLod(ctx.U32[dim], texture, ctx.u32_zero_value)};
        Id offset{dim == 2 ? ctx.ConstantComposite(ctx.F32[dim], nudge, nudge)
                           : ctx.ConstantComposite(ctx.F32[dim], nudge, nudge, ctx.f32_zero_value)};
        offset = ctx.OpFDiv(ctx.F32[dim], offset, ctx.OpConvertUToF(ctx.F32[dim], image_size));
        return ctx.OpFAdd(ctx.F32[dim], coords, offset);
    }};
    switch (type) {
    case TextureType::Color2D:
    case TextureType::Color2DRect:
        return calculate_coords(2);
    case TextureType::ColorArray2D:
    case TextureType::ColorCube:
        return calculate_coords(3);
    default:
        return coords;
    }
}

void AddOffsetToCoordinates(EmitContext& ctx, TextureType type, bool promoted_from_1d, Id& coords,
                            Id offset) {
    if (!Sirit::ValidId(offset)) {
        return;
    }

    auto PadScalarOffset = [&](u32 components) {
        boost::container::static_vector<Id, 3> elems;
        elems.push_back(offset);
        while (elems.size() < components) {
            elems.push_back(ctx.u32_zero_value);
        }
        offset = ctx.OpCompositeConstruct(ctx.U32[components],
                                          std::span{elems.data(), elems.size()});
    };

    Id result_type{};
    switch (type) {
    case TextureType::Buffer:
    case TextureType::Color1D: {
        result_type = ctx.U32[1];
        break;
    }
    case TextureType::ColorArray1D:
        offset = ctx.OpCompositeConstruct(ctx.U32[2], offset, ctx.u32_zero_value);
        [[fallthrough]];
    case TextureType::Color2D:
    case TextureType::Color2DRect: {
        if (promoted_from_1d) {
            PadScalarOffset(2);
        }
        result_type = ctx.U32[2];
        break;
    }
    case TextureType::ColorArray2D:
        if (promoted_from_1d) {
            PadScalarOffset(3);
        } else {
            offset = ctx.OpCompositeConstruct(ctx.U32[3],
                                              ctx.OpCompositeExtract(ctx.U32[1], coords, 0),
                                              ctx.OpCompositeExtract(ctx.U32[1], coords, 1),
                                              ctx.u32_zero_value);
        }
        [[fallthrough]];
    case TextureType::Color3D: {
        result_type = ctx.U32[3];
        break;
    }
    case TextureType::ColorCube:
    case TextureType::ColorArrayCube:
        return;
    }
    coords = ctx.OpIAdd(result_type, coords, offset);
}
} // Anonymous namespace

Id EmitBindlessImageSampleImplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageSampleExplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageSampleDrefImplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageSampleDrefExplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageGather(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageGatherDref(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageFetch(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageQueryDimensions(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageQueryLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageGradient(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageRead(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageWrite(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageSampleImplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageSampleExplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageSampleDrefImplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageSampleDrefExplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageGather(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageGatherDref(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageFetch(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageQueryDimensions(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageQueryLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageGradient(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageRead(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageWrite(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitImageSampleImplicitLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                              Id bias_lc, const IR::Value& offset) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const TextureDefinition& def{ctx.textures.at(info.descriptor_index)};
    const TextureType effective_type{GetEffectiveType(ctx, info.type)};
    const bool needs_promotion{Needs1DPromotion(ctx.profile, info.type)};
    const Id color_type{TextureColorResultType(ctx, def)};
    const Id texture{Texture(ctx, info, index)};
    coords = PromoteCoordinate(ctx, inst, info, coords);
    Id color{};
    if (ctx.stage == Stage::Fragment) {
        const ImageOperands operands(ctx, effective_type, needs_promotion, info.has_bias != 0,
                         false, info.has_lod_clamp != 0, bias_lc, offset);
        color = Emit(&EmitContext::OpImageSparseSampleImplicitLod,
                     &EmitContext::OpImageSampleImplicitLod, ctx, inst, color_type, texture,
                     coords, operands.MaskOptional(), operands.Span());
    } else {
        // We can't use implicit lods on non-fragment stages on SPIR-V. Maxwell hardware behaves as
        // if the lod was explicitly zero.  This may change on Turing with implicit compute
        // derivatives
        const Id lod{ctx.Const(0.0f)};
        const ImageOperands operands(ctx, effective_type, needs_promotion, false, true,
                         info.has_lod_clamp != 0, lod, offset);
        color = Emit(&EmitContext::OpImageSparseSampleExplicitLod,
                     &EmitContext::OpImageSampleExplicitLod, ctx, inst, color_type, texture,
                     coords, operands.Mask(), operands.Span());
    }
    return TextureSampleResultToFloat(ctx, def, color);
}

Id EmitImageSampleExplicitLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                              Id lod, const IR::Value& offset) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const TextureDefinition& def{ctx.textures.at(info.descriptor_index)};
    const TextureType effective_type{GetEffectiveType(ctx, info.type)};
    const bool needs_promotion{Needs1DPromotion(ctx.profile, info.type)};
    const Id color_type{TextureColorResultType(ctx, def)};
    coords = PromoteCoordinate(ctx, inst, info, coords);
    const ImageOperands operands(ctx, effective_type, needs_promotion, false, true, false, lod,
                                 offset);
    const Id color{Emit(&EmitContext::OpImageSparseSampleExplicitLod,
                        &EmitContext::OpImageSampleExplicitLod, ctx, inst, color_type,
                        Texture(ctx, info, index), coords, operands.Mask(), operands.Span())};
    return TextureSampleResultToFloat(ctx, def, color);
}

Id EmitImageSampleDrefImplicitLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                                  Id coords, Id dref, Id bias_lc, const IR::Value& offset) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const TextureType effective_type{GetEffectiveType(ctx, info.type)};
    const bool needs_promotion{Needs1DPromotion(ctx.profile, info.type)};
    coords = PromoteCoordinate(ctx, inst, info, coords);
    if (ctx.stage == Stage::Fragment) {
        const ImageOperands operands(ctx, effective_type, needs_promotion, info.has_bias != 0,
                                     false, info.has_lod_clamp != 0, bias_lc, offset);
        return Emit(&EmitContext::OpImageSparseSampleDrefImplicitLod,
                    &EmitContext::OpImageSampleDrefImplicitLod, ctx, inst, ctx.F32[1],
                    Texture(ctx, info, index), coords, dref, operands.MaskOptional(),
                    operands.Span());
    } else {
        // Implicit lods in compute behave on hardware as if sampling from LOD 0.
        // This check is to ensure all drivers behave this way.
        const Id lod{ctx.Const(0.0f)};
        const ImageOperands operands(ctx, effective_type, needs_promotion, false, true, false,
                         lod, offset);
        return Emit(&EmitContext::OpImageSparseSampleDrefExplicitLod,
                    &EmitContext::OpImageSampleDrefExplicitLod, ctx, inst, ctx.F32[1],
                    Texture(ctx, info, index), coords, dref, operands.Mask(), operands.Span());
    }
}

Id EmitImageSampleDrefExplicitLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                                  Id coords, Id dref, Id lod, const IR::Value& offset) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const TextureType effective_type{GetEffectiveType(ctx, info.type)};
    const bool needs_promotion{Needs1DPromotion(ctx.profile, info.type)};
    coords = PromoteCoordinate(ctx, inst, info, coords);
    const ImageOperands operands(ctx, effective_type, needs_promotion, false, true, false, lod,
                                 offset);
    return Emit(&EmitContext::OpImageSparseSampleDrefExplicitLod,
                &EmitContext::OpImageSampleDrefExplicitLod, ctx, inst, ctx.F32[1],
                Texture(ctx, info, index), coords, dref, operands.Mask(), operands.Span());
}

Id EmitImageGather(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                   const IR::Value& offset, const IR::Value& offset2) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const TextureDefinition& def{ctx.textures.at(info.descriptor_index)};
    const TextureType effective_type{GetEffectiveType(ctx, info.type)};
    const bool needs_promotion{Needs1DPromotion(ctx.profile, info.type)};
    const Id color_type{TextureColorResultType(ctx, def)};
    coords = PromoteCoordinate(ctx, inst, info, coords);
    const ImageOperands operands(ctx, effective_type, needs_promotion, offset, offset2);
    const Id texture{Texture(ctx, info, index)};
    if (ctx.profile.need_gather_subpixel_offset) {
        coords = ImageGatherSubpixelOffset(ctx, effective_type, TextureImage(ctx, info, index),
                                           coords);
    }
    const Id color{
        Emit(&EmitContext::OpImageSparseGather, &EmitContext::OpImageGather, ctx, inst, color_type,
             texture, coords, ctx.Const(info.gather_component), operands.MaskOptional(),
             operands.Span())};
    return TextureSampleResultToFloat(ctx, def, color);
}

Id EmitImageGatherDref(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                       const IR::Value& offset, const IR::Value& offset2, Id dref) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const TextureType effective_type{GetEffectiveType(ctx, info.type)};
    const bool needs_promotion{Needs1DPromotion(ctx.profile, info.type)};
    coords = PromoteCoordinate(ctx, inst, info, coords);
    const ImageOperands operands(ctx, effective_type, needs_promotion, offset, offset2);
    if (ctx.profile.need_gather_subpixel_offset) {
        coords = ImageGatherSubpixelOffset(ctx, effective_type, TextureImage(ctx, info, index),
                                           coords);
    }
    return Emit(&EmitContext::OpImageSparseDrefGather, &EmitContext::OpImageDrefGather, ctx, inst,
                ctx.F32[4], Texture(ctx, info, index), coords, dref, operands.MaskOptional(),
                operands.Span());
}

Id EmitImageFetch(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords, Id offset,
                  Id lod, Id ms) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const TextureDefinition* def =
        info.type == TextureType::Buffer ? nullptr : &ctx.textures.at(info.descriptor_index);
    const TextureType effective_type{GetEffectiveType(ctx, info.type)};
    const bool needs_promotion{Needs1DPromotion(ctx.profile, info.type)};
    const Id result_type{def ? TextureColorResultType(ctx, *def) : ctx.F32[4]};
    coords = PromoteCoordinate(ctx, inst, info, coords);
    AddOffsetToCoordinates(ctx, effective_type, needs_promotion, coords, offset);
    if (info.type == TextureType::Buffer) {
        lod = Id{};
    }
    if (Sirit::ValidId(ms)) {
        // This image is multisampled, lod must be implicit
        lod = Id{};
    }
    const ImageOperands operands(lod, ms);
    Id color{Emit(&EmitContext::OpImageSparseFetch, &EmitContext::OpImageFetch, ctx, inst,
                  result_type, TextureImage(ctx, info, index), coords, operands.MaskOptional(),
                  operands.Span())};
    if (def) {
        color = TextureSampleResultToFloat(ctx, *def, color);
    }
    return color;
}

Id EmitImageQueryDimensions(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id lod,
                            const IR::Value& skip_mips_val) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const TextureType effective_type{GetEffectiveType(ctx, info.type)};
    const bool needs_promotion{Needs1DPromotion(ctx.profile, info.type)};
    const Id image{TextureImage(ctx, info, index)};
    const Id zero{ctx.u32_zero_value};
    const bool skip_mips{skip_mips_val.U1()};
    const auto mips{[&] { return skip_mips ? zero : ctx.OpImageQueryLevels(ctx.U32[1], image); }};
    const bool is_msaa{IsTextureMsaa(ctx, info)};
    const bool uses_lod{!is_msaa && info.type != TextureType::Buffer};

    const u32 query_components = effective_type == TextureType::Buffer
                                     ? 1u
                                     : BaseDimension(effective_type) +
                                           (HasLayerComponent(effective_type) ? 1u : 0u);
    const Id query_type{ctx.U32[std::max(1u, query_components)]};
    const Id size = uses_lod ? ctx.OpImageQuerySizeLod(query_type, image, lod)
                             : ctx.OpImageQuerySize(query_type, image);
    const auto extract = [&](u32 index) -> Id {
        if (query_components == 1) {
            return size;
        }
        return ctx.OpCompositeExtract(ctx.U32[1], size, index);
    };

    switch (info.type) {
    case TextureType::Color1D:
        return ctx.OpCompositeConstruct(ctx.U32[4], extract(0), zero, zero, mips());
    case TextureType::ColorArray1D: {
        const Id width{extract(0)};
        const Id layers{needs_promotion ? extract(2) : extract(1)};
        return ctx.OpCompositeConstruct(ctx.U32[4], width, layers, zero, mips());
    }
    case TextureType::Color2D:
    case TextureType::ColorCube:
    case TextureType::Color2DRect: {
        const Id width{extract(0)};
        const Id height{extract(1)};
        return ctx.OpCompositeConstruct(ctx.U32[4], width, height, zero, mips());
    }
    case TextureType::ColorArray2D:
    case TextureType::Color3D:
    case TextureType::ColorArrayCube: {
        const Id width{extract(0)};
        const Id height{extract(1)};
        const Id depth{extract(2)};
        return ctx.OpCompositeConstruct(ctx.U32[4], width, height, depth, mips());
    }
    case TextureType::Buffer:
        return ctx.OpCompositeConstruct(ctx.U32[4], extract(0), zero, zero, mips());
    }
    throw LogicError("Unspecified image type {}", info.type.Value());
}

Id EmitImageQueryLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const Id zero{ctx.f32_zero_value};
    const Id sampler{Texture(ctx, info, index)};
    coords = PromoteCoordinate(ctx, inst, info, coords);
    return ctx.OpCompositeConstruct(ctx.F32[4], ctx.OpImageQueryLod(ctx.F32[2], sampler, coords),
                                    zero, zero);
}

Id EmitImageGradient(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                     Id derivatives, const IR::Value& offset, Id lod_clamp) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const TextureDefinition& def{ctx.textures.at(info.descriptor_index)};
    const TextureType effective_type{GetEffectiveType(ctx, info.type)};
    const bool needs_promotion{Needs1DPromotion(ctx.profile, info.type)};
    const Id color_type{TextureColorResultType(ctx, def)};
    coords = PromoteCoordinate(ctx, inst, info, coords);
    const auto operands = info.num_derivatives == 3
                              ? ImageOperands(ctx, effective_type, needs_promotion,
                                              info.has_lod_clamp != 0, derivatives,
                                              ctx.Def(offset), IR::Value{}, lod_clamp)
                              : ImageOperands(ctx, effective_type, needs_promotion,
                                              info.has_lod_clamp != 0, derivatives,
                                              info.num_derivatives, offset, lod_clamp);
    const Id color{Emit(&EmitContext::OpImageSparseSampleExplicitLod,
                        &EmitContext::OpImageSampleExplicitLod, ctx, inst, color_type,
                        Texture(ctx, info, index), coords, operands.Mask(), operands.Span())};
    return TextureSampleResultToFloat(ctx, def, color);
}

Id EmitImageRead(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    if (info.image_format == ImageFormat::Typeless && !ctx.profile.support_typeless_image_loads) {
        LOG_WARNING(Shader_SPIRV, "Typeless image read not supported by host");
        return ctx.ConstantNull(ctx.U32[4]);
    }
    coords = PromoteCoordinate(ctx, inst, info, coords);
    const auto [image, is_integer] = Image(ctx, index, info);
    const Id result_type{is_integer ? ctx.U32[4] : ctx.F32[4]};
    Id color{Emit(&EmitContext::OpImageSparseRead, &EmitContext::OpImageRead, ctx, inst,
                  result_type, image, coords, std::nullopt, std::span<const Id>{})};
    if (!is_integer) {
        color = ctx.OpBitcast(ctx.U32[4], color);
    }
    return color;
}

void EmitImageWrite(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords, Id color) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    coords = PromoteCoordinate(ctx, inst, info, coords);
    const auto [image, is_integer] = Image(ctx, index, info);
    if (!is_integer) {
        color = ctx.OpBitcast(ctx.F32[4], color);
    }
    ctx.OpImageWrite(image, coords, color);
}

Id EmitIsTextureScaled(EmitContext& ctx, const IR::Value& index) {
    if (ctx.profile.unified_descriptor_binding) {
        const Id member_index{ctx.Const(ctx.rescaling_textures_member_index)};
        return IsScaled(ctx, index, member_index, ctx.texture_rescaling_index);
    } else {
        const Id composite{ctx.OpLoad(ctx.F32[4], ctx.rescaling_uniform_constant)};
        const Id mask_f32{ctx.OpCompositeExtract(ctx.F32[1], composite, 0u)};
        const Id mask{ctx.OpBitcast(ctx.U32[1], mask_f32)};
        return BitTest(ctx, mask, ctx.Def(index));
    }
}

Id EmitIsImageScaled(EmitContext& ctx, const IR::Value& index) {
    if (ctx.profile.unified_descriptor_binding) {
        const Id member_index{ctx.Const(ctx.rescaling_images_member_index)};
        return IsScaled(ctx, index, member_index, ctx.image_rescaling_index);
    } else {
        const Id composite{ctx.OpLoad(ctx.F32[4], ctx.rescaling_uniform_constant)};
        const Id mask_f32{ctx.OpCompositeExtract(ctx.F32[1], composite, 1u)};
        const Id mask{ctx.OpBitcast(ctx.U32[1], mask_f32)};
        return BitTest(ctx, mask, ctx.Def(index));
    }
}

} // namespace Shader::Backend::SPIRV
