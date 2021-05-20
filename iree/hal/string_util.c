// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "iree/hal/string_util.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "iree/base/api.h"
#include "iree/base/internal/math.h"
#include "iree/hal/buffer_view.h"

IREE_API_EXPORT iree_status_t iree_hal_parse_shape(
    iree_string_view_t value, iree_host_size_t shape_capacity,
    iree_hal_dim_t* out_shape, iree_host_size_t* out_shape_rank) {
  IREE_ASSERT_ARGUMENT(out_shape_rank);
  *out_shape_rank = 0;

  if (iree_string_view_is_empty(value)) {
    return iree_ok_status();  // empty shape
  }

  // Count the number of dimensions to see if we have capacity.
  iree_host_size_t shape_rank = 1;  // always at least one if we are not empty
  for (iree_host_size_t i = 0; i < value.size; ++i) {
    if (value.data[i] == 'x') ++shape_rank;
  }
  if (out_shape_rank) {
    *out_shape_rank = shape_rank;
  }
  if (shape_rank > shape_capacity) {
    // NOTE: fast return for capacity queries.
    return iree_status_from_code(IREE_STATUS_OUT_OF_RANGE);
  }

  iree_host_size_t dim_index = 0;
  iree_string_view_t lhs;
  iree_string_view_t rhs = value;
  while (iree_string_view_split(rhs, 'x', &lhs, &rhs) &&
         !iree_string_view_is_empty(lhs)) {
    int32_t dim_value = 0;
    if (!iree_string_view_atoi_int32(lhs, &dim_value)) {
      return iree_make_status(IREE_STATUS_INVALID_ARGUMENT,
                              "shape[%zu] invalid value '%.*s' of '%.*s'",
                              dim_index, (int)lhs.size, lhs.data,
                              (int)value.size, value.data);
    }
    if (dim_value < 0) {
      return iree_make_status(IREE_STATUS_INVALID_ARGUMENT,
                              "shape[%zu] unsupported value %d of '%.*s'",
                              dim_index, dim_value, (int)value.size,
                              value.data);
    }
    out_shape[dim_index++] = dim_value;
  }
  if (dim_index != shape_rank) {
    return iree_make_status(IREE_STATUS_INVALID_ARGUMENT,
                            "invalid shape specification: '%.*s'",
                            (int)value.size, value.data);
  }
  return iree_ok_status();
}

IREE_API_EXPORT iree_status_t
iree_hal_format_shape(const iree_hal_dim_t* shape, iree_host_size_t shape_rank,
                      iree_host_size_t buffer_capacity, char* buffer,
                      iree_host_size_t* out_buffer_length) {
  if (out_buffer_length) {
    *out_buffer_length = 0;
  }
  iree_host_size_t buffer_length = 0;
  for (iree_host_size_t i = 0; i < shape_rank; ++i) {
    int n = snprintf(buffer ? buffer + buffer_length : NULL,
                     buffer ? buffer_capacity - buffer_length : 0,
                     (i < shape_rank - 1) ? "%dx" : "%d", shape[i]);
    if (n < 0) {
      return iree_make_status(IREE_STATUS_FAILED_PRECONDITION,
                              "snprintf failed to write dimension %zu", i);
    } else if (buffer && n >= buffer_capacity - buffer_length) {
      buffer = NULL;
    }
    buffer_length += n;
  }
  if (out_buffer_length) {
    *out_buffer_length = buffer_length;
  }
  return buffer ? iree_ok_status()
                : iree_status_from_code(IREE_STATUS_OUT_OF_RANGE);
}

