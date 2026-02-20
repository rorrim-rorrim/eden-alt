// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <span>
#include <zlib.h>

#include "common/settings.h"
#include "common/settings_enums.h"
#include "common/string_util.h"
#include "common/swap.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/vfs/vfs.h"

namespace FileSys {

const std::array<const char*, size_t(Language::Count)> LANGUAGE_NAMES{{
    "AmericanEnglish",
    "BritishEnglish",
    "Japanese",
    "French",
    "German",
    "LatinAmericanSpanish",
    "Spanish",
    "Italian",
    "Dutch",
    "CanadianFrench",
    "Portuguese",
    "Russian",
    "Korean",
    "TraditionalChinese",
    "SimplifiedChinese",
    "BrazilianPortuguese",
    "Polish",
    "Thai",
}};

namespace {
constexpr std::size_t LEGACY_LANGUAGE_REGION_SIZE = sizeof(std::array<LanguageEntry, 16>);
constexpr std::size_t PACKED_LANGUAGE_REGION_MAX_SIZE = sizeof(LanguageEntry) * 32;

bool InflateRawDeflate(std::span<const u8> compressed, std::vector<u8>& out) {
    if (compressed.empty() || compressed.size() > std::numeric_limits<uInt>::max()) {
        return false;
    }

    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressed.data()));
    stream.avail_in = static_cast<uInt>(compressed.size());
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        return false;
    }

    std::array<u8, 0x1000> chunk{};
    int ret = Z_OK;
    while (ret == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(chunk.data());
        stream.avail_out = static_cast<uInt>(chunk.size());
        ret = inflate(&stream, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&stream);
            return false;
        }

        const auto produced = chunk.size() - static_cast<std::size_t>(stream.avail_out);
        if (produced != 0) {
            if (out.size() + produced > PACKED_LANGUAGE_REGION_MAX_SIZE) {
                inflateEnd(&stream);
                return false;
            }
            out.insert(out.end(), chunk.begin(),
                       chunk.begin() + static_cast<std::ptrdiff_t>(produced));
        }
    }

    inflateEnd(&stream);
    return ret == Z_STREAM_END;
}

void DecodePackedLanguageEntries(RawNACP& raw) {
    auto* packed_region = reinterpret_cast<u8*>(raw.language_entries.data());
    u16_le compressed_size_le{};
    std::memcpy(&compressed_size_le, packed_region, sizeof(compressed_size_le));
    const auto compressed_size = static_cast<std::size_t>(compressed_size_le);

    if (compressed_size == 0 || compressed_size > LEGACY_LANGUAGE_REGION_SIZE - sizeof(u16_le)) {
        return;
    }

    std::vector<u8> decompressed;
    if (!InflateRawDeflate(
            std::span<const u8>(packed_region + sizeof(u16_le), compressed_size), decompressed)) {
        return;
    }

    if (decompressed.size() < LEGACY_LANGUAGE_REGION_SIZE ||
        decompressed.size() % sizeof(LanguageEntry) != 0) {
        return;
    }

    std::memcpy(raw.language_entries.data(), decompressed.data(), LEGACY_LANGUAGE_REGION_SIZE);
}
} // namespace

std::string LanguageEntry::GetApplicationName() const {
    return Common::StringFromFixedZeroTerminatedBuffer(application_name.data(), application_name.size());
}

std::string LanguageEntry::GetDeveloperName() const {
    return Common::StringFromFixedZeroTerminatedBuffer(developer_name.data(), developer_name.size());
}

constexpr std::array<Language, 20> language_to_codes = {{
    Language::Japanese,
    Language::AmericanEnglish,
    Language::French,
    Language::German,
    Language::Italian,
    Language::Spanish,
    Language::SimplifiedChinese,
    Language::Korean,
    Language::Dutch,
    Language::Portuguese,
    Language::Russian,
    Language::TraditionalChinese,
    Language::BritishEnglish,
    Language::CanadianFrench,
    Language::LatinAmericanSpanish,
    Language::SimplifiedChinese,
    Language::TraditionalChinese,
    Language::BrazilianPortuguese,
    Language::Polish,
    Language::Thai,
}};

