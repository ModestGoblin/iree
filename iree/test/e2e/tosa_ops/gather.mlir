func @gather_float() attributes { iree.module.export } {
  %0 = constant dense<[[[1.0, 2.0], [3.0, 4.0]]]> : tensor<1x2x2xf32>
  %1 = "tosa.const"() { value = dense<[[1, 0]]> : tensor<1x2xi32> } : ()  -> (tensor<1x2xi32>)
  %2 = "tosa.gather"(%0, %1) : (tensor<1x2x2xf32>, tensor<1x2xi32>) -> (tensor<1x2x2xf32>)
  check.expect_eq_const(%2, dense<[[[3.0, 4.0], [1.0, 2.0]]]> : tensor<1x2x2xf32>) : tensor<1x2x2xf32>
  return
}

func @gather_int() attributes { iree.module.export } {
  %0 = constant dense<[[[1, 2], [3, 4]]]> : tensor<1x2x2xi32>
  %1 = "tosa.const"() { value = dense<[[1, 0]]> : tensor<1x2xi32> } : ()  -> (tensor<1x2xi32>)
  %2 = "tosa.gather"(%0, %1) : (tensor<1x2x2xi32>, tensor<1x2xi32>) -> (tensor<1x2x2xi32>)
  check.expect_eq_const(%2, dense<[[[3, 4], [1, 2]]]> : tensor<1x2x2xi32>) : tensor<1x2x2xi32>
  return
}