IREE_API_EXPORT iree_status_t iree_hal_parse_element_type(
    iree_string_view_t value, iree_hal_element_type_t* out_element_type) {
  IREE_ASSERT_ARGUMENT(out_element_type);
  *out_element_type = IREE_HAL_ELEMENT_TYPE_NONE;

  iree_string_view_t str_value = value;
  iree_hal_numerical_type_t numerical_type = IREE_HAL_NUMERICAL_TYPE_UNKNOWN;
  if (iree_string_view_consume_prefix(&str_value, IREE_SV("i"))) {
    numerical_type = IREE_HAL_NUMERICAL_TYPE_INTEGER_SIGNED;
  } else if (iree_string_view_consume_prefix(&str_value, IREE_SV("u"))) {
    numerical_type = IREE_HAL_NUMERICAL_TYPE_INTEGER_UNSIGNED;
  } else if (iree_string_view_consume_prefix(&str_value, IREE_SV("f"))) {
    numerical_type = IREE_HAL_NUMERICAL_TYPE_FLOAT_IEEE;
  } else if (iree_string_view_consume_prefix(&str_value, IREE_SV("x")) ||
             iree_string_view_consume_prefix(&str_value, IREE_SV("*"))) {
    numerical_type = IREE_HAL_NUMERICAL_TYPE_UNKNOWN;
  } else {
    return iree_make_status(IREE_STATUS_INVALID_ARGUMENT,
                            "unhandled element type prefix in '%.*s'",
                            (int)value.size, value.data);
  }

  uint32_t bit_count = 0;
  if (!iree_string_view_atoi_uint32(str_value, &bit_count) ||
      bit_count > 0xFFu) {
    return iree_make_status(IREE_STATUS_INVALID_ARGUMENT,
                            "out of range bit count in '%.*s'", (int)value.size,
                            value.data);
  }

  *out_element_type = iree_hal_make_element_type(numerical_type, bit_count);
  return iree_ok_status();
}

IREE_API_EXPORT iree_status_t iree_hal_format_element_type(
    iree_hal_element_type_t element_type, iree_host_size_t buffer_capacity,
    char* buffer, iree_host_size_t* out_buffer_length) {
  if (out_buffer_length) {
    *out_buffer_length = 0;
  }
  const char* prefix;
  switch (iree_hal_element_numerical_type(element_type)) {
    case IREE_HAL_NUMERICAL_TYPE_INTEGER_SIGNED:
      prefix = "i";
      break;
    case IREE_HAL_NUMERICAL_TYPE_INTEGER_UNSIGNED:
      prefix = "u";
      break;
    case IREE_HAL_NUMERICAL_TYPE_FLOAT_IEEE:
      prefix = "f";
      break;
    default:
      prefix = "*";
      break;
  }
  int n = snprintf(buffer, buffer_capacity, "%s%d", prefix,
                   (int32_t)iree_hal_element_bit_count(element_type));
  if (n < 0) {
    return iree_make_status(IREE_STATUS_FAILED_PRECONDITION, "snprintf failed");
  }
  if (out_buffer_length) {
    *out_buffer_length = n;
  }
  return n >= buffer_capacity ? iree_status_from_code(IREE_STATUS_OUT_OF_RANGE)
                              : iree_ok_status();
}

// Parses a string of two character pairs representing hex numbers into bytes.
static void iree_hal_hex_string_to_bytes(const char* from, uint8_t* to,
                                         ptrdiff_t num) {
  /* clang-format off */
  static const char kHexValue[256] = {
      0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  1,  2,  3,  4,  5,  6, 7, 8, 9, 0, 0, 0, 0, 0, 0,  // '0'..'9'
      0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 'A'..'F'
      0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 'a'..'f'
      0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };
  /* clang-format on */
  for (int i = 0; i < num; i++) {
    to[i] = (kHexValue[from[i * 2] & 0xFF] << 4) +
            (kHexValue[from[i * 2 + 1] & 0xFF]);
  }
}

