// Copyright 2021 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef IREE_COMPILER_DIALECT_MODULES_VMVX_IR_VMVXOPS_H_
#define IREE_COMPILER_DIALECT_MODULES_VMVX_IR_VMVXOPS_H_

#include <cstdint>

#include "iree/compiler/Dialect/IREE/IR/IREETraits.h"
#include "iree/compiler/Dialect/Modules/VMVX/IR/VMVXTypes.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Dialect.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

#define GET_OP_CLASSES
#include "iree/compiler/Dialect/Modules/VMVX/IR/VMVXOps.h.inc"

#endif  // IREE_COMPILER_DIALECT_MODULES_VMVX_IR_VMVXOPS_H_