NACP::NACP() = default;

NACP::NACP(VirtualFile file) {
    file->ReadObject(&raw);
    DecodePackedLanguageEntries(raw);
}

NACP::~NACP() = default;

const LanguageEntry& NACP::GetLanguageEntry() const {
    auto const language = []{
        switch (Settings::values.language_index.GetValue()) {
        case Settings::Language::Chinese: return Language::SimplifiedChinese;
        case Settings::Language::ChineseSimplified: return Language::SimplifiedChinese;
        case Settings::Language::ChineseTraditional: return Language::TraditionalChinese;
        case Settings::Language::Dutch: return Language::Dutch;
        case Settings::Language::EnglishAmerican: return Language::AmericanEnglish;
        case Settings::Language::EnglishBritish: return Language::BritishEnglish;
        case Settings::Language::French: return Language::French;
        case Settings::Language::FrenchCanadian: return Language::CanadianFrench;
        case Settings::Language::German: return Language::German;
        case Settings::Language::Italian: return Language::Italian;
        case Settings::Language::Korean: return Language::Korean;
        case Settings::Language::Japanese: return Language::Japanese;
        case Settings::Language::Portuguese: return Language::Portuguese;
        case Settings::Language::PortugueseBrazilian: return Language::BrazilianPortuguese;
        case Settings::Language::Russian: return Language::Russian;
        case Settings::Language::Serbian: return Language::Russian;
        case Settings::Language::Spanish: return Language::Spanish;
        case Settings::Language::SpanishLatin: return Language::LatinAmericanSpanish;
        case Settings::Language::Taiwanese: return Language::SimplifiedChinese;
        case Settings::Language::Thai: return Language::Thai;
        case Settings::Language::Polish: return Language::Polish;
        }
    }();
    {
        const auto& language_entry = raw.language_entries.at(static_cast<u8>(language));
        if (!language_entry.GetApplicationName().empty())
            return language_entry;
    }
    for (const auto& language_entry : raw.language_entries) {
        if (!language_entry.GetApplicationName().empty())
            return language_entry;
    }
    return raw.language_entries.at(static_cast<u8>(Language::AmericanEnglish));
}

std::array<std::string, 16> NACP::GetApplicationNames() const {
    std::array<std::string, 16> names{};
    for (size_t i = 0; i < raw.language_entries.size(); ++i) {
        names[i] = raw.language_entries[i].GetApplicationName();
    }
    return names;
}

std::string NACP::GetApplicationName() const {
    return GetLanguageEntry().GetApplicationName();
}

std::string NACP::GetDeveloperName() const {
    return GetLanguageEntry().GetDeveloperName();
}

u64 NACP::GetTitleId() const {
    return raw.save_data_owner_id;
}

u64 NACP::GetDLCBaseTitleId() const {
    return raw.dlc_base_title_id;
}

std::string NACP::GetVersionString() const {
    return Common::StringFromFixedZeroTerminatedBuffer(raw.version_string.data(),
                                                       raw.version_string.size());
}

u64 NACP::GetDefaultNormalSaveSize() const {
    return raw.user_account_save_data_size;
}

u64 NACP::GetDefaultJournalSaveSize() const {
    return raw.user_account_save_data_journal_size;
}

bool NACP::GetUserAccountSwitchLock() const {
    return raw.user_account_switch_lock != 0;
}

u32 NACP::GetSupportedLanguages() const {
    return raw.supported_languages;
}

u64 NACP::GetDeviceSaveDataSize() const {
    return raw.device_save_data_size;
}

u32 NACP::GetParentalControlFlag() const {
    return raw.parental_control;
}

const std::array<u8, 0x20>& NACP::GetRatingAge() const {
    return raw.rating_age;
}

std::vector<u8> NACP::GetRawBytes() const {
    std::vector<u8> out(sizeof(RawNACP));
    std::memcpy(out.data(), &raw, sizeof(RawNACP));
    return out;
}
} // namespace FileSys