// Parses a signal element string, assuming that the caller has validated that
// |out_data| has enough storage space for the parsed element data.
static iree_status_t iree_hal_parse_element_unsafe(
    iree_string_view_t data_str, iree_hal_element_type_t element_type,
    uint8_t* out_data) {
  switch (element_type) {
    case IREE_HAL_ELEMENT_TYPE_SINT_8: {
      int32_t temp = 0;
      if (!iree_string_view_atoi_int32(data_str, &temp) || temp > INT8_MAX) {
        return iree_status_from_code(IREE_STATUS_INVALID_ARGUMENT);
      }
      *(int8_t*)out_data = (int8_t)temp;
      return iree_ok_status();
    }
    case IREE_HAL_ELEMENT_TYPE_UINT_8: {
      uint32_t temp = 0;
      if (!iree_string_view_atoi_uint32(data_str, &temp) || temp > UINT8_MAX) {
        return iree_status_from_code(IREE_STATUS_INVALID_ARGUMENT);
      }
      *(uint8_t*)out_data = (uint8_t)temp;
      return iree_ok_status();
    }
    case IREE_HAL_ELEMENT_TYPE_SINT_16: {
      int32_t temp = 0;
      if (!iree_string_view_atoi_int32(data_str, &temp) || temp > INT16_MAX) {
        return iree_status_from_code(IREE_STATUS_INVALID_ARGUMENT);
      }
      *(int16_t*)out_data = (int16_t)temp;
      return iree_ok_status();
    }
    case IREE_HAL_ELEMENT_TYPE_UINT_16: {
      uint32_t temp = 0;
      if (!iree_string_view_atoi_uint32(data_str, &temp) || temp > UINT16_MAX) {
        return iree_status_from_code(IREE_STATUS_INVALID_ARGUMENT);
      }
      *(uint16_t*)out_data = (uint16_t)temp;
      return iree_ok_status();
    }
    case IREE_HAL_ELEMENT_TYPE_SINT_32:
      return iree_string_view_atoi_int32(data_str, (int32_t*)out_data)
                 ? iree_ok_status()
                 : iree_status_from_code(IREE_STATUS_INVALID_ARGUMENT);
    case IREE_HAL_ELEMENT_TYPE_UINT_32:
      return iree_string_view_atoi_uint32(data_str, (uint32_t*)out_data)
                 ? iree_ok_status()
                 : iree_status_from_code(IREE_STATUS_INVALID_ARGUMENT);
    case IREE_HAL_ELEMENT_TYPE_SINT_64:
      return iree_string_view_atoi_int64(data_str, (int64_t*)out_data)
                 ? iree_ok_status()
                 : iree_status_from_code(IREE_STATUS_INVALID_ARGUMENT);
    case IREE_HAL_ELEMENT_TYPE_UINT_64:
      return iree_string_view_atoi_uint64(data_str, (uint64_t*)out_data)
                 ? iree_ok_status()
                 : iree_status_from_code(IREE_STATUS_INVALID_ARGUMENT);
    case IREE_HAL_ELEMENT_TYPE_FLOAT_16: {
      float temp = 0;
      if (!iree_string_view_atof(data_str, &temp)) {
        return iree_status_from_code(IREE_STATUS_INVALID_ARGUMENT);
      }
      *(uint16_t*)out_data = iree_math_f32_to_f16(temp);
      return iree_ok_status();
    }
    case IREE_HAL_ELEMENT_TYPE_FLOAT_32:
      return iree_string_view_atof(data_str, (float*)out_data)
                 ? iree_ok_status()
                 : iree_status_from_code(IREE_STATUS_INVALID_ARGUMENT);
    case IREE_HAL_ELEMENT_TYPE_FLOAT_64:
      return iree_string_view_atod(data_str, (double*)out_data)
                 ? iree_ok_status()
                 : iree_status_from_code(IREE_STATUS_INVALID_ARGUMENT);
    default: {
      // Treat any unknown format as binary.
      iree_host_size_t element_size = iree_hal_element_byte_count(element_type);
      if (data_str.size != element_size * 2) {
        return iree_make_status(IREE_STATUS_INVALID_ARGUMENT,
                                "binary hex element count mismatch: buffer "
                                "length=%zu < expected=%zu",
                                data_str.size, element_size * 2);
      }
      iree_hal_hex_string_to_bytes(data_str.data, out_data, element_size);
      return iree_ok_status();
    }
  }
}

IREE_API_EXPORT iree_status_t iree_hal_parse_element(
    iree_string_view_t data_str, iree_hal_element_type_t element_type,
    iree_byte_span_t data_ptr) {
  iree_host_size_t element_size = iree_hal_element_byte_count(element_type);
  if (data_ptr.data_length < element_size) {
    return iree_make_status(
        IREE_STATUS_INVALID_ARGUMENT,
        "output data buffer overflow: data_length=%zu < element_size=%zu",
        data_ptr.data_length, element_size);
  }
  return iree_hal_parse_element_unsafe(data_str, element_type, data_ptr.data);
}

