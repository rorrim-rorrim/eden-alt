// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <boost/container/static_vector.hpp>
#include "common/alignment.h"
#include "common/swap.h"
#include "core/file_sys/fssystem/fssystem_aes_ctr_storage.h"
#include "core/file_sys/fssystem/fssystem_utility.h"

namespace FileSys {

void AesCtrStorage::MakeIv(void* dst, size_t dst_size, u64 upper, s64 offset) {
    ASSERT(dst != nullptr);
    ASSERT(dst_size == IvSize);
    ASSERT(offset >= 0);

    const uintptr_t out_addr = reinterpret_cast<uintptr_t>(dst);

    *reinterpret_cast<u64_be*>(out_addr + 0) = upper;
    *reinterpret_cast<s64_be*>(out_addr + sizeof(u64)) = static_cast<s64>(offset / BlockSize);
}

AesCtrStorage::AesCtrStorage(VirtualFile base, const void* key, size_t key_size, const void* iv,
                             size_t iv_size)
    : m_base_storage(std::move(base)) {
    ASSERT(m_base_storage != nullptr);
    ASSERT(key != nullptr);
    ASSERT(iv != nullptr);
    ASSERT(key_size == KeySize);
    ASSERT(iv_size == IvSize);

    std::memcpy(m_key.data(), key, KeySize);
    std::memcpy(m_iv.data(), iv, IvSize);

    m_cipher.emplace(m_key, Core::Crypto::Mode::CTR);
}

size_t AesCtrStorage::Read(u8* buffer, size_t size, size_t offset) const {
    // Allow zero-size reads.
    if (size == 0) {
        return size;
    }

    // Ensure buffer is valid.
    ASSERT(buffer != nullptr);

    // We can only read at block aligned offsets.
    ASSERT(Common::IsAligned(offset, BlockSize));
    ASSERT(Common::IsAligned(size, BlockSize));

    // Read the data.
    m_base_storage->Read(buffer, size, offset);

    // Setup the counter.
    std::array<u8, IvSize> ctr;
    std::memcpy(ctr.data(), m_iv.data(), IvSize);
    AddCounter(ctr.data(), IvSize, offset / BlockSize);

    // Decrypt.
    m_cipher->SetIV(ctr);
    m_cipher->Transcode(buffer, size, buffer, Core::Crypto::Op::Decrypt);

    return size;
}

size_t AesCtrStorage::Write(const u8* buffer, size_t size, size_t offset) {
    // Allow zero-size writes.
    if (size == 0) {
        return size;
    }

    // Ensure buffer is valid.
    ASSERT(buffer != nullptr);

    // We can only write at block aligned offsets.
    ASSERT(Common::IsAligned(offset, BlockSize));
    ASSERT(Common::IsAligned(size, BlockSize));

    // Setup the counter.
    std::array<u8, IvSize> ctr;
    std::memcpy(ctr.data(), m_iv.data(), IvSize);
    AddCounter(ctr.data(), IvSize, offset / BlockSize);

    // Loop until all data is written using a pooled buffer residing on the stack (blocksize = 0x10)
    boost::container::static_vector<u8, BlockSize> pooled_buffer;
    for (size_t remaining = size; remaining > 0; ) {
        // Determine data we're writing and where.
        auto const write_size = (std::min)(pooled_buffer.size(), remaining);
        u8* write_buf = pooled_buffer.data();

        // Encrypt the data and then write it.
        m_cipher->SetIV(ctr);
        m_cipher->Transcode(buffer, write_size, write_buf, Core::Crypto::Op::Encrypt);
        m_base_storage->Write(write_buf, write_size, offset);

        // Advance next write chunk
        offset += write_size;
        remaining -= write_size;
        if (remaining > 0)
            AddCounter(ctr.data(), IvSize, write_size / BlockSize);
    }
    return size;
}

size_t AesCtrStorage::GetSize() const {
    return m_base_storage->GetSize();
}

} // namespace FileSys
