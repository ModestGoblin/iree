// Copyright 2020 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// This files defines a helper to trigger the registration of dialects to
// the system.
//
// Based on MLIR's InitAllDialects but without dialects we don't care about.

#ifndef IREE_TOOLS_INIT_MLIR_DIALECTS_H_
#define IREE_TOOLS_INIT_MLIR_DIALECTS_H_

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/GPU/GPUDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/Linalg/IR/LinalgOps.h"
#include "mlir/Dialect/Math/IR/Math.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/Quant/QuantOps.h"
#include "mlir/Dialect/SCF/SCF.h"
#include "mlir/Dialect/SDBM/SDBMDialect.h"
#include "mlir/Dialect/SPIRV/IR/SPIRVDialect.h"
#include "mlir/Dialect/Shape/IR/Shape.h"
#include "mlir/Dialect/StandardOps/IR/Ops.h"
#include "mlir/Dialect/Tensor/IR/Tensor.h"
#include "mlir/Dialect/Tosa/IR/TosaOps.h"
#include "mlir/Dialect/Vector/VectorOps.h"
#include "mlir/IR/Dialect.h"

namespace mlir {

// Add all the MLIR dialects to the provided registry.
inline void registerMlirDialects(DialectRegistry &registry) {
  // clang-format off
  registry.insert<AffineDialect,
                  gpu::GPUDialect,
                  LLVM::LLVMDialect,
                  linalg::LinalgDialect,
                  math::MathDialect,
                  memref::MemRefDialect,
                  scf::SCFDialect,
                  quant::QuantizationDialect,
                  spirv::SPIRVDialect,
                  StandardOpsDialect,
                  vector::VectorDialect,
                  tensor::TensorDialect,
                  tosa::TosaDialect,
                  SDBMDialect,
                  shape::ShapeDialect>();
  // clang-format on
}

}  // namespace mlir

#endif  // IREE_TOOLS_INIT_MLIR_DIALECTS_H_