// Converts a sequence of bytes into hex number strings.
static void iree_hal_bytes_to_hex_string(const uint8_t* src, char* dest,
                                         ptrdiff_t num) {
  static const char kHexTable[513] =
      "000102030405060708090A0B0C0D0E0F"
      "101112131415161718191A1B1C1D1E1F"
      "202122232425262728292A2B2C2D2E2F"
      "303132333435363738393A3B3C3D3E3F"
      "404142434445464748494A4B4C4D4E4F"
      "505152535455565758595A5B5C5D5E5F"
      "606162636465666768696A6B6C6D6E6F"
      "707172737475767778797A7B7C7D7E7F"
      "808182838485868788898A8B8C8D8E8F"
      "909192939495969798999A9B9C9D9E9F"
      "A0A1A2A3A4A5A6A7A8A9AAABACADAEAF"
      "B0B1B2B3B4B5B6B7B8B9BABBBCBDBEBF"
      "C0C1C2C3C4C5C6C7C8C9CACBCCCDCECF"
      "D0D1D2D3D4D5D6D7D8D9DADBDCDDDEDF"
      "E0E1E2E3E4E5E6E7E8E9EAEBECEDEEEF"
      "F0F1F2F3F4F5F6F7F8F9FAFBFCFDFEFF";
  for (const uint8_t* src_ptr = src; src_ptr != (src + num);
       ++src_ptr, dest += 2) {
    const char* hex_p = &kHexTable[*src_ptr * 2];
    memcpy(dest, hex_p, 2);
  }
}

IREE_API_EXPORT iree_status_t iree_hal_format_element(
    iree_const_byte_span_t data, iree_hal_element_type_t element_type,
    iree_host_size_t buffer_capacity, char* buffer,
    iree_host_size_t* out_buffer_length) {
  iree_host_size_t element_size = iree_hal_element_byte_count(element_type);
  if (data.data_length < element_size) {
    return iree_make_status(
        IREE_STATUS_OUT_OF_RANGE,
        "data buffer underflow: data_length=%zu < element_size=%zu",
        data.data_length, element_size);
  }
  int n = 0;
  switch (element_type) {
    case IREE_HAL_ELEMENT_TYPE_SINT_8:
      n = snprintf(buffer, buffer ? buffer_capacity : 0, "%" PRIi8,
                   *(const int8_t*)data.data);
      break;
    case IREE_HAL_ELEMENT_TYPE_UINT_8:
      n = snprintf(buffer, buffer ? buffer_capacity : 0, "%" PRIu8,
                   *(const uint8_t*)data.data);
      break;
    case IREE_HAL_ELEMENT_TYPE_SINT_16:
      n = snprintf(buffer, buffer ? buffer_capacity : 0, "%" PRIi16,
                   *(const int16_t*)data.data);
      break;
    case IREE_HAL_ELEMENT_TYPE_UINT_16:
      n = snprintf(buffer, buffer ? buffer_capacity : 0, "%" PRIu16,
                   *(const uint16_t*)data.data);
      break;
    case IREE_HAL_ELEMENT_TYPE_SINT_32:
      n = snprintf(buffer, buffer ? buffer_capacity : 0, "%" PRIi32,
                   *(const int32_t*)data.data);
      break;
    case IREE_HAL_ELEMENT_TYPE_UINT_32:
      n = snprintf(buffer, buffer ? buffer_capacity : 0, "%" PRIu32,
                   *(const uint32_t*)data.data);
      break;
    case IREE_HAL_ELEMENT_TYPE_SINT_64:
      n = snprintf(buffer, buffer ? buffer_capacity : 0, "%" PRIi64,
                   *(const int64_t*)data.data);
      break;
    case IREE_HAL_ELEMENT_TYPE_UINT_64:
      n = snprintf(buffer, buffer ? buffer_capacity : 0, "%" PRIu64,
                   *(const uint64_t*)data.data);
      break;
    case IREE_HAL_ELEMENT_TYPE_FLOAT_16:
      n = snprintf(buffer, buffer ? buffer_capacity : 0, "%G",
                   iree_math_f16_to_f32(*(const uint16_t*)data.data));
      break;
    case IREE_HAL_ELEMENT_TYPE_FLOAT_32:
      n = snprintf(buffer, buffer ? buffer_capacity : 0, "%G",
                   *(const float*)data.data);
      break;
    case IREE_HAL_ELEMENT_TYPE_FLOAT_64:
      n = snprintf(buffer, buffer ? buffer_capacity : 0, "%G",
                   *(const double*)data.data);
      break;
    default: {
      // Treat any unknown format as binary.
      n = 2 * (int)element_size;
      if (buffer && buffer_capacity > n) {
        iree_hal_bytes_to_hex_string(data.data, buffer, element_size);
        buffer[n] = 0;
      }
    }
  }
  if (n < 0) {
    return iree_make_status(IREE_STATUS_FAILED_PRECONDITION, "snprintf failed");
  } else if (buffer && n >= buffer_capacity) {
    buffer = NULL;
  }
  if (out_buffer_length) {
    *out_buffer_length = n;
  }
  return buffer ? iree_ok_status()
                : iree_status_from_code(IREE_STATUS_OUT_OF_RANGE);
}

