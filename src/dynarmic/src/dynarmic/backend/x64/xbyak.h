// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

// TODO(lizzie): Defining this for Ankerl crashes e v e r y t h i n g, see issue #228 in xbyak
#define XBYAK_STD_UNORDERED_SET boost::unordered_set
#define XBYAK_STD_UNORDERED_MAP boost::unordered_map
#define XBYAK_STD_UNORDERED_MULTIMAP boost::unordered_multimap

#include <xbyak/xbyak.h>
#include <xbyak/xbyak_util.h>
