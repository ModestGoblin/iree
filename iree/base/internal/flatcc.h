#// Copyright 2020 The IREE Authors
//
#// Licensed under the Apache License v2.0 with LLVM Exceptions.
#// See https://llvm.org/LICENSE.txt for license information.
#// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef IREE_BASE_INTERNAL_FLATCC_H_
#define IREE_BASE_INTERNAL_FLATCC_H_

//===----------------------------------------------------------------------===//
// flatcc include order fixes
//===----------------------------------------------------------------------===//
//
// This header merely wraps the flatcc headers that are generally useful to
// include in various places that may not know the specific messages they are
// working with.
//
// If using flatcc prefer to include this file over any hard-to-handle flatcc
// file such as flatbuffers_common_reader.h or flatbuffers_common_builder.h.
//
// NOTE: order matters for these includes so stop clang from messing with it:
// clang-format off

#include "flatcc/reflection/flatbuffers_common_reader.h"
#include "iree/base/internal/flatcc_reader.h"

#include "flatcc/flatcc_verifier.h"
#include "iree/base/internal/flatcc_verifier.h"

#include "flatcc/flatcc_builder.h"
#include "flatcc/reflection/flatbuffers_common_builder.h"
#include "iree/base/internal/flatcc_builder.h"

#include "flatcc/flatcc_json_parser.h"
#include "iree/base/internal/flatcc_json_parser.h"

#include "flatcc/flatcc_json_printer.h"
#include "iree/base/internal/flatcc_json_printer.h"

// clang-format on

#endif  // IREE_BASE_INTERNAL_FLATCC_H_