IREE_API_EXPORT iree_status_t iree_hal_parse_buffer_elements(
    iree_string_view_t data_str, iree_hal_element_type_t element_type,
    iree_byte_span_t data_ptr) {
  iree_host_size_t element_size = iree_hal_element_byte_count(element_type);
  iree_host_size_t element_capacity = data_ptr.data_length / element_size;
  if (iree_string_view_is_empty(data_str)) {
    memset(data_ptr.data, 0, data_ptr.data_length);
    return iree_ok_status();
  }
  size_t src_i = 0;
  size_t dst_i = 0;
  size_t token_start = IREE_STRING_VIEW_NPOS;
  while (src_i < data_str.size) {
    char c = data_str.data[src_i++];
    bool is_separator = isspace(c) || c == ',' || c == '[' || c == ']';
    if (token_start == IREE_STRING_VIEW_NPOS) {
      if (!is_separator) {
        token_start = src_i - 1;
      }
      continue;
    } else if (token_start != IREE_STRING_VIEW_NPOS && !is_separator) {
      continue;
    }
    if (dst_i >= element_capacity) {
      return iree_make_status(
          IREE_STATUS_OUT_OF_RANGE,
          "output data buffer overflow: element_capacity=%zu < dst_i=%zu+",
          element_capacity, dst_i);
    }
    IREE_RETURN_IF_ERROR(iree_hal_parse_element_unsafe(
        iree_make_string_view(data_str.data + token_start,
                              src_i - 2 - token_start + 1),
        element_type, data_ptr.data + dst_i * element_size));
    ++dst_i;
    token_start = IREE_STRING_VIEW_NPOS;
  }
  if (token_start != IREE_STRING_VIEW_NPOS) {
    if (dst_i >= element_capacity) {
      return iree_make_status(
          IREE_STATUS_OUT_OF_RANGE,
          "output data overflow: element_capacity=%zu < dst_i=%zu",
          element_capacity, dst_i);
    }
    IREE_RETURN_IF_ERROR(iree_hal_parse_element_unsafe(
        iree_make_string_view(data_str.data + token_start,
                              data_str.size - token_start),
        element_type, data_ptr.data + dst_i * element_size));
    ++dst_i;
  }
  if (dst_i == 1 && element_capacity > 1) {
    // Splat the single value we got to the entire buffer.
    uint8_t* p = data_ptr.data + element_size;
    for (int i = 1; i < element_capacity; ++i, p += element_size) {
      memcpy(p, data_ptr.data, element_size);
    }
  } else if (dst_i < element_capacity) {
    return iree_make_status(
        IREE_STATUS_OUT_OF_RANGE,
        "input data string underflow: dst_i=%zu < element_capacity=%zu", dst_i,
        element_capacity);
  }
  return iree_ok_status();
}

#define APPEND_CHAR(c)                           \
  {                                              \
    if (buffer) {                                \
      if (buffer_length < buffer_capacity - 1) { \
        buffer[buffer_length] = c;               \
        buffer[buffer_length + 1] = '\0';        \
      } else {                                   \
        buffer = NULL;                           \
      }                                          \
    }                                            \
    ++buffer_length;                             \
  }

