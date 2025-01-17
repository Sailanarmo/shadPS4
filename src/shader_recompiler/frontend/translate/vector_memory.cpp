// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/frontend/translate/translate.h"

namespace Shader::Gcn {

void Translator::EmitVectorMemory(const GcnInst& inst) {
    switch (inst.opcode) {
    case Opcode::IMAGE_SAMPLE_LZ_O:
    case Opcode::IMAGE_SAMPLE_O:
    case Opcode::IMAGE_SAMPLE_C:
    case Opcode::IMAGE_SAMPLE_C_LZ:
    case Opcode::IMAGE_SAMPLE_LZ:
    case Opcode::IMAGE_SAMPLE:
    case Opcode::IMAGE_SAMPLE_L:
    case Opcode::IMAGE_SAMPLE_C_O:
    case Opcode::IMAGE_SAMPLE_B:
    case Opcode::IMAGE_SAMPLE_C_LZ_O:
        return IMAGE_SAMPLE(inst);
    case Opcode::IMAGE_GATHER4_C:
    case Opcode::IMAGE_GATHER4_LZ:
    case Opcode::IMAGE_GATHER4_LZ_O:
        return IMAGE_GATHER(inst);
    case Opcode::IMAGE_ATOMIC_ADD:
        return IMAGE_ATOMIC(AtomicOp::Add, inst);
    case Opcode::IMAGE_ATOMIC_AND:
        return IMAGE_ATOMIC(AtomicOp::And, inst);
    case Opcode::IMAGE_ATOMIC_OR:
        return IMAGE_ATOMIC(AtomicOp::Or, inst);
    case Opcode::IMAGE_ATOMIC_XOR:
        return IMAGE_ATOMIC(AtomicOp::Xor, inst);
    case Opcode::IMAGE_ATOMIC_UMAX:
        return IMAGE_ATOMIC(AtomicOp::Umax, inst);
    case Opcode::IMAGE_ATOMIC_SMAX:
        return IMAGE_ATOMIC(AtomicOp::Smax, inst);
    case Opcode::IMAGE_ATOMIC_UMIN:
        return IMAGE_ATOMIC(AtomicOp::Umin, inst);
    case Opcode::IMAGE_ATOMIC_SMIN:
        return IMAGE_ATOMIC(AtomicOp::Smin, inst);
    case Opcode::IMAGE_ATOMIC_INC:
        return IMAGE_ATOMIC(AtomicOp::Inc, inst);
    case Opcode::IMAGE_ATOMIC_DEC:
        return IMAGE_ATOMIC(AtomicOp::Dec, inst);
    case Opcode::IMAGE_GET_LOD:
        return IMAGE_GET_LOD(inst);
    case Opcode::IMAGE_STORE:
        return IMAGE_STORE(inst);
    case Opcode::IMAGE_LOAD_MIP:
        return IMAGE_LOAD(true, inst);
    case Opcode::IMAGE_LOAD:
        return IMAGE_LOAD(false, inst);
    case Opcode::IMAGE_GET_RESINFO:
        return IMAGE_GET_RESINFO(inst);

        // Buffer load operations
    case Opcode::TBUFFER_LOAD_FORMAT_X:
        return BUFFER_LOAD_FORMAT(1, true, true, inst);
    case Opcode::TBUFFER_LOAD_FORMAT_XY:
        return BUFFER_LOAD_FORMAT(2, true, true, inst);
    case Opcode::TBUFFER_LOAD_FORMAT_XYZ:
        return BUFFER_LOAD_FORMAT(3, true, true, inst);
    case Opcode::TBUFFER_LOAD_FORMAT_XYZW:
        return BUFFER_LOAD_FORMAT(4, true, true, inst);

    case Opcode::BUFFER_LOAD_FORMAT_X:
        return BUFFER_LOAD_FORMAT(1, false, true, inst);
    case Opcode::BUFFER_LOAD_FORMAT_XY:
        return BUFFER_LOAD_FORMAT(2, false, true, inst);
    case Opcode::BUFFER_LOAD_FORMAT_XYZ:
        return BUFFER_LOAD_FORMAT(3, false, true, inst);
    case Opcode::BUFFER_LOAD_FORMAT_XYZW:
        return BUFFER_LOAD_FORMAT(4, false, true, inst);

    case Opcode::BUFFER_LOAD_DWORD:
        return BUFFER_LOAD_FORMAT(1, false, false, inst);
    case Opcode::BUFFER_LOAD_DWORDX2:
        return BUFFER_LOAD_FORMAT(2, false, false, inst);
    case Opcode::BUFFER_LOAD_DWORDX3:
        return BUFFER_LOAD_FORMAT(3, false, false, inst);
    case Opcode::BUFFER_LOAD_DWORDX4:
        return BUFFER_LOAD_FORMAT(4, false, false, inst);

        // Buffer store operations
    case Opcode::BUFFER_STORE_FORMAT_X:
        return BUFFER_STORE_FORMAT(1, false, true, inst);
    case Opcode::BUFFER_STORE_FORMAT_XY:
        return BUFFER_STORE_FORMAT(2, false, true, inst);
    case Opcode::BUFFER_STORE_FORMAT_XYZ:
        return BUFFER_STORE_FORMAT(3, false, true, inst);
    case Opcode::BUFFER_STORE_FORMAT_XYZW:
        return BUFFER_STORE_FORMAT(4, false, true, inst);

    case Opcode::BUFFER_STORE_DWORD:
        return BUFFER_STORE_FORMAT(1, false, false, inst);
    case Opcode::BUFFER_STORE_DWORDX2:
        return BUFFER_STORE_FORMAT(2, false, false, inst);
    case Opcode::BUFFER_STORE_DWORDX3:
        return BUFFER_STORE_FORMAT(3, false, false, inst);
    case Opcode::BUFFER_STORE_DWORDX4:
        return BUFFER_STORE_FORMAT(4, false, false, inst);
    default:
        LogMissingOpcode(inst);
    }
}

void Translator::IMAGE_GET_RESINFO(const GcnInst& inst) {
    IR::VectorReg dst_reg{inst.dst[0].code};
    const IR::ScalarReg tsharp_reg{inst.src[2].code * 4};
    const auto flags = ImageResFlags(inst.control.mimg.dmask);
    const bool has_mips = flags.test(ImageResComponent::MipCount);
    const IR::U32 lod = ir.GetVectorReg(IR::VectorReg(inst.src[0].code));
    const IR::Value tsharp = ir.GetScalarReg(tsharp_reg);
    const IR::Value size = ir.ImageQueryDimension(tsharp, lod, ir.Imm1(has_mips));

    if (flags.test(ImageResComponent::Width)) {
        ir.SetVectorReg(dst_reg++, IR::U32{ir.CompositeExtract(size, 0)});
    }
    if (flags.test(ImageResComponent::Height)) {
        ir.SetVectorReg(dst_reg++, IR::U32{ir.CompositeExtract(size, 1)});
    }
    if (flags.test(ImageResComponent::Depth)) {
        ir.SetVectorReg(dst_reg++, IR::U32{ir.CompositeExtract(size, 2)});
    }
    if (has_mips) {
        ir.SetVectorReg(dst_reg++, IR::U32{ir.CompositeExtract(size, 3)});
    }
}

void Translator::IMAGE_SAMPLE(const GcnInst& inst) {
    const auto& mimg = inst.control.mimg;
    if (mimg.da) {
        LOG_WARNING(Render_Vulkan, "Image instruction declares an array");
    }

    IR::VectorReg addr_reg{inst.src[0].code};
    IR::VectorReg dest_reg{inst.dst[0].code};
    const IR::ScalarReg tsharp_reg{inst.src[2].code * 4};
    const IR::ScalarReg sampler_reg{inst.src[3].code * 4};
    const auto flags = MimgModifierFlags(mimg.mod);

    // Load first dword of T# and S#. We will use them as the handle that will guide resource
    // tracking pass where to read the sharps. This will later also get patched to the SPIRV texture
    // binding index.
    const IR::Value handle =
        ir.CompositeConstruct(ir.GetScalarReg(tsharp_reg), ir.GetScalarReg(sampler_reg));

    // Load first address components as denoted in 8.2.4 VGPR Usage Sea Islands Series Instruction
    // Set Architecture
    const IR::U32 offset =
        flags.test(MimgModifier::Offset) ? ir.GetVectorReg<IR::U32>(addr_reg++) : IR::U32{};
    const IR::F32 bias =
        flags.test(MimgModifier::LodBias) ? ir.GetVectorReg<IR::F32>(addr_reg++) : IR::F32{};
    const IR::F32 dref =
        flags.test(MimgModifier::Pcf) ? ir.GetVectorReg<IR::F32>(addr_reg++) : IR::F32{};

    // Derivatives are tricky because their number depends on the texture type which is located in
    // T#. We don't have access to T# though until resource tracking pass. For now assume no
    // derivatives are present, otherwise we don't know where coordinates are placed in the address
    // stream.
    ASSERT_MSG(!flags.test(MimgModifier::Derivative), "Derivative image instruction");

    // Now we can load body components as noted in Table 8.9 Image Opcodes with Sampler
    // Since these are at most 4 dwords, we load them into a single uvec4 and place them
    // in coords field of the instruction. Then the resource tracking pass will patch the
    // IR instruction to fill in lod_clamp field.
    const IR::Value body = ir.CompositeConstruct(
        ir.GetVectorReg<IR::F32>(addr_reg), ir.GetVectorReg<IR::F32>(addr_reg + 1),
        ir.GetVectorReg<IR::F32>(addr_reg + 2), ir.GetVectorReg<IR::F32>(addr_reg + 3));

    const bool explicit_lod = flags.any(MimgModifier::Level0, MimgModifier::Lod);

    IR::TextureInstInfo info{};
    info.is_depth.Assign(flags.test(MimgModifier::Pcf));
    info.has_bias.Assign(flags.test(MimgModifier::LodBias));
    info.has_lod_clamp.Assign(flags.test(MimgModifier::LodClamp));
    info.force_level0.Assign(flags.test(MimgModifier::Level0));
    info.has_offset.Assign(flags.test(MimgModifier::Offset));
    info.explicit_lod.Assign(explicit_lod);

    // Issue IR instruction, leaving unknown fields blank to patch later.
    const IR::Value texel = [&]() -> IR::Value {
        if (!flags.test(MimgModifier::Pcf)) {
            if (explicit_lod) {
                return ir.ImageSampleExplicitLod(handle, body, offset, info);
            } else {
                return ir.ImageSampleImplicitLod(handle, body, bias, offset, info);
            }
        }
        if (explicit_lod) {
            return ir.ImageSampleDrefExplicitLod(handle, body, dref, offset, info);
        }
        return ir.ImageSampleDrefImplicitLod(handle, body, dref, bias, offset, info);
    }();

    for (u32 i = 0; i < 4; i++) {
        if (((mimg.dmask >> i) & 1) == 0) {
            continue;
        }
        IR::F32 value;
        if (flags.test(MimgModifier::Pcf)) {
            value = i < 3 ? IR::F32{texel} : ir.Imm32(1.0f);
        } else {
            value = IR::F32{ir.CompositeExtract(texel, i)};
        }
        ir.SetVectorReg(dest_reg++, value);
    }
}

void Translator::IMAGE_GATHER(const GcnInst& inst) {
    const auto& mimg = inst.control.mimg;
    if (mimg.da) {
        LOG_WARNING(Render_Vulkan, "Image instruction declares an array");
    }

    IR::VectorReg addr_reg{inst.src[0].code};
    IR::VectorReg dest_reg{inst.dst[0].code};
    const IR::ScalarReg tsharp_reg{inst.src[2].code * 4};
    const IR::ScalarReg sampler_reg{inst.src[3].code * 4};
    const auto flags = MimgModifierFlags(mimg.mod);

    // Load first dword of T# and S#. We will use them as the handle that will guide resource
    // tracking pass where to read the sharps. This will later also get patched to the SPIRV texture
    // binding index.
    const IR::Value handle =
        ir.CompositeConstruct(ir.GetScalarReg(tsharp_reg), ir.GetScalarReg(sampler_reg));

    // Load first address components as denoted in 8.2.4 VGPR Usage Sea Islands Series Instruction
    // Set Architecture
    const IR::Value offset =
        flags.test(MimgModifier::Offset) ? ir.GetVectorReg(addr_reg++) : IR::Value{};
    const IR::F32 bias =
        flags.test(MimgModifier::LodBias) ? ir.GetVectorReg<IR::F32>(addr_reg++) : IR::F32{};
    const IR::F32 dref =
        flags.test(MimgModifier::Pcf) ? ir.GetVectorReg<IR::F32>(addr_reg++) : IR::F32{};

    // Derivatives are tricky because their number depends on the texture type which is located in
    // T#. We don't have access to T# though until resource tracking pass. For now assume no
    // derivatives are present, otherwise we don't know where coordinates are placed in the address
    // stream.
    ASSERT_MSG(!flags.test(MimgModifier::Derivative), "Derivative image instruction");

    // Now we can load body components as noted in Table 8.9 Image Opcodes with Sampler
    // Since these are at most 4 dwords, we load them into a single uvec4 and place them
    // in coords field of the instruction. Then the resource tracking pass will patch the
    // IR instruction to fill in lod_clamp field.
    const IR::Value body = ir.CompositeConstruct(
        ir.GetVectorReg<IR::F32>(addr_reg), ir.GetVectorReg<IR::F32>(addr_reg + 1),
        ir.GetVectorReg<IR::F32>(addr_reg + 2), ir.GetVectorReg<IR::F32>(addr_reg + 3));

    const bool explicit_lod = flags.any(MimgModifier::Level0, MimgModifier::Lod);

    IR::TextureInstInfo info{};
    info.is_depth.Assign(flags.test(MimgModifier::Pcf));
    info.has_bias.Assign(flags.test(MimgModifier::LodBias));
    info.has_lod_clamp.Assign(flags.test(MimgModifier::LodClamp));
    info.force_level0.Assign(flags.test(MimgModifier::Level0));
    // info.explicit_lod.Assign(explicit_lod);
    info.gather_comp.Assign(std::bit_width(mimg.dmask) - 1);

    // Issue IR instruction, leaving unknown fields blank to patch later.
    const IR::Value texel = [&]() -> IR::Value {
        const IR::F32 lod = flags.test(MimgModifier::Level0) ? ir.Imm32(0.f) : IR::F32{};
        if (!flags.test(MimgModifier::Pcf)) {
            return ir.ImageGather(handle, body, offset, info);
        }
        ASSERT(mimg.dmask & 1); // should be always 1st (R) component
        return ir.ImageGatherDref(handle, body, offset, dref, info);
    }();

    // For gather4 instructions dmask selects which component to read and must have
    // only one bit set to 1
    ASSERT_MSG(std::popcount(mimg.dmask) == 1, "Unexpected bits in gather dmask");
    for (u32 i = 0; i < 4; i++) {
        const IR::F32 value = IR::F32{ir.CompositeExtract(texel, i)};
        ir.SetVectorReg(dest_reg++, value);
    }
}

void Translator::IMAGE_LOAD(bool has_mip, const GcnInst& inst) {
    const auto& mimg = inst.control.mimg;
    IR::VectorReg addr_reg{inst.src[0].code};
    IR::VectorReg dest_reg{inst.dst[0].code};
    const IR::ScalarReg tsharp_reg{inst.src[2].code * 4};

    const IR::Value handle = ir.GetScalarReg(tsharp_reg);
    const IR::Value body =
        ir.CompositeConstruct(ir.GetVectorReg(addr_reg), ir.GetVectorReg(addr_reg + 1),
                              ir.GetVectorReg(addr_reg + 2), ir.GetVectorReg(addr_reg + 3));

    IR::TextureInstInfo info{};
    info.explicit_lod.Assign(has_mip);
    const IR::Value texel = ir.ImageFetch(handle, body, {}, {}, {}, info);

    for (u32 i = 0; i < 4; i++) {
        if (((mimg.dmask >> i) & 1) == 0) {
            continue;
        }
        IR::F32 value = IR::F32{ir.CompositeExtract(texel, i)};
        ir.SetVectorReg(dest_reg++, value);
    }
}

void Translator::IMAGE_STORE(const GcnInst& inst) {
    const auto& mimg = inst.control.mimg;
    IR::VectorReg addr_reg{inst.src[0].code};
    IR::VectorReg data_reg{inst.dst[0].code};
    const IR::ScalarReg tsharp_reg{inst.src[2].code * 4};

    const IR::Value handle = ir.GetScalarReg(tsharp_reg);
    const IR::Value body =
        ir.CompositeConstruct(ir.GetVectorReg(addr_reg), ir.GetVectorReg(addr_reg + 1),
                              ir.GetVectorReg(addr_reg + 2), ir.GetVectorReg(addr_reg + 3));

    boost::container::static_vector<IR::F32, 4> comps;
    for (u32 i = 0; i < 4; i++) {
        if (((mimg.dmask >> i) & 1) == 0) {
            comps.push_back(ir.Imm32(0.f));
            continue;
        }
        comps.push_back(ir.GetVectorReg<IR::F32>(data_reg++));
    }
    const IR::Value value = ir.CompositeConstruct(comps[0], comps[1], comps[2], comps[3]);
    ir.ImageWrite(handle, body, value, {});
}

void Translator::BUFFER_LOAD_FORMAT(u32 num_dwords, bool is_typed, bool is_format,
                                    const GcnInst& inst) {
    const auto& mtbuf = inst.control.mtbuf;
    const IR::VectorReg vaddr{inst.src[0].code};
    const IR::ScalarReg sharp{inst.src[2].code * 4};
    const IR::Value address = [&] -> IR::Value {
        if (mtbuf.idxen && mtbuf.offen) {
            return ir.CompositeConstruct(ir.GetVectorReg(vaddr), ir.GetVectorReg(vaddr + 1));
        }
        if (mtbuf.idxen || mtbuf.offen) {
            return ir.GetVectorReg(vaddr);
        }
        return {};
    }();
    const IR::Value soffset{GetSrc(inst.src[3])};
    ASSERT_MSG(soffset.IsImmediate() && soffset.U32() == 0, "Non immediate offset not supported");

    IR::BufferInstInfo info{};
    info.index_enable.Assign(mtbuf.idxen);
    info.offset_enable.Assign(mtbuf.offen);
    info.inst_offset.Assign(mtbuf.offset);
    info.is_typed.Assign(is_typed);
    if (is_typed) {
        info.dmft.Assign(static_cast<AmdGpu::DataFormat>(mtbuf.dfmt));
        info.nfmt.Assign(static_cast<AmdGpu::NumberFormat>(mtbuf.nfmt));
        ASSERT(info.nfmt == AmdGpu::NumberFormat::Float &&
               (info.dmft == AmdGpu::DataFormat::Format32_32_32_32 ||
                info.dmft == AmdGpu::DataFormat::Format32_32_32 ||
                info.dmft == AmdGpu::DataFormat::Format32_32 ||
                info.dmft == AmdGpu::DataFormat::Format32));
    }

    const IR::Value handle =
        ir.CompositeConstruct(ir.GetScalarReg(sharp), ir.GetScalarReg(sharp + 1),
                              ir.GetScalarReg(sharp + 2), ir.GetScalarReg(sharp + 3));
    const IR::Value value = is_format ? ir.LoadBufferFormat(num_dwords, handle, address, info)
                                      : ir.LoadBuffer(num_dwords, handle, address, info);
    const IR::VectorReg dst_reg{inst.src[1].code};
    if (num_dwords == 1) {
        ir.SetVectorReg(dst_reg, IR::F32{value});
        return;
    }
    for (u32 i = 0; i < num_dwords; i++) {
        ir.SetVectorReg(dst_reg + i, IR::F32{ir.CompositeExtract(value, i)});
    }
}

void Translator::BUFFER_STORE_FORMAT(u32 num_dwords, bool is_typed, bool is_format,
                                     const GcnInst& inst) {
    const auto& mtbuf = inst.control.mtbuf;
    const IR::VectorReg vaddr{inst.src[0].code};
    const IR::ScalarReg sharp{inst.src[2].code * 4};
    const IR::Value address = [&] -> IR::Value {
        if (mtbuf.idxen && mtbuf.offen) {
            return ir.CompositeConstruct(ir.GetVectorReg(vaddr), ir.GetVectorReg(vaddr + 1));
        }
        if (mtbuf.idxen || mtbuf.offen) {
            return ir.GetVectorReg(vaddr);
        }
        return {};
    }();
    const IR::Value soffset{GetSrc(inst.src[3])};
    ASSERT_MSG(soffset.IsImmediate() && soffset.U32() == 0, "Non immediate offset not supported");

    IR::BufferInstInfo info{};
    info.index_enable.Assign(mtbuf.idxen);
    info.offset_enable.Assign(mtbuf.offen);
    info.inst_offset.Assign(mtbuf.offset);
    info.is_typed.Assign(is_typed);
    if (is_typed) {
        info.dmft.Assign(static_cast<AmdGpu::DataFormat>(mtbuf.dfmt));
        info.nfmt.Assign(static_cast<AmdGpu::NumberFormat>(mtbuf.nfmt));
    }

    IR::Value value{};
    const IR::VectorReg src_reg{inst.src[1].code};
    switch (num_dwords) {
    case 1:
        value = ir.GetVectorReg<Shader::IR::F32>(src_reg);
        break;
    case 2:
        value = ir.CompositeConstruct(ir.GetVectorReg<Shader::IR::F32>(src_reg),
                                      ir.GetVectorReg<Shader::IR::F32>(src_reg + 1));
        break;
    case 3:
        value = ir.CompositeConstruct(ir.GetVectorReg<Shader::IR::F32>(src_reg),
                                      ir.GetVectorReg<Shader::IR::F32>(src_reg + 1),
                                      ir.GetVectorReg<Shader::IR::F32>(src_reg + 2));
        break;
    case 4:
        value = ir.CompositeConstruct(ir.GetVectorReg<Shader::IR::F32>(src_reg),
                                      ir.GetVectorReg<Shader::IR::F32>(src_reg + 1),
                                      ir.GetVectorReg<Shader::IR::F32>(src_reg + 2),
                                      ir.GetVectorReg<Shader::IR::F32>(src_reg + 3));
        break;
    }
    const IR::Value handle =
        ir.CompositeConstruct(ir.GetScalarReg(sharp), ir.GetScalarReg(sharp + 1),
                              ir.GetScalarReg(sharp + 2), ir.GetScalarReg(sharp + 3));
    if (is_format) {
        ir.StoreBufferFormat(num_dwords, handle, address, value, info);
    } else {
        ir.StoreBuffer(num_dwords, handle, address, value, info);
    }
}

void Translator::IMAGE_GET_LOD(const GcnInst& inst) {
    const auto& mimg = inst.control.mimg;
    IR::VectorReg dst_reg{inst.dst[0].code};
    IR::VectorReg addr_reg{inst.src[0].code};
    const IR::ScalarReg tsharp_reg{inst.src[2].code * 4};

    const IR::Value handle = ir.GetScalarReg(tsharp_reg);
    const IR::Value body = ir.CompositeConstruct(
        ir.GetVectorReg<IR::F32>(addr_reg), ir.GetVectorReg<IR::F32>(addr_reg + 1),
        ir.GetVectorReg<IR::F32>(addr_reg + 2), ir.GetVectorReg<IR::F32>(addr_reg + 3));
    const IR::Value lod = ir.ImageQueryLod(handle, body, {});
    ir.SetVectorReg(dst_reg++, IR::F32{ir.CompositeExtract(lod, 0)});
    ir.SetVectorReg(dst_reg++, IR::F32{ir.CompositeExtract(lod, 1)});
}

void Translator::IMAGE_ATOMIC(AtomicOp op, const GcnInst& inst) {
    const auto& mimg = inst.control.mimg;
    IR::VectorReg val_reg{inst.dst[0].code};
    IR::VectorReg addr_reg{inst.src[0].code};
    const IR::ScalarReg tsharp_reg{inst.src[2].code * 4};

    const IR::Value value = ir.GetVectorReg(val_reg);
    const IR::Value handle = ir.GetScalarReg(tsharp_reg);
    const IR::Value body =
        ir.CompositeConstruct(ir.GetVectorReg(addr_reg), ir.GetVectorReg(addr_reg + 1),
                              ir.GetVectorReg(addr_reg + 2), ir.GetVectorReg(addr_reg + 3));
    const IR::Value prev = [&] {
        switch (op) {
        case AtomicOp::Swap:
            return ir.ImageAtomicExchange(handle, body, value, {});
        case AtomicOp::Add:
            return ir.ImageAtomicIAdd(handle, body, value, {});
        case AtomicOp::Smin:
            return ir.ImageAtomicIMin(handle, body, value, true, {});
        case AtomicOp::Umin:
            return ir.ImageAtomicUMin(handle, body, value, {});
        case AtomicOp::Smax:
            return ir.ImageAtomicIMax(handle, body, value, true, {});
        case AtomicOp::Umax:
            return ir.ImageAtomicUMax(handle, body, value, {});
        case AtomicOp::And:
            return ir.ImageAtomicAnd(handle, body, value, {});
        case AtomicOp::Or:
            return ir.ImageAtomicOr(handle, body, value, {});
        case AtomicOp::Xor:
            return ir.ImageAtomicXor(handle, body, value, {});
        case AtomicOp::Inc:
            return ir.ImageAtomicInc(handle, body, value, {});
        case AtomicOp::Dec:
            return ir.ImageAtomicDec(handle, body, value, {});
        default:
            UNREACHABLE();
        }
    }();
    if (mimg.glc) {
        ir.SetVectorReg(val_reg, IR::U32{prev});
    }
}

} // namespace Shader::Gcn