static iree_status_t iree_hal_format_buffer_elements_recursive(
    iree_const_byte_span_t data, const iree_hal_dim_t* shape,
    iree_host_size_t shape_rank, iree_hal_element_type_t element_type,
    iree_host_size_t* max_element_count, iree_host_size_t buffer_capacity,
    char* buffer, iree_host_size_t* out_buffer_length) {
  iree_host_size_t buffer_length = 0;
  if (shape_rank == 0) {
    // Scalar value; recurse to get on to the leaf dimension path.
    const iree_hal_dim_t one = 1;
    return iree_hal_format_buffer_elements_recursive(
        data, &one, 1, element_type, max_element_count, buffer_capacity, buffer,
        out_buffer_length);
  } else if (shape_rank > 1) {
    // Nested dimension; recurse into the next innermost dimension.
    iree_hal_dim_t dim_length = 1;
    for (iree_host_size_t i = 1; i < shape_rank; ++i) {
      dim_length *= shape[i];
    }
    iree_device_size_t dim_stride =
        dim_length * iree_hal_element_byte_count(element_type);
    if (data.data_length < dim_stride * shape[0]) {
      return iree_make_status(
          IREE_STATUS_OUT_OF_RANGE,
          "input data underflow: data_length=%zu < expected=%zu",
          data.data_length, (iree_host_size_t)(dim_stride * shape[0]));
    }
    iree_const_byte_span_t subdata;
    subdata.data = data.data;
    subdata.data_length = dim_stride;
    for (iree_hal_dim_t i = 0; i < shape[0]; ++i) {
      APPEND_CHAR('[');
      iree_host_size_t actual_length = 0;
      iree_status_t status = iree_hal_format_buffer_elements_recursive(
          subdata, shape + 1, shape_rank - 1, element_type, max_element_count,
          buffer ? buffer_capacity - buffer_length : 0,
          buffer ? buffer + buffer_length : NULL, &actual_length);
      buffer_length += actual_length;
      if (iree_status_is_out_of_range(status)) {
        buffer = NULL;
      } else if (!iree_status_is_ok(status)) {
        return status;
      }
      subdata.data += dim_stride;
      APPEND_CHAR(']');
    }
  } else {
    // Leaf dimension; output data.
    iree_host_size_t max_count =
        iree_min(*max_element_count, (iree_host_size_t)shape[0]);
    iree_device_size_t element_stride =
        iree_hal_element_byte_count(element_type);
    if (data.data_length < max_count * element_stride) {
      return iree_make_status(
          IREE_STATUS_OUT_OF_RANGE,
          "input data underflow; data_length=%zu < expected=%zu",
          data.data_length, (iree_host_size_t)(max_count * element_stride));
    }
    *max_element_count -= max_count;
    iree_const_byte_span_t subdata;
    subdata.data = data.data;
    subdata.data_length = element_stride;
    for (iree_hal_dim_t i = 0; i < max_count; ++i) {
      if (i > 0) APPEND_CHAR(' ');
      iree_host_size_t actual_length = 0;
      iree_status_t status = iree_hal_format_element(
          subdata, element_type, buffer ? buffer_capacity - buffer_length : 0,
          buffer ? buffer + buffer_length : NULL, &actual_length);
      subdata.data += element_stride;
      buffer_length += actual_length;
      if (iree_status_is_out_of_range(status)) {
        buffer = NULL;
      } else if (!iree_status_is_ok(status)) {
        return status;
      }
    }
    if (max_count < shape[0]) {
      APPEND_CHAR('.');
      APPEND_CHAR('.');
      APPEND_CHAR('.');
    }
  }
  if (out_buffer_length) {
    *out_buffer_length = buffer_length;
  }
  return buffer ? iree_ok_status()
                : iree_status_from_code(IREE_STATUS_OUT_OF_RANGE);
}

IREE_API_EXPORT iree_status_t iree_hal_format_buffer_elements(
    iree_const_byte_span_t data, const iree_hal_dim_t* shape,
    iree_host_size_t shape_rank, iree_hal_element_type_t element_type,
    iree_host_size_t max_element_count, iree_host_size_t buffer_capacity,
    char* buffer, iree_host_size_t* out_buffer_length) {
  if (out_buffer_length) {
    *out_buffer_length = 0;
  }
  if (buffer && buffer_capacity) {
    buffer[0] = '\0';
  }
  return iree_hal_format_buffer_elements_recursive(
      data, shape, shape_rank, element_type, &max_element_count,
      buffer_capacity, buffer, out_buffer_length);
}
