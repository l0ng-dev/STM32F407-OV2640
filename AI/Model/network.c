/**
  ******************************************************************************
  * @file    network.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-11-05T18:27:58+0800
  * @brief   AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */

#include "ai_lite_inspect.h"
#include "ai_platform_interface.h"
#include "layers.h"
#include "core_convert.h"
#include "network.h"
#include "network_details.h"
#include "network_data.h"
#include "stai_events.h"

#include "lite_operators.h"

#include "ai_lite_inspect.h"
/*****************************************************************************/
#define STAI_INTERNAL_API_MAJOR               (1)
#define STAI_INTERNAL_API_MINOR               (0)
#define STAI_INTERNAL_API_MICRO               (0)

#define STAI_MAGIC                            (0xB1C00100)

/*****************************************************************************/
#define _STAI_CONCAT_ARG(a, b)     a ## b
#define STAI_CONCAT(a, b)         _STAI_CONCAT_ARG(a, b)

/*!  STAI_CAST SECTION                       *********************************/
#define STAI_CAST(type, expr) \
  ((type)(expr))


/*****************************************************************************/
#define STAI_SIZE(_size) \
  ((stai_size)(_size))

/*****************************************************************************/
#define STAI_INIT_BUFFER(_flags, _size, _address) \
  { \
    .size = (_size), \
    .address = (uintptr_t)(_address), \
    .flags = (_flags), \
  }

#define STAI_INIT_TENSOR(_name, _flags, _fmt, _size_bytes, _shape, _scale, _zeropoint) \
  { \
    .size_bytes = (_size_bytes), \
    .flags = (_flags), \
    .format = (stai_format)(_fmt), \
    .shape = STAI_PACK(_shape), \
    .scale = STAI_PACK(_scale), \
    .zeropoint = STAI_PACK(_zeropoint), \
    .name = (_name) \
  }

#define STAI_INIT_ARRAY(_size, _ptr) \
  { .size = STAI_SIZE(_size), .data = STAI_PACK(_ptr) }


#define STAI_CAST_ARRAY(_type, _size, _ptr) \
  { .size = STAI_SIZE(_size), .data = (_type)STAI_PACK(_ptr) }


#define STAI_DECLARE_ARRAY(_type, _size, ...) \
  { .size = STAI_SIZE(_size), .data = (_type[_size]) { STAI_PACK(__VA_ARGS__) } }


#define STAI_EMPTY_ARRAY() \
  { .size = 0, .data = NULL }


#define STAI_INIT_VERSION(_major, _minor, _micro) \
  { .major = (_major), .minor = (_minor), .micro = (_micro), .reserved = 0x0 }

/*****************************************************************************/
/**  Getters and setters  **/

#define STAI_GET_ARRAY_SIZE(nd_array) \
  (nd_array.size)


#define STAI_GET_ARRAY_ELEM(nd_array, pos) \
  (nd_array.data[(pos)])

#define _STAI_SET_ERROR(net_ctx, cond, value, exit) { \
  if (!(net_ctx)) { return STAI_ERROR_NETWORK_INVALID_CONTEXT_HANDLE; } \
  if (((uintptr_t)net_ctx) & (_STAI_CONTEXT_ALIGNMENT-1)) { return STAI_ERROR_NETWORK_INVALID_CONTEXT_ALIGNMENT; } \
  if (((value) >= STAI_ERROR_GENERIC) && (cond)) { \
    if ((net_ctx)->_return_code == STAI_SUCCESS) { \
      (net_ctx)->_return_code = (value); \
    } \
    return (exit); \
  } \
}

/*****************************************************************************/
/* TODO REMOVE THESE TWO MACROS */
#define STAI_EVENT_NODE_START_CB
#define STAI_EVENT_NODE_STOP_CB

#ifdef STAI_EVENT_NODE_START_CB
#ifndef _STAI_NETWORK_EVENT_NODE_START_CB
  #define _STAI_NETWORK_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
  if (net_ctx->_callback) { \
    const stai_event_node_start_stop _start_event = { \
      .node_id=(_node_id), \
      .buffers={ \
        .size=(_buffers_size), \
        .data=(stai_ptr const*)(const stai_ptr[_buffers_size])STAI_PACK(__VA_ARGS__) \
      } \
    }; \
    net_ctx->_callback(net_ctx->_callback_cookie, STAI_EVENT_NODE_START, (const void*)&_start_event); \
  }
#endif
#else
  #define _STAI_NETWORK_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_NETWORK_EVENT_NODE_START_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_START_CB */

#ifdef STAI_EVENT_NODE_STOP_CB
#ifndef _STAI_NETWORK_EVENT_NODE_STOP_CB
  #define _STAI_NETWORK_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
  if (net_ctx->_callback) { \
    const stai_event_node_start_stop _stop_event = { \
      .node_id=(_node_id), \
      .buffers={ \
        .size=(_buffers_size), \
        .data=(stai_ptr const*)(stai_ptr[_buffers_size])STAI_PACK(__VA_ARGS__) \
      } \
    }; \
    net_ctx->_callback(net_ctx->_callback_cookie, STAI_EVENT_NODE_STOP, (const void*)&_stop_event); \
  }
#endif
#else
  #define _STAI_NETWORK_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_NETWORK_EVENT_NODE_STOP_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_STOP_CB */


/*****************************************************************************/
#define _STAI_NETWORK_MODEL_SIGNATURE     "0x880b1d9db45287639240f46f4c19d9f4"
#define _STAI_NETWORK_DATETIME            "2026-11-05T18:27:58+0800"
#define _STAI_NETWORK_COMPILE_DATETIME    __DATE__ " " __TIME__

#define _STAI_CONTEXT_ALIGNMENT        STAI_NETWORK_CONTEXT_ALIGNMENT

/*****************************************************************************/
#define g_network_activations_1     (NULL)
#define g_network_activations_2     (NULL)




#if defined(HAVE_NETWORK_INFO)
/*****************************************************************************/
static const stai_network_info g_network_info = {
  .model_signature = _STAI_NETWORK_MODEL_SIGNATURE,
  .c_compile_datetime = _STAI_NETWORK_COMPILE_DATETIME,
  .c_model_name = STAI_NETWORK_MODEL_NAME,
  .c_model_datetime = _STAI_NETWORK_DATETIME,
  .c_model_signature = 0x0,
  .runtime_version = STAI_INIT_VERSION(12, 1, 0),
  .tool_version = STAI_INIT_VERSION(4, 1, 0),
  .api_version = STAI_INIT_VERSION(1, 0, 0),
  .n_macc = STAI_NETWORK_MACC_NUM,
  .n_nodes = STAI_NETWORK_NODES_NUM,
  .flags = STAI_NETWORK_FLAGS,
  .n_inputs = STAI_NETWORK_IN_NUM,
  .n_outputs = STAI_NETWORK_OUT_NUM,
  .n_activations = STAI_NETWORK_ACTIVATIONS_NUM,
  .n_weights = STAI_NETWORK_WEIGHTS_NUM,
  .n_states = STAI_NETWORK_STATES_NUM,
  .inputs = (stai_tensor[STAI_NETWORK_IN_NUM]) {
    STAI_INIT_TENSOR(
      STAI_NETWORK_IN_1_NAME,
      STAI_NETWORK_IN_1_FLAGS,
      STAI_NETWORK_IN_1_FORMAT,
      STAI_NETWORK_IN_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 4, 1, 3, 160, 160),
      STAI_DECLARE_ARRAY(float, 1, 0.01865844801068306f),
      STAI_DECLARE_ARRAY(int16_t, 1, -14)),
    },
    .outputs = (stai_tensor[STAI_NETWORK_OUT_NUM]) {
    STAI_INIT_TENSOR(
      STAI_NETWORK_OUT_1_NAME,
      STAI_NETWORK_OUT_1_FLAGS,
      STAI_NETWORK_OUT_1_FORMAT,
      STAI_NETWORK_OUT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 2, 1, 3),
      STAI_DECLARE_ARRAY(float, 1, 0.012564831413328648f),
      STAI_DECLARE_ARRAY(int16_t, 1, 48)),
    },
  .activations = (stai_tensor[STAI_NETWORK_ACTIVATIONS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_NETWORK_ACTIVATION_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_NETWORK_ACTIVATION_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 102400),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    STAI_INIT_TENSOR(
      (NULL),
      STAI_NETWORK_ACTIVATION_2_FLAGS,
      STAI_FORMAT_U8,
      STAI_NETWORK_ACTIVATION_2_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 76804),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .weights = (stai_tensor[STAI_NETWORK_WEIGHTS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_NETWORK_WEIGHT_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_NETWORK_WEIGHT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 352832),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },

  .states = NULL
};
#endif

#define _STAI_CONTEXT_ACQUIRE(_net_ctx, _net_handle) \
  _stai_network_context* _net_ctx = (_stai_network_context*)(_net_handle); \
  STAI_ASSERT(_net_ctx != NULL) \
  _STAI_SET_ERROR(_net_ctx, _net_ctx->_magic != STAI_MAGIC, \
                  STAI_ERROR_NETWORK_INVALID_CONTEXT_HANDLE, _net_ctx->_return_code)


/*****************************************************************************/
static
void _stai_network_check(_stai_network_context* net_ctx)
{
  stai_size idx;

// Check activations status
  for (idx=0; idx<STAI_NETWORK_ACTIVATIONS_NUM; idx++) {
    if (net_ctx->_activations[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_ACTIVATIONS_NUM) ? STAI_FLAG_ACTIVATIONS : STAI_FLAG_NONE;
// Check inputs status
  for (idx=0; idx<STAI_NETWORK_IN_NUM; idx++) {
    if (net_ctx->_inputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_IN_NUM) ? STAI_FLAG_INPUTS : STAI_FLAG_NONE;

  // Check outputs status
  for (idx=0; idx<STAI_NETWORK_OUT_NUM; idx++) {
    if (net_ctx->_outputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_OUT_NUM) ? STAI_FLAG_OUTPUTS : STAI_FLAG_NONE;

// Check weights status
  for (idx=0; idx<STAI_NETWORK_WEIGHTS_NUM; idx++) {
    if (net_ctx->_weights[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_WEIGHTS_NUM) ? STAI_FLAG_WEIGHTS : STAI_FLAG_NONE;
STAI_PRINT("  [_stai_network_check] flags: 0x%08x\n", net_ctx->_flags)
}


/*****************************************************************************/
STAI_API_ENTRY
stai_return_code stai_network_init(
  stai_network* network)
{
  /* Memory where to store internal context is provided by applications as a raw byte buffer */
  _stai_network_context* net_ctx = (_stai_network_context*)(network);
  net_ctx->_return_code = STAI_SUCCESS;
  STAI_PRINT("[Entering Network Init] network(%p) context_size(%d)\n", net_ctx, (int32_t)sizeof(_stai_network_context))

  _STAI_SET_ERROR(net_ctx, STAI_NETWORK_CONTEXT_SIZE != sizeof(_stai_network_context),
                 STAI_ERROR_NETWORK_INVALID_CONTEXT_SIZE, net_ctx->_return_code)

  {
    const _stai_network_context _network_context = {
      ._magic = STAI_MAGIC,
      ._signature = STAI_NETWORK_MODEL_SIGNATURE,
      ._flags = STAI_NETWORK_FLAGS,
      ._return_code = STAI_SUCCESS,
      ._callback = NULL,
      ._callback_cookie = NULL,
      ._activations = {
      (stai_ptr)g_network_activations_1,(stai_ptr)g_network_activations_2
      },
      ._weights = {
      (stai_ptr)g_network_weights_array
      },
      ._inputs = {
    NULL},
      ._outputs = {
    NULL},
    };

    // Deep copy of internal context to opaque buffer provided by app
    *net_ctx = _network_context;

    _stai_network_check(net_ctx);
  }

  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_deinit(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /*  Reset flags to initial state  */
  net_ctx->_flags = STAI_NETWORK_FLAGS;
  return net_ctx->_return_code;
}

/*****************************************************************************/



/* Int quant #0 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(image_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01865844801068306f),
    AI_PACK_INTQ_ZP(-14)))

/* Int quant #1 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(image_Transpose_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01865844801068306f),
    AI_PACK_INTQ_ZP(-14)))

/* Int quant #2 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_conv1_conv1_2_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03621755167841911f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #3 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_conv1_conv1_2_Relu_output_0_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 24,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0010149874724447727f, 1.783251519782425e-07f, 0.0037332249339669943f, 0.0007301971199922264f, 0.0013550668954849243f, 0.002734749810770154f, 1.1084063800126387e-07f, 0.000827812880743295f, 0.0046216282062232494f, 0.000829168944619596f, 1.1695111652443302e-06f, 0.012058121152222157f, 6.850764293631073e-07f, 0.006968181114643812f, 0.002166490536183119f, 0.0006023240857757628f, 3.9236372373352424e-08f, 0.005059359595179558f, 3.183077690316338e-10f, 0.006329031195491552f, 0.006460015662014484f, 0.009881864301860332f, 0.0007532142917625606f, 1.1624656082176443e-07f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)))

/* Int quant #4 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_conv1_conv1_2_Relu_output_0_scratch1_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03621755167841911f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #5 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_0_branch1_branch1_4_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.00990772433578968f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #6 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_0_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01578989252448082f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #7 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_0_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01578989252448082f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #8 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_0_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01578989252448082f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #9 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_1_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01578989252448082f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #10 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_1_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01578989252448082f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #11 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_1_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009181622415781021f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #12 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_1_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01578989252448082f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #13 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_1_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01578989252448082f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #14 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_2_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01578989252448082f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #15 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_2_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01578989252448082f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #16 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_2_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.012105477973818779f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #17 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_2_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01578989252448082f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #18 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_2_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01578989252448082f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #19 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_3_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01578989252448082f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #20 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_3_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01578989252448082f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #21 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_3_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.015372813679277897f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #22 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_3_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01578989252448082f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #23 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_3_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01578989252448082f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #24 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_0_branch1_branch1_4_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009784913621842861f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #25 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_0_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.008621389977633953f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #26 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_0_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009784913621842861f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #27 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_0_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009784913621842861f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #28 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_1_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009784913621842861f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #29 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_1_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009784913621842861f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #30 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_1_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.00963827408850193f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #31 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_1_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009784913621842861f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #32 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_1_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009784913621842861f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #33 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_2_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009784913621842861f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #34 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_2_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009784913621842861f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #35 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_2_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.00911688432097435f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #36 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_2_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009784913621842861f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #37 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_2_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009784913621842861f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #38 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_3_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009784913621842861f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #39 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_3_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009784913621842861f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #40 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_3_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.012644290924072266f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #41 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_3_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.012644290924072266f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #42 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_3_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.012644290924072266f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #43 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_4_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.012644290924072266f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #44 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_4_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.012644290924072266f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #45 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_4_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009977243840694427f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #46 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_4_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009977243840694427f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #47 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_4_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009977243840694427f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #48 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_5_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009977243840694427f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #49 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_5_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009977243840694427f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #50 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_5_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011004479601979256f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #51 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_5_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011004479601979256f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #52 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_5_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011004479601979256f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #53 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_6_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011004479601979256f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #54 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_6_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011004479601979256f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #55 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_6_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011528286151587963f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #56 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_6_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011528286151587963f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #57 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_6_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011528286151587963f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #58 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_7_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011528286151587963f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #59 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_7_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011528286151587963f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #60 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_7_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.013492636382579803f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #61 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_7_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.013492636382579803f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #62 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_7_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.013492636382579803f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #63 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_0_branch1_branch1_4_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010431465692818165f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #64 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_0_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.017641980201005936f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #65 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_0_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.017641980201005936f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #66 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_0_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.017641980201005936f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #67 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_1_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.017641980201005936f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #68 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_1_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.017641980201005936f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #69 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_1_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.014554073102772236f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #70 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_1_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.014554073102772236f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #71 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_1_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.014554073102772236f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #72 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_2_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.014554073102772236f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #73 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_2_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.014554073102772236f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #74 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_2_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.012706417590379715f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #75 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_2_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.014554073102772236f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #76 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_2_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.014554073102772236f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #77 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_3_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.014554073102772236f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #78 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_3_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.014554073102772236f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #79 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_3_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.019763272255659103f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #80 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_3_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.019763272255659103f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #81 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_3_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.019763272255659103f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #82 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.002931835362687707f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #83 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(logits_QuantizeLinear_Input_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.012564831413328648f),
    AI_PACK_INTQ_ZP(48)))

/* Int quant #84 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(logits_QuantizeLinear_Input_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 3,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0005110917263664305f, 0.0005083428113721311f, 0.0005147481570020318f),
    AI_PACK_INTQ_ZP(0, 0, 0)))



/* Array#0 */
AI_ARRAY_OBJ_DECLARE(
  image_output_array, AI_ARRAY_FORMAT_S8|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 76800, AI_STATIC)

/* Array#1 */
AI_ARRAY_OBJ_DECLARE(
  image_Transpose_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 76801, AI_STATIC)

/* Array#2 */
AI_ARRAY_OBJ_DECLARE(
  _conv1_conv1_2_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 38400, AI_STATIC)

/* Array#3 */
AI_ARRAY_OBJ_DECLARE(
  _conv1_conv1_2_Relu_output_0_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 648, AI_STATIC)

/* Array#4 */
AI_ARRAY_OBJ_DECLARE(
  _conv1_conv1_2_Relu_output_0_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 24, AI_STATIC)

/* Array#5 */
AI_ARRAY_OBJ_DECLARE(
  _conv1_conv1_2_Relu_output_0_scratch0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1740, AI_STATIC)

/* Array#6 */
AI_ARRAY_OBJ_DECLARE(
  _conv1_conv1_2_Relu_output_0_scratch1_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 5760, AI_STATIC)

/* Array#7 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_0_branch1_branch1_4_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#8 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_0_branch2_branch2_7_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#9 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_0_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#10 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_0_Transpose_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#11 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_1_Slice_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#12 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_1_Slice_1_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#13 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_1_branch2_branch2_7_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#14 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_1_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#15 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_1_Transpose_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#16 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_2_Slice_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#17 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_2_Slice_1_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#18 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_2_branch2_branch2_7_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#19 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_2_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#20 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_2_Transpose_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#21 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_3_Slice_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#22 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_3_Slice_1_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#23 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_3_branch2_branch2_7_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#24 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_3_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#25 */
AI_ARRAY_OBJ_DECLARE(
  _stage2_stage2_3_Transpose_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#26 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_0_branch1_branch1_4_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#27 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_0_branch2_branch2_7_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#28 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_0_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#29 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_0_Transpose_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#30 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_1_Slice_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#31 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_1_Slice_1_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#32 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_1_branch2_branch2_7_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#33 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_1_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#34 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_1_Transpose_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#35 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_2_Slice_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#36 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_2_Slice_1_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#37 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_2_branch2_branch2_7_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#38 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_2_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#39 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_2_Transpose_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#40 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_3_Slice_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#41 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_3_Slice_1_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#42 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_3_branch2_branch2_7_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#43 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_3_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#44 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_3_Transpose_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#45 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_4_Slice_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#46 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_4_Slice_1_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#47 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_4_branch2_branch2_7_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#48 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_4_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#49 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_4_Transpose_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#50 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_5_Slice_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#51 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_5_Slice_1_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#52 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_5_branch2_branch2_7_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#53 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_5_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#54 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_5_Transpose_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#55 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_6_Slice_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#56 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_6_Slice_1_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#57 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_6_branch2_branch2_7_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#58 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_6_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#59 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_6_Transpose_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#60 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_7_Slice_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#61 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_7_Slice_1_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#62 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_7_branch2_branch2_7_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#63 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_7_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#64 */
AI_ARRAY_OBJ_DECLARE(
  _stage3_stage3_7_Transpose_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9600, AI_STATIC)

/* Array#65 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_0_branch1_branch1_4_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2400, AI_STATIC)

/* Array#66 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_0_branch2_branch2_7_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2400, AI_STATIC)

/* Array#67 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_0_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#68 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_0_Transpose_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#69 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_1_Slice_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2400, AI_STATIC)

/* Array#70 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_1_Slice_1_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2400, AI_STATIC)

/* Array#71 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_1_branch2_branch2_7_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2400, AI_STATIC)

/* Array#72 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_1_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#73 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_1_Transpose_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#74 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_2_Slice_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2400, AI_STATIC)

/* Array#75 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_2_Slice_1_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2400, AI_STATIC)

/* Array#76 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_2_branch2_branch2_7_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2400, AI_STATIC)

/* Array#77 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_2_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#78 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_2_Transpose_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#79 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_3_Slice_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2400, AI_STATIC)

/* Array#80 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_3_Slice_1_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2400, AI_STATIC)

/* Array#81 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_3_branch2_branch2_7_Relu_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2400, AI_STATIC)

/* Array#82 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_3_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#83 */
AI_ARRAY_OBJ_DECLARE(
  _stage4_stage4_3_Transpose_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4800, AI_STATIC)

/* Array#84 */
AI_ARRAY_OBJ_DECLARE(
  _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 25600, AI_STATIC)

/* Array#85 */
AI_ARRAY_OBJ_DECLARE(
  _ReduceMean_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1024, AI_STATIC)

/* Array#86 */
AI_ARRAY_OBJ_DECLARE(
  _ReduceMean_output_0_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1024, AI_STATIC)

/* Array#87 */
AI_ARRAY_OBJ_DECLARE(
  _ReduceMean_output_0_Placeholder_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)

/* Array#88 */
AI_ARRAY_OBJ_DECLARE(
  _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1024, AI_STATIC)

/* Array#89 */
AI_ARRAY_OBJ_DECLARE(
  logits_QuantizeLinear_Input_output_array, AI_ARRAY_FORMAT_S8|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 3, AI_STATIC)

/* Array#90 */
AI_ARRAY_OBJ_DECLARE(
  logits_QuantizeLinear_Input_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3072, AI_STATIC)

/* Array#91 */
AI_ARRAY_OBJ_DECLARE(
  logits_QuantizeLinear_Input_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 3, AI_STATIC)

/* Array#92 */
AI_ARRAY_OBJ_DECLARE(
  logits_QuantizeLinear_Input_scratch0_array, AI_ARRAY_FORMAT_S16,
  NULL, NULL, 1039, AI_STATIC)



/* Tensor #0 */
AI_TENSOR_OBJ_DECLARE(
  image_Transpose_output, AI_STATIC,
  320, 0x1,
  AI_SHAPE_INIT(4, 1, 3, 160, 160), AI_STRIDE_INIT(4, 1, 1, 3, 480),
  1, &image_Transpose_output_array, &image_Transpose_output_array_intq)

/* Tensor #1 */
AI_TENSOR_OBJ_DECLARE(
  image_output, AI_STATIC,
  321, 0x1,
  AI_SHAPE_INIT(4, 1, 160, 160, 3), AI_STRIDE_INIT(4, 1, 1, 160, 25600),
  1, &image_output_array, &image_output_array_intq)

/* Tensor #2 */
AI_TENSOR_OBJ_DECLARE(
  _conv1_conv1_2_Relu_output_0_bias, AI_STATIC,
  4, 0x0,
  AI_SHAPE_INIT(4, 1, 24, 1, 1), AI_STRIDE_INIT(4, 4, 4, 96, 96),
  1, &_conv1_conv1_2_Relu_output_0_bias_array, NULL)

/* Tensor #3 */
AI_TENSOR_OBJ_DECLARE(
  _conv1_conv1_2_Relu_output_0_output, AI_STATIC,
  5, 0x1,
  AI_SHAPE_INIT(4, 1, 24, 40, 40), AI_STRIDE_INIT(4, 1, 1, 24, 960),
  1, &_conv1_conv1_2_Relu_output_0_output_array, &_conv1_conv1_2_Relu_output_0_output_array_intq)

/* Tensor #4 */
AI_TENSOR_OBJ_DECLARE(
  _conv1_conv1_2_Relu_output_0_scratch0, AI_STATIC,
  6, 0x0,
  AI_SHAPE_INIT(4, 1, 1740, 1, 1), AI_STRIDE_INIT(4, 1, 1, 1740, 1740),
  1, &_conv1_conv1_2_Relu_output_0_scratch0_array, NULL)

/* Tensor #5 */
AI_TENSOR_OBJ_DECLARE(
  _conv1_conv1_2_Relu_output_0_scratch1, AI_STATIC,
  7, 0x1,
  AI_SHAPE_INIT(4, 1, 24, 80, 3), AI_STRIDE_INIT(4, 1, 1, 24, 1920),
  1, &_conv1_conv1_2_Relu_output_0_scratch1_array, &_conv1_conv1_2_Relu_output_0_scratch1_array_intq)

/* Tensor #6 */
AI_TENSOR_OBJ_DECLARE(
  _conv1_conv1_2_Relu_output_0_weights, AI_STATIC,
  8, 0x1,
  AI_SHAPE_INIT(4, 3, 3, 3, 24), AI_STRIDE_INIT(4, 1, 3, 72, 216),
  1, &_conv1_conv1_2_Relu_output_0_weights_array, &_conv1_conv1_2_Relu_output_0_weights_array_intq)

/* Tensor #7 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_0_Concat_output_0_output, AI_STATIC,
  14, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 20, 20), AI_STRIDE_INIT(4, 1, 1, 48, 960),
  1, &_stage2_stage2_0_Concat_output_0_output_array, &_stage2_stage2_0_Concat_output_0_output_array_intq)

/* Tensor #8 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_0_branch1_branch1_4_Relu_output_0_output, AI_STATIC,
  23, 0x1,
  AI_SHAPE_INIT(4, 1, 24, 20, 20), AI_STRIDE_INIT(4, 1, 1, 24, 480),
  1, &_stage2_stage2_0_branch1_branch1_4_Relu_output_0_output_array, &_stage2_stage2_0_branch1_branch1_4_Relu_output_0_output_array_intq)

/* Tensor #9 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_0_branch2_branch2_7_Relu_output_0_output, AI_STATIC,
  35, 0x1,
  AI_SHAPE_INIT(4, 1, 24, 20, 20), AI_STRIDE_INIT(4, 1, 1, 24, 480),
  1, &_stage2_stage2_0_branch2_branch2_7_Relu_output_0_output_array, &_stage2_stage2_0_branch2_branch2_7_Relu_output_0_output_array_intq)

/* Tensor #10 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_0_Concat_output_0_output0, AI_STATIC,
  15, 0x1,
  AI_SHAPE_INIT(5, 1, 24, 20, 20, 2), AI_STRIDE_INIT(5, 1, 1, 48, 960, 24),
  1, &_stage2_stage2_0_Concat_output_0_output_array, &_stage2_stage2_0_Concat_output_0_output_array_intq)

/* Tensor #11 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_0_Transpose_output_0_output, AI_STATIC,
  16, 0x1,
  AI_SHAPE_INIT(5, 1, 2, 20, 20, 24), AI_STRIDE_INIT(5, 1, 1, 48, 960, 2),
  1, &_stage2_stage2_0_Transpose_output_0_output_array, &_stage2_stage2_0_Transpose_output_0_output_array_intq)

/* Tensor #12 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_0_Transpose_output_0_output0, AI_STATIC,
  17, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 20, 20), AI_STRIDE_INIT(4, 1, 1, 48, 960),
  1, &_stage2_stage2_0_Transpose_output_0_output_array, &_stage2_stage2_0_Transpose_output_0_output_array_intq)

/* Tensor #13 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_1_Slice_output_0_output, AI_STATIC,
  41, 0x1,
  AI_SHAPE_INIT(4, 1, 24, 20, 20), AI_STRIDE_INIT(4, 1, 1, 24, 480),
  1, &_stage2_stage2_1_Slice_output_0_output_array, &_stage2_stage2_1_Slice_output_0_output_array_intq)

/* Tensor #14 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_1_Slice_1_output_0_output, AI_STATIC,
  40, 0x1,
  AI_SHAPE_INIT(4, 1, 24, 20, 20), AI_STRIDE_INIT(4, 1, 1, 24, 480),
  1, &_stage2_stage2_1_Slice_1_output_0_output_array, &_stage2_stage2_1_Slice_1_output_0_output_array_intq)

/* Tensor #15 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_1_Concat_output_0_output, AI_STATIC,
  38, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 20, 20), AI_STRIDE_INIT(4, 1, 1, 48, 960),
  1, &_stage2_stage2_1_Concat_output_0_output_array, &_stage2_stage2_1_Concat_output_0_output_array_intq)

/* Tensor #16 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_1_branch2_branch2_7_Relu_output_0_output, AI_STATIC,
  53, 0x1,
  AI_SHAPE_INIT(4, 1, 24, 20, 20), AI_STRIDE_INIT(4, 1, 1, 24, 480),
  1, &_stage2_stage2_1_branch2_branch2_7_Relu_output_0_output_array, &_stage2_stage2_1_branch2_branch2_7_Relu_output_0_output_array_intq)

/* Tensor #17 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_1_Concat_output_0_output0, AI_STATIC,
  39, 0x1,
  AI_SHAPE_INIT(5, 1, 24, 20, 20, 2), AI_STRIDE_INIT(5, 1, 1, 48, 960, 24),
  1, &_stage2_stage2_1_Concat_output_0_output_array, &_stage2_stage2_1_Concat_output_0_output_array_intq)

/* Tensor #18 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_1_Transpose_output_0_output, AI_STATIC,
  42, 0x1,
  AI_SHAPE_INIT(5, 1, 2, 20, 20, 24), AI_STRIDE_INIT(5, 1, 1, 48, 960, 2),
  1, &_stage2_stage2_1_Transpose_output_0_output_array, &_stage2_stage2_1_Transpose_output_0_output_array_intq)

/* Tensor #19 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_1_Transpose_output_0_output0, AI_STATIC,
  43, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 20, 20), AI_STRIDE_INIT(4, 1, 1, 48, 960),
  1, &_stage2_stage2_1_Transpose_output_0_output_array, &_stage2_stage2_1_Transpose_output_0_output_array_intq)

/* Tensor #20 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_2_Slice_output_0_output, AI_STATIC,
  59, 0x1,
  AI_SHAPE_INIT(4, 1, 24, 20, 20), AI_STRIDE_INIT(4, 1, 1, 24, 480),
  1, &_stage2_stage2_2_Slice_output_0_output_array, &_stage2_stage2_2_Slice_output_0_output_array_intq)

/* Tensor #21 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_2_Slice_1_output_0_output, AI_STATIC,
  58, 0x1,
  AI_SHAPE_INIT(4, 1, 24, 20, 20), AI_STRIDE_INIT(4, 1, 1, 24, 480),
  1, &_stage2_stage2_2_Slice_1_output_0_output_array, &_stage2_stage2_2_Slice_1_output_0_output_array_intq)

/* Tensor #22 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_2_Concat_output_0_output, AI_STATIC,
  56, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 20, 20), AI_STRIDE_INIT(4, 1, 1, 48, 960),
  1, &_stage2_stage2_2_Concat_output_0_output_array, &_stage2_stage2_2_Concat_output_0_output_array_intq)

/* Tensor #23 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_2_branch2_branch2_7_Relu_output_0_output, AI_STATIC,
  71, 0x1,
  AI_SHAPE_INIT(4, 1, 24, 20, 20), AI_STRIDE_INIT(4, 1, 1, 24, 480),
  1, &_stage2_stage2_2_branch2_branch2_7_Relu_output_0_output_array, &_stage2_stage2_2_branch2_branch2_7_Relu_output_0_output_array_intq)

/* Tensor #24 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_2_Concat_output_0_output0, AI_STATIC,
  57, 0x1,
  AI_SHAPE_INIT(5, 1, 24, 20, 20, 2), AI_STRIDE_INIT(5, 1, 1, 48, 960, 24),
  1, &_stage2_stage2_2_Concat_output_0_output_array, &_stage2_stage2_2_Concat_output_0_output_array_intq)

/* Tensor #25 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_2_Transpose_output_0_output, AI_STATIC,
  60, 0x1,
  AI_SHAPE_INIT(5, 1, 2, 20, 20, 24), AI_STRIDE_INIT(5, 1, 1, 48, 960, 2),
  1, &_stage2_stage2_2_Transpose_output_0_output_array, &_stage2_stage2_2_Transpose_output_0_output_array_intq)

/* Tensor #26 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_2_Transpose_output_0_output0, AI_STATIC,
  61, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 20, 20), AI_STRIDE_INIT(4, 1, 1, 48, 960),
  1, &_stage2_stage2_2_Transpose_output_0_output_array, &_stage2_stage2_2_Transpose_output_0_output_array_intq)

/* Tensor #27 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_3_Slice_output_0_output, AI_STATIC,
  77, 0x1,
  AI_SHAPE_INIT(4, 1, 24, 20, 20), AI_STRIDE_INIT(4, 1, 1, 24, 480),
  1, &_stage2_stage2_3_Slice_output_0_output_array, &_stage2_stage2_3_Slice_output_0_output_array_intq)

/* Tensor #28 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_3_Slice_1_output_0_output, AI_STATIC,
  76, 0x1,
  AI_SHAPE_INIT(4, 1, 24, 20, 20), AI_STRIDE_INIT(4, 1, 1, 24, 480),
  1, &_stage2_stage2_3_Slice_1_output_0_output_array, &_stage2_stage2_3_Slice_1_output_0_output_array_intq)

/* Tensor #29 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_3_Concat_output_0_output, AI_STATIC,
  74, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 20, 20), AI_STRIDE_INIT(4, 1, 1, 48, 960),
  1, &_stage2_stage2_3_Concat_output_0_output_array, &_stage2_stage2_3_Concat_output_0_output_array_intq)

/* Tensor #30 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_3_branch2_branch2_7_Relu_output_0_output, AI_STATIC,
  89, 0x1,
  AI_SHAPE_INIT(4, 1, 24, 20, 20), AI_STRIDE_INIT(4, 1, 1, 24, 480),
  1, &_stage2_stage2_3_branch2_branch2_7_Relu_output_0_output_array, &_stage2_stage2_3_branch2_branch2_7_Relu_output_0_output_array_intq)

/* Tensor #31 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_3_Concat_output_0_output0, AI_STATIC,
  75, 0x1,
  AI_SHAPE_INIT(5, 1, 24, 20, 20, 2), AI_STRIDE_INIT(5, 1, 1, 48, 960, 24),
  1, &_stage2_stage2_3_Concat_output_0_output_array, &_stage2_stage2_3_Concat_output_0_output_array_intq)

/* Tensor #32 */
AI_TENSOR_OBJ_DECLARE(
  _stage2_stage2_3_Transpose_output_0_output, AI_STATIC,
  78, 0x1,
  AI_SHAPE_INIT(5, 1, 2, 20, 20, 24), AI_STRIDE_INIT(5, 1, 1, 48, 960, 2),
  1, &_stage2_stage2_3_Transpose_output_0_output_array, &_stage2_stage2_3_Transpose_output_0_output_array_intq)

/* Tensor #33 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_0_Concat_output_0_output, AI_STATIC,
  92, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 10, 10), AI_STRIDE_INIT(4, 1, 1, 96, 960),
  1, &_stage3_stage3_0_Concat_output_0_output_array, &_stage3_stage3_0_Concat_output_0_output_array_intq)

/* Tensor #34 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_0_branch1_branch1_4_Relu_output_0_output, AI_STATIC,
  101, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_0_branch1_branch1_4_Relu_output_0_output_array, &_stage3_stage3_0_branch1_branch1_4_Relu_output_0_output_array_intq)

/* Tensor #35 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_0_branch2_branch2_7_Relu_output_0_output, AI_STATIC,
  113, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_0_branch2_branch2_7_Relu_output_0_output_array, &_stage3_stage3_0_branch2_branch2_7_Relu_output_0_output_array_intq)

/* Tensor #36 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_0_Concat_output_0_output0, AI_STATIC,
  93, 0x1,
  AI_SHAPE_INIT(5, 1, 48, 10, 10, 2), AI_STRIDE_INIT(5, 1, 1, 96, 960, 48),
  1, &_stage3_stage3_0_Concat_output_0_output_array, &_stage3_stage3_0_Concat_output_0_output_array_intq)

/* Tensor #37 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_0_Transpose_output_0_output, AI_STATIC,
  94, 0x1,
  AI_SHAPE_INIT(5, 1, 2, 10, 10, 48), AI_STRIDE_INIT(5, 1, 1, 96, 960, 2),
  1, &_stage3_stage3_0_Transpose_output_0_output_array, &_stage3_stage3_0_Transpose_output_0_output_array_intq)

/* Tensor #38 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_0_Transpose_output_0_output0, AI_STATIC,
  95, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 10, 10), AI_STRIDE_INIT(4, 1, 1, 96, 960),
  1, &_stage3_stage3_0_Transpose_output_0_output_array, &_stage3_stage3_0_Transpose_output_0_output_array_intq)

/* Tensor #39 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_1_Slice_output_0_output, AI_STATIC,
  119, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_1_Slice_output_0_output_array, &_stage3_stage3_1_Slice_output_0_output_array_intq)

/* Tensor #40 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_1_Slice_1_output_0_output, AI_STATIC,
  118, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_1_Slice_1_output_0_output_array, &_stage3_stage3_1_Slice_1_output_0_output_array_intq)

/* Tensor #41 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_1_Concat_output_0_output, AI_STATIC,
  116, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 10, 10), AI_STRIDE_INIT(4, 1, 1, 96, 960),
  1, &_stage3_stage3_1_Concat_output_0_output_array, &_stage3_stage3_1_Concat_output_0_output_array_intq)

/* Tensor #42 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_1_branch2_branch2_7_Relu_output_0_output, AI_STATIC,
  131, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_1_branch2_branch2_7_Relu_output_0_output_array, &_stage3_stage3_1_branch2_branch2_7_Relu_output_0_output_array_intq)

/* Tensor #43 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_1_Concat_output_0_output0, AI_STATIC,
  117, 0x1,
  AI_SHAPE_INIT(5, 1, 48, 10, 10, 2), AI_STRIDE_INIT(5, 1, 1, 96, 960, 48),
  1, &_stage3_stage3_1_Concat_output_0_output_array, &_stage3_stage3_1_Concat_output_0_output_array_intq)

/* Tensor #44 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_1_Transpose_output_0_output, AI_STATIC,
  120, 0x1,
  AI_SHAPE_INIT(5, 1, 2, 10, 10, 48), AI_STRIDE_INIT(5, 1, 1, 96, 960, 2),
  1, &_stage3_stage3_1_Transpose_output_0_output_array, &_stage3_stage3_1_Transpose_output_0_output_array_intq)

/* Tensor #45 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_1_Transpose_output_0_output0, AI_STATIC,
  121, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 10, 10), AI_STRIDE_INIT(4, 1, 1, 96, 960),
  1, &_stage3_stage3_1_Transpose_output_0_output_array, &_stage3_stage3_1_Transpose_output_0_output_array_intq)

/* Tensor #46 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_2_Slice_output_0_output, AI_STATIC,
  137, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_2_Slice_output_0_output_array, &_stage3_stage3_2_Slice_output_0_output_array_intq)

/* Tensor #47 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_2_Slice_1_output_0_output, AI_STATIC,
  136, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_2_Slice_1_output_0_output_array, &_stage3_stage3_2_Slice_1_output_0_output_array_intq)

/* Tensor #48 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_2_Concat_output_0_output, AI_STATIC,
  134, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 10, 10), AI_STRIDE_INIT(4, 1, 1, 96, 960),
  1, &_stage3_stage3_2_Concat_output_0_output_array, &_stage3_stage3_2_Concat_output_0_output_array_intq)

/* Tensor #49 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_2_branch2_branch2_7_Relu_output_0_output, AI_STATIC,
  149, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_2_branch2_branch2_7_Relu_output_0_output_array, &_stage3_stage3_2_branch2_branch2_7_Relu_output_0_output_array_intq)

/* Tensor #50 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_2_Concat_output_0_output0, AI_STATIC,
  135, 0x1,
  AI_SHAPE_INIT(5, 1, 48, 10, 10, 2), AI_STRIDE_INIT(5, 1, 1, 96, 960, 48),
  1, &_stage3_stage3_2_Concat_output_0_output_array, &_stage3_stage3_2_Concat_output_0_output_array_intq)

/* Tensor #51 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_2_Transpose_output_0_output, AI_STATIC,
  138, 0x1,
  AI_SHAPE_INIT(5, 1, 2, 10, 10, 48), AI_STRIDE_INIT(5, 1, 1, 96, 960, 2),
  1, &_stage3_stage3_2_Transpose_output_0_output_array, &_stage3_stage3_2_Transpose_output_0_output_array_intq)

/* Tensor #52 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_2_Transpose_output_0_output0, AI_STATIC,
  139, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 10, 10), AI_STRIDE_INIT(4, 1, 1, 96, 960),
  1, &_stage3_stage3_2_Transpose_output_0_output_array, &_stage3_stage3_2_Transpose_output_0_output_array_intq)

/* Tensor #53 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_3_Slice_output_0_output, AI_STATIC,
  155, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_3_Slice_output_0_output_array, &_stage3_stage3_3_Slice_output_0_output_array_intq)

/* Tensor #54 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_3_Slice_1_output_0_output, AI_STATIC,
  154, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_3_Slice_1_output_0_output_array, &_stage3_stage3_3_Slice_1_output_0_output_array_intq)

/* Tensor #55 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_3_Concat_output_0_output, AI_STATIC,
  152, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 10, 10), AI_STRIDE_INIT(4, 1, 1, 96, 960),
  1, &_stage3_stage3_3_Concat_output_0_output_array, &_stage3_stage3_3_Concat_output_0_output_array_intq)

/* Tensor #56 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_3_branch2_branch2_7_Relu_output_0_output, AI_STATIC,
  167, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_3_branch2_branch2_7_Relu_output_0_output_array, &_stage3_stage3_3_branch2_branch2_7_Relu_output_0_output_array_intq)

/* Tensor #57 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_3_Concat_output_0_output0, AI_STATIC,
  153, 0x1,
  AI_SHAPE_INIT(5, 1, 48, 10, 10, 2), AI_STRIDE_INIT(5, 1, 1, 96, 960, 48),
  1, &_stage3_stage3_3_Concat_output_0_output_array, &_stage3_stage3_3_Concat_output_0_output_array_intq)

/* Tensor #58 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_3_Transpose_output_0_output, AI_STATIC,
  156, 0x1,
  AI_SHAPE_INIT(5, 1, 2, 10, 10, 48), AI_STRIDE_INIT(5, 1, 1, 96, 960, 2),
  1, &_stage3_stage3_3_Transpose_output_0_output_array, &_stage3_stage3_3_Transpose_output_0_output_array_intq)

/* Tensor #59 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_3_Transpose_output_0_output0, AI_STATIC,
  157, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 10, 10), AI_STRIDE_INIT(4, 1, 1, 96, 960),
  1, &_stage3_stage3_3_Transpose_output_0_output_array, &_stage3_stage3_3_Transpose_output_0_output_array_intq)

/* Tensor #60 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_4_Slice_output_0_output, AI_STATIC,
  173, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_4_Slice_output_0_output_array, &_stage3_stage3_4_Slice_output_0_output_array_intq)

/* Tensor #61 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_4_Slice_1_output_0_output, AI_STATIC,
  172, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_4_Slice_1_output_0_output_array, &_stage3_stage3_4_Slice_1_output_0_output_array_intq)

/* Tensor #62 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_4_Concat_output_0_output, AI_STATIC,
  170, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 10, 10), AI_STRIDE_INIT(4, 1, 1, 96, 960),
  1, &_stage3_stage3_4_Concat_output_0_output_array, &_stage3_stage3_4_Concat_output_0_output_array_intq)

/* Tensor #63 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_4_branch2_branch2_7_Relu_output_0_output, AI_STATIC,
  185, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_4_branch2_branch2_7_Relu_output_0_output_array, &_stage3_stage3_4_branch2_branch2_7_Relu_output_0_output_array_intq)

/* Tensor #64 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_4_Concat_output_0_output0, AI_STATIC,
  171, 0x1,
  AI_SHAPE_INIT(5, 1, 48, 10, 10, 2), AI_STRIDE_INIT(5, 1, 1, 96, 960, 48),
  1, &_stage3_stage3_4_Concat_output_0_output_array, &_stage3_stage3_4_Concat_output_0_output_array_intq)

/* Tensor #65 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_4_Transpose_output_0_output, AI_STATIC,
  174, 0x1,
  AI_SHAPE_INIT(5, 1, 2, 10, 10, 48), AI_STRIDE_INIT(5, 1, 1, 96, 960, 2),
  1, &_stage3_stage3_4_Transpose_output_0_output_array, &_stage3_stage3_4_Transpose_output_0_output_array_intq)

/* Tensor #66 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_4_Transpose_output_0_output0, AI_STATIC,
  175, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 10, 10), AI_STRIDE_INIT(4, 1, 1, 96, 960),
  1, &_stage3_stage3_4_Transpose_output_0_output_array, &_stage3_stage3_4_Transpose_output_0_output_array_intq)

/* Tensor #67 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_5_Slice_output_0_output, AI_STATIC,
  191, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_5_Slice_output_0_output_array, &_stage3_stage3_5_Slice_output_0_output_array_intq)

/* Tensor #68 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_5_Slice_1_output_0_output, AI_STATIC,
  190, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_5_Slice_1_output_0_output_array, &_stage3_stage3_5_Slice_1_output_0_output_array_intq)

/* Tensor #69 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_5_Concat_output_0_output, AI_STATIC,
  188, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 10, 10), AI_STRIDE_INIT(4, 1, 1, 96, 960),
  1, &_stage3_stage3_5_Concat_output_0_output_array, &_stage3_stage3_5_Concat_output_0_output_array_intq)

/* Tensor #70 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_5_branch2_branch2_7_Relu_output_0_output, AI_STATIC,
  203, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_5_branch2_branch2_7_Relu_output_0_output_array, &_stage3_stage3_5_branch2_branch2_7_Relu_output_0_output_array_intq)

/* Tensor #71 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_5_Concat_output_0_output0, AI_STATIC,
  189, 0x1,
  AI_SHAPE_INIT(5, 1, 48, 10, 10, 2), AI_STRIDE_INIT(5, 1, 1, 96, 960, 48),
  1, &_stage3_stage3_5_Concat_output_0_output_array, &_stage3_stage3_5_Concat_output_0_output_array_intq)

/* Tensor #72 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_5_Transpose_output_0_output, AI_STATIC,
  192, 0x1,
  AI_SHAPE_INIT(5, 1, 2, 10, 10, 48), AI_STRIDE_INIT(5, 1, 1, 96, 960, 2),
  1, &_stage3_stage3_5_Transpose_output_0_output_array, &_stage3_stage3_5_Transpose_output_0_output_array_intq)

/* Tensor #73 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_5_Transpose_output_0_output0, AI_STATIC,
  193, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 10, 10), AI_STRIDE_INIT(4, 1, 1, 96, 960),
  1, &_stage3_stage3_5_Transpose_output_0_output_array, &_stage3_stage3_5_Transpose_output_0_output_array_intq)

/* Tensor #74 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_6_Slice_output_0_output, AI_STATIC,
  209, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_6_Slice_output_0_output_array, &_stage3_stage3_6_Slice_output_0_output_array_intq)

/* Tensor #75 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_6_Slice_1_output_0_output, AI_STATIC,
  208, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_6_Slice_1_output_0_output_array, &_stage3_stage3_6_Slice_1_output_0_output_array_intq)

/* Tensor #76 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_6_Concat_output_0_output, AI_STATIC,
  206, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 10, 10), AI_STRIDE_INIT(4, 1, 1, 96, 960),
  1, &_stage3_stage3_6_Concat_output_0_output_array, &_stage3_stage3_6_Concat_output_0_output_array_intq)

/* Tensor #77 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_6_branch2_branch2_7_Relu_output_0_output, AI_STATIC,
  221, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_6_branch2_branch2_7_Relu_output_0_output_array, &_stage3_stage3_6_branch2_branch2_7_Relu_output_0_output_array_intq)

/* Tensor #78 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_6_Concat_output_0_output0, AI_STATIC,
  207, 0x1,
  AI_SHAPE_INIT(5, 1, 48, 10, 10, 2), AI_STRIDE_INIT(5, 1, 1, 96, 960, 48),
  1, &_stage3_stage3_6_Concat_output_0_output_array, &_stage3_stage3_6_Concat_output_0_output_array_intq)

/* Tensor #79 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_6_Transpose_output_0_output, AI_STATIC,
  210, 0x1,
  AI_SHAPE_INIT(5, 1, 2, 10, 10, 48), AI_STRIDE_INIT(5, 1, 1, 96, 960, 2),
  1, &_stage3_stage3_6_Transpose_output_0_output_array, &_stage3_stage3_6_Transpose_output_0_output_array_intq)

/* Tensor #80 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_6_Transpose_output_0_output0, AI_STATIC,
  211, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 10, 10), AI_STRIDE_INIT(4, 1, 1, 96, 960),
  1, &_stage3_stage3_6_Transpose_output_0_output_array, &_stage3_stage3_6_Transpose_output_0_output_array_intq)

/* Tensor #81 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_7_Slice_output_0_output, AI_STATIC,
  227, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_7_Slice_output_0_output_array, &_stage3_stage3_7_Slice_output_0_output_array_intq)

/* Tensor #82 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_7_Slice_1_output_0_output, AI_STATIC,
  226, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_7_Slice_1_output_0_output_array, &_stage3_stage3_7_Slice_1_output_0_output_array_intq)

/* Tensor #83 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_7_Concat_output_0_output, AI_STATIC,
  224, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 10, 10), AI_STRIDE_INIT(4, 1, 1, 96, 960),
  1, &_stage3_stage3_7_Concat_output_0_output_array, &_stage3_stage3_7_Concat_output_0_output_array_intq)

/* Tensor #84 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_7_branch2_branch2_7_Relu_output_0_output, AI_STATIC,
  239, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 10, 10), AI_STRIDE_INIT(4, 1, 1, 48, 480),
  1, &_stage3_stage3_7_branch2_branch2_7_Relu_output_0_output_array, &_stage3_stage3_7_branch2_branch2_7_Relu_output_0_output_array_intq)

/* Tensor #85 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_7_Concat_output_0_output0, AI_STATIC,
  225, 0x1,
  AI_SHAPE_INIT(5, 1, 48, 10, 10, 2), AI_STRIDE_INIT(5, 1, 1, 96, 960, 48),
  1, &_stage3_stage3_7_Concat_output_0_output_array, &_stage3_stage3_7_Concat_output_0_output_array_intq)

/* Tensor #86 */
AI_TENSOR_OBJ_DECLARE(
  _stage3_stage3_7_Transpose_output_0_output, AI_STATIC,
  228, 0x1,
  AI_SHAPE_INIT(5, 1, 2, 10, 10, 48), AI_STRIDE_INIT(5, 1, 1, 96, 960, 2),
  1, &_stage3_stage3_7_Transpose_output_0_output_array, &_stage3_stage3_7_Transpose_output_0_output_array_intq)

/* Tensor #87 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_0_Concat_output_0_output, AI_STATIC,
  242, 0x1,
  AI_SHAPE_INIT(4, 1, 192, 5, 5), AI_STRIDE_INIT(4, 1, 1, 192, 960),
  1, &_stage4_stage4_0_Concat_output_0_output_array, &_stage4_stage4_0_Concat_output_0_output_array_intq)

/* Tensor #88 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_0_branch1_branch1_4_Relu_output_0_output, AI_STATIC,
  251, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 5, 5), AI_STRIDE_INIT(4, 1, 1, 96, 480),
  1, &_stage4_stage4_0_branch1_branch1_4_Relu_output_0_output_array, &_stage4_stage4_0_branch1_branch1_4_Relu_output_0_output_array_intq)

/* Tensor #89 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_0_branch2_branch2_7_Relu_output_0_output, AI_STATIC,
  263, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 5, 5), AI_STRIDE_INIT(4, 1, 1, 96, 480),
  1, &_stage4_stage4_0_branch2_branch2_7_Relu_output_0_output_array, &_stage4_stage4_0_branch2_branch2_7_Relu_output_0_output_array_intq)

/* Tensor #90 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_0_Concat_output_0_output0, AI_STATIC,
  243, 0x1,
  AI_SHAPE_INIT(5, 1, 96, 5, 5, 2), AI_STRIDE_INIT(5, 1, 1, 192, 960, 96),
  1, &_stage4_stage4_0_Concat_output_0_output_array, &_stage4_stage4_0_Concat_output_0_output_array_intq)

/* Tensor #91 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_0_Transpose_output_0_output, AI_STATIC,
  244, 0x1,
  AI_SHAPE_INIT(5, 1, 2, 5, 5, 96), AI_STRIDE_INIT(5, 1, 1, 192, 960, 2),
  1, &_stage4_stage4_0_Transpose_output_0_output_array, &_stage4_stage4_0_Transpose_output_0_output_array_intq)

/* Tensor #92 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_0_Transpose_output_0_output0, AI_STATIC,
  245, 0x1,
  AI_SHAPE_INIT(4, 1, 192, 5, 5), AI_STRIDE_INIT(4, 1, 1, 192, 960),
  1, &_stage4_stage4_0_Transpose_output_0_output_array, &_stage4_stage4_0_Transpose_output_0_output_array_intq)

/* Tensor #93 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_1_Slice_output_0_output, AI_STATIC,
  269, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 5, 5), AI_STRIDE_INIT(4, 1, 1, 96, 480),
  1, &_stage4_stage4_1_Slice_output_0_output_array, &_stage4_stage4_1_Slice_output_0_output_array_intq)

/* Tensor #94 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_1_Slice_1_output_0_output, AI_STATIC,
  268, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 5, 5), AI_STRIDE_INIT(4, 1, 1, 96, 480),
  1, &_stage4_stage4_1_Slice_1_output_0_output_array, &_stage4_stage4_1_Slice_1_output_0_output_array_intq)

/* Tensor #95 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_1_Concat_output_0_output, AI_STATIC,
  266, 0x1,
  AI_SHAPE_INIT(4, 1, 192, 5, 5), AI_STRIDE_INIT(4, 1, 1, 192, 960),
  1, &_stage4_stage4_1_Concat_output_0_output_array, &_stage4_stage4_1_Concat_output_0_output_array_intq)

/* Tensor #96 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_1_branch2_branch2_7_Relu_output_0_output, AI_STATIC,
  281, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 5, 5), AI_STRIDE_INIT(4, 1, 1, 96, 480),
  1, &_stage4_stage4_1_branch2_branch2_7_Relu_output_0_output_array, &_stage4_stage4_1_branch2_branch2_7_Relu_output_0_output_array_intq)

/* Tensor #97 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_1_Concat_output_0_output0, AI_STATIC,
  267, 0x1,
  AI_SHAPE_INIT(5, 1, 96, 5, 5, 2), AI_STRIDE_INIT(5, 1, 1, 192, 960, 96),
  1, &_stage4_stage4_1_Concat_output_0_output_array, &_stage4_stage4_1_Concat_output_0_output_array_intq)

/* Tensor #98 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_1_Transpose_output_0_output, AI_STATIC,
  270, 0x1,
  AI_SHAPE_INIT(5, 1, 2, 5, 5, 96), AI_STRIDE_INIT(5, 1, 1, 192, 960, 2),
  1, &_stage4_stage4_1_Transpose_output_0_output_array, &_stage4_stage4_1_Transpose_output_0_output_array_intq)

/* Tensor #99 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_1_Transpose_output_0_output0, AI_STATIC,
  271, 0x1,
  AI_SHAPE_INIT(4, 1, 192, 5, 5), AI_STRIDE_INIT(4, 1, 1, 192, 960),
  1, &_stage4_stage4_1_Transpose_output_0_output_array, &_stage4_stage4_1_Transpose_output_0_output_array_intq)

/* Tensor #100 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_2_Slice_output_0_output, AI_STATIC,
  287, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 5, 5), AI_STRIDE_INIT(4, 1, 1, 96, 480),
  1, &_stage4_stage4_2_Slice_output_0_output_array, &_stage4_stage4_2_Slice_output_0_output_array_intq)

/* Tensor #101 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_2_Slice_1_output_0_output, AI_STATIC,
  286, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 5, 5), AI_STRIDE_INIT(4, 1, 1, 96, 480),
  1, &_stage4_stage4_2_Slice_1_output_0_output_array, &_stage4_stage4_2_Slice_1_output_0_output_array_intq)

/* Tensor #102 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_2_Concat_output_0_output, AI_STATIC,
  284, 0x1,
  AI_SHAPE_INIT(4, 1, 192, 5, 5), AI_STRIDE_INIT(4, 1, 1, 192, 960),
  1, &_stage4_stage4_2_Concat_output_0_output_array, &_stage4_stage4_2_Concat_output_0_output_array_intq)

/* Tensor #103 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_2_branch2_branch2_7_Relu_output_0_output, AI_STATIC,
  299, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 5, 5), AI_STRIDE_INIT(4, 1, 1, 96, 480),
  1, &_stage4_stage4_2_branch2_branch2_7_Relu_output_0_output_array, &_stage4_stage4_2_branch2_branch2_7_Relu_output_0_output_array_intq)

/* Tensor #104 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_2_Concat_output_0_output0, AI_STATIC,
  285, 0x1,
  AI_SHAPE_INIT(5, 1, 96, 5, 5, 2), AI_STRIDE_INIT(5, 1, 1, 192, 960, 96),
  1, &_stage4_stage4_2_Concat_output_0_output_array, &_stage4_stage4_2_Concat_output_0_output_array_intq)

/* Tensor #105 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_2_Transpose_output_0_output, AI_STATIC,
  288, 0x1,
  AI_SHAPE_INIT(5, 1, 2, 5, 5, 96), AI_STRIDE_INIT(5, 1, 1, 192, 960, 2),
  1, &_stage4_stage4_2_Transpose_output_0_output_array, &_stage4_stage4_2_Transpose_output_0_output_array_intq)

/* Tensor #106 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_2_Transpose_output_0_output0, AI_STATIC,
  289, 0x1,
  AI_SHAPE_INIT(4, 1, 192, 5, 5), AI_STRIDE_INIT(4, 1, 1, 192, 960),
  1, &_stage4_stage4_2_Transpose_output_0_output_array, &_stage4_stage4_2_Transpose_output_0_output_array_intq)

/* Tensor #107 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_3_Slice_output_0_output, AI_STATIC,
  305, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 5, 5), AI_STRIDE_INIT(4, 1, 1, 96, 480),
  1, &_stage4_stage4_3_Slice_output_0_output_array, &_stage4_stage4_3_Slice_output_0_output_array_intq)

/* Tensor #108 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_3_Slice_1_output_0_output, AI_STATIC,
  304, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 5, 5), AI_STRIDE_INIT(4, 1, 1, 96, 480),
  1, &_stage4_stage4_3_Slice_1_output_0_output_array, &_stage4_stage4_3_Slice_1_output_0_output_array_intq)

/* Tensor #109 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_3_Concat_output_0_output, AI_STATIC,
  302, 0x1,
  AI_SHAPE_INIT(4, 1, 192, 5, 5), AI_STRIDE_INIT(4, 1, 1, 192, 960),
  1, &_stage4_stage4_3_Concat_output_0_output_array, &_stage4_stage4_3_Concat_output_0_output_array_intq)

/* Tensor #110 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_3_branch2_branch2_7_Relu_output_0_output, AI_STATIC,
  317, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 5, 5), AI_STRIDE_INIT(4, 1, 1, 96, 480),
  1, &_stage4_stage4_3_branch2_branch2_7_Relu_output_0_output_array, &_stage4_stage4_3_branch2_branch2_7_Relu_output_0_output_array_intq)

/* Tensor #111 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_3_Concat_output_0_output0, AI_STATIC,
  303, 0x1,
  AI_SHAPE_INIT(5, 1, 96, 5, 5, 2), AI_STRIDE_INIT(5, 1, 1, 192, 960, 96),
  1, &_stage4_stage4_3_Concat_output_0_output_array, &_stage4_stage4_3_Concat_output_0_output_array_intq)

/* Tensor #112 */
AI_TENSOR_OBJ_DECLARE(
  _stage4_stage4_3_Transpose_output_0_output, AI_STATIC,
  306, 0x1,
  AI_SHAPE_INIT(5, 1, 2, 5, 5, 96), AI_STRIDE_INIT(5, 1, 1, 192, 960, 2),
  1, &_stage4_stage4_3_Transpose_output_0_output_array, &_stage4_stage4_3_Transpose_output_0_output_array_intq)

/* Tensor #113 */
AI_TENSOR_OBJ_DECLARE(
  _ReduceMean_output_0_output, AI_STATIC,
  3, 0x0,
  AI_SHAPE_INIT(4, 1, 1024, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4096, 4096),
  1, &_ReduceMean_output_0_output_array, NULL)

/* Tensor #114 */
AI_TENSOR_OBJ_DECLARE(
  _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_output, AI_STATIC,
  9, 0x0,
  AI_SHAPE_INIT(4, 1, 1024, 5, 5), AI_STRIDE_INIT(4, 4, 4, 4096, 20480),
  1, &_conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_output_array, NULL)

/* Tensor #115 */
AI_TENSOR_OBJ_DECLARE(
  _ReduceMean_output_0_Mul_output, AI_STATIC,
  1, 0x0,
  AI_SHAPE_INIT(4, 1, 1024, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4096, 4096),
  1, &_ReduceMean_output_0_Mul_output_array, NULL)

/* Tensor #116 */
AI_TENSOR_OBJ_DECLARE(
  _ReduceMean_output_0_Placeholder, AI_STATIC,
  2, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_ReduceMean_output_0_Placeholder_array, NULL)

/* Tensor #117 */
AI_TENSOR_OBJ_DECLARE(
  _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_output, AI_STATIC,
  0, 0x1,
  AI_SHAPE_INIT(4, 1, 1024, 1, 1), AI_STRIDE_INIT(4, 1, 1, 1024, 1024),
  1, &_ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_output_array, &_ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_output_array_intq)

/* Tensor #118 */
AI_TENSOR_OBJ_DECLARE(
  logits_QuantizeLinear_Input_bias, AI_STATIC,
  322, 0x0,
  AI_SHAPE_INIT(4, 1, 3, 1, 1), AI_STRIDE_INIT(4, 4, 4, 12, 12),
  1, &logits_QuantizeLinear_Input_bias_array, NULL)

/* Tensor #119 */
AI_TENSOR_OBJ_DECLARE(
  logits_QuantizeLinear_Input_output, AI_STATIC,
  323, 0x1,
  AI_SHAPE_INIT(4, 1, 3, 1, 1), AI_STRIDE_INIT(4, 1, 1, 3, 3),
  1, &logits_QuantizeLinear_Input_output_array, &logits_QuantizeLinear_Input_output_array_intq)

/* Tensor #120 */
AI_TENSOR_OBJ_DECLARE(
  logits_QuantizeLinear_Input_scratch0, AI_STATIC,
  324, 0x0,
  AI_SHAPE_INIT(4, 1, 1039, 1, 1), AI_STRIDE_INIT(4, 2, 2, 2078, 2078),
  1, &logits_QuantizeLinear_Input_scratch0_array, NULL)

/* Tensor #121 */
AI_TENSOR_OBJ_DECLARE(
  logits_QuantizeLinear_Input_weights, AI_STATIC,
  325, 0x1,
  AI_SHAPE_INIT(4, 1024, 3, 1, 1), AI_STRIDE_INIT(4, 1, 1024, 3072, 3072),
  1, &logits_QuantizeLinear_Input_weights_array, &logits_QuantizeLinear_Input_weights_array_intq)


AI_TENSOR_CHAIN_OBJ_DECLARE(
  image_Transpose_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&image_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&image_Transpose_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  image_Transpose_layer, 2,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &image_Transpose_chain,
  NULL, &image_Transpose_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _conv1_conv1_2_Relu_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&image_Transpose_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_conv1_conv1_2_Relu_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, (ai_tensor *)&_conv1_conv1_2_Relu_output_0_weights, (ai_tensor *)&_conv1_conv1_2_Relu_output_0_bias, (ai_tensor *)NULL),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_conv1_conv1_2_Relu_output_0_scratch0, (ai_tensor *)&_conv1_conv1_2_Relu_output_0_scratch1)
)

AI_LAYER_OBJ_DECLARE(
  _conv1_conv1_2_Relu_output_0_layer, 120,
  OPTIMIZED_CONV2D_TYPE, 0x0, NULL,
  conv2d_nl_pool, forward_conv2d_sssa8_ch_nl_pool,
  &_conv1_conv1_2_Relu_output_0_chain,
  NULL, &_conv1_conv1_2_Relu_output_0_layer, AI_STATIC, 
  .groups = 1, 
  .filter_stride = AI_SHAPE_2D_INIT(2, 2), 
  .dilation = AI_SHAPE_2D_INIT(1, 1), 
  .filter_pad = AI_SHAPE_INIT(4, 1, 1, 1, 1), 
  .pool_size = AI_SHAPE_2D_INIT(3, 3), 
  .pool_stride = AI_SHAPE_2D_INIT(2, 2), 
  .pool_pad = AI_SHAPE_INIT(4, 1, 1, 1, 1), 
  .pool_func = AI_HANDLE_PTR(pool_func_mp_array_integer_INT8), 
  .in_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_SAME, 
  .out_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage2_stage2_0_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stage2_stage2_0_branch1_branch1_4_Relu_output_0_output, (ai_tensor *)&_stage2_stage2_0_branch2_branch2_7_Relu_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_0_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage2_stage2_0_Concat_output_0_layer, 138,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_stage2_stage2_0_Concat_output_0_chain,
  NULL, &_stage2_stage2_0_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage2_stage2_0_Transpose_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_0_Concat_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_0_Transpose_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage2_stage2_0_Transpose_output_0_layer, 144,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_stage2_stage2_0_Transpose_output_0_chain,
  NULL, &_stage2_stage2_0_Transpose_output_0_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_WIDTH, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_EXTENSION), 
)


AI_STATIC_CONST ai_u8 _stage2_stage2_1_Slice_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage2_stage2_1_Slice_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage2_stage2_1_Slice_output_0_axes_data, _stage2_stage2_1_Slice_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage2_stage2_1_Slice_output_0_starts_data[] = { 0 };
AI_ARRAY_OBJ_DECLARE(
    _stage2_stage2_1_Slice_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage2_stage2_1_Slice_output_0_starts_data, _stage2_stage2_1_Slice_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage2_stage2_1_Slice_output_0_ends_data[] = { 24 };
AI_ARRAY_OBJ_DECLARE(
    _stage2_stage2_1_Slice_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage2_stage2_1_Slice_output_0_ends_data, _stage2_stage2_1_Slice_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage2_stage2_1_Slice_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_0_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_1_Slice_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage2_stage2_1_Slice_output_0_layer, 151,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage2_stage2_1_Slice_output_0_chain,
  NULL, &_stage2_stage2_1_Slice_output_0_layer, AI_STATIC, 
  .axes = &_stage2_stage2_1_Slice_output_0_axes, 
  .starts = &_stage2_stage2_1_Slice_output_0_starts, 
  .ends = &_stage2_stage2_1_Slice_output_0_ends, 
)


AI_STATIC_CONST ai_u8 _stage2_stage2_1_Slice_1_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage2_stage2_1_Slice_1_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage2_stage2_1_Slice_1_output_0_axes_data, _stage2_stage2_1_Slice_1_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage2_stage2_1_Slice_1_output_0_starts_data[] = { 24 };
AI_ARRAY_OBJ_DECLARE(
    _stage2_stage2_1_Slice_1_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage2_stage2_1_Slice_1_output_0_starts_data, _stage2_stage2_1_Slice_1_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage2_stage2_1_Slice_1_output_0_ends_data[] = { 48 };
AI_ARRAY_OBJ_DECLARE(
    _stage2_stage2_1_Slice_1_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage2_stage2_1_Slice_1_output_0_ends_data, _stage2_stage2_1_Slice_1_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage2_stage2_1_Slice_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_0_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_1_Slice_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage2_stage2_1_Slice_1_output_0_layer, 150,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage2_stage2_1_Slice_1_output_0_chain,
  NULL, &_stage2_stage2_1_Slice_1_output_0_layer, AI_STATIC, 
  .axes = &_stage2_stage2_1_Slice_1_output_0_axes, 
  .starts = &_stage2_stage2_1_Slice_1_output_0_starts, 
  .ends = &_stage2_stage2_1_Slice_1_output_0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage2_stage2_1_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stage2_stage2_1_Slice_output_0_output, (ai_tensor *)&_stage2_stage2_1_branch2_branch2_7_Relu_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_1_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage2_stage2_1_Concat_output_0_layer, 165,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_stage2_stage2_1_Concat_output_0_chain,
  NULL, &_stage2_stage2_1_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage2_stage2_1_Transpose_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_1_Concat_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_1_Transpose_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage2_stage2_1_Transpose_output_0_layer, 171,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_stage2_stage2_1_Transpose_output_0_chain,
  NULL, &_stage2_stage2_1_Transpose_output_0_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_WIDTH, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_EXTENSION), 
)


AI_STATIC_CONST ai_u8 _stage2_stage2_2_Slice_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage2_stage2_2_Slice_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage2_stage2_2_Slice_output_0_axes_data, _stage2_stage2_2_Slice_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage2_stage2_2_Slice_output_0_starts_data[] = { 0 };
AI_ARRAY_OBJ_DECLARE(
    _stage2_stage2_2_Slice_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage2_stage2_2_Slice_output_0_starts_data, _stage2_stage2_2_Slice_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage2_stage2_2_Slice_output_0_ends_data[] = { 24 };
AI_ARRAY_OBJ_DECLARE(
    _stage2_stage2_2_Slice_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage2_stage2_2_Slice_output_0_ends_data, _stage2_stage2_2_Slice_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage2_stage2_2_Slice_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_1_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_2_Slice_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage2_stage2_2_Slice_output_0_layer, 178,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage2_stage2_2_Slice_output_0_chain,
  NULL, &_stage2_stage2_2_Slice_output_0_layer, AI_STATIC, 
  .axes = &_stage2_stage2_2_Slice_output_0_axes, 
  .starts = &_stage2_stage2_2_Slice_output_0_starts, 
  .ends = &_stage2_stage2_2_Slice_output_0_ends, 
)


AI_STATIC_CONST ai_u8 _stage2_stage2_2_Slice_1_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage2_stage2_2_Slice_1_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage2_stage2_2_Slice_1_output_0_axes_data, _stage2_stage2_2_Slice_1_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage2_stage2_2_Slice_1_output_0_starts_data[] = { 24 };
AI_ARRAY_OBJ_DECLARE(
    _stage2_stage2_2_Slice_1_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage2_stage2_2_Slice_1_output_0_starts_data, _stage2_stage2_2_Slice_1_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage2_stage2_2_Slice_1_output_0_ends_data[] = { 48 };
AI_ARRAY_OBJ_DECLARE(
    _stage2_stage2_2_Slice_1_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage2_stage2_2_Slice_1_output_0_ends_data, _stage2_stage2_2_Slice_1_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage2_stage2_2_Slice_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_1_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_2_Slice_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage2_stage2_2_Slice_1_output_0_layer, 177,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage2_stage2_2_Slice_1_output_0_chain,
  NULL, &_stage2_stage2_2_Slice_1_output_0_layer, AI_STATIC, 
  .axes = &_stage2_stage2_2_Slice_1_output_0_axes, 
  .starts = &_stage2_stage2_2_Slice_1_output_0_starts, 
  .ends = &_stage2_stage2_2_Slice_1_output_0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage2_stage2_2_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stage2_stage2_2_Slice_output_0_output, (ai_tensor *)&_stage2_stage2_2_branch2_branch2_7_Relu_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_2_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage2_stage2_2_Concat_output_0_layer, 192,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_stage2_stage2_2_Concat_output_0_chain,
  NULL, &_stage2_stage2_2_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage2_stage2_2_Transpose_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_2_Concat_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_2_Transpose_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage2_stage2_2_Transpose_output_0_layer, 198,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_stage2_stage2_2_Transpose_output_0_chain,
  NULL, &_stage2_stage2_2_Transpose_output_0_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_WIDTH, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_EXTENSION), 
)


AI_STATIC_CONST ai_u8 _stage2_stage2_3_Slice_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage2_stage2_3_Slice_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage2_stage2_3_Slice_output_0_axes_data, _stage2_stage2_3_Slice_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage2_stage2_3_Slice_output_0_starts_data[] = { 0 };
AI_ARRAY_OBJ_DECLARE(
    _stage2_stage2_3_Slice_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage2_stage2_3_Slice_output_0_starts_data, _stage2_stage2_3_Slice_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage2_stage2_3_Slice_output_0_ends_data[] = { 24 };
AI_ARRAY_OBJ_DECLARE(
    _stage2_stage2_3_Slice_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage2_stage2_3_Slice_output_0_ends_data, _stage2_stage2_3_Slice_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage2_stage2_3_Slice_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_2_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_3_Slice_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage2_stage2_3_Slice_output_0_layer, 205,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage2_stage2_3_Slice_output_0_chain,
  NULL, &_stage2_stage2_3_Slice_output_0_layer, AI_STATIC, 
  .axes = &_stage2_stage2_3_Slice_output_0_axes, 
  .starts = &_stage2_stage2_3_Slice_output_0_starts, 
  .ends = &_stage2_stage2_3_Slice_output_0_ends, 
)


AI_STATIC_CONST ai_u8 _stage2_stage2_3_Slice_1_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage2_stage2_3_Slice_1_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage2_stage2_3_Slice_1_output_0_axes_data, _stage2_stage2_3_Slice_1_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage2_stage2_3_Slice_1_output_0_starts_data[] = { 24 };
AI_ARRAY_OBJ_DECLARE(
    _stage2_stage2_3_Slice_1_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage2_stage2_3_Slice_1_output_0_starts_data, _stage2_stage2_3_Slice_1_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage2_stage2_3_Slice_1_output_0_ends_data[] = { 48 };
AI_ARRAY_OBJ_DECLARE(
    _stage2_stage2_3_Slice_1_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage2_stage2_3_Slice_1_output_0_ends_data, _stage2_stage2_3_Slice_1_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage2_stage2_3_Slice_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_2_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_3_Slice_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage2_stage2_3_Slice_1_output_0_layer, 204,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage2_stage2_3_Slice_1_output_0_chain,
  NULL, &_stage2_stage2_3_Slice_1_output_0_layer, AI_STATIC, 
  .axes = &_stage2_stage2_3_Slice_1_output_0_axes, 
  .starts = &_stage2_stage2_3_Slice_1_output_0_starts, 
  .ends = &_stage2_stage2_3_Slice_1_output_0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage2_stage2_3_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stage2_stage2_3_Slice_output_0_output, (ai_tensor *)&_stage2_stage2_3_branch2_branch2_7_Relu_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_3_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage2_stage2_3_Concat_output_0_layer, 219,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_stage2_stage2_3_Concat_output_0_chain,
  NULL, &_stage2_stage2_3_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage2_stage2_3_Transpose_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_3_Concat_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage2_stage2_3_Transpose_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage2_stage2_3_Transpose_output_0_layer, 225,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_stage2_stage2_3_Transpose_output_0_chain,
  NULL, &_stage2_stage2_3_Transpose_output_0_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_WIDTH, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_0_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stage3_stage3_0_branch1_branch1_4_Relu_output_0_output, (ai_tensor *)&_stage3_stage3_0_branch2_branch2_7_Relu_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_0_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_0_Concat_output_0_layer, 246,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_stage3_stage3_0_Concat_output_0_chain,
  NULL, &_stage3_stage3_0_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_0_Transpose_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_0_Concat_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_0_Transpose_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_0_Transpose_output_0_layer, 252,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_stage3_stage3_0_Transpose_output_0_chain,
  NULL, &_stage3_stage3_0_Transpose_output_0_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_WIDTH, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_EXTENSION), 
)


AI_STATIC_CONST ai_u8 _stage3_stage3_1_Slice_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_1_Slice_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage3_stage3_1_Slice_output_0_axes_data, _stage3_stage3_1_Slice_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_1_Slice_output_0_starts_data[] = { 0 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_1_Slice_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_1_Slice_output_0_starts_data, _stage3_stage3_1_Slice_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_1_Slice_output_0_ends_data[] = { 48 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_1_Slice_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_1_Slice_output_0_ends_data, _stage3_stage3_1_Slice_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_1_Slice_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_0_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_1_Slice_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_1_Slice_output_0_layer, 259,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage3_stage3_1_Slice_output_0_chain,
  NULL, &_stage3_stage3_1_Slice_output_0_layer, AI_STATIC, 
  .axes = &_stage3_stage3_1_Slice_output_0_axes, 
  .starts = &_stage3_stage3_1_Slice_output_0_starts, 
  .ends = &_stage3_stage3_1_Slice_output_0_ends, 
)


AI_STATIC_CONST ai_u8 _stage3_stage3_1_Slice_1_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_1_Slice_1_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage3_stage3_1_Slice_1_output_0_axes_data, _stage3_stage3_1_Slice_1_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_1_Slice_1_output_0_starts_data[] = { 48 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_1_Slice_1_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_1_Slice_1_output_0_starts_data, _stage3_stage3_1_Slice_1_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_1_Slice_1_output_0_ends_data[] = { 96 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_1_Slice_1_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_1_Slice_1_output_0_ends_data, _stage3_stage3_1_Slice_1_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_1_Slice_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_0_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_1_Slice_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_1_Slice_1_output_0_layer, 258,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage3_stage3_1_Slice_1_output_0_chain,
  NULL, &_stage3_stage3_1_Slice_1_output_0_layer, AI_STATIC, 
  .axes = &_stage3_stage3_1_Slice_1_output_0_axes, 
  .starts = &_stage3_stage3_1_Slice_1_output_0_starts, 
  .ends = &_stage3_stage3_1_Slice_1_output_0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_1_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stage3_stage3_1_Slice_output_0_output, (ai_tensor *)&_stage3_stage3_1_branch2_branch2_7_Relu_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_1_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_1_Concat_output_0_layer, 273,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_stage3_stage3_1_Concat_output_0_chain,
  NULL, &_stage3_stage3_1_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_1_Transpose_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_1_Concat_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_1_Transpose_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_1_Transpose_output_0_layer, 279,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_stage3_stage3_1_Transpose_output_0_chain,
  NULL, &_stage3_stage3_1_Transpose_output_0_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_WIDTH, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_EXTENSION), 
)


AI_STATIC_CONST ai_u8 _stage3_stage3_2_Slice_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_2_Slice_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage3_stage3_2_Slice_output_0_axes_data, _stage3_stage3_2_Slice_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_2_Slice_output_0_starts_data[] = { 0 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_2_Slice_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_2_Slice_output_0_starts_data, _stage3_stage3_2_Slice_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_2_Slice_output_0_ends_data[] = { 48 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_2_Slice_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_2_Slice_output_0_ends_data, _stage3_stage3_2_Slice_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_2_Slice_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_1_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_2_Slice_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_2_Slice_output_0_layer, 286,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage3_stage3_2_Slice_output_0_chain,
  NULL, &_stage3_stage3_2_Slice_output_0_layer, AI_STATIC, 
  .axes = &_stage3_stage3_2_Slice_output_0_axes, 
  .starts = &_stage3_stage3_2_Slice_output_0_starts, 
  .ends = &_stage3_stage3_2_Slice_output_0_ends, 
)


AI_STATIC_CONST ai_u8 _stage3_stage3_2_Slice_1_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_2_Slice_1_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage3_stage3_2_Slice_1_output_0_axes_data, _stage3_stage3_2_Slice_1_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_2_Slice_1_output_0_starts_data[] = { 48 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_2_Slice_1_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_2_Slice_1_output_0_starts_data, _stage3_stage3_2_Slice_1_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_2_Slice_1_output_0_ends_data[] = { 96 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_2_Slice_1_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_2_Slice_1_output_0_ends_data, _stage3_stage3_2_Slice_1_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_2_Slice_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_1_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_2_Slice_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_2_Slice_1_output_0_layer, 285,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage3_stage3_2_Slice_1_output_0_chain,
  NULL, &_stage3_stage3_2_Slice_1_output_0_layer, AI_STATIC, 
  .axes = &_stage3_stage3_2_Slice_1_output_0_axes, 
  .starts = &_stage3_stage3_2_Slice_1_output_0_starts, 
  .ends = &_stage3_stage3_2_Slice_1_output_0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_2_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stage3_stage3_2_Slice_output_0_output, (ai_tensor *)&_stage3_stage3_2_branch2_branch2_7_Relu_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_2_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_2_Concat_output_0_layer, 300,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_stage3_stage3_2_Concat_output_0_chain,
  NULL, &_stage3_stage3_2_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_2_Transpose_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_2_Concat_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_2_Transpose_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_2_Transpose_output_0_layer, 306,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_stage3_stage3_2_Transpose_output_0_chain,
  NULL, &_stage3_stage3_2_Transpose_output_0_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_WIDTH, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_EXTENSION), 
)


AI_STATIC_CONST ai_u8 _stage3_stage3_3_Slice_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_3_Slice_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage3_stage3_3_Slice_output_0_axes_data, _stage3_stage3_3_Slice_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_3_Slice_output_0_starts_data[] = { 0 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_3_Slice_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_3_Slice_output_0_starts_data, _stage3_stage3_3_Slice_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_3_Slice_output_0_ends_data[] = { 48 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_3_Slice_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_3_Slice_output_0_ends_data, _stage3_stage3_3_Slice_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_3_Slice_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_2_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_3_Slice_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_3_Slice_output_0_layer, 313,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage3_stage3_3_Slice_output_0_chain,
  NULL, &_stage3_stage3_3_Slice_output_0_layer, AI_STATIC, 
  .axes = &_stage3_stage3_3_Slice_output_0_axes, 
  .starts = &_stage3_stage3_3_Slice_output_0_starts, 
  .ends = &_stage3_stage3_3_Slice_output_0_ends, 
)


AI_STATIC_CONST ai_u8 _stage3_stage3_3_Slice_1_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_3_Slice_1_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage3_stage3_3_Slice_1_output_0_axes_data, _stage3_stage3_3_Slice_1_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_3_Slice_1_output_0_starts_data[] = { 48 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_3_Slice_1_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_3_Slice_1_output_0_starts_data, _stage3_stage3_3_Slice_1_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_3_Slice_1_output_0_ends_data[] = { 96 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_3_Slice_1_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_3_Slice_1_output_0_ends_data, _stage3_stage3_3_Slice_1_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_3_Slice_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_2_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_3_Slice_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_3_Slice_1_output_0_layer, 312,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage3_stage3_3_Slice_1_output_0_chain,
  NULL, &_stage3_stage3_3_Slice_1_output_0_layer, AI_STATIC, 
  .axes = &_stage3_stage3_3_Slice_1_output_0_axes, 
  .starts = &_stage3_stage3_3_Slice_1_output_0_starts, 
  .ends = &_stage3_stage3_3_Slice_1_output_0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_3_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stage3_stage3_3_Slice_output_0_output, (ai_tensor *)&_stage3_stage3_3_branch2_branch2_7_Relu_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_3_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_3_Concat_output_0_layer, 327,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_stage3_stage3_3_Concat_output_0_chain,
  NULL, &_stage3_stage3_3_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_3_Transpose_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_3_Concat_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_3_Transpose_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_3_Transpose_output_0_layer, 333,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_stage3_stage3_3_Transpose_output_0_chain,
  NULL, &_stage3_stage3_3_Transpose_output_0_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_WIDTH, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_EXTENSION), 
)


AI_STATIC_CONST ai_u8 _stage3_stage3_4_Slice_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_4_Slice_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage3_stage3_4_Slice_output_0_axes_data, _stage3_stage3_4_Slice_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_4_Slice_output_0_starts_data[] = { 0 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_4_Slice_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_4_Slice_output_0_starts_data, _stage3_stage3_4_Slice_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_4_Slice_output_0_ends_data[] = { 48 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_4_Slice_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_4_Slice_output_0_ends_data, _stage3_stage3_4_Slice_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_4_Slice_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_3_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_4_Slice_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_4_Slice_output_0_layer, 340,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage3_stage3_4_Slice_output_0_chain,
  NULL, &_stage3_stage3_4_Slice_output_0_layer, AI_STATIC, 
  .axes = &_stage3_stage3_4_Slice_output_0_axes, 
  .starts = &_stage3_stage3_4_Slice_output_0_starts, 
  .ends = &_stage3_stage3_4_Slice_output_0_ends, 
)


AI_STATIC_CONST ai_u8 _stage3_stage3_4_Slice_1_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_4_Slice_1_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage3_stage3_4_Slice_1_output_0_axes_data, _stage3_stage3_4_Slice_1_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_4_Slice_1_output_0_starts_data[] = { 48 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_4_Slice_1_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_4_Slice_1_output_0_starts_data, _stage3_stage3_4_Slice_1_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_4_Slice_1_output_0_ends_data[] = { 96 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_4_Slice_1_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_4_Slice_1_output_0_ends_data, _stage3_stage3_4_Slice_1_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_4_Slice_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_3_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_4_Slice_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_4_Slice_1_output_0_layer, 339,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage3_stage3_4_Slice_1_output_0_chain,
  NULL, &_stage3_stage3_4_Slice_1_output_0_layer, AI_STATIC, 
  .axes = &_stage3_stage3_4_Slice_1_output_0_axes, 
  .starts = &_stage3_stage3_4_Slice_1_output_0_starts, 
  .ends = &_stage3_stage3_4_Slice_1_output_0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_4_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stage3_stage3_4_Slice_output_0_output, (ai_tensor *)&_stage3_stage3_4_branch2_branch2_7_Relu_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_4_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_4_Concat_output_0_layer, 354,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_stage3_stage3_4_Concat_output_0_chain,
  NULL, &_stage3_stage3_4_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_4_Transpose_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_4_Concat_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_4_Transpose_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_4_Transpose_output_0_layer, 360,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_stage3_stage3_4_Transpose_output_0_chain,
  NULL, &_stage3_stage3_4_Transpose_output_0_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_WIDTH, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_EXTENSION), 
)


AI_STATIC_CONST ai_u8 _stage3_stage3_5_Slice_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_5_Slice_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage3_stage3_5_Slice_output_0_axes_data, _stage3_stage3_5_Slice_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_5_Slice_output_0_starts_data[] = { 0 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_5_Slice_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_5_Slice_output_0_starts_data, _stage3_stage3_5_Slice_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_5_Slice_output_0_ends_data[] = { 48 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_5_Slice_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_5_Slice_output_0_ends_data, _stage3_stage3_5_Slice_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_5_Slice_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_4_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_5_Slice_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_5_Slice_output_0_layer, 367,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage3_stage3_5_Slice_output_0_chain,
  NULL, &_stage3_stage3_5_Slice_output_0_layer, AI_STATIC, 
  .axes = &_stage3_stage3_5_Slice_output_0_axes, 
  .starts = &_stage3_stage3_5_Slice_output_0_starts, 
  .ends = &_stage3_stage3_5_Slice_output_0_ends, 
)


AI_STATIC_CONST ai_u8 _stage3_stage3_5_Slice_1_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_5_Slice_1_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage3_stage3_5_Slice_1_output_0_axes_data, _stage3_stage3_5_Slice_1_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_5_Slice_1_output_0_starts_data[] = { 48 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_5_Slice_1_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_5_Slice_1_output_0_starts_data, _stage3_stage3_5_Slice_1_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_5_Slice_1_output_0_ends_data[] = { 96 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_5_Slice_1_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_5_Slice_1_output_0_ends_data, _stage3_stage3_5_Slice_1_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_5_Slice_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_4_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_5_Slice_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_5_Slice_1_output_0_layer, 366,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage3_stage3_5_Slice_1_output_0_chain,
  NULL, &_stage3_stage3_5_Slice_1_output_0_layer, AI_STATIC, 
  .axes = &_stage3_stage3_5_Slice_1_output_0_axes, 
  .starts = &_stage3_stage3_5_Slice_1_output_0_starts, 
  .ends = &_stage3_stage3_5_Slice_1_output_0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_5_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stage3_stage3_5_Slice_output_0_output, (ai_tensor *)&_stage3_stage3_5_branch2_branch2_7_Relu_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_5_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_5_Concat_output_0_layer, 381,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_stage3_stage3_5_Concat_output_0_chain,
  NULL, &_stage3_stage3_5_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_5_Transpose_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_5_Concat_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_5_Transpose_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_5_Transpose_output_0_layer, 387,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_stage3_stage3_5_Transpose_output_0_chain,
  NULL, &_stage3_stage3_5_Transpose_output_0_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_WIDTH, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_EXTENSION), 
)


AI_STATIC_CONST ai_u8 _stage3_stage3_6_Slice_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_6_Slice_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage3_stage3_6_Slice_output_0_axes_data, _stage3_stage3_6_Slice_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_6_Slice_output_0_starts_data[] = { 0 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_6_Slice_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_6_Slice_output_0_starts_data, _stage3_stage3_6_Slice_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_6_Slice_output_0_ends_data[] = { 48 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_6_Slice_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_6_Slice_output_0_ends_data, _stage3_stage3_6_Slice_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_6_Slice_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_5_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_6_Slice_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_6_Slice_output_0_layer, 394,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage3_stage3_6_Slice_output_0_chain,
  NULL, &_stage3_stage3_6_Slice_output_0_layer, AI_STATIC, 
  .axes = &_stage3_stage3_6_Slice_output_0_axes, 
  .starts = &_stage3_stage3_6_Slice_output_0_starts, 
  .ends = &_stage3_stage3_6_Slice_output_0_ends, 
)


AI_STATIC_CONST ai_u8 _stage3_stage3_6_Slice_1_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_6_Slice_1_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage3_stage3_6_Slice_1_output_0_axes_data, _stage3_stage3_6_Slice_1_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_6_Slice_1_output_0_starts_data[] = { 48 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_6_Slice_1_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_6_Slice_1_output_0_starts_data, _stage3_stage3_6_Slice_1_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_6_Slice_1_output_0_ends_data[] = { 96 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_6_Slice_1_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_6_Slice_1_output_0_ends_data, _stage3_stage3_6_Slice_1_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_6_Slice_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_5_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_6_Slice_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_6_Slice_1_output_0_layer, 393,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage3_stage3_6_Slice_1_output_0_chain,
  NULL, &_stage3_stage3_6_Slice_1_output_0_layer, AI_STATIC, 
  .axes = &_stage3_stage3_6_Slice_1_output_0_axes, 
  .starts = &_stage3_stage3_6_Slice_1_output_0_starts, 
  .ends = &_stage3_stage3_6_Slice_1_output_0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_6_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stage3_stage3_6_Slice_output_0_output, (ai_tensor *)&_stage3_stage3_6_branch2_branch2_7_Relu_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_6_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_6_Concat_output_0_layer, 408,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_stage3_stage3_6_Concat_output_0_chain,
  NULL, &_stage3_stage3_6_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_6_Transpose_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_6_Concat_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_6_Transpose_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_6_Transpose_output_0_layer, 414,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_stage3_stage3_6_Transpose_output_0_chain,
  NULL, &_stage3_stage3_6_Transpose_output_0_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_WIDTH, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_EXTENSION), 
)


AI_STATIC_CONST ai_u8 _stage3_stage3_7_Slice_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_7_Slice_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage3_stage3_7_Slice_output_0_axes_data, _stage3_stage3_7_Slice_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_7_Slice_output_0_starts_data[] = { 0 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_7_Slice_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_7_Slice_output_0_starts_data, _stage3_stage3_7_Slice_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_7_Slice_output_0_ends_data[] = { 48 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_7_Slice_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_7_Slice_output_0_ends_data, _stage3_stage3_7_Slice_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_7_Slice_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_6_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_7_Slice_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_7_Slice_output_0_layer, 421,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage3_stage3_7_Slice_output_0_chain,
  NULL, &_stage3_stage3_7_Slice_output_0_layer, AI_STATIC, 
  .axes = &_stage3_stage3_7_Slice_output_0_axes, 
  .starts = &_stage3_stage3_7_Slice_output_0_starts, 
  .ends = &_stage3_stage3_7_Slice_output_0_ends, 
)


AI_STATIC_CONST ai_u8 _stage3_stage3_7_Slice_1_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_7_Slice_1_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage3_stage3_7_Slice_1_output_0_axes_data, _stage3_stage3_7_Slice_1_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_7_Slice_1_output_0_starts_data[] = { 48 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_7_Slice_1_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_7_Slice_1_output_0_starts_data, _stage3_stage3_7_Slice_1_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage3_stage3_7_Slice_1_output_0_ends_data[] = { 96 };
AI_ARRAY_OBJ_DECLARE(
    _stage3_stage3_7_Slice_1_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage3_stage3_7_Slice_1_output_0_ends_data, _stage3_stage3_7_Slice_1_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_7_Slice_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_6_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_7_Slice_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_7_Slice_1_output_0_layer, 420,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage3_stage3_7_Slice_1_output_0_chain,
  NULL, &_stage3_stage3_7_Slice_1_output_0_layer, AI_STATIC, 
  .axes = &_stage3_stage3_7_Slice_1_output_0_axes, 
  .starts = &_stage3_stage3_7_Slice_1_output_0_starts, 
  .ends = &_stage3_stage3_7_Slice_1_output_0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_7_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stage3_stage3_7_Slice_output_0_output, (ai_tensor *)&_stage3_stage3_7_branch2_branch2_7_Relu_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_7_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_7_Concat_output_0_layer, 435,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_stage3_stage3_7_Concat_output_0_chain,
  NULL, &_stage3_stage3_7_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage3_stage3_7_Transpose_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_7_Concat_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage3_stage3_7_Transpose_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage3_stage3_7_Transpose_output_0_layer, 441,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_stage3_stage3_7_Transpose_output_0_chain,
  NULL, &_stage3_stage3_7_Transpose_output_0_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_WIDTH, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage4_stage4_0_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stage4_stage4_0_branch1_branch1_4_Relu_output_0_output, (ai_tensor *)&_stage4_stage4_0_branch2_branch2_7_Relu_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_0_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage4_stage4_0_Concat_output_0_layer, 462,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_stage4_stage4_0_Concat_output_0_chain,
  NULL, &_stage4_stage4_0_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage4_stage4_0_Transpose_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_0_Concat_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_0_Transpose_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage4_stage4_0_Transpose_output_0_layer, 468,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_stage4_stage4_0_Transpose_output_0_chain,
  NULL, &_stage4_stage4_0_Transpose_output_0_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_WIDTH, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_EXTENSION), 
)


AI_STATIC_CONST ai_u8 _stage4_stage4_1_Slice_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage4_stage4_1_Slice_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage4_stage4_1_Slice_output_0_axes_data, _stage4_stage4_1_Slice_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage4_stage4_1_Slice_output_0_starts_data[] = { 0 };
AI_ARRAY_OBJ_DECLARE(
    _stage4_stage4_1_Slice_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage4_stage4_1_Slice_output_0_starts_data, _stage4_stage4_1_Slice_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage4_stage4_1_Slice_output_0_ends_data[] = { 96 };
AI_ARRAY_OBJ_DECLARE(
    _stage4_stage4_1_Slice_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage4_stage4_1_Slice_output_0_ends_data, _stage4_stage4_1_Slice_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage4_stage4_1_Slice_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_0_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_1_Slice_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage4_stage4_1_Slice_output_0_layer, 475,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage4_stage4_1_Slice_output_0_chain,
  NULL, &_stage4_stage4_1_Slice_output_0_layer, AI_STATIC, 
  .axes = &_stage4_stage4_1_Slice_output_0_axes, 
  .starts = &_stage4_stage4_1_Slice_output_0_starts, 
  .ends = &_stage4_stage4_1_Slice_output_0_ends, 
)


AI_STATIC_CONST ai_u8 _stage4_stage4_1_Slice_1_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage4_stage4_1_Slice_1_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage4_stage4_1_Slice_1_output_0_axes_data, _stage4_stage4_1_Slice_1_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage4_stage4_1_Slice_1_output_0_starts_data[] = { 96 };
AI_ARRAY_OBJ_DECLARE(
    _stage4_stage4_1_Slice_1_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage4_stage4_1_Slice_1_output_0_starts_data, _stage4_stage4_1_Slice_1_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage4_stage4_1_Slice_1_output_0_ends_data[] = { 192 };
AI_ARRAY_OBJ_DECLARE(
    _stage4_stage4_1_Slice_1_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage4_stage4_1_Slice_1_output_0_ends_data, _stage4_stage4_1_Slice_1_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage4_stage4_1_Slice_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_0_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_1_Slice_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage4_stage4_1_Slice_1_output_0_layer, 474,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage4_stage4_1_Slice_1_output_0_chain,
  NULL, &_stage4_stage4_1_Slice_1_output_0_layer, AI_STATIC, 
  .axes = &_stage4_stage4_1_Slice_1_output_0_axes, 
  .starts = &_stage4_stage4_1_Slice_1_output_0_starts, 
  .ends = &_stage4_stage4_1_Slice_1_output_0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage4_stage4_1_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stage4_stage4_1_Slice_output_0_output, (ai_tensor *)&_stage4_stage4_1_branch2_branch2_7_Relu_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_1_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage4_stage4_1_Concat_output_0_layer, 489,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_stage4_stage4_1_Concat_output_0_chain,
  NULL, &_stage4_stage4_1_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage4_stage4_1_Transpose_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_1_Concat_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_1_Transpose_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage4_stage4_1_Transpose_output_0_layer, 495,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_stage4_stage4_1_Transpose_output_0_chain,
  NULL, &_stage4_stage4_1_Transpose_output_0_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_WIDTH, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_EXTENSION), 
)


AI_STATIC_CONST ai_u8 _stage4_stage4_2_Slice_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage4_stage4_2_Slice_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage4_stage4_2_Slice_output_0_axes_data, _stage4_stage4_2_Slice_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage4_stage4_2_Slice_output_0_starts_data[] = { 0 };
AI_ARRAY_OBJ_DECLARE(
    _stage4_stage4_2_Slice_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage4_stage4_2_Slice_output_0_starts_data, _stage4_stage4_2_Slice_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage4_stage4_2_Slice_output_0_ends_data[] = { 96 };
AI_ARRAY_OBJ_DECLARE(
    _stage4_stage4_2_Slice_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage4_stage4_2_Slice_output_0_ends_data, _stage4_stage4_2_Slice_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage4_stage4_2_Slice_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_1_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_2_Slice_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage4_stage4_2_Slice_output_0_layer, 502,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage4_stage4_2_Slice_output_0_chain,
  NULL, &_stage4_stage4_2_Slice_output_0_layer, AI_STATIC, 
  .axes = &_stage4_stage4_2_Slice_output_0_axes, 
  .starts = &_stage4_stage4_2_Slice_output_0_starts, 
  .ends = &_stage4_stage4_2_Slice_output_0_ends, 
)


AI_STATIC_CONST ai_u8 _stage4_stage4_2_Slice_1_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage4_stage4_2_Slice_1_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage4_stage4_2_Slice_1_output_0_axes_data, _stage4_stage4_2_Slice_1_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage4_stage4_2_Slice_1_output_0_starts_data[] = { 96 };
AI_ARRAY_OBJ_DECLARE(
    _stage4_stage4_2_Slice_1_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage4_stage4_2_Slice_1_output_0_starts_data, _stage4_stage4_2_Slice_1_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage4_stage4_2_Slice_1_output_0_ends_data[] = { 192 };
AI_ARRAY_OBJ_DECLARE(
    _stage4_stage4_2_Slice_1_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage4_stage4_2_Slice_1_output_0_ends_data, _stage4_stage4_2_Slice_1_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage4_stage4_2_Slice_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_1_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_2_Slice_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage4_stage4_2_Slice_1_output_0_layer, 501,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage4_stage4_2_Slice_1_output_0_chain,
  NULL, &_stage4_stage4_2_Slice_1_output_0_layer, AI_STATIC, 
  .axes = &_stage4_stage4_2_Slice_1_output_0_axes, 
  .starts = &_stage4_stage4_2_Slice_1_output_0_starts, 
  .ends = &_stage4_stage4_2_Slice_1_output_0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage4_stage4_2_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stage4_stage4_2_Slice_output_0_output, (ai_tensor *)&_stage4_stage4_2_branch2_branch2_7_Relu_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_2_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage4_stage4_2_Concat_output_0_layer, 516,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_stage4_stage4_2_Concat_output_0_chain,
  NULL, &_stage4_stage4_2_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage4_stage4_2_Transpose_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_2_Concat_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_2_Transpose_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage4_stage4_2_Transpose_output_0_layer, 522,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_stage4_stage4_2_Transpose_output_0_chain,
  NULL, &_stage4_stage4_2_Transpose_output_0_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_WIDTH, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_EXTENSION), 
)


AI_STATIC_CONST ai_u8 _stage4_stage4_3_Slice_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage4_stage4_3_Slice_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage4_stage4_3_Slice_output_0_axes_data, _stage4_stage4_3_Slice_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage4_stage4_3_Slice_output_0_starts_data[] = { 0 };
AI_ARRAY_OBJ_DECLARE(
    _stage4_stage4_3_Slice_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage4_stage4_3_Slice_output_0_starts_data, _stage4_stage4_3_Slice_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage4_stage4_3_Slice_output_0_ends_data[] = { 96 };
AI_ARRAY_OBJ_DECLARE(
    _stage4_stage4_3_Slice_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage4_stage4_3_Slice_output_0_ends_data, _stage4_stage4_3_Slice_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage4_stage4_3_Slice_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_2_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_3_Slice_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage4_stage4_3_Slice_output_0_layer, 529,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage4_stage4_3_Slice_output_0_chain,
  NULL, &_stage4_stage4_3_Slice_output_0_layer, AI_STATIC, 
  .axes = &_stage4_stage4_3_Slice_output_0_axes, 
  .starts = &_stage4_stage4_3_Slice_output_0_starts, 
  .ends = &_stage4_stage4_3_Slice_output_0_ends, 
)


AI_STATIC_CONST ai_u8 _stage4_stage4_3_Slice_1_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _stage4_stage4_3_Slice_1_output_0_axes, AI_ARRAY_FORMAT_U8,
    _stage4_stage4_3_Slice_1_output_0_axes_data, _stage4_stage4_3_Slice_1_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage4_stage4_3_Slice_1_output_0_starts_data[] = { 96 };
AI_ARRAY_OBJ_DECLARE(
    _stage4_stage4_3_Slice_1_output_0_starts, AI_ARRAY_FORMAT_S32,
    _stage4_stage4_3_Slice_1_output_0_starts_data, _stage4_stage4_3_Slice_1_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i32 _stage4_stage4_3_Slice_1_output_0_ends_data[] = { 192 };
AI_ARRAY_OBJ_DECLARE(
    _stage4_stage4_3_Slice_1_output_0_ends, AI_ARRAY_FORMAT_S32,
    _stage4_stage4_3_Slice_1_output_0_ends_data, _stage4_stage4_3_Slice_1_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage4_stage4_3_Slice_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_2_Transpose_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_3_Slice_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage4_stage4_3_Slice_1_output_0_layer, 528,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_stage4_stage4_3_Slice_1_output_0_chain,
  NULL, &_stage4_stage4_3_Slice_1_output_0_layer, AI_STATIC, 
  .axes = &_stage4_stage4_3_Slice_1_output_0_axes, 
  .starts = &_stage4_stage4_3_Slice_1_output_0_starts, 
  .ends = &_stage4_stage4_3_Slice_1_output_0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage4_stage4_3_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stage4_stage4_3_Slice_output_0_output, (ai_tensor *)&_stage4_stage4_3_branch2_branch2_7_Relu_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_3_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage4_stage4_3_Concat_output_0_layer, 543,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_stage4_stage4_3_Concat_output_0_chain,
  NULL, &_stage4_stage4_3_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stage4_stage4_3_Transpose_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_3_Concat_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stage4_stage4_3_Transpose_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stage4_stage4_3_Transpose_output_0_layer, 549,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_stage4_stage4_3_Transpose_output_0_chain,
  NULL, &_stage4_stage4_3_Transpose_output_0_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_WIDTH, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_EXTENSION), 
)


AI_STATIC_CONST ai_float _ReduceMean_output_0_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    _ReduceMean_output_0_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    _ReduceMean_output_0_neutral_value_data, _ReduceMean_output_0_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _ReduceMean_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_ReduceMean_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _ReduceMean_output_0_layer, 558,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &_ReduceMean_output_0_chain,
  NULL, &_ReduceMean_output_0_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &_ReduceMean_output_0_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _ReduceMean_output_0_Mul_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_ReduceMean_output_0_output, (ai_tensor *)&_ReduceMean_output_0_Placeholder),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_ReduceMean_output_0_Mul_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _ReduceMean_output_0_Mul_layer, 558,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_ReduceMean_output_0_Mul_chain,
  NULL, &_ReduceMean_output_0_Mul_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  logits_QuantizeLinear_Input_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&logits_QuantizeLinear_Input_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&logits_QuantizeLinear_Input_weights, (ai_tensor *)&logits_QuantizeLinear_Input_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&logits_QuantizeLinear_Input_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  logits_QuantizeLinear_Input_layer, 561,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense_integer_SSSA_ch,
  &logits_QuantizeLinear_Input_chain,
  NULL, &logits_QuantizeLinear_Input_layer, AI_STATIC, 
)
/**  Hybrid layers declarations section  *************************************/

LITE_FORCE_INLINE void forward_lite_transpose_image_Transpose_network(_stai_network_context* net_ctx)
{
  image_output_array.data = AI_PTR(net_ctx->_inputs[0] + 0);
  image_output_array.data_start = AI_PTR(net_ctx->_inputs[0] + 0);
  image_Transpose_output_array.data = AI_PTR(net_ctx->_activations[1] + 0);
  image_Transpose_output_array.data_start = AI_PTR(net_ctx->_activations[1] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(2, 1, { image_output.data->data});
  forward_transpose((ai_layer *)&image_Transpose_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(2, 1, { image_Transpose_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_conv2d_sssa8_ch_nl_pool__conv1_conv1_2_Relu_output_0_network(_stai_network_context* net_ctx)
{
  image_Transpose_output_array.data = AI_PTR(net_ctx->_activations[1] + 0);
  image_Transpose_output_array.data_start = AI_PTR(net_ctx->_activations[1] + 0);
  _conv1_conv1_2_Relu_output_0_weights_array.data = AI_PTR(net_ctx->_weights[0] + 4);
  _conv1_conv1_2_Relu_output_0_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 4);
  _conv1_conv1_2_Relu_output_0_bias_array.data = AI_PTR(net_ctx->_weights[0] + 652);
  _conv1_conv1_2_Relu_output_0_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 652);
  _conv1_conv1_2_Relu_output_0_scratch0_array.data = AI_PTR(net_ctx->_activations[0] + 100660);
  _conv1_conv1_2_Relu_output_0_scratch0_array.data_start = AI_PTR(net_ctx->_activations[0] + 100660);
  _conv1_conv1_2_Relu_output_0_scratch1_array.data = AI_PTR(net_ctx->_activations[0] + 25600);
  _conv1_conv1_2_Relu_output_0_scratch1_array.data_start = AI_PTR(net_ctx->_activations[0] + 25600);
  _conv1_conv1_2_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 62260);
  _conv1_conv1_2_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 62260);
  _STAI_NETWORK_EVENT_NODE_START_CB(120, 1, { image_Transpose_output.data->data});
  forward_conv2d_sssa8_ch_nl_pool((ai_layer *)&_conv1_conv1_2_Relu_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(120, 1, { _conv1_conv1_2_Relu_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_concat__stage2_stage2_0_Concat_output_0_network(_stai_network_context* net_ctx)
{
  _stage2_stage2_0_branch1_branch1_4_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 43060);
  _stage2_stage2_0_branch1_branch1_4_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 43060);
  _stage2_stage2_0_branch2_branch2_7_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25936);
  _stage2_stage2_0_branch2_branch2_7_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25936);
  _stage2_stage2_0_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 52660);
  _stage2_stage2_0_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 52660);
  _STAI_NETWORK_EVENT_NODE_START_CB(138, 2, { _stage2_stage2_0_branch1_branch1_4_Relu_output_0_output.data->data,_stage2_stage2_0_branch2_branch2_7_Relu_output_0_output.data->data});
  forward_concat((ai_layer *)&_stage2_stage2_0_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(138, 1, { _stage2_stage2_0_Concat_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__stage2_stage2_0_Transpose_output_0_network(_stai_network_context* net_ctx)
{
  _stage2_stage2_0_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 52660);
  _stage2_stage2_0_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 52660);
  _stage2_stage2_0_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage2_stage2_0_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25600);
  _STAI_NETWORK_EVENT_NODE_START_CB(144, 1, { _stage2_stage2_0_Concat_output_0_output0.data->data});
  forward_transpose((ai_layer *)&_stage2_stage2_0_Transpose_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(144, 1, { _stage2_stage2_0_Transpose_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage2_stage2_1_Slice_output_0_network(_stai_network_context* net_ctx)
{
  _stage2_stage2_0_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage2_stage2_0_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage2_stage2_1_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 44800);
  _stage2_stage2_1_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 44800);
  _STAI_NETWORK_EVENT_NODE_START_CB(151, 1, { _stage2_stage2_0_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage2_stage2_1_Slice_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(151, 1, { _stage2_stage2_1_Slice_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage2_stage2_1_Slice_1_output_0_network(_stai_network_context* net_ctx)
{
  _stage2_stage2_0_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage2_stage2_0_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage2_stage2_1_Slice_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 54400);
  _stage2_stage2_1_Slice_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 54400);
  _STAI_NETWORK_EVENT_NODE_START_CB(150, 1, { _stage2_stage2_0_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage2_stage2_1_Slice_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(150, 1, { _stage2_stage2_1_Slice_1_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_concat__stage2_stage2_1_Concat_output_0_network(_stai_network_context* net_ctx)
{
  _stage2_stage2_1_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 44800);
  _stage2_stage2_1_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 44800);
  _stage2_stage2_1_branch2_branch2_7_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 54400);
  _stage2_stage2_1_branch2_branch2_7_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 54400);
  _stage2_stage2_1_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage2_stage2_1_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25600);
  _STAI_NETWORK_EVENT_NODE_START_CB(165, 2, { _stage2_stage2_1_Slice_output_0_output.data->data,_stage2_stage2_1_branch2_branch2_7_Relu_output_0_output.data->data});
  forward_concat((ai_layer *)&_stage2_stage2_1_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(165, 1, { _stage2_stage2_1_Concat_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__stage2_stage2_1_Transpose_output_0_network(_stai_network_context* net_ctx)
{
  _stage2_stage2_1_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage2_stage2_1_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage2_stage2_1_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 44800);
  _stage2_stage2_1_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 44800);
  _STAI_NETWORK_EVENT_NODE_START_CB(171, 1, { _stage2_stage2_1_Concat_output_0_output0.data->data});
  forward_transpose((ai_layer *)&_stage2_stage2_1_Transpose_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(171, 1, { _stage2_stage2_1_Transpose_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage2_stage2_2_Slice_output_0_network(_stai_network_context* net_ctx)
{
  _stage2_stage2_1_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 44800);
  _stage2_stage2_1_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 44800);
  _stage2_stage2_2_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64000);
  _stage2_stage2_2_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64000);
  _STAI_NETWORK_EVENT_NODE_START_CB(178, 1, { _stage2_stage2_1_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage2_stage2_2_Slice_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(178, 1, { _stage2_stage2_2_Slice_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage2_stage2_2_Slice_1_output_0_network(_stai_network_context* net_ctx)
{
  _stage2_stage2_1_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 44800);
  _stage2_stage2_1_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 44800);
  _stage2_stage2_2_Slice_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 92800);
  _stage2_stage2_2_Slice_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92800);
  _STAI_NETWORK_EVENT_NODE_START_CB(177, 1, { _stage2_stage2_1_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage2_stage2_2_Slice_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(177, 1, { _stage2_stage2_2_Slice_1_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_concat__stage2_stage2_2_Concat_output_0_network(_stai_network_context* net_ctx)
{
  _stage2_stage2_2_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64000);
  _stage2_stage2_2_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64000);
  _stage2_stage2_2_branch2_branch2_7_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25936);
  _stage2_stage2_2_branch2_branch2_7_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25936);
  _stage2_stage2_2_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 35536);
  _stage2_stage2_2_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 35536);
  _STAI_NETWORK_EVENT_NODE_START_CB(192, 2, { _stage2_stage2_2_Slice_output_0_output.data->data,_stage2_stage2_2_branch2_branch2_7_Relu_output_0_output.data->data});
  forward_concat((ai_layer *)&_stage2_stage2_2_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(192, 1, { _stage2_stage2_2_Concat_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__stage2_stage2_2_Transpose_output_0_network(_stai_network_context* net_ctx)
{
  _stage2_stage2_2_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 35536);
  _stage2_stage2_2_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 35536);
  _stage2_stage2_2_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 54736);
  _stage2_stage2_2_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 54736);
  _STAI_NETWORK_EVENT_NODE_START_CB(198, 1, { _stage2_stage2_2_Concat_output_0_output0.data->data});
  forward_transpose((ai_layer *)&_stage2_stage2_2_Transpose_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(198, 1, { _stage2_stage2_2_Transpose_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage2_stage2_3_Slice_output_0_network(_stai_network_context* net_ctx)
{
  _stage2_stage2_2_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 54736);
  _stage2_stage2_2_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 54736);
  _stage2_stage2_3_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 73936);
  _stage2_stage2_3_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 73936);
  _STAI_NETWORK_EVENT_NODE_START_CB(205, 1, { _stage2_stage2_2_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage2_stage2_3_Slice_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(205, 1, { _stage2_stage2_3_Slice_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage2_stage2_3_Slice_1_output_0_network(_stai_network_context* net_ctx)
{
  _stage2_stage2_2_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 54736);
  _stage2_stage2_2_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 54736);
  _stage2_stage2_3_Slice_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 92800);
  _stage2_stage2_3_Slice_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92800);
  _STAI_NETWORK_EVENT_NODE_START_CB(204, 1, { _stage2_stage2_2_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage2_stage2_3_Slice_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(204, 1, { _stage2_stage2_3_Slice_1_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_concat__stage2_stage2_3_Concat_output_0_network(_stai_network_context* net_ctx)
{
  _stage2_stage2_3_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 73936);
  _stage2_stage2_3_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 73936);
  _stage2_stage2_3_branch2_branch2_7_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25936);
  _stage2_stage2_3_branch2_branch2_7_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25936);
  _stage2_stage2_3_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 35536);
  _stage2_stage2_3_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 35536);
  _STAI_NETWORK_EVENT_NODE_START_CB(219, 2, { _stage2_stage2_3_Slice_output_0_output.data->data,_stage2_stage2_3_branch2_branch2_7_Relu_output_0_output.data->data});
  forward_concat((ai_layer *)&_stage2_stage2_3_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(219, 1, { _stage2_stage2_3_Concat_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__stage2_stage2_3_Transpose_output_0_network(_stai_network_context* net_ctx)
{
  _stage2_stage2_3_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 35536);
  _stage2_stage2_3_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 35536);
  _stage2_stage2_3_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 83200);
  _stage2_stage2_3_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 83200);
  _STAI_NETWORK_EVENT_NODE_START_CB(225, 1, { _stage2_stage2_3_Concat_output_0_output0.data->data});
  forward_transpose((ai_layer *)&_stage2_stage2_3_Transpose_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(225, 1, { _stage2_stage2_3_Transpose_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_concat__stage3_stage3_0_Concat_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_0_branch1_branch1_4_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 73600);
  _stage3_stage3_0_branch1_branch1_4_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 73600);
  _stage3_stage3_0_branch2_branch2_7_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 26272);
  _stage3_stage3_0_branch2_branch2_7_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 26272);
  _stage3_stage3_0_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_0_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31072);
  _STAI_NETWORK_EVENT_NODE_START_CB(246, 2, { _stage3_stage3_0_branch1_branch1_4_Relu_output_0_output.data->data,_stage3_stage3_0_branch2_branch2_7_Relu_output_0_output.data->data});
  forward_concat((ai_layer *)&_stage3_stage3_0_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(246, 1, { _stage3_stage3_0_Concat_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__stage3_stage3_0_Transpose_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_0_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_0_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_0_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_0_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _STAI_NETWORK_EVENT_NODE_START_CB(252, 1, { _stage3_stage3_0_Concat_output_0_output0.data->data});
  forward_transpose((ai_layer *)&_stage3_stage3_0_Transpose_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(252, 1, { _stage3_stage3_0_Transpose_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage3_stage3_1_Slice_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_0_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_0_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_1_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_1_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 50272);
  _STAI_NETWORK_EVENT_NODE_START_CB(259, 1, { _stage3_stage3_0_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage3_stage3_1_Slice_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(259, 1, { _stage3_stage3_1_Slice_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage3_stage3_1_Slice_1_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_0_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_0_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_1_Slice_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 97600);
  _stage3_stage3_1_Slice_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 97600);
  _STAI_NETWORK_EVENT_NODE_START_CB(258, 1, { _stage3_stage3_0_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage3_stage3_1_Slice_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(258, 1, { _stage3_stage3_1_Slice_1_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_concat__stage3_stage3_1_Concat_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_1_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_1_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_1_branch2_branch2_7_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 26272);
  _stage3_stage3_1_branch2_branch2_7_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 26272);
  _stage3_stage3_1_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_1_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31072);
  _STAI_NETWORK_EVENT_NODE_START_CB(273, 2, { _stage3_stage3_1_Slice_output_0_output.data->data,_stage3_stage3_1_branch2_branch2_7_Relu_output_0_output.data->data});
  forward_concat((ai_layer *)&_stage3_stage3_1_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(273, 1, { _stage3_stage3_1_Concat_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__stage3_stage3_1_Transpose_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_1_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_1_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_1_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_1_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _STAI_NETWORK_EVENT_NODE_START_CB(279, 1, { _stage3_stage3_1_Concat_output_0_output0.data->data});
  forward_transpose((ai_layer *)&_stage3_stage3_1_Transpose_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(279, 1, { _stage3_stage3_1_Transpose_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage3_stage3_2_Slice_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_1_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_1_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_2_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_2_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 50272);
  _STAI_NETWORK_EVENT_NODE_START_CB(286, 1, { _stage3_stage3_1_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage3_stage3_2_Slice_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(286, 1, { _stage3_stage3_2_Slice_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage3_stage3_2_Slice_1_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_1_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_1_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_2_Slice_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 97600);
  _stage3_stage3_2_Slice_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 97600);
  _STAI_NETWORK_EVENT_NODE_START_CB(285, 1, { _stage3_stage3_1_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage3_stage3_2_Slice_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(285, 1, { _stage3_stage3_2_Slice_1_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_concat__stage3_stage3_2_Concat_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_2_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_2_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_2_branch2_branch2_7_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 26272);
  _stage3_stage3_2_branch2_branch2_7_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 26272);
  _stage3_stage3_2_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_2_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31072);
  _STAI_NETWORK_EVENT_NODE_START_CB(300, 2, { _stage3_stage3_2_Slice_output_0_output.data->data,_stage3_stage3_2_branch2_branch2_7_Relu_output_0_output.data->data});
  forward_concat((ai_layer *)&_stage3_stage3_2_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(300, 1, { _stage3_stage3_2_Concat_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__stage3_stage3_2_Transpose_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_2_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_2_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_2_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_2_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _STAI_NETWORK_EVENT_NODE_START_CB(306, 1, { _stage3_stage3_2_Concat_output_0_output0.data->data});
  forward_transpose((ai_layer *)&_stage3_stage3_2_Transpose_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(306, 1, { _stage3_stage3_2_Transpose_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage3_stage3_3_Slice_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_2_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_2_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_3_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_3_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 50272);
  _STAI_NETWORK_EVENT_NODE_START_CB(313, 1, { _stage3_stage3_2_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage3_stage3_3_Slice_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(313, 1, { _stage3_stage3_3_Slice_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage3_stage3_3_Slice_1_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_2_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_2_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_3_Slice_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 97600);
  _stage3_stage3_3_Slice_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 97600);
  _STAI_NETWORK_EVENT_NODE_START_CB(312, 1, { _stage3_stage3_2_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage3_stage3_3_Slice_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(312, 1, { _stage3_stage3_3_Slice_1_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_concat__stage3_stage3_3_Concat_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_3_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_3_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_3_branch2_branch2_7_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 26272);
  _stage3_stage3_3_branch2_branch2_7_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 26272);
  _stage3_stage3_3_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_3_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31072);
  _STAI_NETWORK_EVENT_NODE_START_CB(327, 2, { _stage3_stage3_3_Slice_output_0_output.data->data,_stage3_stage3_3_branch2_branch2_7_Relu_output_0_output.data->data});
  forward_concat((ai_layer *)&_stage3_stage3_3_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(327, 1, { _stage3_stage3_3_Concat_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__stage3_stage3_3_Transpose_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_3_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_3_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_3_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_3_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _STAI_NETWORK_EVENT_NODE_START_CB(333, 1, { _stage3_stage3_3_Concat_output_0_output0.data->data});
  forward_transpose((ai_layer *)&_stage3_stage3_3_Transpose_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(333, 1, { _stage3_stage3_3_Transpose_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage3_stage3_4_Slice_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_3_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_3_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_4_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_4_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 50272);
  _STAI_NETWORK_EVENT_NODE_START_CB(340, 1, { _stage3_stage3_3_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage3_stage3_4_Slice_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(340, 1, { _stage3_stage3_4_Slice_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage3_stage3_4_Slice_1_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_3_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_3_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_4_Slice_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 97600);
  _stage3_stage3_4_Slice_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 97600);
  _STAI_NETWORK_EVENT_NODE_START_CB(339, 1, { _stage3_stage3_3_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage3_stage3_4_Slice_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(339, 1, { _stage3_stage3_4_Slice_1_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_concat__stage3_stage3_4_Concat_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_4_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_4_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_4_branch2_branch2_7_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 26272);
  _stage3_stage3_4_branch2_branch2_7_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 26272);
  _stage3_stage3_4_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_4_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31072);
  _STAI_NETWORK_EVENT_NODE_START_CB(354, 2, { _stage3_stage3_4_Slice_output_0_output.data->data,_stage3_stage3_4_branch2_branch2_7_Relu_output_0_output.data->data});
  forward_concat((ai_layer *)&_stage3_stage3_4_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(354, 1, { _stage3_stage3_4_Concat_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__stage3_stage3_4_Transpose_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_4_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_4_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_4_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_4_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _STAI_NETWORK_EVENT_NODE_START_CB(360, 1, { _stage3_stage3_4_Concat_output_0_output0.data->data});
  forward_transpose((ai_layer *)&_stage3_stage3_4_Transpose_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(360, 1, { _stage3_stage3_4_Transpose_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage3_stage3_5_Slice_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_4_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_4_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_5_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_5_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 50272);
  _STAI_NETWORK_EVENT_NODE_START_CB(367, 1, { _stage3_stage3_4_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage3_stage3_5_Slice_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(367, 1, { _stage3_stage3_5_Slice_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage3_stage3_5_Slice_1_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_4_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_4_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_5_Slice_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 97600);
  _stage3_stage3_5_Slice_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 97600);
  _STAI_NETWORK_EVENT_NODE_START_CB(366, 1, { _stage3_stage3_4_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage3_stage3_5_Slice_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(366, 1, { _stage3_stage3_5_Slice_1_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_concat__stage3_stage3_5_Concat_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_5_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_5_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_5_branch2_branch2_7_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 26272);
  _stage3_stage3_5_branch2_branch2_7_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 26272);
  _stage3_stage3_5_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_5_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31072);
  _STAI_NETWORK_EVENT_NODE_START_CB(381, 2, { _stage3_stage3_5_Slice_output_0_output.data->data,_stage3_stage3_5_branch2_branch2_7_Relu_output_0_output.data->data});
  forward_concat((ai_layer *)&_stage3_stage3_5_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(381, 1, { _stage3_stage3_5_Concat_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__stage3_stage3_5_Transpose_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_5_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_5_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_5_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_5_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _STAI_NETWORK_EVENT_NODE_START_CB(387, 1, { _stage3_stage3_5_Concat_output_0_output0.data->data});
  forward_transpose((ai_layer *)&_stage3_stage3_5_Transpose_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(387, 1, { _stage3_stage3_5_Transpose_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage3_stage3_6_Slice_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_5_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_5_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_6_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_6_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 50272);
  _STAI_NETWORK_EVENT_NODE_START_CB(394, 1, { _stage3_stage3_5_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage3_stage3_6_Slice_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(394, 1, { _stage3_stage3_6_Slice_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage3_stage3_6_Slice_1_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_5_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_5_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_6_Slice_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 97600);
  _stage3_stage3_6_Slice_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 97600);
  _STAI_NETWORK_EVENT_NODE_START_CB(393, 1, { _stage3_stage3_5_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage3_stage3_6_Slice_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(393, 1, { _stage3_stage3_6_Slice_1_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_concat__stage3_stage3_6_Concat_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_6_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_6_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_6_branch2_branch2_7_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 26272);
  _stage3_stage3_6_branch2_branch2_7_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 26272);
  _stage3_stage3_6_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_6_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31072);
  _STAI_NETWORK_EVENT_NODE_START_CB(408, 2, { _stage3_stage3_6_Slice_output_0_output.data->data,_stage3_stage3_6_branch2_branch2_7_Relu_output_0_output.data->data});
  forward_concat((ai_layer *)&_stage3_stage3_6_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(408, 1, { _stage3_stage3_6_Concat_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__stage3_stage3_6_Transpose_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_6_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_6_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31072);
  _stage3_stage3_6_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_6_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _STAI_NETWORK_EVENT_NODE_START_CB(414, 1, { _stage3_stage3_6_Concat_output_0_output0.data->data});
  forward_transpose((ai_layer *)&_stage3_stage3_6_Transpose_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(414, 1, { _stage3_stage3_6_Transpose_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage3_stage3_7_Slice_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_6_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_6_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_7_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_7_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 50272);
  _STAI_NETWORK_EVENT_NODE_START_CB(421, 1, { _stage3_stage3_6_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage3_stage3_7_Slice_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(421, 1, { _stage3_stage3_7_Slice_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage3_stage3_7_Slice_1_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_6_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_6_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 40672);
  _stage3_stage3_7_Slice_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 97600);
  _stage3_stage3_7_Slice_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 97600);
  _STAI_NETWORK_EVENT_NODE_START_CB(420, 1, { _stage3_stage3_6_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage3_stage3_7_Slice_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(420, 1, { _stage3_stage3_7_Slice_1_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_concat__stage3_stage3_7_Concat_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_7_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_7_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 50272);
  _stage3_stage3_7_branch2_branch2_7_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 26272);
  _stage3_stage3_7_branch2_branch2_7_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 26272);
  _stage3_stage3_7_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 55072);
  _stage3_stage3_7_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 55072);
  _STAI_NETWORK_EVENT_NODE_START_CB(435, 2, { _stage3_stage3_7_Slice_output_0_output.data->data,_stage3_stage3_7_branch2_branch2_7_Relu_output_0_output.data->data});
  forward_concat((ai_layer *)&_stage3_stage3_7_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(435, 1, { _stage3_stage3_7_Concat_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__stage3_stage3_7_Transpose_output_0_network(_stai_network_context* net_ctx)
{
  _stage3_stage3_7_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 55072);
  _stage3_stage3_7_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 55072);
  _stage3_stage3_7_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 45472);
  _stage3_stage3_7_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 45472);
  _STAI_NETWORK_EVENT_NODE_START_CB(441, 1, { _stage3_stage3_7_Concat_output_0_output0.data->data});
  forward_transpose((ai_layer *)&_stage3_stage3_7_Transpose_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(441, 1, { _stage3_stage3_7_Transpose_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_concat__stage4_stage4_0_Concat_output_0_network(_stai_network_context* net_ctx)
{
  _stage4_stage4_0_branch1_branch1_4_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31556);
  _stage4_stage4_0_branch1_branch1_4_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31556);
  _stage4_stage4_0_branch2_branch2_7_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 26944);
  _stage4_stage4_0_branch2_branch2_7_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 26944);
  _stage4_stage4_0_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 33956);
  _stage4_stage4_0_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 33956);
  _STAI_NETWORK_EVENT_NODE_START_CB(462, 2, { _stage4_stage4_0_branch1_branch1_4_Relu_output_0_output.data->data,_stage4_stage4_0_branch2_branch2_7_Relu_output_0_output.data->data});
  forward_concat((ai_layer *)&_stage4_stage4_0_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(462, 1, { _stage4_stage4_0_Concat_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__stage4_stage4_0_Transpose_output_0_network(_stai_network_context* net_ctx)
{
  _stage4_stage4_0_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 33956);
  _stage4_stage4_0_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 33956);
  _stage4_stage4_0_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage4_stage4_0_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25600);
  _STAI_NETWORK_EVENT_NODE_START_CB(468, 1, { _stage4_stage4_0_Concat_output_0_output0.data->data});
  forward_transpose((ai_layer *)&_stage4_stage4_0_Transpose_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(468, 1, { _stage4_stage4_0_Transpose_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage4_stage4_1_Slice_output_0_network(_stai_network_context* net_ctx)
{
  _stage4_stage4_0_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage4_stage4_0_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage4_stage4_1_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 30400);
  _stage4_stage4_1_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30400);
  _STAI_NETWORK_EVENT_NODE_START_CB(475, 1, { _stage4_stage4_0_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage4_stage4_1_Slice_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(475, 1, { _stage4_stage4_1_Slice_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage4_stage4_1_Slice_1_output_0_network(_stai_network_context* net_ctx)
{
  _stage4_stage4_0_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage4_stage4_0_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage4_stage4_1_Slice_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 32800);
  _stage4_stage4_1_Slice_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 32800);
  _STAI_NETWORK_EVENT_NODE_START_CB(474, 1, { _stage4_stage4_0_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage4_stage4_1_Slice_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(474, 1, { _stage4_stage4_1_Slice_1_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_concat__stage4_stage4_1_Concat_output_0_network(_stai_network_context* net_ctx)
{
  _stage4_stage4_1_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 30400);
  _stage4_stage4_1_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30400);
  _stage4_stage4_1_branch2_branch2_7_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 26944);
  _stage4_stage4_1_branch2_branch2_7_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 26944);
  _stage4_stage4_1_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 32800);
  _stage4_stage4_1_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 32800);
  _STAI_NETWORK_EVENT_NODE_START_CB(489, 2, { _stage4_stage4_1_Slice_output_0_output.data->data,_stage4_stage4_1_branch2_branch2_7_Relu_output_0_output.data->data});
  forward_concat((ai_layer *)&_stage4_stage4_1_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(489, 1, { _stage4_stage4_1_Concat_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__stage4_stage4_1_Transpose_output_0_network(_stai_network_context* net_ctx)
{
  _stage4_stage4_1_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 32800);
  _stage4_stage4_1_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 32800);
  _stage4_stage4_1_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage4_stage4_1_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25600);
  _STAI_NETWORK_EVENT_NODE_START_CB(495, 1, { _stage4_stage4_1_Concat_output_0_output0.data->data});
  forward_transpose((ai_layer *)&_stage4_stage4_1_Transpose_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(495, 1, { _stage4_stage4_1_Transpose_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage4_stage4_2_Slice_output_0_network(_stai_network_context* net_ctx)
{
  _stage4_stage4_1_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage4_stage4_1_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage4_stage4_2_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 30400);
  _stage4_stage4_2_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30400);
  _STAI_NETWORK_EVENT_NODE_START_CB(502, 1, { _stage4_stage4_1_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage4_stage4_2_Slice_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(502, 1, { _stage4_stage4_2_Slice_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage4_stage4_2_Slice_1_output_0_network(_stai_network_context* net_ctx)
{
  _stage4_stage4_1_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage4_stage4_1_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage4_stage4_2_Slice_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 32800);
  _stage4_stage4_2_Slice_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 32800);
  _STAI_NETWORK_EVENT_NODE_START_CB(501, 1, { _stage4_stage4_1_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage4_stage4_2_Slice_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(501, 1, { _stage4_stage4_2_Slice_1_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_concat__stage4_stage4_2_Concat_output_0_network(_stai_network_context* net_ctx)
{
  _stage4_stage4_2_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 30400);
  _stage4_stage4_2_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30400);
  _stage4_stage4_2_branch2_branch2_7_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 26944);
  _stage4_stage4_2_branch2_branch2_7_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 26944);
  _stage4_stage4_2_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 32800);
  _stage4_stage4_2_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 32800);
  _STAI_NETWORK_EVENT_NODE_START_CB(516, 2, { _stage4_stage4_2_Slice_output_0_output.data->data,_stage4_stage4_2_branch2_branch2_7_Relu_output_0_output.data->data});
  forward_concat((ai_layer *)&_stage4_stage4_2_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(516, 1, { _stage4_stage4_2_Concat_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__stage4_stage4_2_Transpose_output_0_network(_stai_network_context* net_ctx)
{
  _stage4_stage4_2_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 32800);
  _stage4_stage4_2_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 32800);
  _stage4_stage4_2_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage4_stage4_2_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25600);
  _STAI_NETWORK_EVENT_NODE_START_CB(522, 1, { _stage4_stage4_2_Concat_output_0_output0.data->data});
  forward_transpose((ai_layer *)&_stage4_stage4_2_Transpose_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(522, 1, { _stage4_stage4_2_Transpose_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage4_stage4_3_Slice_output_0_network(_stai_network_context* net_ctx)
{
  _stage4_stage4_2_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage4_stage4_2_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage4_stage4_3_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 30400);
  _stage4_stage4_3_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30400);
  _STAI_NETWORK_EVENT_NODE_START_CB(529, 1, { _stage4_stage4_2_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage4_stage4_3_Slice_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(529, 1, { _stage4_stage4_3_Slice_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_slice__stage4_stage4_3_Slice_1_output_0_network(_stai_network_context* net_ctx)
{
  _stage4_stage4_2_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage4_stage4_2_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25600);
  _stage4_stage4_3_Slice_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 32800);
  _stage4_stage4_3_Slice_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 32800);
  _STAI_NETWORK_EVENT_NODE_START_CB(528, 1, { _stage4_stage4_2_Transpose_output_0_output0.data->data});
  forward_slice((ai_layer *)&_stage4_stage4_3_Slice_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(528, 1, { _stage4_stage4_3_Slice_1_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_concat__stage4_stage4_3_Concat_output_0_network(_stai_network_context* net_ctx)
{
  _stage4_stage4_3_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 30400);
  _stage4_stage4_3_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30400);
  _stage4_stage4_3_branch2_branch2_7_Relu_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 35876);
  _stage4_stage4_3_branch2_branch2_7_Relu_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 35876);
  _stage4_stage4_3_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 97600);
  _stage4_stage4_3_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 97600);
  _STAI_NETWORK_EVENT_NODE_START_CB(543, 2, { _stage4_stage4_3_Slice_output_0_output.data->data,_stage4_stage4_3_branch2_branch2_7_Relu_output_0_output.data->data});
  forward_concat((ai_layer *)&_stage4_stage4_3_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(543, 1, { _stage4_stage4_3_Concat_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__stage4_stage4_3_Transpose_output_0_network(_stai_network_context* net_ctx)
{
  _stage4_stage4_3_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 97600);
  _stage4_stage4_3_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 97600);
  _stage4_stage4_3_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[1] + 0);
  _stage4_stage4_3_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[1] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(549, 1, { _stage4_stage4_3_Concat_output_0_output0.data->data});
  forward_transpose((ai_layer *)&_stage4_stage4_3_Transpose_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(549, 1, { _stage4_stage4_3_Transpose_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_reduce__ReduceMean_output_0_network(_stai_network_context* net_ctx)
{
  _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _ReduceMean_output_0_output_array.data = AI_PTR(net_ctx->_activations[1] + 0);
  _ReduceMean_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[1] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(558, 1, { _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_output.data->data});
  forward_reduce((ai_layer *)&_ReduceMean_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(558, 1, { _ReduceMean_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__ReduceMean_output_0_Mul_network(_stai_network_context* net_ctx)
{
  _ReduceMean_output_0_output_array.data = AI_PTR(net_ctx->_activations[1] + 0);
  _ReduceMean_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[1] + 0);
  _ReduceMean_output_0_Placeholder_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  _ReduceMean_output_0_Placeholder_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  _ReduceMean_output_0_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _ReduceMean_output_0_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(558, 2, { _ReduceMean_output_0_output.data->data,_ReduceMean_output_0_Placeholder.data->data});
  forward_eltwise((ai_layer *)&_ReduceMean_output_0_Mul_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(558, 1, { _ReduceMean_output_0_Mul_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_dense_integer_SSSA_ch_logits_QuantizeLinear_Input_network(_stai_network_context* net_ctx)
{
  _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 4096);
  _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 4096);
  logits_QuantizeLinear_Input_weights_array.data = AI_PTR(net_ctx->_weights[0] + 349748);
  logits_QuantizeLinear_Input_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 349748);
  logits_QuantizeLinear_Input_bias_array.data = AI_PTR(net_ctx->_weights[0] + 352820);
  logits_QuantizeLinear_Input_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 352820);
  logits_QuantizeLinear_Input_scratch0_array.data = AI_PTR(net_ctx->_activations[1] + 0);
  logits_QuantizeLinear_Input_scratch0_array.data_start = AI_PTR(net_ctx->_activations[1] + 0);
  logits_QuantizeLinear_Input_output_array.data = AI_PTR(net_ctx->_outputs[0] + 0);
  logits_QuantizeLinear_Input_output_array.data_start = AI_PTR(net_ctx->_outputs[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(561, 1, { _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_output.data->data});
  forward_dense_integer_SSSA_ch((ai_layer *)&logits_QuantizeLinear_Input_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(561, 1, { logits_QuantizeLinear_Input_output.data->data});
}

/*****************************************************************************/




static const ai_u16 _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_in_0_shape_w_const_u16 = 40;
static const ai_u16 _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_in_0_shape_h_const_u16 = 40;
static const ai_u16 _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage2_stage2_0_branch1_branch1_0_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage2_stage2_0_branch1_branch1_0_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage2_stage2_0_branch1_branch1_0_Conv_output_0_l_stride_1_const_u16 = 2;
static const ai_u16 _stage2_stage2_0_branch1_branch1_0_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_i8 _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_out_0_fmt_zero_const_s8 = 3;
static const ai_float _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.03621755167841911f;
static const ai_float _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.05068863928318024f;
static const ai_float _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.007611373905092478f, 0.006357572041451931f, 0.0012056350242346525f, 0.008121411316096783f, 0.002737338887527585f, 0.012157696299254894f, 0.00018533962429501116f, 0.019366757944226265f, 0.0014433938777074218f, 0.009026075713336468f, 0.022087767720222473f, 0.0018552758265286684f, 0.009797992184758186f, 0.0017190303187817335f, 0.008164022117853165f, 0.009419750422239304f, 0.0006166623788885772f, 0.0024950362276285887f, 0.002948698354884982f, 0.0013732252409681678f, 0.0016952126752585173f, 0.0012204877566546202f, 0.004557241685688496f, 0.0018897184636443853f);
static const ai_u16 _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_out_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_out_0_shape_h_const_u16 = 20;

static const ai_u16 _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage2_stage2_0_branch1_branch1_4_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_0_branch1_branch1_4_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_out_0_shape_ch_const_u16 = 24;
static const ai_i8 _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_in_0_fmt_zero_const_s8 = 3;
static const ai_i8 _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.05068863928318024f;
static const ai_float _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.00990772433578968f;
static const ai_float _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002937697572633624f, 0.0021113543771207333f, 0.0015000695129856467f, 0.0025334095116704702f, 0.0037037860602140427f, 0.0039376127533614635f, 0.004801934584975243f, 0.004323695320636034f, 0.009554442018270493f, 0.0028813090175390244f, 0.009256711229681969f, 0.005332565400749445f, 0.003818532219156623f, 0.0021929179783910513f, 0.004671763628721237f, 0.0057742358185350895f, 0.003279212862253189f, 0.0027981854509562254f, 0.0020901660900563f, 0.003965574782341719f, 0.0034820938017219305f, 0.0027849215548485518f, 0.0036711820866912603f, 0.0055951946415007114f);
static const ai_layer_format_type _stage2_stage2_0_branch1_branch1_4_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 40;
static const ai_u16 _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 40;
static const ai_u16 _stage2_stage2_0_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_0_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 24;
static const ai_i8 _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.03621755167841911f;
static const ai_float _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.011892857030034065f;
static const ai_float _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.004083888605237007f, 0.004405612125992775f, 0.0031899132300168276f, 0.004816368222236633f, 0.004290937911719084f, 0.005026726983487606f, 0.005140349734574556f, 0.0035346488002687693f, 2.840795332303969e-07f, 0.0012253460008651018f, 0.002634615171700716f, 0.002061846200376749f, 2.652651573953335e-07f, 0.0036237926688045263f, 0.0023643053136765957f, 0.004035459831357002f, 0.006430847570300102f, 1.8580054472749907e-07f, 0.004562159068882465f, 0.0028772682417184114f, 0.00605853833258152f, 0.004238627851009369f, 0.003220407059416175f, 0.005350287538021803f);
static const ai_layer_format_type _stage2_stage2_0_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16 = 40;
static const ai_u16 _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16 = 40;
static const ai_u16 _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage2_stage2_0_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage2_stage2_0_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage2_stage2_0_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16 = 2;
static const ai_u16 _stage2_stage2_0_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_i8 _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = -26;
static const ai_float _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.011892857030034065f;
static const ai_float _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.04356582090258598f;
static const ai_float _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.008415557444095612f, 0.006314311642199755f, 0.005048438441008329f, 0.004989912733435631f, 0.0056830295361578465f, 0.022983476519584656f, 0.007343412842601538f, 0.006273173727095127f, 0.01547498069703579f, 0.05403276905417442f, 0.007133120205253363f, 0.003913010936230421f, 0.017901035025715828f, 0.004895209334790707f, 0.004889364819973707f, 0.011777227744460106f, 0.018760573118925095f, 0.006115236785262823f, 0.004721976816654205f, 0.0036020891275256872f, 0.02134345844388008f, 0.005085899960249662f, 0.011410078033804893f, 0.01093143317848444f);
static const ai_u16 _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 20;

static const ai_u16 _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage2_stage2_0_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_0_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 24;
static const ai_i8 _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = -26;
static const ai_i8 _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.04356582090258598f;
static const ai_float _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.01578989252448082f;
static const ai_float _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0020822593942284584f, 0.005425082053989172f, 0.0010140507947653532f, 0.0027978301513940096f, 0.0017552602803334594f, 0.001994805410504341f, 0.0017774270381778479f, 0.004112243186682463f, 0.0022973327431827784f, 0.004079929552972317f, 6.13963138107465e-08f, 0.004408746026456356f, 0.004909820854663849f, 0.0038059146609157324f, 0.003583339974284172f, 0.0035757855512201786f, 0.0029945815913379192f, 0.0043363189324736595f, 0.0018463564338162541f, 0.00363331101834774f, 0.0026478134095668793f, 0.0023044156841933727f, 0.0015013492666184902f, 0.0041500721126794815f);
static const ai_layer_format_type _stage2_stage2_0_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage2_stage2_1_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_1_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 24;
static const ai_i8 _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.01578989252448082f;
static const ai_float _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.008982603438198566f;
static const ai_float _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0029810024425387383f, 0.0025988842826336622f, 0.004904738627374172f, 0.0028113839216530323f, 0.0062275659292936325f, 0.00712583540007472f, 0.004196982830762863f, 0.00625971844419837f, 0.004154111724346876f, 0.005406666547060013f, 0.0047213369980454445f, 0.005186229478567839f, 0.006377300713211298f, 0.006364032160490751f, 0.005566807929426432f, 0.003723472123965621f, 0.003791544819250703f, 0.0030801210086792707f, 0.005343703553080559f, 0.004226251970976591f, 0.0038847217801958323f, 0.004005268216133118f, 0.0031856358982622623f, 0.008275403641164303f);
static const ai_layer_format_type _stage2_stage2_1_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage2_stage2_1_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage2_stage2_1_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage2_stage2_1_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_1_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i8 _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = 0;
static const ai_float _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.008982603438198566f;
static const ai_float _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.02497435174882412f;
static const ai_float _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.00874469056725502f, 0.01802307553589344f, 0.026589538902044296f, 0.018604464828968048f, 0.004474760498851538f, 0.010199978947639465f, 0.01698361150920391f, 0.006241271737962961f, 0.005523052532225847f, 0.010071482509374619f, 0.0032894329633563757f, 0.019693298265337944f, 0.05778619647026062f, 0.023336347192525864f, 0.020548734813928604f, 0.020222501829266548f, 0.027071820572018623f, 0.023014532402157784f, 0.01460797619074583f, 0.018244491890072823f, 0.005308298859745264f, 0.013298147358000278f, 0.03680305927991867f, 0.01797519251704216f);
static const ai_u16 _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 20;

static const ai_u16 _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage2_stage2_1_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_1_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 24;
static const ai_i8 _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = 0;
static const ai_i8 _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.02497435174882412f;
static const ai_float _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.009181622415781021f;
static const ai_float _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0030692981090396643f, 0.0032174286898225546f, 0.002504615345969796f, 0.003203359432518482f, 0.005056548863649368f, 0.00500876922160387f, 0.0019069716800004244f, 0.002486873883754015f, 0.0031368585769087076f, 0.0022959287744015455f, 0.0031721240375190973f, 0.002748636994510889f, 0.003161993343383074f, 0.0036970535293221474f, 0.0037135013844817877f, 0.0027832204941660166f, 0.003090386278927326f, 0.003435168182477355f, 0.003924046643078327f, 0.0052546290680766106f, 0.003389356890693307f, 0.0029363646171987057f, 0.003035354893654585f, 0.004102000035345554f);
static const ai_layer_format_type _stage2_stage2_1_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage2_stage2_2_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_2_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 24;
static const ai_i8 _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.01578989252448082f;
static const ai_float _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.012213435024023056f;
static const ai_float _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0033426396548748016f, 0.0059577301144599915f, 0.00016896025044843554f, 0.005972791463136673f, 0.0021761299576610327f, 0.007335244677960873f, 0.0011936263181269169f, 0.003249155590310693f, 0.004017211031168699f, 0.0036416943185031414f, 0.003462685039266944f, 0.004849631804972887f, 0.003638037946075201f, 0.003063047304749489f, 0.006298464257270098f, 0.0011304296785965562f, 0.002125396393239498f, 0.0017987224273383617f, 0.003945059608668089f, 0.0031023903284221888f, 0.004924213048070669f, 0.0029606586322188377f, 0.006989913992583752f, 0.001521555008366704f);
static const ai_layer_format_type _stage2_stage2_2_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage2_stage2_2_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage2_stage2_2_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage2_stage2_2_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_2_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i8 _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = -3;
static const ai_float _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.012213435024023056f;
static const ai_float _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.03110096976161003f;
static const ai_float _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.027142148464918137f, 0.032308176159858704f, 0.6211004257202148f, 0.012113132514059544f, 0.005836044438183308f, 0.0084142554551363f, 0.030933791771531105f, 0.006220234092324972f, 0.006758270785212517f, 0.03617994114756584f, 0.003597938921302557f, 0.014289502054452896f, 0.025868797674775124f, 0.022643735632300377f, 0.00904221273958683f, 0.029680917039513588f, 0.019137127324938774f, 0.009931729175150394f, 0.013038760982453823f, 0.032933786511421204f, 0.0072355978190898895f, 0.009008348919451237f, 0.006355674006044865f, 0.04678158089518547f);
static const ai_u16 _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 20;

static const ai_u16 _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage2_stage2_2_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_2_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 24;
static const ai_i8 _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = -3;
static const ai_i8 _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.03110096976161003f;
static const ai_float _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.012105477973818779f;
static const ai_float _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0027588859666138887f, 0.005973904859274626f, 0.0029339813627302647f, 0.005658704321831465f, 0.003626823890954256f, 0.003360666800290346f, 0.0036626840010285378f, 0.002014569938182831f, 0.0019295968813821673f, 0.00574119808152318f, 0.0012878513662144542f, 0.00364455278031528f, 0.003255899529904127f, 0.001678626867942512f, 0.0023605728056281805f, 0.005006557796150446f, 0.0021193954162299633f, 0.0038081605453044176f, 6.903924543166795e-08f, 0.002509793732315302f, 0.004484378732740879f, 0.003116216976195574f, 1.5851202306294e-07f, 0.0039750756695866585f);
static const ai_layer_format_type _stage2_stage2_2_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage2_stage2_3_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_3_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 24;
static const ai_i8 _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.01578989252448082f;
static const ai_float _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.009597525000572205f;
static const ai_float _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0097679253667593f, 0.004900273866951466f, 0.005192545708268881f, 0.0020941239781677723f, 0.0071419961750507355f, 0.0018996476428583264f, 0.005498412065207958f, 0.001701160566881299f, 0.0050899251364171505f, 0.0026723570190370083f, 0.004689064808189869f, 0.0044206054881215096f, 0.0060916682705283165f, 0.007334618363529444f, 0.00128743436653167f, 0.00360168912447989f, 0.0024504349566996098f, 0.003702603280544281f, 0.003016388276591897f, 0.0030402399133890867f, 0.006905706133693457f, 0.0026843156665563583f, 0.001962764887139201f, 0.01053890585899353f);
static const ai_layer_format_type _stage2_stage2_3_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage2_stage2_3_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage2_stage2_3_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage2_stage2_3_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_3_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i8 _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = 17;
static const ai_float _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.009597525000572205f;
static const ai_float _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.02939709834754467f;
static const ai_float _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.006350931245833635f, 0.010789832100272179f, 0.025916146114468575f, 0.02328023687005043f, 0.004330574534833431f, 0.016944628208875656f, 0.016680724918842316f, 0.016973068937659264f, 0.020825136452913284f, 0.00853634625673294f, 0.011788087897002697f, 0.01350926049053669f, 0.024478178471326828f, 0.007417671848088503f, 0.030208775773644447f, 0.010638189502060413f, 0.022288359701633453f, 0.023165132850408554f, 0.022948315367102623f, 0.022620949894189835f, 0.0099765220656991f, 0.026048559695482254f, 0.03115682676434517f, 0.0044027878902852535f);
static const ai_u16 _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 20;

static const ai_u16 _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage2_stage2_3_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_3_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 24;
static const ai_i8 _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = 17;
static const ai_i8 _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.02939709834754467f;
static const ai_float _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.015372813679277897f;
static const ai_float _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.005663324613124132f, 0.004773805849254131f, 0.0035489159636199474f, 0.008267597295343876f, 0.0030271399300545454f, 0.004123888444155455f, 0.003391010221093893f, 0.003201848827302456f, 0.005199178587645292f, 0.0035648467019200325f, 0.005403245333582163f, 0.0029787432868033648f, 0.007397054694592953f, 0.005867088213562965f, 0.004968133755028248f, 0.0036685431841760874f, 0.003997613210231066f, 0.0060757966712117195f, 0.0045748562552034855f, 0.003998773638159037f, 0.0053283642046153545f, 0.003968444652855396f, 0.0034660217352211475f, 0.004554602317512035f);
static const ai_layer_format_type _stage2_stage2_3_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;



static const ai_u16 _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage3_stage3_0_branch1_branch1_0_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage3_stage3_0_branch1_branch1_0_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage3_stage3_0_branch1_branch1_0_Conv_output_0_l_stride_1_const_u16 = 2;
static const ai_u16 _stage3_stage3_0_branch1_branch1_0_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_i8 _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_out_0_fmt_zero_const_s8 = -7;
static const ai_float _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.01578989252448082f;
static const ai_float _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.025220617651939392f;
static const ai_float _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.004240676295012236f, 0.006387331988662481f, 0.004530555102974176f, 0.007135234773159027f, 0.005128399934619665f, 0.005883203819394112f, 0.004640208091586828f, 0.0036620446480810642f, 0.0024560801684856415f, 0.006362963002175093f, 0.0038983586709946394f, 0.004085055552423f, 0.005447487812489271f, 0.004824662581086159f, 0.004178435541689396f, 0.008258141577243805f, 0.010075160302221775f, 0.006340416613966227f, 0.003701066831126809f, 0.006776073481887579f, 0.005267153959721327f, 0.006450159940868616f, 0.004553882870823145f, 0.005445965565741062f, 0.0054130153730511665f, 0.0033819973468780518f, 0.006327448878437281f, 0.006267620250582695f, 0.004536188207566738f, 0.004882009234279394f, 0.004641862586140633f, 0.009442451409995556f, 0.011483468115329742f, 0.008343406021595001f, 0.005395390558987856f, 0.005790979601442814f, 0.007575186435133219f, 0.007214512676000595f, 0.0023072557523846626f, 0.005244185216724873f, 0.0046531204134225845f, 0.004190416075289249f, 0.006235805340111256f, 0.005358993541449308f, 0.006647203117609024f, 0.003517036559060216f, 0.008969387039542198f, 0.008208908140659332f);
static const ai_u16 _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_out_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_out_0_shape_h_const_u16 = 10;

static const ai_u16 _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_0_branch1_branch1_4_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_0_branch1_branch1_4_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_in_0_fmt_zero_const_s8 = -7;
static const ai_i8 _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.025220617651939392f;
static const ai_float _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.009784913621842861f;
static const ai_float _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0017363728256896138f, 0.002201146213337779f, 0.001602445961907506f, 0.001679098466411233f, 0.002060897881165147f, 0.0025664072018116713f, 0.003149746684357524f, 0.001957851927727461f, 0.0031000978779047728f, 0.0017933775670826435f, 0.003215693635866046f, 0.002079129684716463f, 0.0024847884196788073f, 0.0016514970920979977f, 0.0035784083884209394f, 0.002718150382861495f, 0.002140815369784832f, 0.002650207607075572f, 0.002937858924269676f, 0.0018943678587675095f, 0.0032388772815465927f, 0.0018645020900294185f, 0.002570994198322296f, 0.0026217505801469088f, 0.004304523114115f, 0.0022881370969116688f, 0.0017636429984122515f, 0.0014113006182014942f, 0.0027950515504926443f, 0.00378358899615705f, 0.004877855535596609f, 0.002588664647191763f, 0.006250180769711733f, 0.00450525339692831f, 0.0022478497121483088f, 0.0062324716709554195f, 0.002821236150339246f, 0.004930179566144943f, 0.0040273480117321014f, 0.003428451018407941f, 0.0040018921718001366f, 0.0021890094503760338f, 0.0027652967255562544f, 0.0027815313078463078f, 0.0025051573757082224f, 0.001334083266556263f, 0.0027829075697809458f, 0.0018890461651608348f);
static const ai_layer_format_type _stage3_stage3_0_branch1_branch1_4_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage3_stage3_0_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_0_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.01578989252448082f;
static const ai_float _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.012108846567571163f;
static const ai_float _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.009443801827728748f, 0.004364976659417152f, 0.0008719348697923124f, 0.007019217126071453f, 0.004005371127277613f, 0.006382806226611137f, 0.007354258093982935f, 0.0023929462768137455f, 0.005148343276232481f, 0.002074477029964328f, 0.004357218276709318f, 0.0025548995472490788f, 0.0029394985176622868f, 0.005873486865311861f, 0.00606220867484808f, 0.005565335042774677f, 2.6429629542690236e-06f, 0.004272180143743753f, 0.003940812312066555f, 0.004334368743002415f, 0.005000889766961336f, 0.0035305412020534277f, 0.004205209203064442f, 0.0039010385517030954f, 0.005673488136380911f, 0.0058583952486515045f, 0.00560168270021677f, 0.003503808518871665f, 0.0015379476826637983f, 0.003525508102029562f, 0.008870064280927181f, 0.0014629964716732502f, 0.005269933026283979f, 0.0029304008930921555f, 0.009064815007150173f, 0.009272301569581032f, 0.004969156812876463f, 0.0032727369107306004f, 0.00760602205991745f, 0.007126606535166502f, 0.0062330239452421665f, 0.003608228638768196f, 0.003928677178919315f, 0.0028875661082565784f, 0.00424437690526247f, 0.004618693608790636f, 0.005115503910928965f, 0.002234712475910783f);
static const ai_layer_format_type _stage3_stage3_0_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage3_stage3_0_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage3_stage3_0_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage3_stage3_0_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16 = 2;
static const ai_u16 _stage3_stage3_0_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_i8 _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = 15;
static const ai_float _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.012108846567571163f;
static const ai_float _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.030471378937363625f;
static const ai_float _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.005533344112336636f, 0.0037437507417052984f, 0.5753169059753418f, 0.008129434660077095f, 0.006188039667904377f, 0.014637192711234093f, 0.0043206787668168545f, 0.011949505656957626f, 0.011548326350748539f, 0.010685760527849197f, 0.01222851499915123f, 0.03720015287399292f, 0.005707235541194677f, 0.008023994974792004f, 0.0066488320007920265f, 0.0033899801783263683f, 0.005123259499669075f, 0.006479842122644186f, 0.007912692613899708f, 0.006583590526133776f, 0.005949043203145266f, 0.00637622969225049f, 0.007250193972140551f, 0.006837226916104555f, 0.009488144889473915f, 0.007735073566436768f, 0.008462668396532536f, 0.012823992408812046f, 0.012270268984138966f, 0.006161163095384836f, 0.004108676686882973f, 0.011299286969006062f, 0.005460415035486221f, 0.008828269317746162f, 0.008134175091981888f, 0.0065208557061851025f, 0.006375018507242203f, 0.008254553191363811f, 0.010629368014633656f, 0.0031374688260257244f, 0.0038592384662479162f, 0.006694874260574579f, 0.007874413393437862f, 0.009151517413556576f, 0.00963727105408907f, 0.005956417880952358f, 0.006945877801626921f, 0.0076345899142324924f);
static const ai_u16 _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 10;

static const ai_u16 _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_0_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_0_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = 15;
static const ai_i8 _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.030471378937363625f;
static const ai_float _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.008621389977633953f;
static const ai_float _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0018165127839893103f, 0.001821441575884819f, 0.0026487980503588915f, 0.0026527890004217625f, 0.0031012084800750017f, 0.0022948298137634993f, 0.0021764454431831837f, 0.0016909473342821002f, 0.0025841339956969023f, 0.0030555955599993467f, 0.002998004900291562f, 0.002663663122802973f, 0.0015001654392108321f, 0.003168701194226742f, 0.001842931262217462f, 0.0024207294918596745f, 0.0025375904515385628f, 0.0036852278281003237f, 0.0018669780110940337f, 0.002584317931905389f, 0.005343955475836992f, 0.0037611760199069977f, 0.003134483704343438f, 0.0022429500240832567f, 0.002330186078324914f, 0.0038868349511176348f, 0.0024339540395885706f, 0.004869681317359209f, 0.0023529359605163336f, 0.0033645236399024725f, 0.0021861535497009754f, 0.004039329942315817f, 0.004201212432235479f, 0.0017794849118217826f, 0.0019453930435702205f, 0.003542711492627859f, 0.0015549982199445367f, 0.003590598003938794f, 0.0035692467354238033f, 0.004097698722034693f, 0.004672269802540541f, 0.002380644902586937f, 0.0015302343526855111f, 0.0022548791021108627f, 0.003594605717808008f, 0.0031395116820931435f, 0.0022864353377372026f, 0.0034343507140874863f);
static const ai_layer_format_type _stage3_stage3_0_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_1_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_1_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.009784913621842861f;
static const ai_float _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.007343852426856756f;
static const ai_float _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002815603045746684f, 0.002989334287121892f, 0.0038333286065608263f, 0.004786570090800524f, 0.0016606185818091035f, 0.0022074109874665737f, 0.00283692660741508f, 0.0031211182940751314f, 0.005746128037571907f, 0.0034729852341115475f, 0.0051807076670229435f, 0.0024744404945522547f, 0.007038216572254896f, 0.003173896111547947f, 0.0034111973363906145f, 0.003738600993528962f, 0.0034058834426105022f, 0.0022760494612157345f, 0.0015569002134725451f, 0.003741973778232932f, 0.006314795929938555f, 0.0032347659580409527f, 0.004842018708586693f, 0.0012779362732544541f, 0.004140268545597792f, 0.0030671744607388973f, 0.003337515750899911f, 0.002864204114302993f, 0.0009054862894117832f, 0.0016808006912469864f, 0.0033182280603796244f, 0.0032119674142450094f, 0.0024124737828969955f, 0.0015579775208607316f, 0.0033379043452441692f, 0.0033538949210196733f, 0.004651037510484457f, 0.00346460472792387f, 0.002186079043895006f, 0.0033634330611675978f, 0.004386227112263441f, 0.0031966674141585827f, 0.005523208994418383f, 0.0034886186476796865f, 0.00253069750033319f, 0.00338121154345572f, 0.002652060938999057f, 0.0016241052653640509f);
static const ai_layer_format_type _stage3_stage3_1_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage3_stage3_1_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage3_stage3_1_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage3_stage3_1_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_1_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i8 _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = 2;
static const ai_float _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.007343852426856756f;
static const ai_float _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.023529132828116417f;
static const ai_float _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.014121085405349731f, 0.004782442003488541f, 0.005407024174928665f, 0.017108066007494926f, 0.019883926957845688f, 0.016316071152687073f, 0.025969916954636574f, 0.007018390577286482f, 0.011313201859593391f, 0.005499027669429779f, 0.010582756251096725f, 0.024158285930752754f, 0.01837671361863613f, 0.017533166334033012f, 0.013789234682917595f, 0.017136801034212112f, 0.00756816053763032f, 0.02087695151567459f, 0.023890750482678413f, 0.00809994712471962f, 0.02323903888463974f, 0.015019378624856472f, 0.012251931242644787f, 0.019478628411889076f, 0.007217127829790115f, 0.007775937207043171f, 0.016407961025834084f, 0.0056154849007725716f, 0.04002997279167175f, 0.01326029747724533f, 0.020170530304312706f, 0.008294946514070034f, 0.013581705279648304f, 0.03649526834487915f, 0.01034214161336422f, 0.017588265240192413f, 0.009972401894629002f, 0.02279643341898918f, 0.028559928759932518f, 0.006874267943203449f, 0.002543730428442359f, 0.012919365428388119f, 0.014823012985289097f, 0.008744623512029648f, 0.018269360065460205f, 0.01425807923078537f, 0.014299774542450905f, 0.01633913442492485f);
static const ai_u16 _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 10;

static const ai_u16 _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_1_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_1_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = 2;
static const ai_i8 _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.023529132828116417f;
static const ai_float _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.00963827408850193f;
static const ai_float _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0031625244300812483f, 0.0022264609578996897f, 0.0027459608390927315f, 0.0015194129664450884f, 0.003125287126749754f, 0.002886649454012513f, 0.004650574177503586f, 0.00274298177100718f, 0.0026621806900948286f, 0.002991432324051857f, 0.0026095404755324125f, 0.002201647264882922f, 0.003189011011272669f, 0.0016416056314483285f, 0.002088541630655527f, 0.0024193660356104374f, 0.003958497662097216f, 0.00313622341491282f, 0.0027873292565345764f, 0.002269827062264085f, 0.0034897644072771072f, 0.0016448238166049123f, 0.001892515574581921f, 0.001641205046325922f, 0.005432476755231619f, 0.005508339032530785f, 0.0021318565122783184f, 0.0036855419166386127f, 0.004307559225708246f, 0.004396782256662846f, 0.0037970454432070255f, 0.008144662715494633f, 0.0050018359906971455f, 0.004340221174061298f, 0.005675979889929295f, 0.004308826755732298f, 0.005332029890269041f, 0.0026083309203386307f, 0.0023210898507386446f, 0.006704885978251696f, 0.006043217610567808f, 0.003753444878384471f, 0.004573843441903591f, 0.002629785565659404f, 0.004396319389343262f, 0.007317686453461647f, 0.0025248785968869925f, 0.003378583351150155f);
static const ai_layer_format_type _stage3_stage3_1_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_2_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_2_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.009784913621842861f;
static const ai_float _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.005554745439440012f;
static const ai_float _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0019158297218382359f, 3.7927836729068076e-06f, 0.002241397276520729f, 0.004529590252786875f, 0.002693725284188986f, 0.0022723425645381212f, 0.0018721050582826138f, 0.005080436356365681f, 0.002321600215509534f, 0.003548275213688612f, 0.0030907082837074995f, 0.0036736088804900646f, 0.002929202513769269f, 0.0017326801316812634f, 0.0038649835623800755f, 0.002765391953289509f, 0.004514479078352451f, 0.002666980493813753f, 0.003302827011793852f, 0.0025630376767367125f, 0.006671963259577751f, 0.003925451077520847f, 0.003731505014002323f, 0.0012476140400394797f, 0.0009587373933754861f, 0.003714553778991103f, 0.0033066635951399803f, 0.005705467890948057f, 0.003536913078278303f, 0.002544909715652466f, 0.004091799259185791f, 0.002067559864372015f, 0.004841609857976437f, 0.005057047121226788f, 0.00224491860717535f, 0.002392218913882971f, 0.003223688807338476f, 0.00557306595146656f, 0.0018281484954059124f, 0.0035288436338305473f, 0.002847113413736224f, 0.0029676235280930996f, 0.0033668060787022114f, 0.0015407133614644408f, 0.00297167687676847f, 0.0033026139717549086f, 0.004135291092097759f, 0.0014415058540180326f);
static const ai_layer_format_type _stage3_stage3_2_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage3_stage3_2_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage3_stage3_2_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage3_stage3_2_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_2_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i8 _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = 24;
static const ai_float _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.005554745439440012f;
static const ai_float _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.016870904713869095f;
static const ai_float _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.00810567382723093f, 0.01689939573407173f, 0.01604284904897213f, 0.015354860574007034f, 0.02270716242492199f, 0.019621262326836586f, 0.03355719521641731f, 0.02761598490178585f, 0.014090958051383495f, 0.0183230210095644f, 0.012132829986512661f, 0.013881297782063484f, 0.012589026242494583f, 0.013902225531637669f, 0.013165242038667202f, 0.012844109907746315f, 0.020144395530223846f, 0.019609201699495316f, 0.017178036272525787f, 0.015941470861434937f, 0.010827763937413692f, 0.004480239003896713f, 0.013010538183152676f, 0.019430013373494148f, 0.0663374811410904f, 0.013081035576760769f, 0.007214603014290333f, 0.012610000558197498f, 0.010442404076457024f, 0.008675994351506233f, 0.013670430518686771f, 0.02583703026175499f, 0.01579391583800316f, 0.012472612783312798f, 0.01526560541242361f, 0.010740479454398155f, 0.006490870378911495f, 0.01098624151200056f, 0.01158502884209156f, 0.012526463717222214f, 0.006679189391434193f, 0.023123690858483315f, 0.02140786312520504f, 0.03641754761338234f, 0.010826720856130123f, 0.015529322437942028f, 0.00883978046476841f, 0.025905804708600044f);
static const ai_u16 _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 10;

static const ai_u16 _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_2_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_2_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = 24;
static const ai_i8 _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.016870904713869095f;
static const ai_float _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.00911688432097435f;
static const ai_float _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.00314691080711782f, 0.0021541095338761806f, 0.0021890723146498203f, 0.0018474719254299998f, 0.0028281917329877615f, 0.001488754409365356f, 0.0035742460750043392f, 0.003302675671875477f, 0.0019802136812359095f, 0.0021700910292565823f, 0.0027919055428355932f, 0.003359036287292838f, 0.004270202945917845f, 0.0014986797468736768f, 0.002276761457324028f, 0.00486241327598691f, 0.004283739719539881f, 0.002255254192277789f, 0.0017815888859331608f, 0.0017400893848389387f, 0.003024491248652339f, 0.002917194738984108f, 0.0032407664693892f, 0.004276024643331766f, 0.00167006254196167f, 0.0068968222476542f, 0.0030272002331912518f, 0.0031733678188174963f, 0.005617947783321142f, 0.004265017341822386f, 0.002289052354171872f, 0.0021147835068404675f, 0.0025856602005660534f, 0.0044464413076639175f, 0.001807160209864378f, 0.0019202535040676594f, 0.006053955294191837f, 0.0025006362702697515f, 0.002688467502593994f, 0.0016020787879824638f, 0.002959642792120576f, 0.0028449739329516888f, 0.003759532468393445f, 0.0014151097275316715f, 0.0017807543044909835f, 0.002843832829967141f, 0.004310257267206907f, 0.0043338448740541935f);
static const ai_layer_format_type _stage3_stage3_2_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_3_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_3_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.009784913621842861f;
static const ai_float _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.007323624100536108f;
static const ai_float _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002797849243506789f, 0.0028742330614477396f, 0.00453696446493268f, 0.004106588661670685f, 0.0046157860197126865f, 0.001995597267523408f, 0.0022950174752622843f, 0.005104859825223684f, 0.004241990391165018f, 0.0048447949811816216f, 0.004213717300444841f, 0.002557841595262289f, 0.0023698860313743353f, 0.003942381124943495f, 0.003451047232374549f, 0.005754374898970127f, 0.003407635260373354f, 0.0019267256138846278f, 0.0025546427350491285f, 0.0030284083914011717f, 0.0033997062128037214f, 0.008722820319235325f, 0.0025065618101507425f, 0.0035062190145254135f, 0.003882921300828457f, 0.002453399822115898f, 0.0038973239716142416f, 0.0037378412671387196f, 0.0026102771516889334f, 0.004232799168676138f, 0.002466013887897134f, 0.002662449609488249f, 0.005429759155958891f, 0.002929303329437971f, 0.007830689661204815f, 0.002178999362513423f, 0.0032385895028710365f, 0.008035816252231598f, 0.00878818891942501f, 0.003118247492238879f, 0.0024756642524152994f, 0.0031667565926909447f, 0.0016488436376675963f, 0.0036307780537754297f, 0.002405255101621151f, 0.0038327095098793507f, 0.0031315379310399294f, 0.0021403462160378695f);
static const ai_layer_format_type _stage3_stage3_3_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage3_stage3_3_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage3_stage3_3_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage3_stage3_3_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_3_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i8 _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = 6;
static const ai_float _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.007323624100536108f;
static const ai_float _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.02086583524942398f;
static const ai_float _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.011132518760859966f, 0.014410726726055145f, 0.01807943359017372f, 0.011737141758203506f, 0.00827017892152071f, 0.017509866505861282f, 0.022287575528025627f, 0.014424539171159267f, 0.012311634607613087f, 0.016894491389393806f, 0.010229242034256458f, 0.016695821657776833f, 0.015501872636377811f, 0.022011974826455116f, 0.0188401248306036f, 0.01731659471988678f, 0.006045191083103418f, 0.0316455215215683f, 0.011374209076166153f, 0.008692479692399502f, 0.012622306123375893f, 0.010510534048080444f, 0.012953318655490875f, 0.0167376771569252f, 0.01096439454704523f, 0.025811240077018738f, 0.011487212963402271f, 0.01963864639401436f, 0.02164211869239807f, 0.013103887438774109f, 0.009654097259044647f, 0.015011146664619446f, 0.008219452574849129f, 0.02062338963150978f, 0.021287087351083755f, 0.013745170086622238f, 0.009515459649264812f, 0.008896151557564735f, 0.008564663119614124f, 0.01625560224056244f, 0.022867484018206596f, 0.014300103299319744f, 0.009221045300364494f, 0.0163583867251873f, 0.009436266496777534f, 0.009395677596330643f, 0.007700437214225531f, 0.02514687180519104f);
static const ai_u16 _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 10;

static const ai_u16 _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_3_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_3_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = 6;
static const ai_i8 _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.02086583524942398f;
static const ai_float _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.012644290924072266f;
static const ai_float _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0022813729010522366f, 0.00399418268352747f, 0.0037948975805193186f, 0.002105757361277938f, 0.00427234685048461f, 0.002264229580760002f, 0.0020658636931329966f, 0.0031964825466275215f, 0.0028574552852660418f, 0.0016620575916022062f, 0.0023781871423125267f, 0.0033160720486193895f, 0.0052443682216107845f, 0.005681129172444344f, 0.0019329111091792583f, 0.00408982252702117f, 0.003467921633273363f, 0.0019381847232580185f, 0.002451745793223381f, 0.00562750780954957f, 0.001776361488737166f, 0.0057448153384029865f, 0.002522461349144578f, 0.004852065350860357f, 0.0023683225736021996f, 0.002212083199992776f, 0.0035803988575935364f, 0.0022225496359169483f, 0.0033284162636846304f, 0.0029443600215017796f, 0.00322206923738122f, 0.00310242990963161f, 0.005994145758450031f, 0.0027612533885985613f, 0.0062084682285785675f, 0.0037940857000648975f, 0.0039355093613266945f, 0.0022806881461292505f, 0.003204154781997204f, 0.002289478899911046f, 0.004752662032842636f, 0.003200383624061942f, 0.004141464829444885f, 0.0029992007184773684f, 0.006222452502697706f, 0.004024340305477381f, 0.0029684349428862333f, 0.003217688063159585f);
static const ai_layer_format_type _stage3_stage3_3_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_4_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_4_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.012644290924072266f;
static const ai_float _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.0064444225281476974f;
static const ai_float _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.00233031390234828f, 0.0019685495644807816f, 0.00268838694319129f, 0.003232650924474001f, 0.0054315426386892796f, 0.005068494938313961f, 0.0029220946598798037f, 0.005513861309736967f, 0.003197743557393551f, 0.0011348293628543615f, 0.004742827266454697f, 0.00736970966681838f, 0.0028514477889984846f, 0.002931842114776373f, 0.004555209074169397f, 0.004379235208034515f, 0.0006471314118243754f, 0.0032777993474155664f, 0.004717584233731031f, 0.007102716714143753f, 0.004669658374041319f, 0.0022428464144468307f, 0.001771719427779317f, 0.0038713759277015924f, 0.0017084252322092652f, 0.0032408691477030516f, 0.0050050197169184685f, 0.002939502941444516f, 0.0019909318070858717f, 0.0031000503804534674f, 0.00158579065464437f, 0.003259485587477684f, 0.0011219014413654804f, 0.0028950751293450594f, 0.00170934631023556f, 0.0036839665845036507f, 0.003000963246449828f, 0.002213680185377598f, 0.0014519606484100223f, 0.0034064454957842827f, 0.0012992048868909478f, 0.0023828018456697464f, 0.0030155526474118233f, 0.0017238733125850558f, 0.0019170885207131505f, 0.004453263711184263f, 0.0016715559177100658f, 0.0031508116517215967f);
static const ai_layer_format_type _stage3_stage3_4_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage3_stage3_4_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage3_stage3_4_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage3_stage3_4_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_4_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i8 _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = 14;
static const ai_float _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.0064444225281476974f;
static const ai_float _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.021930551156401634f;
static const ai_float _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.00961020216345787f, 0.00956113263964653f, 0.014884437434375286f, 0.019026920199394226f, 0.013686677441000938f, 0.011312109418213367f, 0.005648861173540354f, 0.015674643218517303f, 0.009302995167672634f, 0.03470584750175476f, 0.020406102761626244f, 0.027877768501639366f, 0.010405102744698524f, 0.011785577982664108f, 0.027558084577322006f, 0.013824325054883957f, 0.05380791053175926f, 0.011248020455241203f, 0.012478109449148178f, 0.004931545350700617f, 0.012109384872019291f, 0.012825999408960342f, 0.02223125472664833f, 0.011924799531698227f, 0.011716770939528942f, 0.007124152034521103f, 0.014601980336010456f, 0.008845562115311623f, 0.017289476469159126f, 0.011894231662154198f, 0.017012543976306915f, 0.017720026895403862f, 0.014789027161896229f, 0.008970260620117188f, 0.03204431012272835f, 0.013339713215827942f, 0.014972240664064884f, 0.014148401096463203f, 0.015855174511671066f, 0.00470677949488163f, 0.018873222172260284f, 0.00935582909733057f, 0.010534624569118023f, 0.02707655541598797f, 0.009617434814572334f, 0.01434411108493805f, 0.023697687312960625f, 0.03251277655363083f);
static const ai_u16 _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 10;

static const ai_u16 _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_4_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_4_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = 14;
static const ai_i8 _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.021930551156401634f;
static const ai_float _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.009977243840694427f;
static const ai_float _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0027589108794927597f, 0.0022581741213798523f, 0.0015665520913898945f, 0.0015789810568094254f, 0.0020456130150705576f, 0.0020862517412751913f, 0.0015902408631518483f, 0.002960542682558298f, 0.0020201776642352343f, 0.0027511578518897295f, 0.0017043750267475843f, 0.0015959610464051366f, 0.004287555813789368f, 0.003899750066921115f, 0.001689037075266242f, 0.0015748648438602686f, 0.001697819447144866f, 0.0024309339933097363f, 0.0021300818771123886f, 0.002448656363412738f, 0.0019027722300961614f, 0.002547674812376499f, 0.0017483009723946452f, 0.0027359977830201387f, 0.0015364611754193902f, 0.0028379294089972973f, 0.005471603944897652f, 0.0022451309487223625f, 0.0026986796874552965f, 0.0029804713558405638f, 0.0024863802827894688f, 0.004957832396030426f, 0.003980305045843124f, 0.0022369090002030134f, 0.00130836246535182f, 0.0016406598733738065f, 0.005432463716715574f, 0.004127587657421827f, 0.0027763466350734234f, 0.0037196450866758823f, 0.004102038219571114f, 0.006072152405977249f, 0.003149969968944788f, 0.0028078234754502773f, 0.0028442973271012306f, 0.0033035108353942633f, 0.006095469929277897f, 0.0029100815299898386f);
static const ai_layer_format_type _stage3_stage3_4_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_5_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_5_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.009977243840694427f;
static const ai_float _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.007539808284491301f;
static const ai_float _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0037520811893045902f, 0.004365058150142431f, 0.004782810341566801f, 0.0015081624733284116f, 0.0021203472279012203f, 0.0017221113666892052f, 0.0012105003697797656f, 0.0035740400198847055f, 0.0042249541729688644f, 0.001872338936664164f, 0.002181010553613305f, 0.003504956839606166f, 0.0040031722746789455f, 0.002428729785606265f, 0.002060151193290949f, 0.004017533268779516f, 0.0031464719213545322f, 0.003988422453403473f, 0.0021906348410993814f, 0.0038589448668062687f, 0.0034885930363088846f, 0.002990701235830784f, 0.003378536319360137f, 0.005243272520601749f, 0.0030542181339114904f, 0.0019107558764517307f, 0.003399972803890705f, 0.0019273866200819612f, 0.004510358441621065f, 0.0023239448200911283f, 0.00459337467327714f, 0.003345814533531666f, 0.0017565537709742785f, 0.0030806702561676502f, 0.004723966587334871f, 0.003308446379378438f, 0.0017679466400295496f, 0.0028809199575334787f, 0.005532181356102228f, 0.003183027496561408f, 0.001456788624636829f, 0.0019336623372510076f, 0.004363756161183119f, 0.0023546689189970493f, 0.002086023334413767f, 0.0021953845862299204f, 0.001669274177402258f, 0.002751139458268881f);
static const ai_layer_format_type _stage3_stage3_5_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage3_stage3_5_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage3_stage3_5_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage3_stage3_5_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_5_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i8 _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = -3;
static const ai_float _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.007539808284491301f;
static const ai_float _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.023926332592964172f;
static const ai_float _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.01453493069857359f, 0.01246398501098156f, 0.014495453797280788f, 0.044129639863967896f, 0.038767773658037186f, 0.01472010463476181f, 0.01877654902637005f, 0.007037131581455469f, 0.01166917011141777f, 0.01320604793727398f, 0.006700614467263222f, 0.01303150039166212f, 0.025908051058650017f, 0.012463514693081379f, 0.03156870976090431f, 0.008133661933243275f, 0.014933934435248375f, 0.011846384964883327f, 0.01342941727489233f, 0.009495929814875126f, 0.013560356572270393f, 0.014884809963405132f, 0.009281850419938564f, 0.014944828115403652f, 0.00932229682803154f, 0.012625165283679962f, 0.032151468098163605f, 0.012180345132946968f, 0.02416161261498928f, 0.007833835668861866f, 0.006145676597952843f, 0.010830259881913662f, 0.017246033996343613f, 0.008098525926470757f, 0.011503416113555431f, 0.004384967964142561f, 0.013200795277953148f, 0.01293406542390585f, 0.008193272165954113f, 0.0056728520430624485f, 0.021909328177571297f, 0.0077531407587230206f, 0.012942220084369183f, 0.009156106039881706f, 0.013723033480346203f, 0.021238472312688828f, 0.03071708232164383f, 0.007530246861279011f);
static const ai_u16 _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 10;

static const ai_u16 _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_5_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_5_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = -3;
static const ai_i8 _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.023926332592964172f;
static const ai_float _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.011004479601979256f;
static const ai_float _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0018899986753240228f, 0.0022347131744027138f, 0.0019447013037279248f, 0.003942572511732578f, 0.00247262604534626f, 0.003164994064718485f, 0.0026265361811965704f, 0.0030184960924088955f, 0.0028814421966671944f, 0.001708348747342825f, 0.0033601243048906326f, 0.0017388546839356422f, 0.004169367719441652f, 0.002547750249505043f, 0.0018734249752014875f, 0.0015380381373688579f, 0.0013494410086423159f, 0.0032062360551208258f, 0.0020022077951580286f, 0.0016911159036681056f, 0.0019092168658971786f, 0.0020522295963019133f, 0.0013928738189861178f, 0.0021631300915032625f, 0.005635680630803108f, 0.00335058500058949f, 0.003362437942996621f, 0.0020039500668644905f, 0.004293380305171013f, 0.002210547449067235f, 0.0037609380669891834f, 0.003432303434237838f, 0.0036430740728974342f, 0.0020995032973587513f, 0.004068559035658836f, 0.0013737467816099524f, 0.0016949056880548596f, 0.0023030757438391447f, 0.001642268500290811f, 0.0020435662008821964f, 0.0037382300943136215f, 0.004128218162804842f, 0.001777380588464439f, 0.0019526284886524081f, 0.0021976802963763475f, 0.0028220582753419876f, 0.002186864148825407f, 0.001532477792352438f);
static const ai_layer_format_type _stage3_stage3_5_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_6_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_6_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.011004479601979256f;
static const ai_float _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.009234469383955002f;
static const ai_float _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0032268587965518236f, 0.0015533014666289091f, 0.003125334158539772f, 0.00336670340038836f, 0.001973825041204691f, 0.0035844736266881227f, 0.003857279662042856f, 0.007451036013662815f, 0.00446353992447257f, 0.0030664894729852676f, 0.0026874211616814137f, 0.007188091054558754f, 0.0032842953223735094f, 0.003865508595481515f, 0.003508886555209756f, 0.004254680126905441f, 0.003073676722124219f, 0.004272953607141972f, 0.003403356298804283f, 0.0026636074762791395f, 0.0016432029660791159f, 0.002304692752659321f, 0.004444457590579987f, 0.005700096022337675f, 0.0033184720668941736f, 0.0038537117652595043f, 0.005301675293594599f, 0.004808065481483936f, 0.003918407019227743f, 0.008610590361058712f, 0.003059598384425044f, 0.008222260512411594f, 0.008638028055429459f, 0.0023911905009299517f, 0.0031863530166447163f, 0.004744233097881079f, 0.003261593636125326f, 0.00560237979516387f, 0.004423013422638178f, 0.006623169407248497f, 0.008795141242444515f, 0.007853982970118523f, 0.004048039671033621f, 0.002788609592244029f, 0.006011213641613722f, 0.00415105652064085f, 0.0038084830157458782f, 0.003175276331603527f);
static const ai_layer_format_type _stage3_stage3_6_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage3_stage3_6_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage3_stage3_6_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage3_stage3_6_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_6_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i8 _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = -6;
static const ai_float _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.009234469383955002f;
static const ai_float _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.030912429094314575f;
static const ai_float _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.022649915888905525f, 0.010691613890230656f, 0.012888576835393906f, 0.018421955406665802f, 0.028640972450375557f, 0.005422234069555998f, 0.0059958528727293015f, 0.009652948006987572f, 0.004729635547846556f, 0.013512506149709225f, 0.03565927967429161f, 0.017068883404135704f, 0.007504865527153015f, 0.008161822333931923f, 0.01095561496913433f, 0.011501082219183445f, 0.004397256299853325f, 0.007989302277565002f, 0.006072043441236019f, 0.012701458297669888f, 0.03940128907561302f, 0.007238402497023344f, 0.013195183128118515f, 0.014197501353919506f, 0.018462900072336197f, 0.01064174436032772f, 0.010313564911484718f, 0.008943449705839157f, 0.003197628306224942f, 0.026059353724122047f, 0.013362116180360317f, 0.017874514684081078f, 0.008905633352696896f, 0.018220368772745132f, 0.009008660912513733f, 0.006098352838307619f, 0.007012768182903528f, 0.006047822535037994f, 0.0045471941120922565f, 0.00591807346791029f, 0.012134422548115253f, 0.021841542795300484f, 0.016435079276561737f, 0.007364692632108927f, 0.01712297648191452f, 0.007445713970810175f, 0.011285309679806232f, 0.039359718561172485f);
static const ai_u16 _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 10;

static const ai_u16 _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_6_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_6_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = -6;
static const ai_i8 _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.030912429094314575f;
static const ai_float _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.011528286151587963f;
static const ai_float _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002103431150317192f, 0.0018172409618273377f, 0.0025114058516919613f, 0.0019370386144146323f, 0.00378318689763546f, 0.0017829379066824913f, 0.002556086517870426f, 0.003381788032129407f, 0.0023299921303987503f, 0.0023300577886402607f, 0.001732122851535678f, 0.00344777200371027f, 0.0016998539213091135f, 0.002724735764786601f, 0.0019231594633311033f, 0.0017183987656608224f, 0.002612670185044408f, 0.0042518386617302895f, 0.0018476360710337758f, 0.003623406635597348f, 0.001831697067245841f, 0.002780621638521552f, 0.0014864741824567318f, 0.001892048865556717f, 0.0019403488840907812f, 0.002445494756102562f, 0.002057510893791914f, 0.0035370816476643085f, 0.0018501020967960358f, 0.0035959065426141024f, 0.0048716408200562f, 0.0014692464610561728f, 0.003968488425016403f, 0.0030596561264246702f, 0.0053312103264033794f, 0.002680547535419464f, 0.0020877609495073557f, 0.005329463630914688f, 0.001625662436708808f, 0.0028569495771080256f, 0.004493772052228451f, 0.004625755362212658f, 0.0020135946106165648f, 0.0022239151876419783f, 0.002025319030508399f, 0.001658368157222867f, 0.002565889386460185f, 0.002679192926734686f);
static const ai_layer_format_type _stage3_stage3_6_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_7_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_7_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.011528286151587963f;
static const ai_float _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.009342649951577187f;
static const ai_float _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0037294181529432535f, 0.0025106219109147787f, 0.0024799606762826443f, 0.0019871427211910486f, 0.00228576036170125f, 0.002634444274008274f, 0.0029193784575909376f, 0.004161014687269926f, 0.004008489195257425f, 0.005669362843036652f, 0.0013607455184683204f, 0.003241198603063822f, 0.0036498717963695526f, 0.0014079433167353272f, 0.002519726986065507f, 0.004224139731377363f, 0.0022944749798625708f, 0.0013891900889575481f, 0.005141268018633127f, 0.004598683211952448f, 0.002928698668256402f, 0.004589171148836613f, 0.006724468432366848f, 0.00491638807579875f, 0.0037853019312024117f, 0.0018525168998166919f, 0.005949028301984072f, 0.002394450828433037f, 0.0019099765922874212f, 0.003909493796527386f, 0.0049126846715807915f, 0.0031137384939938784f, 0.0025293913204222918f, 0.003432260360568762f, 0.004699683748185635f, 0.006401801481842995f, 0.004773806780576706f, 0.004267345182597637f, 0.0038697582203894854f, 0.0016014337306842208f, 0.0012725896667689085f, 0.005128693301230669f, 0.004550596699118614f, 0.003851956455036998f, 0.003415399696677923f, 0.00442455755546689f, 0.0028022166807204485f, 0.004164352547377348f);
static const ai_layer_format_type _stage3_stage3_7_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage3_stage3_7_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage3_stage3_7_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage3_stage3_7_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_7_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i8 _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = -27;
static const ai_float _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.009342649951577187f;
static const ai_float _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.028668759390711784f;
static const ai_float _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.010287066921591759f, 0.01641281694173813f, 0.011137083172798157f, 0.011911747045814991f, 0.010680925101041794f, 0.024000978097319603f, 0.0048653376288712025f, 0.005245500709861517f, 0.01132914423942566f, 0.015467149205505848f, 0.0133505305275321f, 0.005025660619139671f, 0.008917273953557014f, 0.031124772503972054f, 0.004940151236951351f, 0.020157480612397194f, 0.01239258237183094f, 0.01593705080449581f, 0.024372929707169533f, 0.016263622790575027f, 0.006738852709531784f, 0.007802892010658979f, 0.01680729165673256f, 0.02869332768023014f, 0.006339541636407375f, 0.014486949890851974f, 0.006894081365317106f, 0.026470674201846123f, 0.03363829851150513f, 0.005841734819114208f, 0.02197469398379326f, 0.004720119759440422f, 0.0311350729316473f, 0.02658109739422798f, 0.017279643565416336f, 0.005953771062195301f, 0.00572065357118845f, 0.0055649555288255215f, 0.031035281717777252f, 0.028973544016480446f, 0.04663855955004692f, 0.0180876012891531f, 0.00620816508308053f, 0.012353640981018543f, 0.011824837885797024f, 0.01065522339195013f, 0.005238433368504047f, 0.007631628308445215f);
static const ai_u16 _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 10;

static const ai_u16 _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_7_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_7_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = -27;
static const ai_i8 _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.028668759390711784f;
static const ai_float _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.013492636382579803f;
static const ai_float _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002573549747467041f, 0.0028395322151482105f, 0.0026445251423865557f, 0.0023971027694642544f, 0.0018647807883098722f, 0.005253210198134184f, 0.0023529164027422667f, 0.003988685552030802f, 0.002532106591388583f, 0.0026189996860921383f, 0.0016629199963063002f, 0.002907956950366497f, 0.0021349710877984762f, 0.0025081257335841656f, 0.0022291408386081457f, 0.002238911110907793f, 0.002151582855731249f, 0.002274139551445842f, 0.0021342593245208263f, 0.0026732126716524363f, 0.0022837244905531406f, 0.002832048572599888f, 0.0026279687881469727f, 0.004233138635754585f, 0.002064755419269204f, 0.004283171612769365f, 0.0023721905890852213f, 0.0032594555523246527f, 0.0015408830950036645f, 0.005754945799708366f, 0.0036513337399810553f, 0.00521481316536665f, 0.0041924575343728065f, 0.0031684928108006716f, 0.0034972087014466524f, 0.0034287048038095236f, 0.002254288177937269f, 0.002970321336761117f, 0.002670042449608445f, 0.002172356005758047f, 0.005266756750643253f, 0.003388271201401949f, 0.002218312583863735f, 0.0027341837994754314f, 0.0020386495161801577f, 0.0037747775204479694f, 0.003218607511371374f, 0.004331372678279877f);
static const ai_layer_format_type _stage3_stage3_7_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;



static const ai_u16 _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage4_stage4_0_branch1_branch1_0_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage4_stage4_0_branch1_branch1_0_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage4_stage4_0_branch1_branch1_0_Conv_output_0_l_stride_1_const_u16 = 2;
static const ai_u16 _stage4_stage4_0_branch1_branch1_0_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_i8 _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_out_0_fmt_zero_const_s8 = -24;
static const ai_float _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.013492636382579803f;
static const ai_float _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.028619926422834396f;
static const ai_float _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.004162369761615992f, 0.011614060960710049f, 0.00423844251781702f, 0.00500528933480382f, 0.005152784753590822f, 0.005183749832212925f, 0.004062596242874861f, 0.004624936729669571f, 0.0031967603135854006f, 0.004068240988999605f, 0.005524101667106152f, 0.003629844868555665f, 0.004713102709501982f, 0.004439161159098148f, 0.005957955028861761f, 0.005110254976898432f, 0.006630427669733763f, 0.0047450861893594265f, 0.004609187599271536f, 0.006129574496299028f, 0.003547472646459937f, 0.005686132237315178f, 0.004179550334811211f, 0.004687633831053972f, 0.004348309710621834f, 0.005756905302405357f, 0.005346095189452171f, 0.004394607152789831f, 0.0056448341347277164f, 0.0031379275023937225f, 0.004307866562157869f, 0.00437812739983201f, 0.0033516655676066875f, 0.004596228711307049f, 0.004604356363415718f, 0.00503379013389349f, 0.005036776419728994f, 0.004869180731475353f, 0.004605254624038935f, 0.0036237812601029873f, 0.004082249011844397f, 0.004308212082833052f, 0.004841933958232403f, 0.004651197697967291f, 0.0033296910114586353f, 0.0028411364182829857f, 0.0049123563803732395f, 0.005075177177786827f, 0.003736887127161026f, 0.004840373992919922f, 0.0047190384939312935f, 0.006203155033290386f, 0.003623069729655981f, 0.0040397909469902515f, 0.005335087422281504f, 0.002960809040814638f, 0.0038088057190179825f, 0.005368217825889587f, 0.005074792075902224f, 0.004431445151567459f, 0.004488494247198105f, 0.0032940807286649942f, 0.005852649919688702f, 0.003926215227693319f, 0.0031369524076581f, 0.0069473544135689735f, 0.0036973024252802134f, 0.003285784972831607f, 0.004323194734752178f, 0.004970166366547346f, 0.0035663240123540163f, 0.004125647712498903f, 0.0037144930101931095f, 0.005525632295757532f, 0.004360374994575977f, 0.005488034337759018f, 0.004586801398545504f, 0.004188312217593193f, 0.004910982679575682f, 0.0062044281512498856f, 0.0037826858460903168f, 0.0029861240182071924f, 0.005349657032638788f, 0.0047437045723199844f, 0.005180425476282835f, 0.004181909840553999f, 0.004988156259059906f, 0.00321319792419672f, 0.004136295523494482f, 0.004657466430217028f, 0.00489152641966939f, 0.002460984280332923f, 0.004136505536735058f, 0.007289482746273279f, 0.005937162321060896f, 0.005927347578108311f);
static const ai_u16 _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_out_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_out_0_shape_h_const_u16 = 5;

static const ai_u16 _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_0_branch1_branch1_4_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_0_branch1_branch1_4_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_in_0_fmt_zero_const_s8 = -24;
static const ai_i8 _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.028619926422834396f;
static const ai_float _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.010431465692818165f;
static const ai_float _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.003203026484698057f, 0.0018594431458041072f, 0.0018267693230882287f, 0.0021922828163951635f, 0.0019235577201470733f, 0.0020840640645474195f, 0.0016290650237351656f, 0.001962475711479783f, 0.0025201591197401285f, 0.0017989837797358632f, 0.0023741889744997025f, 0.001879768562503159f, 0.0010484920348972082f, 0.0014196070842444897f, 0.0013221545377746224f, 0.0020331672858446836f, 0.0016515520401299f, 0.001626180368475616f, 0.0019750993233174086f, 0.0013083405792713165f, 0.0011114514200016856f, 0.001349824946373701f, 0.0021324539557099342f, 0.001198992133140564f, 0.0016769187059253454f, 0.001758488593623042f, 0.0014842087402939796f, 0.0018551723333075643f, 0.004292048513889313f, 0.002060273429378867f, 0.0016524982638657093f, 0.001718674204312265f, 0.0032920935191214085f, 0.002677902113646269f, 0.0011069984175264835f, 0.0016326598124578595f, 0.001142310444265604f, 0.0015693255700170994f, 0.0026679611764848232f, 0.0017922663828358054f, 0.0016651299083605409f, 0.0024468880146741867f, 0.0017348788678646088f, 0.0015323400730267167f, 0.0026165335439145565f, 0.0015175937442108989f, 0.0015771356411278248f, 0.0019192738691344857f, 0.0015185828087851405f, 0.002772918902337551f, 0.0017967632738873363f, 0.0016384554328396916f, 0.0017491227481514215f, 0.0013796263374388218f, 0.002005938906222582f, 0.002242880640551448f, 0.004166086670011282f, 0.001781179802492261f, 0.0016928169643506408f, 0.0029728966765105724f, 0.0025209393352270126f, 0.0022175649646669626f, 0.002673140726983547f, 0.0020182367879897356f, 0.0023863883689045906f, 0.0014448078582063317f, 0.0012906045885756612f, 0.0026222995948046446f, 0.0019292994402348995f, 0.0016541030490770936f, 0.0013635288923978806f, 0.001443569315597415f, 0.002298475243151188f, 0.001835073227994144f, 0.002649995032697916f, 0.0016795923002064228f, 0.0024970616213977337f, 0.0018006248865276575f, 0.001851691515184939f, 0.0023812048602849245f, 0.0015534047270193696f, 0.0023519787937402725f, 0.0028649908490478992f, 0.0014742229832336307f, 0.0022697346284985542f, 0.0015210239216685295f, 0.0014534458750858903f, 0.0024728295393288136f, 0.0013709107879549265f, 0.002208276418969035f, 0.002413452137261629f, 0.0032929133158177137f, 0.001525025232695043f, 0.002876361832022667f, 0.0017281919717788696f, 0.0019495030865073204f);
static const ai_layer_format_type _stage4_stage4_0_branch1_branch1_4_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage4_stage4_0_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_0_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.013492636382579803f;
static const ai_float _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.009827804751694202f;
static const ai_float _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002688564360141754f, 0.0026727078948169947f, 0.004027151502668858f, 0.0029571247287094593f, 0.0020771536510437727f, 0.0021381299011409283f, 0.0028458377346396446f, 0.0030664047226309776f, 0.0022302186116576195f, 0.002817294793203473f, 0.0021915838588029146f, 0.002893925877287984f, 0.005002086982131004f, 0.0028332453221082687f, 0.002848125295713544f, 0.0031505986116826534f, 0.002409212989732623f, 0.0021291691809892654f, 0.002461987314745784f, 0.0023349204566329718f, 0.002283302368596196f, 0.002676259959116578f, 0.0024763622786849737f, 0.002590061165392399f, 0.0024919547140598297f, 0.005106043070554733f, 0.0024485248140990734f, 0.0015618330799043179f, 0.005520974285900593f, 0.0019811931997537613f, 0.0017305902438238263f, 0.002964529674500227f, 0.0028180282097309828f, 0.002277818275615573f, 0.002231507794931531f, 0.0032881463412195444f, 0.0015285268891602755f, 0.003794242860749364f, 0.001968435477465391f, 0.004037444945424795f, 0.003234241157770157f, 0.0023764020297676325f, 0.004766344558447599f, 0.005914019886404276f, 0.003939946182072163f, 0.003190542571246624f, 0.0023913865443319082f, 0.0032071988098323345f, 7.812268449924886e-07f, 0.0024055903777480125f, 0.0022610914893448353f, 0.002269966062158346f, 0.004132409114390612f, 0.001746077323332429f, 0.003074906300753355f, 0.0021787642035633326f, 0.0024243565276265144f, 0.0024360467214137316f, 0.0029450790025293827f, 0.003125278977677226f, 0.0023168441839516163f, 0.002502168994396925f, 0.002373468829318881f, 0.003859843360260129f, 0.002204847289249301f, 0.0025578998029232025f, 0.0024347580038011074f, 0.003624585224315524f, 0.0035997701343148947f, 0.0025019363965839148f, 0.002677837386727333f, 0.003983968868851662f, 0.002887286711484194f, 0.002813105471432209f, 0.003480668645352125f, 0.002905583009123802f, 0.001958763925358653f, 0.0029512967448681593f, 0.004522987641394138f, 0.004329245537519455f, 0.0023250302765518427f, 0.009371642954647541f, 0.003835038049146533f, 0.003245903877541423f, 0.0029302972834557295f, 0.0025429914239794016f, 0.0032039612997323275f, 0.002486564451828599f, 0.004487730097025633f, 0.004487198311835527f, 0.002107843291014433f, 0.003877119394019246f, 0.0037446757778525352f, 0.002532136859372258f, 0.00250297412276268f, 0.0019478825852274895f);
static const ai_layer_format_type _stage4_stage4_0_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage4_stage4_0_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage4_stage4_0_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage4_stage4_0_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16 = 2;
static const ai_u16 _stage4_stage4_0_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_i8 _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = -13;
static const ai_float _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.009827804751694202f;
static const ai_float _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.03821628913283348f;
static const ai_float _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.008412139490246773f, 0.008022990077733994f, 0.008214050903916359f, 0.018845535814762115f, 0.010183148086071014f, 0.008510928601026535f, 0.011135674081742764f, 0.007591794710606337f, 0.00839099008589983f, 0.0078107453882694244f, 0.010751691646873951f, 0.007387103047221899f, 0.010204448364675045f, 0.010567734949290752f, 0.008949565701186657f, 0.008204883895814419f, 0.010281343944370747f, 0.01028219424188137f, 0.006842387840151787f, 0.0078887352719903f, 0.008388263173401356f, 0.011472790502011776f, 0.008311936631798744f, 0.008198062889277935f, 0.008278559893369675f, 0.007737727835774422f, 0.009859869256615639f, 0.00920235924422741f, 0.006388275418430567f, 0.00900105107575655f, 0.008551668375730515f, 0.007476075552403927f, 0.012231489643454552f, 0.007621948141604662f, 0.008903607726097107f, 0.009589915163815022f, 0.009360742755234241f, 0.00857597216963768f, 0.010666145011782646f, 0.011169071309268475f, 0.007709575816988945f, 0.00779148330911994f, 0.008525462821125984f, 0.008259848691523075f, 0.008979316800832748f, 0.007627350743860006f, 0.008480550721287727f, 0.009960976429283619f, 0.01086219772696495f, 0.0064392187632620335f, 0.010003294795751572f, 0.009602206759154797f, 0.008452944457530975f, 0.008799920789897442f, 0.015390843152999878f, 0.009523647837340832f, 0.00928732380270958f, 0.008377177640795708f, 0.010000071488320827f, 0.00966139417141676f, 0.008808700367808342f, 0.010190092958509922f, 0.008343882858753204f, 0.00879613682627678f, 0.00802356656640768f, 0.007899696007370949f, 0.007395217195153236f, 0.007582783233374357f, 0.011188676580786705f, 0.008038646541535854f, 0.008728149347007275f, 0.008589322678744793f, 0.009880983270704746f, 0.007772745098918676f, 0.0071245520375669f, 0.013583274558186531f, 0.011991691775619984f, 0.009419522248208523f, 0.00860193558037281f, 0.0129170510917902f, 0.010256395675241947f, 0.008366306312382221f, 0.012788006104528904f, 0.00848323479294777f, 0.007208030205219984f, 0.008727126754820347f, 0.007951252162456512f, 0.0079897940158844f, 0.009733756072819233f, 0.006352816708385944f, 0.010269774124026299f, 0.007125623524188995f, 0.009109606966376305f, 0.011797018349170685f, 0.008295300416648388f, 0.01093036774545908f);
static const ai_u16 _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 5;

static const ai_u16 _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_0_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_0_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = -13;
static const ai_i8 _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.03821628913283348f;
static const ai_float _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.017641980201005936f;
static const ai_float _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0018567848019301891f, 0.002445181366056204f, 0.002474609762430191f, 0.0016416048165410757f, 0.002456358401104808f, 0.002034979173913598f, 0.002953988965600729f, 0.0019507176475599408f, 0.0014416597550734878f, 0.001790734939277172f, 0.0022259473334997892f, 0.0019448022358119488f, 0.0012003738665953279f, 0.002073882380500436f, 0.0013704892480745912f, 0.0013643921120092273f, 0.0023884237743914127f, 0.0014741778140887618f, 0.0015338718658313155f, 0.0013879946200177073f, 0.0013142114039510489f, 0.0016951503930613399f, 0.0018271966837346554f, 0.0015065514016896486f, 0.003230605972930789f, 0.0013021552003920078f, 0.0014455210184678435f, 0.0019225263968110085f, 0.005700602661818266f, 0.0021695562172681093f, 0.0015599617036059499f, 0.0017479539383202791f, 0.0016890200786292553f, 0.0015686929691582918f, 0.0015690185828134418f, 0.001298645744100213f, 0.0016932471189647913f, 0.0024056679103523493f, 0.001810421934351325f, 0.0027066790498793125f, 0.0026031071320176125f, 0.00176511041354388f, 0.0018569856183603406f, 0.0010312872473150492f, 0.0020788274705410004f, 0.0014924373244866729f, 0.001216219854541123f, 0.0016360525041818619f, 0.0015800001565366983f, 0.0015636535827070475f, 0.003960287664085627f, 0.002229952486231923f, 0.001672196201980114f, 0.002851086901500821f, 0.0020199811551719904f, 0.002933429554104805f, 0.003967896103858948f, 0.0028949547559022903f, 0.002623504027724266f, 0.0020982264541089535f, 0.001862972043454647f, 0.003133011283352971f, 0.001309505314566195f, 0.0026582442224025726f, 0.0013239597901701927f, 0.0018110618693754077f, 0.0014276442816480994f, 0.0015532845864072442f, 0.001779094454832375f, 0.002533135237172246f, 0.0020400455687195063f, 0.0018922638846561313f, 0.0016588495345786214f, 0.001181830302812159f, 0.001954806735739112f, 0.0040889522060751915f, 0.0036489409394562244f, 0.0018674660241231322f, 0.0014527533203363419f, 0.0014665229246020317f, 0.0021477045956999063f, 0.0017777676694095135f, 0.0014478799421340227f, 0.0016742601292207837f, 0.002990003675222397f, 0.003653797088190913f, 0.0012181623606011271f, 0.0016379663720726967f, 0.002845313400030136f, 0.0022648475132882595f, 0.003947777673602104f, 0.002179945120587945f, 0.004128714557737112f, 0.002268918789923191f, 0.0032261316664516926f, 0.0014109316980466247f);
static const ai_layer_format_type _stage4_stage4_0_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_1_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_1_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.017641980201005936f;
static const ai_float _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.008073816075921059f;
static const ai_float _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0026448392309248447f, 0.0019568903371691704f, 0.002495002234354615f, 0.00351607333868742f, 0.0036831265315413475f, 0.0014250645181164145f, 0.001815258408896625f, 0.0021870662458240986f, 0.0027074096724390984f, 0.0022961185313761234f, 0.0009886056650429964f, 0.0050710891373455524f, 0.0017705329228192568f, 0.0024302571546286345f, 0.001069951569661498f, 0.0031154353637248278f, 0.0014326354721561074f, 0.002080420032143593f, 0.0020081130787730217f, 0.0017997303511947393f, 0.002364599844440818f, 0.003441884182393551f, 0.0015982396434992552f, 0.0014215509872883558f, 0.0018842447316274047f, 0.0015803788555786014f, 0.0014496054500341415f, 0.00325656495988369f, 0.0023363514337688684f, 0.0025987308472394943f, 0.0022058323957026005f, 0.002707341220229864f, 0.001971254590898752f, 0.0021298741921782494f, 0.0019840849563479424f, 0.0023025900591164827f, 0.0012782954145222902f, 0.002857701387256384f, 0.0014455056516453624f, 0.0029448331333696842f, 0.0012480852892622352f, 0.0020131149794906378f, 0.002678817370906472f, 0.001870537525974214f, 0.0027181983459740877f, 0.0016956980107352138f, 0.004467597231268883f, 0.002529675140976906f, 0.0026114629581570625f, 0.0019131971057504416f, 0.0019980180077254772f, 0.0024463734589517117f, 0.0021249568089842796f, 0.002635990036651492f, 0.002157229697331786f, 0.0027835445944219828f, 0.0016744362656027079f, 0.0011398809729143977f, 0.004358948674052954f, 0.0030432245694100857f, 0.0017150348285213113f, 0.004538679029792547f, 0.0011483357520774007f, 0.002147328108549118f, 0.002074627671390772f, 0.00203220103867352f, 0.001715123187750578f, 0.0025909075047820807f, 0.0008682425250299275f, 0.00043208495480939746f, 0.0027825951110571623f, 0.0017239291919395328f, 0.0030093835666775703f, 0.003020638832822442f, 0.0017063855193555355f, 0.00364582403562963f, 0.0021379415411502123f, 0.0014822452794760466f, 0.002172822132706642f, 0.0015510035445913672f, 0.002869587391614914f, 0.0017986777238547802f, 0.0017487992299720645f, 0.0012032587546855211f, 0.0024753441102802753f, 0.001208811765536666f, 0.002195662120357156f, 0.0009686563280411065f, 0.002246913965791464f, 0.0020148232579231262f, 0.003064580960199237f, 0.002594288671389222f, 0.002995971357449889f, 0.0023521289695054293f, 0.0017922447295859456f, 0.0020857176277786493f);
static const ai_layer_format_type _stage4_stage4_1_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage4_stage4_1_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage4_stage4_1_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage4_stage4_1_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_1_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i8 _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = -9;
static const ai_float _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.008073816075921059f;
static const ai_float _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.027435651049017906f;
static const ai_float _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.01017944049090147f, 0.025042330846190453f, 0.007441350258886814f, 0.013233976438641548f, 0.007487399969249964f, 0.021344657987356186f, 0.010761270299553871f, 0.011590821668505669f, 0.008028215728700161f, 0.013153567910194397f, 0.009962382726371288f, 0.0037340375129133463f, 0.010813526809215546f, 0.009164347313344479f, 0.012728254310786724f, 0.013567373156547546f, 0.016340132802724838f, 0.008576998487114906f, 0.00723495427519083f, 0.013413599692285061f, 0.011419513262808323f, 0.006764815654605627f, 0.01322072371840477f, 0.01562277227640152f, 0.007112883497029543f, 0.006154927890747786f, 0.021768851205706596f, 0.003946630749851465f, 0.007330869324505329f, 0.009196381084620953f, 0.007791790645569563f, 0.011372664012014866f, 0.007568105589598417f, 0.010140212252736092f, 0.010977593250572681f, 0.00856427289545536f, 0.008671567775309086f, 0.006532469764351845f, 0.011791900731623173f, 0.009001322090625763f, 0.016246214509010315f, 0.0244874469935894f, 0.007240735925734043f, 0.0048733847215771675f, 0.02592075616121292f, 0.009406622499227524f, 0.009644980542361736f, 0.010508894920349121f, 0.008538354188203812f, 0.006672060117125511f, 0.004373017232865095f, 0.011358839459717274f, 0.005562417209148407f, 0.0067791324108839035f, 0.012668851763010025f, 0.023761875927448273f, 0.00375502766110003f, 0.010560179129242897f, 0.004818389192223549f, 0.01047794334590435f, 0.01243975106626749f, 0.009252702817320824f, 0.025070441886782646f, 0.007882374338805676f, 0.008978137746453285f, 0.010687737725675106f, 0.012655464000999928f, 0.005792508367449045f, 0.037055470049381256f, 0.0631793886423111f, 0.010289371944963932f, 0.010913542471826077f, 0.012413179501891136f, 0.0050209336914122105f, 0.015351749956607819f, 0.00737842358648777f, 0.005943181924521923f, 0.013987431302666664f, 0.00765849556773901f, 0.012762753292918205f, 0.004022808745503426f, 0.003827911801636219f, 0.007589507382363081f, 0.02567553147673607f, 0.009779863059520721f, 0.030463701114058495f, 0.008060071617364883f, 0.031337857246398926f, 0.010667896829545498f, 0.003959707450121641f, 0.010178707540035248f, 0.0073273503221571445f, 0.009191635996103287f, 0.006415433716028929f, 0.019298136234283447f, 0.011515793390572071f);
static const ai_u16 _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 5;

static const ai_u16 _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_1_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_1_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = -9;
static const ai_i8 _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.027435651049017906f;
static const ai_float _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.014554073102772236f;
static const ai_float _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0020522084087133408f, 0.0014510160544887185f, 0.002409335458651185f, 0.00254241656512022f, 0.0020137301180511713f, 0.0023409288842231035f, 0.001857300754636526f, 0.0018113208934664726f, 0.0028965703677386045f, 0.0025692780036479235f, 0.0037433740217238665f, 0.0027054189704358578f, 0.0024501197040081024f, 0.0018014298984780908f, 0.0019361310405656695f, 0.0012924207840114832f, 0.002099664183333516f, 0.002604707609862089f, 0.002054431475698948f, 0.001706595765426755f, 0.0016749025089666247f, 0.0032147688325494528f, 0.0021460375282913446f, 0.001924989279359579f, 0.0018484094180166721f, 0.0016623493283987045f, 0.00171748292632401f, 0.0036090549547225237f, 0.0014995315577834845f, 0.0021340728271752596f, 0.0021431418135762215f, 0.0013851006515324116f, 0.001973729347810149f, 0.0018385187722742558f, 0.0017445995472371578f, 0.0020548119209706783f, 0.002129038330167532f, 0.0014048082521185279f, 0.013149987906217575f, 0.0015183374052867293f, 0.0010906580137088895f, 0.0014025704003870487f, 0.003065470140427351f, 0.0017337558092549443f, 0.00197501084767282f, 0.0015916383126750588f, 0.0026133775245398283f, 0.001642873277887702f, 0.005749421194195747f, 0.0028190144803375006f, 0.005521215498447418f, 0.003540572477504611f, 0.0016017009038478136f, 0.0035628555342555046f, 0.005606752820312977f, 0.003112729638814926f, 0.002747654216364026f, 0.0027096723206341267f, 0.004194209817796946f, 0.0028988283593207598f, 0.0025478266179561615f, 0.002148452214896679f, 0.003006042679771781f, 0.002826663199812174f, 0.001979314023628831f, 0.002209907164797187f, 0.0026869422290474176f, 0.0023531122133135796f, 0.0024615402799099684f, 0.0017707787919789553f, 0.002291050273925066f, 0.0035269048530608416f, 0.0031574058812111616f, 0.0021316574420779943f, 0.0030453212093561888f, 0.0020073293708264828f, 0.0018252284498885274f, 0.003926076926290989f, 0.0022380356676876545f, 0.0031725489534437656f, 0.0026622721925377846f, 0.003048159182071686f, 0.0037695367354899645f, 0.0032584567088633776f, 0.00466013140976429f, 0.0037621089722961187f, 0.001947130891494453f, 0.0042725116945803165f, 0.002404030878096819f, 0.0020619418937712908f, 0.002064388943836093f, 0.002820349531248212f, 0.0024048190098255873f, 0.0042341323569417f, 0.0029398060869425535f, 0.002207042882218957f);
static const ai_layer_format_type _stage4_stage4_1_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_2_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_2_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.014554073102772236f;
static const ai_float _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.009506400674581528f;
static const ai_float _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002086290856823325f, 0.002020586049184203f, 0.002697936026379466f, 0.002560086315497756f, 0.00301336869597435f, 0.0030430208425968885f, 0.001991033088415861f, 0.002319144317880273f, 0.0016771750524640083f, 0.0031099531333893538f, 0.0018431257922202349f, 0.0032775707077234983f, 0.0020073852501809597f, 0.004735616035759449f, 0.00281545496545732f, 0.001960981637239456f, 0.0023008682765066624f, 0.0014032580656930804f, 0.001955736428499222f, 0.0017409514402970672f, 0.0017723242053762078f, 0.002317465376108885f, 0.0025644113775342703f, 0.0020902843680232763f, 0.002091551898047328f, 0.0035845832899212837f, 0.0036581053864210844f, 0.0033873862121254206f, 0.005401868373155594f, 0.004907271359115839f, 0.0013386863283813f, 0.0013224297435954213f, 0.0032001640647649765f, 0.0017316760495305061f, 0.0015474726678803563f, 0.002323104767128825f, 0.003685584058985114f, 0.001629517413675785f, 0.0032034385949373245f, 0.001706905197352171f, 0.002793183783069253f, 0.0024510561488568783f, 0.0028567889239639044f, 0.0023091230541467667f, 0.0024497832637280226f, 0.0025516939349472523f, 0.001844235579483211f, 0.002064376836642623f, 0.0020029922015964985f, 0.0025565391406416893f, 0.0025832471437752247f, 0.002082593971863389f, 0.004105782136321068f, 0.002766817342489958f, 0.001922413008287549f, 0.0027264209929853678f, 0.0021941433660686016f, 0.0020966327283531427f, 0.002076047705486417f, 0.0023404001258313656f, 0.0027390476316213608f, 0.0017262025503441691f, 0.00232895789667964f, 0.00257686129771173f, 0.0017967286985367537f, 0.0025060181505978107f, 0.0020816735923290253f, 0.00269042095169425f, 0.002903018845245242f, 0.002699870616197586f, 0.0020062162075191736f, 0.003408211749047041f, 0.003437619423493743f, 0.0018004492158070207f, 0.002602232387289405f, 0.0012673569144681096f, 0.0043581463396549225f, 0.0017593123484402895f, 0.001774194068275392f, 0.0018794434145092964f, 0.0021308823488652706f, 0.002679032040759921f, 0.003150588134303689f, 0.0013476456515491009f, 0.002261565299704671f, 0.003152597462758422f, 0.00522751035168767f, 0.002609078073874116f, 0.0031140288338065147f, 0.003459976287558675f, 0.0024540575686842203f, 0.0025243666023015976f, 0.001454466488212347f, 0.001223949366249144f, 0.0021779590751975775f, 0.0014865301782265306f);
static const ai_layer_format_type _stage4_stage4_2_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage4_stage4_2_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage4_stage4_2_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage4_stage4_2_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_2_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i8 _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = 16;
static const ai_float _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.009506400674581528f;
static const ai_float _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.02519826591014862f;
static const ai_float _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.007438926957547665f, 0.006380186416208744f, 0.014787202700972557f, 0.01772022247314453f, 0.006477916147559881f, 0.016721211373806f, 0.007048406172543764f, 0.01069890707731247f, 0.025736290961503983f, 0.007283189799636602f, 0.011577297933399677f, 0.01499625202268362f, 0.005227239336818457f, 0.011230308562517166f, 0.01057521253824234f, 0.007246061693876982f, 0.0077298544347286224f, 0.014561906456947327f, 0.013404633849859238f, 0.018129387870430946f, 0.016586249694228172f, 0.008431750349700451f, 0.012598968110978603f, 0.012554535642266273f, 0.007546969689428806f, 0.014966391958296299f, 0.023100702092051506f, 0.008211561478674412f, 0.008233929984271526f, 0.008760638535022736f, 0.011289934627711773f, 0.015835966914892197f, 0.015992194414138794f, 0.02941872552037239f, 0.009377166628837585f, 0.019660377874970436f, 0.0069269523955881596f, 0.017913367599248886f, 0.00623429985716939f, 0.018445735797286034f, 0.007557024713605642f, 0.013406099751591682f, 0.01863732747733593f, 0.014856298454105854f, 0.012424138374626637f, 0.009191682562232018f, 0.012088370509445667f, 0.014312018640339375f, 0.012961485423147678f, 0.006315730977803469f, 0.0106404023244977f, 0.010173863731324673f, 0.01246030442416668f, 0.011351511813700199f, 0.004501641262322664f, 0.009801031090319157f, 0.011855418793857098f, 0.010725077241659164f, 0.012832876294851303f, 0.01296236738562584f, 0.015512919053435326f, 0.006215004716068506f, 0.022069072350859642f, 0.012810010462999344f, 0.007223029620945454f, 0.014790129847824574f, 0.010365564376115799f, 0.012722820043563843f, 0.0073747290298342705f, 0.007236708886921406f, 0.005802449770271778f, 0.014494229108095169f, 0.02159997820854187f, 0.011037417687475681f, 0.01395062729716301f, 0.026760360226035118f, 0.014828776940703392f, 0.009688498452305794f, 0.011762231588363647f, 0.0050402008928358555f, 0.00466951122507453f, 0.013302279636263847f, 0.011640052311122417f, 0.023070765659213066f, 0.00474472064524889f, 0.00534724909812212f, 0.008568146266043186f, 0.009607786312699318f, 0.0051276725716888905f, 0.012144836597144604f, 0.014208352193236351f, 0.009360020980238914f, 0.010950928553938866f, 0.013159597292542458f, 0.005900468677282333f, 0.013196268118917942f);
static const ai_u16 _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 5;

static const ai_u16 _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_2_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_2_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = 16;
static const ai_i8 _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.02519826591014862f;
static const ai_float _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.012706417590379715f;
static const ai_float _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.001839747535996139f, 0.0019960077479481697f, 0.0015516828279942274f, 0.0022554155439138412f, 0.0016745352186262608f, 0.001767647103406489f, 0.001773702446371317f, 0.0016762871528044343f, 0.0026091954205185175f, 0.001971705351024866f, 0.0028597197961062193f, 0.0017952007474377751f, 0.0015892995288595557f, 0.0022955939639359713f, 0.0017261330503970385f, 0.0018524003680795431f, 0.0016502910293638706f, 0.0019609881564974785f, 0.0019768369384109974f, 0.002197262132540345f, 0.0017292567063122988f, 0.0016075829043984413f, 0.0017396163893863559f, 0.002105102641507983f, 0.0019925334490835667f, 0.002935983007773757f, 0.0018159485189244151f, 0.002133911708369851f, 0.0018549042288213968f, 0.0014506911393254995f, 0.002545568160712719f, 0.002320897066965699f, 0.0020314117427915335f, 0.001857905532233417f, 0.0018988966476172209f, 0.0017858707578852773f, 0.0016176220960915089f, 0.0017475534696131945f, 0.0019817170687019825f, 0.0024765566922724247f, 0.0021706484258174896f, 0.0016810980159789324f, 0.0019023192580789328f, 0.0027154607232660055f, 0.0020287330262362957f, 0.002627926878631115f, 0.0026516029611229897f, 0.0019095808966085315f, 0.0025986686814576387f, 0.0017311656847596169f, 0.0029950120951980352f, 0.0025771951768547297f, 0.0024074323009699583f, 0.001692913006991148f, 0.001615450601093471f, 0.0013481074711307883f, 0.001891875988803804f, 0.0018922556191682816f, 0.002841642126441002f, 0.0022864523343741894f, 0.0016146519919857383f, 0.0024369433522224426f, 0.0019615855999290943f, 0.0017136450624093413f, 0.002422020072117448f, 0.0031708667520433664f, 0.002157694660127163f, 0.002159806666895747f, 0.001949061406776309f, 0.00204537995159626f, 0.0019701665733009577f, 0.002176620764657855f, 0.0022893857676535845f, 0.0029089637100696564f, 0.0014386132825165987f, 0.002003474161028862f, 0.0020987780299037695f, 0.002243876690044999f, 0.001848973915912211f, 0.0016030245460569859f, 0.002142318058758974f, 0.0036810245364904404f, 0.0022439544554799795f, 0.0026989183388650417f, 0.002481447532773018f, 0.001560242846608162f, 0.0017551016062498093f, 0.0031268722377717495f, 0.0015255368780344725f, 0.0023854058235883713f, 0.0027843299321830273f, 0.001873566652648151f, 0.0020515441428869963f, 0.0013855525758117437f, 0.002436273032799363f, 0.0023384252563118935f);
static const ai_layer_format_type _stage4_stage4_2_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_3_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_3_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.014554073102772236f;
static const ai_float _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.008323338814079762f;
static const ai_float _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.003391725942492485f, 0.002190872561186552f, 0.002590541960671544f, 0.0022519391495734453f, 0.0035532047040760517f, 0.0016963199013844132f, 0.0019382884493097663f, 0.002361530903726816f, 0.0016188869485631585f, 0.0018363860435783863f, 0.0021185341756790876f, 0.002104616491124034f, 0.0022517056204378605f, 0.002160795032978058f, 0.0020379391498863697f, 0.003128474811092019f, 0.002185214776545763f, 0.0029602099675685167f, 0.0027498085983097553f, 0.002561022061854601f, 0.0016400797758251429f, 0.0025034607388079166f, 0.002793279243633151f, 0.0030290468130260706f, 0.0021809302270412445f, 0.003647817065939307f, 0.0026350333355367184f, 0.0019052326679229736f, 0.003005610080435872f, 0.0031687645241618156f, 0.0028300676494836807f, 0.0016335741383954883f, 0.0017870248993858695f, 0.002933447016403079f, 0.0018081112066283822f, 0.0029424820095300674f, 0.002835628343746066f, 0.0019340182188898325f, 0.00290622073225677f, 0.0015836791135370731f, 0.002589569892734289f, 0.0018257462652400136f, 0.0029273026157170534f, 0.0018133624689653516f, 0.002866082824766636f, 0.0025066437665373087f, 0.0022880190517753363f, 0.0019452377455309033f, 0.0021687543485313654f, 0.0021735397167503834f, 0.0024434165097773075f, 0.001922402298077941f, 0.0019266032613813877f, 0.0017783577786758542f, 0.0016663790447637439f, 0.0017988006584346294f, 0.002712630433961749f, 0.004931020550429821f, 0.002500152913853526f, 0.0018059407593682408f, 0.0013942118966951966f, 0.0027505140751600266f, 0.002118387259542942f, 0.002575506689026952f, 0.0021730009466409683f, 0.0024037063121795654f, 0.00246648327447474f, 0.0019016519654542208f, 0.00232292409054935f, 0.001791230752132833f, 0.002399310003966093f, 0.001377762877382338f, 0.0015094083501026034f, 0.0017142118886113167f, 0.0037646444980055094f, 0.0015507021453231573f, 0.0017661114688962698f, 0.0026806581299751997f, 0.0038177845999598503f, 0.0033245186787098646f, 0.002075577387586236f, 0.0018937216373160481f, 0.0037631315644830465f, 0.0031982457730919123f, 0.0028281453996896744f, 0.0017938679084181786f, 0.002000943524762988f, 0.00246849050745368f, 0.002093431307002902f, 0.0026674496475607157f, 0.003419524524360895f, 0.0026152459904551506f, 0.0021660856436938047f, 0.0030810609459877014f, 0.0035400560591369867f, 0.0027451675850898027f);
static const ai_layer_format_type _stage4_stage4_3_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_i32 _stage4_stage4_3_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _stage4_stage4_3_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stage4_stage4_3_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_3_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i8 _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = 23;
static const ai_float _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.008323338814079762f;
static const ai_float _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.03432915359735489f;
static const ai_float _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.01739976368844509f, 0.007945209741592407f, 0.011774342507123947f, 0.012328943237662315f, 0.007598890457302332f, 0.020092371851205826f, 0.010551933199167252f, 0.008260057307779789f, 0.005911943037062883f, 0.010323808528482914f, 0.025555578991770744f, 0.020274145528674126f, 0.015306822024285793f, 0.02385832369327545f, 0.004890017211437225f, 0.03081275150179863f, 0.01183405239135027f, 0.016137797385454178f, 0.0036942441947758198f, 0.007448324002325535f, 0.006730974651873112f, 0.01500996109098196f, 0.010584198869764805f, 0.012925011105835438f, 0.006986754946410656f, 0.01580895483493805f, 0.009310758672654629f, 0.006244136951863766f, 0.008242463693022728f, 0.01218251883983612f, 0.015308487229049206f, 0.025942517444491386f, 0.025149978697299957f, 0.004559757653623819f, 0.005313108209520578f, 0.011560176499187946f, 0.008780677802860737f, 0.015860646963119507f, 0.017625480890274048f, 0.008670452982187271f, 0.010267348028719425f, 0.028894692659378052f, 0.00677750026807189f, 0.011313661001622677f, 0.0113111836835742f, 0.010764434933662415f, 0.007853087037801743f, 0.015208098106086254f, 0.01725485734641552f, 0.005598291754722595f, 0.010400816798210144f, 0.00732080964371562f, 0.014234292320907116f, 0.0058116307482123375f, 0.008506527170538902f, 0.008154264651238918f, 0.012924827635288239f, 0.011112089268863201f, 0.010127686895430088f, 0.020518573001027107f, 0.02302205003798008f, 0.017304860055446625f, 0.004921854007989168f, 0.013318061828613281f, 0.016966629773378372f, 0.014098732732236385f, 0.009663255885243416f, 0.012203008867800236f, 0.010398434475064278f, 0.013264764100313187f, 0.013168235309422016f, 0.026096483692526817f, 0.021470818668603897f, 0.009252499788999557f, 0.014810851775109768f, 0.014731266535818577f, 0.006892394740134478f, 0.018674200400710106f, 0.019800839945673943f, 0.0064661237411201f, 0.012975698336958885f, 0.010108762420713902f, 0.010468370281159878f, 0.011456944048404694f, 0.010361099615693092f, 0.009300267323851585f, 0.014177060686051846f, 0.0073135304264724255f, 0.004911910742521286f, 0.007834360934793949f, 0.015094855800271034f, 0.010577509179711342f, 0.019441237673163414f, 0.008964058011770248f, 0.013137695379555225f, 0.013317970559000969f);
static const ai_u16 _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 5;

static const ai_u16 _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_3_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_3_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = 23;
static const ai_i8 _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.03432915359735489f;
static const ai_float _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.019763272255659103f;
static const ai_float _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0023311665281653404f, 0.0027785575948655605f, 0.003400043584406376f, 0.0021308870054781437f, 0.002271415200084448f, 0.0027732602320611477f, 0.0025413171388208866f, 0.004834925290197134f, 0.0022472713608294725f, 0.0028925102669745684f, 0.0023065158165991306f, 0.002626510104164481f, 0.004554999526590109f, 0.00168702588416636f, 0.0021065790206193924f, 0.0026770413387566805f, 0.0018012311775237322f, 0.002388948341831565f, 0.0026223245076835155f, 0.0019092111615464091f, 0.0032270525116473436f, 0.0017502460395917296f, 0.0016439597820863128f, 0.0017593540251255035f, 0.0023995430674403906f, 0.0022226504515856504f, 0.00267589557915926f, 0.002196564571931958f, 0.0029306316282600164f, 0.0018727888818830252f, 0.0020425301045179367f, 0.002739507704973221f, 0.002709360793232918f, 0.0015062630409374833f, 0.003438372164964676f, 0.0021639845799654722f, 0.002342062769457698f, 0.0019339370774105191f, 0.003185790963470936f, 0.0030965530313551426f, 0.0030686070676892996f, 0.00218696566298604f, 0.0016173659823834896f, 0.0025476745795458555f, 0.0017456957139074802f, 0.0019034045981243253f, 0.0017852660967037082f, 0.002099889563396573f, 0.001572447014041245f, 0.001770535483956337f, 0.0019944540690630674f, 0.0022064498625695705f, 0.0021454612724483013f, 0.0021857970859855413f, 0.002619207836687565f, 0.001889675622805953f, 0.0019254190847277641f, 0.002428783569484949f, 0.0016396583523601294f, 0.0016549446154385805f, 0.0018837908282876015f, 0.0018008181359618902f, 0.0027502798475325108f, 0.0018704552203416824f, 0.0020930601749569178f, 0.0021734347101300955f, 0.0021495658438652754f, 0.0015713395550847054f, 0.003010815242305398f, 0.0028963384684175253f, 0.002320915926247835f, 0.0025429213419556618f, 0.0017780010821297765f, 0.0029366393573582172f, 0.002564986702054739f, 0.002040422987192869f, 0.0021759257651865482f, 0.0025825004559010267f, 0.002103843493387103f, 0.0016742118168622255f, 0.0018836434464901686f, 0.002591898897662759f, 0.002415559720247984f, 0.0029922672547399998f, 0.002889912808313966f, 0.0018503473838791251f, 0.002170555992051959f, 0.0026466462295502424f, 0.0022308751940727234f, 0.002870058873668313f, 0.0033152734395116568f, 0.001831704517826438f, 0.002956008305773139f, 0.002532611135393381f, 0.0018930237274616957f, 0.0017803504597395658f);
static const ai_layer_format_type _stage4_stage4_3_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;



static const ai_u16 _conv5_conv5_2_Relu_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _conv5_conv5_2_Relu_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _conv5_conv5_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _conv5_conv5_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _conv5_conv5_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 192;
static const ai_u16 _conv5_conv5_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 1024;
static const ai_i8 _conv5_conv5_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _conv5_conv5_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _conv5_conv5_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.019763272255659103f;
static const ai_float _conv5_conv5_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.018465526401996613f;
static const ai_float _conv5_conv5_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002295296173542738f, 0.00182308629155159f, 0.002316379686817527f, 0.002030057366937399f, 0.0026465794071555138f, 0.0019330319482833147f, 0.00300991372205317f, 0.0016325273318216205f, 0.002352637704461813f, 0.00251802708953619f, 0.003200152888894081f, 0.0017217015847563744f, 0.002661449369043112f, 0.0021309887524694204f, 0.0021789672318845987f, 0.0027000615373253822f, 0.0020675985142588615f, 0.0021456750109791756f, 0.0021544736810028553f, 0.0021040942519903183f, 0.001980155473574996f, 0.0021255365572869778f, 0.0021346178837120533f, 0.0024560699239373207f, 0.002712877234444022f, 0.002272249897941947f, 0.00230846437625587f, 0.001472335192374885f, 0.0020269793458282948f, 0.0019617462530732155f, 0.0017841686494648457f, 0.0021294851321727037f, 0.003010775661095977f, 0.002086226362735033f, 0.0023768325336277485f, 0.00169406458735466f, 0.0018815259682014585f, 0.001605140627361834f, 0.0012234493624418974f, 0.0025216012727469206f, 0.0022746147587895393f, 0.00276359636336565f, 0.002776393899694085f, 0.0018243519589304924f, 0.0017653129762038589f, 0.0019882586784660816f, 0.0023709945380687714f, 0.0036429264582693577f, 0.0036141404416412115f, 0.0017214377876371145f, 0.0019327477784827352f, 0.0019173668697476387f, 0.0020607267506420612f, 0.0016055222367867827f, 0.0018979569431394339f, 0.0020906440913677216f, 0.004060322418808937f, 0.0018741351086646318f, 0.002738782437518239f, 0.002500232309103012f, 0.0034280624240636826f, 0.0016532248118892312f, 0.004242577590048313f, 0.00204277690500021f, 0.001658506109379232f, 0.002578948624432087f, 0.002723289653658867f, 0.002380414167419076f, 0.0022795286495238543f, 0.0024145047646015882f, 0.002363055944442749f, 0.0031377701088786125f, 0.002224449533969164f, 0.002609469462186098f, 0.0033020752016454935f, 0.0034603483509272337f, 0.0022009736858308315f, 0.0035224752500653267f, 0.0021433294750750065f, 0.0021570760291069746f, 0.0021869922056794167f, 0.0020369309931993484f, 0.0026430245488882065f, 0.004796812776476145f, 0.0022807244677096605f, 0.002827158197760582f, 0.002427516272291541f, 0.0027121088933199644f, 0.002543815178796649f, 0.001775107579305768f, 0.0015781167894601822f, 0.0023176821414381266f, 0.0017925091087818146f, 0.0021039906423538923f, 0.0020712155383080244f, 0.002132850233465433f, 0.0026843491941690445f, 0.0020572207868099213f, 0.001890822546556592f, 0.002528425073251128f, 0.001733307959511876f, 0.0021279931534081697f, 0.0024513474199920893f, 0.0016624475829303265f, 0.002072724048048258f, 0.0021440680138766766f, 0.002696962794288993f, 0.003499770537018776f, 0.002349626738578081f, 0.0018879951676353812f, 0.001602516626007855f, 0.0020747145172208548f, 0.00256096082739532f, 0.00345664843916893f, 0.003453838638961315f, 0.001350231235846877f, 0.001984764588996768f, 0.0016604542033746839f, 0.0017264774069190025f, 0.002859565895050764f, 0.0020986006129533052f, 0.0022063027136027813f, 0.0019833866972476244f, 0.0039507364854216576f, 0.002284941729158163f, 0.002480428433045745f, 0.0016336641274392605f, 0.0043753767386078835f, 0.0024849737528711557f, 0.002570736687630415f, 0.002460143296048045f, 0.002049903152510524f, 0.001819899189285934f, 0.0019967961125075817f, 0.0024038723204284906f, 0.0018838609103113413f, 0.0015720638912171125f, 0.001874356297776103f, 0.0030785701237618923f, 0.0036423043347895145f, 0.0022564369719475508f, 0.0019432707922533154f, 0.0043331035412848f, 0.001729062874801457f, 0.002168054925277829f, 0.002693388843908906f, 0.0025171376764774323f, 0.0022933045402169228f, 0.0029234695248305798f, 0.0016495116287842393f, 0.0023268316872417927f, 0.0008336809114553034f, 0.0017559747211635113f, 0.001489741262048483f, 0.0016436130972579122f, 0.003046673024073243f, 0.0018160949693992734f, 0.0024743860121816397f, 0.0023139454424381256f, 0.0016186106950044632f, 0.002106869826093316f, 0.0013818115694448352f, 0.0014173779636621475f, 0.001956752734258771f, 0.002323478227481246f, 0.0033193230628967285f, 0.0017748306272551417f, 0.002116061979904771f, 0.0033717150799930096f, 0.002170221647247672f, 0.0024095934350043535f, 0.0027760907541960478f, 0.002353163668885827f, 0.005938205402344465f, 0.003650820115581155f, 0.001767676672898233f, 0.002079774159938097f, 0.00179138348903507f, 0.0025174403563141823f, 0.0034131540451198816f, 0.002829169388860464f, 0.002037049736827612f, 0.003277915995568037f, 0.0017868754221126437f, 0.0021305205300450325f, 0.002018395811319351f, 0.003129885531961918f, 0.0025689557660371065f, 0.0024940301664173603f, 0.0014239964075386524f, 0.002580973319709301f, 0.002823753049597144f, 0.0017324724467471242f, 0.0017923791892826557f, 0.0030504250898957253f, 0.002228223718702793f, 0.0018128426745533943f, 0.0018215513555333018f, 0.002720808144658804f, 0.0019325371831655502f, 0.002107503591105342f, 0.002968007931485772f, 0.0018216423923149705f, 0.0017512653721496463f, 0.0021858818363398314f, 0.0018755949568003416f, 0.00236223335377872f, 0.002274769125506282f, 0.0018520363373681903f, 0.0023811738938093185f, 0.0027390422765165567f, 0.0020515823271125555f, 0.0021807581651955843f, 0.0023749633692204952f, 0.0020250410307198763f, 0.002743165474385023f, 0.002069015521556139f, 0.0031610873993486166f, 0.002208263147622347f, 0.002572663826867938f, 0.00200649187900126f, 0.00203363923355937f, 0.0033014246728271246f, 0.0021153560373932123f, 0.0027043449226766825f, 0.002993997884914279f, 0.0018038105918094516f, 0.0022369851358234882f, 0.0018826432060450315f, 0.0021964998450130224f, 0.0015113645931705832f, 0.002883635926991701f, 0.002233839360997081f, 0.001884730882011354f, 0.0028775304090231657f, 0.001650041900575161f, 0.002489744918420911f, 0.0025425732601433992f, 0.002128352178260684f, 0.001640931237488985f, 0.0025324122980237007f, 0.002068002475425601f, 0.0024119887966662645f, 0.002484719967469573f, 0.0024401589762419462f, 0.0015587760135531425f, 0.0026282425969839096f, 0.0018283214885741472f, 0.0020566394086927176f, 0.0025061985943466425f, 0.0017405429389327765f, 0.0018902553711086512f, 0.0021101548336446285f, 0.001871401909738779f, 0.0025147462729364634f, 0.0019631132017821074f, 0.002239234047010541f, 0.0020569683983922005f, 0.0022890616673976183f, 0.0023293658159673214f, 0.0020823979284614325f, 0.0031615353655070066f, 0.0016876515001058578f, 0.004041690845042467f, 0.0020452160388231277f, 0.0038187874015420675f, 0.001985265640541911f, 0.0018996030557900667f, 0.0026090035680681467f, 0.002479646820574999f, 0.0018105068011209369f, 0.0019126388942822814f, 0.0018710922449827194f, 0.0021290841978043318f, 0.0025272525381296873f, 0.0020636403933167458f, 0.0014821967342868447f, 0.0013925370294600725f, 0.0033681674394756556f, 0.0018908947240561247f, 0.0031349461060017347f, 0.0016591427847743034f, 0.0018513593822717667f, 0.002440906595438719f, 0.0015506348572671413f, 0.002742875600233674f, 0.002314490033313632f, 0.0023937285877764225f, 0.0015806086594238877f, 0.0016194370109587908f, 0.0030403754208236933f, 0.0027155783027410507f, 0.003588529769331217f, 0.001995945116505027f, 0.0018489658832550049f, 0.002490569604560733f, 0.0017987058963626623f, 0.002031081821769476f, 0.003332177409902215f, 0.002157178008928895f, 0.0018213933799415827f, 0.0027645397931337357f, 0.002780216047540307f, 0.002308052033185959f, 0.001778442645445466f, 0.0024421021807938814f, 0.002261702436953783f, 0.002793350489810109f, 0.002252019941806793f, 0.0021385145373642445f, 0.0017792400903999805f, 0.002032236894592643f, 0.00367077742703259f, 0.0015853422228246927f, 0.0020993335638195276f, 0.002537194173783064f, 0.0019414117559790611f, 0.0016404356574639678f, 0.0026212420780211687f, 0.0023041970562189817f, 0.0016614228952676058f, 0.002043534768745303f, 0.002060887636616826f, 0.0034165780525654554f, 0.0017216266132891178f, 0.001913327258080244f, 0.0025957273319363594f, 0.0023379509802907705f, 0.0022042621858417988f, 0.0018695187754929066f, 0.002420423785224557f, 0.0019250509794801474f, 0.00220029940828681f, 0.001699598622508347f, 0.002673147479072213f, 0.0013626773143187165f, 0.0019063904182985425f, 0.001728090806864202f, 0.003453937591984868f, 0.0017343360232189298f, 0.002675387542694807f, 0.0017328759422525764f, 0.0025375070981681347f, 0.0033261142671108246f, 0.002106031868606806f, 0.002276221988722682f, 0.0014771110145375133f, 0.0018036256078630686f, 0.0017503878334537148f, 0.002159889554604888f, 0.0019827554933726788f, 0.0015295798657462f, 0.0021858292166143656f, 0.0018137076403945684f, 0.0021297945640981197f, 0.0027482116129249334f, 0.0019752178341150284f, 0.0021075247786939144f, 0.002383981831371784f, 0.002651310060173273f, 0.002410483080893755f, 0.0023348615504801273f, 0.0023293595295399427f, 0.002362433820962906f, 0.003299863077700138f, 0.001682125381194055f, 0.004113704431802034f, 0.0018078065477311611f, 0.002003689529374242f, 0.0019130107248201966f, 0.0017706634243950248f, 0.0018746795831248164f, 0.0025323759764432907f, 0.002308894880115986f, 0.00395283242687583f, 0.002312714233994484f, 0.002003174740821123f, 0.002263793721795082f, 0.0028819662984460592f, 0.0017143997829407454f, 0.002862136345356703f, 0.002936349017545581f, 0.0018708603456616402f, 0.001928088255226612f, 0.0019015531288459897f, 0.002214990556240082f, 0.0015075001865625381f, 0.0020513064227998257f, 0.0022600130178034306f, 0.003428591648116708f, 0.001975288148969412f, 0.002134936396032572f, 0.0014707844238728285f, 0.001767382607795298f, 0.002188354264944792f, 0.0033905713353306055f, 0.0018944237381219864f, 0.0015979266026988626f, 0.0013647527666762471f, 0.002550860634073615f, 0.002811036305502057f, 0.0025322376750409603f, 0.002107855398207903f, 0.0022844146005809307f, 0.0016511643771082163f, 0.002126491628587246f, 0.002185565186664462f, 0.002569009782746434f, 0.0020216787233948708f, 0.003414742648601532f, 0.0028796186670660973f, 0.0018851911881938577f, 0.002473869826644659f, 0.00197285576723516f, 0.002222123323008418f, 0.002674709539860487f, 0.0018383435672149062f, 0.0019338405691087246f, 0.002213353058323264f, 0.002611905802041292f, 0.0020356173627078533f, 0.0019003023626282811f, 0.0017893974436447024f, 0.0024905558675527573f, 0.0022853410337120295f, 0.0025877307634800673f, 0.0014743393985554576f, 0.002041906351223588f, 0.0019026478985324502f, 0.00210635457187891f, 0.0015672716544941068f, 0.001839705160818994f, 0.003135345410555601f, 0.0031206130515784025f, 0.0035591451451182365f, 0.002403841819614172f, 0.0023816979955881834f, 0.001743655069731176f, 0.001768781105056405f, 0.0015370240435004234f, 0.002027256181463599f, 0.0020951067563146353f, 0.0019325847970321774f, 0.0020913491025567055f, 0.002275855978950858f, 0.002101533580571413f, 0.0014561685966327786f, 0.0027584221679717302f, 0.002482468029484153f, 0.0024981133174151182f, 0.0021434396039694548f, 0.002202931558713317f, 0.002081192098557949f, 0.002670485293492675f, 0.002432245993986726f, 0.0021331205498427153f, 0.0026208937633782625f, 0.0022940414492040873f, 0.002137598115950823f, 0.0022209591697901487f, 0.0017391174333170056f, 0.0023743805941194296f, 0.001740474603138864f, 0.0021549807861447334f, 0.0025001908652484417f, 0.0025164377875626087f, 0.0030330554582178593f, 0.0023141696583479643f, 0.0015014399541541934f, 0.0030823133420199156f, 0.0022101474460214376f, 0.001578467315994203f, 0.0012730328598991036f, 0.0019732206128537655f, 0.0035096118226647377f, 0.002365952590480447f, 0.0020146132446825504f, 0.00149041716940701f, 0.0028179967775940895f, 0.0022892800625413656f, 0.002417814452201128f, 0.0019963134545832872f, 0.00186050811316818f, 0.002454272238537669f, 0.002502753399312496f, 0.003657027380540967f, 0.003054148517549038f, 0.0013269201153889298f, 0.001001653727144003f, 0.0023430287837982178f, 0.002335751662030816f, 0.002467086771503091f, 0.004829736426472664f, 0.002135263755917549f, 0.0025895487051457167f, 0.0020439268555492163f, 0.001628360478207469f, 0.003275289898738265f, 0.0017448548460379243f, 0.0018081504385918379f, 0.001968394499272108f, 0.0024742279201745987f, 0.0033495896495878696f, 0.0018704456742852926f, 0.002297536935657263f, 0.0031237711664289236f, 0.0013759947614744306f, 0.0017142208525910974f, 0.0019359108991920948f, 0.001767028123140335f, 0.002465231576934457f, 0.0019610507879406214f, 0.0016871239058673382f, 0.0020551937632262707f, 0.0021924851462244987f, 0.0022732880897819996f, 0.0034226777497678995f, 0.0014217806747183204f, 0.002984542865306139f, 0.0019994620233774185f, 0.002700324635952711f, 0.0024790605530142784f, 0.0013851870317012072f, 0.002569108037278056f, 0.002034903969615698f, 0.0015727718127891421f, 0.002852918114513159f, 0.0026076408103108406f, 0.0017847067210823298f, 0.00247870828025043f, 0.002467912156134844f, 0.0014845144469290972f, 0.002860275562852621f, 0.0022242958657443523f, 0.0031355249229818583f, 0.00165548303630203f, 0.0021115734707564116f, 0.002488858764991164f, 0.0017333425348624587f, 0.002286948263645172f, 0.002186911879107356f, 0.002109877998009324f, 0.0015653466107323766f, 0.002499170135706663f, 0.0023981903214007616f, 0.0025543656665831804f, 0.0025895354337990284f, 0.002106080297380686f, 0.002373424358665943f, 0.0019529495621100068f, 0.004333803430199623f, 0.0020963840652257204f, 0.0023847201373428106f, 0.0020572738721966743f, 0.002197110326960683f, 0.0023278482258319855f, 0.0015595833538100123f, 0.001705012284219265f, 0.0015747161814942956f, 0.006640979088842869f, 0.0021749946754425764f, 0.002337002195417881f, 0.0025278644170612097f, 0.002142725745216012f, 0.0019984873943030834f, 0.002461245283484459f, 0.001478387974202633f, 0.0022827410139143467f, 0.0020974117796868086f, 0.0023259962908923626f, 0.0016972952289506793f, 0.0035910075530409813f, 0.0021219556219875813f, 0.002847900614142418f, 0.0027164206840097904f, 0.0018779343226924539f, 0.002350609051063657f, 0.0021387250162661076f, 0.002463315147906542f, 0.002159423427656293f, 0.002293077763170004f, 0.0021186729427427053f, 0.004517423454672098f, 0.003047838108614087f, 0.002468056743964553f, 0.0015241539804264903f, 0.001972907455638051f, 0.0019620973616838455f, 0.0020579288247972727f, 0.0019461754709482193f, 0.002288732212036848f, 0.002566218376159668f, 0.0017966864397749305f, 0.0017448477447032928f, 0.003043788019567728f, 0.002055839402601123f, 0.0036484107840806246f, 0.0033200678881257772f, 0.0021367266308516264f, 0.0023801964707672596f, 0.001363759976811707f, 0.003070735139772296f, 0.002742224372923374f, 0.002966986270621419f, 0.0022309236228466034f, 0.0023489061277359724f, 0.002104931976646185f, 0.001608876627869904f, 0.002308733994141221f, 0.001359304180368781f, 0.0024889041669666767f, 0.0020040252711623907f, 0.002214999170973897f, 0.002041684463620186f, 0.002472902415320277f, 0.002903575077652931f, 0.002468028338626027f, 0.002222060225903988f, 0.002537623280659318f, 0.0019916752353310585f, 0.0026452967431396246f, 0.0025653501506894827f, 0.002145966747775674f, 0.002556646941229701f, 0.0021866029128432274f, 0.0036866164300590754f, 0.002835954772308469f, 0.002173088723793626f, 0.0027075742837041616f, 0.0032577605452388525f, 0.002281090710312128f, 0.0015126148937270045f, 0.0020603640004992485f, 0.0028893689159303904f, 0.0023056038189679384f, 0.0030484762974083424f, 0.0021125483326613903f, 0.0016808988293632865f, 0.0017025326378643513f, 0.002128557302057743f, 0.0020770870614796877f, 0.0016633053310215473f, 0.00356217916123569f, 0.0017248850781470537f, 0.004005268216133118f, 0.001882839947938919f, 0.004267073702067137f, 0.001823159633204341f, 0.0019040957558900118f, 0.002107609063386917f, 0.0031269588507711887f, 0.0017171363579109311f, 0.003175164572894573f, 0.002339688828215003f, 0.001879729563370347f, 0.0018922088202089071f, 0.00257592904381454f, 0.0030115547124296427f, 0.003059031441807747f, 0.0021324504632502794f, 0.003227697918191552f, 0.002378724282607436f, 0.0017150947824120522f, 0.0023134786169975996f, 0.0022387374192476273f, 0.002062368905171752f, 0.002162482589483261f, 0.001693455153144896f, 0.003111185971647501f, 0.0020386134274303913f, 0.001627139514312148f, 0.002047652378678322f, 0.0015234380261972547f, 0.0019732885994017124f, 0.0023535252548754215f, 0.0017244296614080667f, 0.003690494457259774f, 0.0021154163405299187f, 0.0027867883909493685f, 0.002137420466169715f, 0.0031357293482869864f, 0.0012631223071366549f, 0.0028519374318420887f, 0.002203473122790456f, 0.002372988499701023f, 0.0018357409862801433f, 0.0021314206533133984f, 0.0015892783412709832f, 0.002409721026197076f, 0.0020171343348920345f, 0.0023759889882057905f, 0.002535727107897401f, 0.0026061111129820347f, 0.001999103929847479f, 0.0022945627570152283f, 0.0024470482021570206f, 0.002511344850063324f, 0.0025089855771511793f, 0.0019364175386726856f, 0.0029951015021651983f, 0.0023532963823527098f, 0.002213625004515052f, 0.0015919842990115285f, 0.00304756429977715f, 0.0020411349833011627f, 0.0017006598645821214f, 0.002339939819648862f, 0.0020716048311442137f, 0.0026781195774674416f, 0.00166348519269377f, 0.0019460741896182299f, 0.0020967558957636356f, 0.0017389538697898388f, 0.002009816700592637f, 0.0019448358798399568f, 0.0023491799365729094f, 0.0024052311200648546f, 0.002560211345553398f, 0.0021499297581613064f, 0.0018314257031306624f, 0.0021700472570955753f, 0.0020734556019306183f, 0.002569003729149699f, 0.002174129243940115f, 0.0027759631630033255f, 0.002377427415922284f, 0.0034225338604301214f, 0.0020401543006300926f, 0.001983893569558859f, 0.002252924954518676f, 0.002044049557298422f, 0.0027365819551050663f, 0.0033689101692289114f, 0.001635092543438077f, 0.002553387777879834f, 0.002708140294998884f, 0.0023083132691681385f, 0.003218012396246195f, 0.0017420784570276737f, 0.0020121156703680754f, 0.002788563258945942f, 0.0024036443792283535f, 0.0027727566193789244f, 0.0016393738333135843f, 0.00190213811583817f, 0.002832390135154128f, 0.0018354188650846481f, 0.002030908362939954f, 0.00412830151617527f, 0.0027690203860402107f, 0.002908246126025915f, 0.0021768060978502035f, 0.0017156637040898204f, 0.0014276755973696709f, 0.004143064841628075f, 0.0021154452115297318f, 0.001158295082859695f, 0.001959481742233038f, 0.002363333012908697f, 0.002425350248813629f, 0.0029276027344167233f, 0.002021394670009613f, 0.0028934343717992306f, 0.0031805236358195543f, 0.0025468897074460983f, 0.0020392739679664373f, 0.0014684322522953153f, 0.0021675119642168283f, 0.0022213731426745653f, 0.002414216985926032f, 0.001740052830427885f, 0.0024921828880906105f, 0.0011365218088030815f, 0.003272437956184149f, 0.0022288276813924313f, 0.0021942080929875374f, 0.0017140445997938514f, 0.002121108118444681f, 0.0035736416466534138f, 0.0013194779166951776f, 0.0020234882831573486f, 0.0019553671590983868f, 0.0016192945186048746f, 0.0019755701068788767f, 0.0025303992442786694f, 0.001499282312579453f, 0.002766781020909548f, 0.001953915925696492f, 0.0017044577980414033f, 0.0021336222998797894f, 0.0025791951920837164f, 0.0018500200239941478f, 0.002237801905721426f, 0.002078411867842078f, 0.0021719743963330984f, 0.002576819621026516f, 0.0023560747504234314f, 0.0017436349298804998f, 0.002164003672078252f, 0.0026409600395709276f, 0.0019420672906562686f, 0.002681293524801731f, 0.001752350595779717f, 0.0022847086656838655f, 0.002237992826849222f, 0.0009464960312470794f, 0.0020001307129859924f, 0.0016896091401576996f, 0.0016713233198970556f, 0.0020726339425891638f, 0.0019288830226287246f, 0.0021884089801460505f, 0.002009614137932658f, 0.001842692494392395f, 0.002365387976169586f, 0.0017463095718994737f, 0.0025500732008367777f, 0.002269901568070054f, 0.0020131058990955353f, 0.004521150607615709f, 0.001683381269685924f, 0.0021357221994549036f, 0.0016594596672803164f, 0.0026251322124153376f, 0.0023487024009227753f, 0.0023294640704989433f, 0.0033640337642282248f, 0.001611956162378192f, 0.0025474217254668474f, 0.002520013367757201f, 0.0022877061273902655f, 0.0027140916790813208f, 0.002040260471403599f, 0.0030938000418245792f, 0.002970445668324828f, 0.0019544002134352922f, 0.0019187760772183537f, 0.0022109695710241795f, 0.0036592527758330107f, 0.002450002124533057f, 0.0020425620023161173f, 0.002467124257236719f, 0.0031245427671819925f, 0.0030855704098939896f, 0.001763559295795858f, 0.0015421132557094097f, 0.00202384521253407f, 0.0014418031787499785f, 0.002176014008000493f, 0.002292032353579998f, 0.002052255207672715f, 0.0017887679859995842f, 0.002238953486084938f, 0.0016043100040405989f, 0.0023624212481081486f, 0.0021535528358072042f, 0.002749584149569273f, 0.0020389100536704063f, 0.003690734039992094f, 0.002280337270349264f, 0.0018265924882143736f, 0.0020469618029892445f, 0.0018661501817405224f, 0.001769684604369104f, 0.002161417854949832f, 0.0025160068180412054f, 0.0013753928942605853f, 0.002112126909196377f, 0.0026438410859555006f, 0.001996806124225259f, 0.002411759691312909f, 0.0014499393291771412f, 0.0027554056141525507f, 0.00248647085390985f, 0.0018821741687133908f, 0.0024738884530961514f, 0.0015649889828637242f, 0.0025461616460233927f, 0.002456018701195717f, 0.0023882139939814806f, 0.002736912574619055f, 0.0033866409212350845f, 0.0029112815391272306f, 0.0025446785148233175f, 0.002425150014460087f, 0.001760484417900443f, 0.0025496145244687796f, 0.00125837919767946f, 0.002201224910095334f, 0.001189136877655983f, 0.002882623579353094f, 0.0014021474635228515f, 0.00266609201207757f, 0.0016346235061064363f, 0.003383647883310914f, 0.001625576289370656f, 0.0022771337535232306f, 0.0025064365472644567f, 0.001906681340187788f, 0.0024466272443532944f, 0.0020606385078281164f, 0.0018635363085195422f, 0.0015645847888663411f, 0.0033840679097920656f, 0.0022208618465811014f, 0.0025142293889075518f, 0.0031636683270335197f, 0.0025125877000391483f, 0.0027391030453145504f, 0.0021283538080751896f, 0.002219543093815446f, 0.001506036496721208f, 0.0020924201235175133f, 0.0010931625729426742f, 0.0020441205706447363f, 0.002516369568184018f, 0.002255199709907174f, 0.0023327087983489037f, 0.00210988475009799f, 0.002156459027901292f, 0.002369536319747567f, 0.002167782047763467f, 0.0018878757255151868f, 0.0025043629575520754f, 0.0025768678169697523f, 0.0027719622012227774f, 0.0025171900633722544f, 0.0025893801357597113f, 0.002062638057395816f, 0.0023507417645305395f, 0.0021830694749951363f, 0.0020706148352473974f, 0.001965849194675684f, 0.002558905864134431f, 0.0019515777239575982f, 0.001861098106019199f, 0.0023174958769232035f, 0.002721779979765415f, 0.0022623937111347914f, 0.003621710930019617f, 0.0016823007026687264f, 0.0022456871811300516f, 0.0020255951676517725f, 0.0018170283874496818f, 0.0017201672308146954f, 0.0023742627818137407f, 0.002104101236909628f, 0.0015011160867288709f, 0.0018042665906250477f, 0.0022772455122321844f, 0.0018408183241263032f, 0.0017861116211861372f, 0.001696064486168325f, 0.0023635041434317827f, 0.002747936174273491f, 0.0022679681424051523f, 0.0019067567773163319f, 0.0021806771401315928f, 0.001893789041787386f, 0.003067552112042904f, 0.0020501783583313227f, 0.002364217536523938f, 0.001814773422665894f, 0.003320824820548296f, 0.0019300127169117332f, 0.0016475172014907002f, 0.0016596445348113775f, 0.0042013260535895824f, 0.0018620452610775828f, 0.0016201729886233807f, 0.002184764016419649f, 0.0036890713963657618f, 0.001971985213458538f, 0.0018340247916057706f, 0.002642038045451045f, 0.0030390911269932985f, 0.0015459167771041393f, 0.002562699606642127f, 0.0016431303229182959f, 0.0018293058965355158f, 0.0022769449278712273f, 0.0024690446443855762f, 0.002258952474221587f, 0.0027586419600993395f, 0.002021130407229066f, 0.002299732295796275f, 0.0021067664492875338f, 0.0017593285301700234f, 0.002056312747299671f, 0.0026039197109639645f, 0.00197342736646533f, 0.0019354884279891849f, 0.0028131571598351f, 0.0028462137561291456f, 0.0028943417128175497f, 0.002246408024802804f, 0.002664236119017005f, 0.001759032835252583f, 0.0026099979877471924f, 0.0018097914289683104f, 0.0020378706976771355f, 0.0025687417946755886f, 0.001919531379826367f, 0.002108141081407666f, 0.0021698102355003357f, 0.0021308590658009052f, 0.003504228312522173f, 0.0022903038188815117f, 0.0024938799906522036f, 0.0022769777569919825f, 0.0024720062501728535f, 0.0021736742928624153f, 0.0018160665640607476f, 0.0019719700794667006f, 0.002838131505995989f, 0.0016466277884319425f, 0.0029587149620056152f, 0.002612934447824955f, 0.0013540979707613587f, 0.0019898314494639635f, 0.0017301494954153895f, 0.002379177138209343f, 0.0018366979202255607f, 0.002423567231744528f);
static const ai_layer_format_type _conv5_conv5_2_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u32 _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 25600;
static const ai_float _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.018465526401996613f;
static const ai_i8 _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_t_in_0_fmt_zero_const_s8 = -128;



static const ai_u32 _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 1024;
static const ai_float _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_t_out_0_fmt_scale_const_f32 = 0.002931835362687707f;
static const ai_i8 _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_t_out_0_fmt_zero_const_s8 = -128;

STAI_API_ENTRY
stai_return_code stai_network_run(
  stai_network* network,
  const stai_run_mode mode)
{
   STAI_UNUSED(mode)
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_ACTIVATIONS) != STAI_FLAG_ACTIVATIONS,
        STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_PTR, net_ctx->_return_code)

  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_INPUTS) != STAI_FLAG_INPUTS,
                  STAI_ERROR_NETWORK_INVALID_IN_PTR, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_OUTPUTS) != STAI_FLAG_OUTPUTS,
                  STAI_ERROR_NETWORK_INVALID_OUT_PTR, net_ctx->_return_code)

  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_WEIGHTS) != STAI_FLAG_WEIGHTS,
                  STAI_ERROR_NETWORK_INVALID_WEIGHTS_PTR, net_ctx->_return_code)


  /* LITE_KERNEL_SECTION BEGIN image_Transpose */
  {
    
  forward_lite_transpose_image_Transpose_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END image_Transpose */
  /* LITE_KERNEL_SECTION BEGIN _conv1_conv1_2_Relu_output_0 */
  {
    
  forward_lite_conv2d_sssa8_ch_nl_pool__conv1_conv1_2_Relu_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _conv1_conv1_2_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_0_branch1_branch1_0_Conv_output_0 */
  {
      const ai_i8* _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 62260);
    const ai_i8* _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 748);
    const ai_i32* _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 964);
    ai_i8* _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 52660);
    ai_i16* _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(124, 1, {(stai_ptr) _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_in_0_ptr_const_s8, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_in_0_shape_w_const_u16, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_in_0_shape_h_const_u16, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_in_0_shape_ch_const_u16, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_weight_0_ptr_const_s8, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_weight_0_shape_w_const_u16, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_weight_0_shape_h_const_u16, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_l_pad_W_0_const_s32, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_l_pad_H_0_const_s32, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_l_stride_1_const_u16, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_l_stride_0_const_u16, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_weight_1_ptr_const_s32, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_out_0_ptr_s8, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_out_0_shape_w_const_u16, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_out_0_shape_h_const_u16, 0, 889, _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(124, 1, {(stai_ptr) _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_0_branch1_branch1_0_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_0_branch1_branch1_4_Relu_output_0 */
  {
      const ai_i8* _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 52660);
    const ai_i8* _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 1060);
    const ai_i32* _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 1636);
    ai_i8* _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 43060);
    ai_i16* _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(130, 1, {(stai_ptr) _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_in_0_ptr_const_s8, _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_w_const_u16, _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_h_const_u16, _stage2_stage2_0_branch1_branch1_4_Relu_output_0_l_stride_1_const_u16, _stage2_stage2_0_branch1_branch1_4_Relu_output_0_l_stride_0_const_u16, _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_ch_const_u16, _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_weight_0_ptr_const_s8, _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_out_0_shape_ch_const_u16, _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_weight_1_ptr_const_s32, _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage2_stage2_0_branch1_branch1_4_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_out_0_ptr_s8, 1, 336, _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(130, 1, {(stai_ptr) _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_0_branch1_branch1_4_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_0_branch2_branch2_2_Relu_output_0 */
  {
      const ai_i8* _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 62260);
    const ai_i8* _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 1732);
    const ai_i32* _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 2308);
    ai_i8* _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 61300);
    ai_i16* _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(123, 1, {(stai_ptr) _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8, _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16, _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16, _stage2_stage2_0_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16, _stage2_stage2_0_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16, _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16, _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8, _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16, _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32, _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage2_stage2_0_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8, 1, 336, _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(123, 1, {(stai_ptr) _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_0_branch2_branch2_2_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_0_branch2_branch2_3_Conv_output_0 */
  {
      const ai_i8* _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 61300);
    const ai_i8* _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 2404);
    const ai_i32* _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 2620);
    ai_i8* _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 60796);
    ai_i16* _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(129, 1, {(stai_ptr) _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16, 0, 889, _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(129, 1, {(stai_ptr) _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_0_branch2_branch2_3_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_0_branch2_branch2_7_Relu_output_0 */
  {
      const ai_i8* _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 60796);
    const ai_i8* _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 2716);
    const ai_i32* _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 3292);
    ai_i8* _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 25936);
    ai_i16* _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(135, 1, {(stai_ptr) _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8, _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16, _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16, _stage2_stage2_0_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16, _stage2_stage2_0_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16, _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16, _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8, _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16, _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32, _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage2_stage2_0_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8, 1, 336, _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(135, 1, {(stai_ptr) _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_0_branch2_branch2_7_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_0_Concat_output_0 */
  {
    
  forward_lite_concat__stage2_stage2_0_Concat_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_0_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_0_Transpose_output_0 */
  {
    
  forward_lite_transpose__stage2_stage2_0_Transpose_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_0_Transpose_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_1_Slice_output_0 */
  {
    
  forward_lite_slice__stage2_stage2_1_Slice_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_1_Slice_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_1_Slice_1_output_0 */
  {
    
  forward_lite_slice__stage2_stage2_1_Slice_1_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_1_Slice_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_1_branch2_branch2_2_Relu_output_0 */
  {
      const ai_i8* _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 54400);
    const ai_i8* _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 3388);
    const ai_i32* _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 3964);
    ai_i8* _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 35200);
    ai_i16* _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[1] + 76464);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(156, 1, {(stai_ptr) _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8, _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16, _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16, _stage2_stage2_1_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16, _stage2_stage2_1_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16, _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16, _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8, _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16, _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32, _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage2_stage2_1_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8, 1, 336, _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(156, 1, {(stai_ptr) _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_1_branch2_branch2_2_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_1_branch2_branch2_3_Conv_output_0 */
  {
      const ai_i8* _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 35200);
    const ai_i8* _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 4060);
    const ai_i32* _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 4276);
    ai_i8* _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 34192);
    ai_i16* _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[1] + 75912);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(159, 1, {(stai_ptr) _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16, 0, 889, _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(159, 1, {(stai_ptr) _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_1_branch2_branch2_3_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_1_branch2_branch2_7_Relu_output_0 */
  {
      const ai_i8* _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 34192);
    const ai_i8* _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 4372);
    const ai_i32* _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 4948);
    ai_i8* _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 54400);
    ai_i16* _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(162, 1, {(stai_ptr) _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8, _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16, _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16, _stage2_stage2_1_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16, _stage2_stage2_1_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16, _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16, _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8, _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16, _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32, _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage2_stage2_1_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8, 1, 336, _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(162, 1, {(stai_ptr) _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_1_branch2_branch2_7_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_1_Concat_output_0 */
  {
    
  forward_lite_concat__stage2_stage2_1_Concat_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_1_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_1_Transpose_output_0 */
  {
    
  forward_lite_transpose__stage2_stage2_1_Transpose_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_1_Transpose_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_2_Slice_output_0 */
  {
    
  forward_lite_slice__stage2_stage2_2_Slice_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_2_Slice_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_2_Slice_1_output_0 */
  {
    
  forward_lite_slice__stage2_stage2_2_Slice_1_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_2_Slice_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_2_branch2_branch2_2_Relu_output_0 */
  {
      const ai_i8* _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 92800);
    const ai_i8* _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 5044);
    const ai_i32* _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 5620);
    ai_i8* _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 73600);
    ai_i16* _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 63664);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(183, 1, {(stai_ptr) _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8, _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16, _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16, _stage2_stage2_2_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16, _stage2_stage2_2_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16, _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16, _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8, _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16, _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32, _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage2_stage2_2_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8, 1, 336, _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(183, 1, {(stai_ptr) _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_2_branch2_branch2_2_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_2_branch2_branch2_3_Conv_output_0 */
  {
      const ai_i8* _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 73600);
    const ai_i8* _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 5716);
    const ai_i32* _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 5932);
    ai_i8* _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 92800);
    ai_i16* _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[1] + 75912);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(186, 1, {(stai_ptr) _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16, 0, 889, _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(186, 1, {(stai_ptr) _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_2_branch2_branch2_3_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_2_branch2_branch2_7_Relu_output_0 */
  {
      const ai_i8* _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 92800);
    const ai_i8* _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 6028);
    const ai_i32* _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 6604);
    ai_i8* _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 25936);
    ai_i16* _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(189, 1, {(stai_ptr) _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8, _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16, _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16, _stage2_stage2_2_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16, _stage2_stage2_2_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16, _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16, _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8, _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16, _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32, _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage2_stage2_2_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8, 1, 336, _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(189, 1, {(stai_ptr) _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_2_branch2_branch2_7_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_2_Concat_output_0 */
  {
    
  forward_lite_concat__stage2_stage2_2_Concat_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_2_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_2_Transpose_output_0 */
  {
    
  forward_lite_transpose__stage2_stage2_2_Transpose_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_2_Transpose_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_3_Slice_output_0 */
  {
    
  forward_lite_slice__stage2_stage2_3_Slice_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_3_Slice_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_3_Slice_1_output_0 */
  {
    
  forward_lite_slice__stage2_stage2_3_Slice_1_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_3_Slice_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_3_branch2_branch2_2_Relu_output_0 */
  {
      const ai_i8* _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 92800);
    const ai_i8* _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 6700);
    const ai_i32* _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 7276);
    ai_i8* _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 92320);
    ai_i16* _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(210, 1, {(stai_ptr) _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8, _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16, _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16, _stage2_stage2_3_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16, _stage2_stage2_3_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16, _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16, _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8, _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16, _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32, _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage2_stage2_3_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8, 1, 336, _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(210, 1, {(stai_ptr) _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_3_branch2_branch2_2_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_3_branch2_branch2_3_Conv_output_0 */
  {
      const ai_i8* _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 92320);
    const ai_i8* _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 7372);
    const ai_i32* _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 7588);
    ai_i8* _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 64336);
    ai_i16* _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 91428);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(213, 1, {(stai_ptr) _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16, 0, 889, _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(213, 1, {(stai_ptr) _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_3_branch2_branch2_3_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_3_branch2_branch2_7_Relu_output_0 */
  {
      const ai_i8* _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 64336);
    const ai_i8* _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 7684);
    const ai_i32* _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 8260);
    ai_i8* _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 25936);
    ai_i16* _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(216, 1, {(stai_ptr) _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8, _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16, _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16, _stage2_stage2_3_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16, _stage2_stage2_3_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16, _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16, _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8, _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16, _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32, _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage2_stage2_3_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8, 1, 336, _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(216, 1, {(stai_ptr) _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_3_branch2_branch2_7_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_3_Concat_output_0 */
  {
    
  forward_lite_concat__stage2_stage2_3_Concat_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_3_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage2_stage2_3_Transpose_output_0 */
  {
    
  forward_lite_transpose__stage2_stage2_3_Transpose_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage2_stage2_3_Transpose_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_0_branch1_branch1_0_Conv_output_0 */
  {
      const ai_i8* _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 83200);
    const ai_i8* _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 8356);
    const ai_i32* _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 8788);
    ai_i8* _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 78400);
    ai_i16* _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(232, 1, {(stai_ptr) _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_in_0_ptr_const_s8, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_weight_0_shape_w_const_u16, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_weight_0_shape_h_const_u16, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_l_pad_W_0_const_s32, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_l_pad_H_0_const_s32, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_l_stride_1_const_u16, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_l_stride_0_const_u16, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_out_0_ptr_s8, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_out_0_shape_w_const_u16, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_out_0_shape_h_const_u16, 0, 1777, _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(232, 1, {(stai_ptr) _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_0_branch1_branch1_0_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_0_branch1_branch1_4_Relu_output_0 */
  {
      const ai_i8* _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 78400);
    const ai_i8* _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 8980);
    const ai_i32* _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 11284);
    ai_i8* _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 73600);
    ai_i16* _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[1] + 0);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(238, 1, {(stai_ptr) _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_in_0_ptr_const_s8, _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_0_branch1_branch1_4_Relu_output_0_l_stride_1_const_u16, _stage3_stage3_0_branch1_branch1_4_Relu_output_0_l_stride_0_const_u16, _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_out_0_shape_ch_const_u16, _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_0_branch1_branch1_4_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_out_0_ptr_s8, 1, 672, _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(238, 1, {(stai_ptr) _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_0_branch1_branch1_4_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_0_branch2_branch2_2_Relu_output_0 */
  {
      const ai_i8* _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 83200);
    const ai_i8* _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 11476);
    const ai_i32* _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 13780);
    ai_i8* _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 54400);
    ai_i16* _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 82528);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(231, 1, {(stai_ptr) _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8, _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_0_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16, _stage3_stage3_0_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16, _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16, _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_0_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8, 1, 672, _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(231, 1, {(stai_ptr) _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_0_branch2_branch2_2_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_0_branch2_branch2_3_Conv_output_0 */
  {
      const ai_i8* _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 54400);
    const ai_i8* _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 13972);
    const ai_i32* _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 14404);
    ai_i8* _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 53872);
    ai_i16* _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 100620);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(237, 1, {(stai_ptr) _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16, 0, 1777, _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(237, 1, {(stai_ptr) _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_0_branch2_branch2_3_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_0_branch2_branch2_7_Relu_output_0 */
  {
      const ai_i8* _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 53872);
    const ai_i8* _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 14596);
    const ai_i32* _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 16900);
    ai_i8* _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 26272);
    ai_i16* _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(243, 1, {(stai_ptr) _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8, _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_0_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16, _stage3_stage3_0_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16, _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16, _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_0_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8, 1, 672, _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(243, 1, {(stai_ptr) _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_0_branch2_branch2_7_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_0_Concat_output_0 */
  {
    
  forward_lite_concat__stage3_stage3_0_Concat_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_0_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_0_Transpose_output_0 */
  {
    
  forward_lite_transpose__stage3_stage3_0_Transpose_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_0_Transpose_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_1_Slice_output_0 */
  {
    
  forward_lite_slice__stage3_stage3_1_Slice_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_1_Slice_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_1_Slice_1_output_0 */
  {
    
  forward_lite_slice__stage3_stage3_1_Slice_1_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_1_Slice_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_1_branch2_branch2_2_Relu_output_0 */
  {
      const ai_i8* _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    const ai_i8* _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 17092);
    const ai_i32* _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 19396);
    ai_i8* _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 55072);
    ai_i16* _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 49600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(264, 1, {(stai_ptr) _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8, _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_1_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16, _stage3_stage3_1_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16, _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16, _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_1_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8, 1, 672, _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(264, 1, {(stai_ptr) _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_1_branch2_branch2_2_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_1_branch2_branch2_3_Conv_output_0 */
  {
      const ai_i8* _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 55072);
    const ai_i8* _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 19588);
    const ai_i32* _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 20020);
    ai_i8* _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    ai_i16* _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[1] + 75024);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(267, 1, {(stai_ptr) _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16, 0, 1777, _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(267, 1, {(stai_ptr) _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_1_branch2_branch2_3_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_1_branch2_branch2_7_Relu_output_0 */
  {
      const ai_i8* _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    const ai_i8* _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 20212);
    const ai_i32* _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 22516);
    ai_i8* _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 26272);
    ai_i16* _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(270, 1, {(stai_ptr) _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8, _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_1_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16, _stage3_stage3_1_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16, _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16, _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_1_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8, 1, 672, _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(270, 1, {(stai_ptr) _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_1_branch2_branch2_7_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_1_Concat_output_0 */
  {
    
  forward_lite_concat__stage3_stage3_1_Concat_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_1_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_1_Transpose_output_0 */
  {
    
  forward_lite_transpose__stage3_stage3_1_Transpose_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_1_Transpose_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_2_Slice_output_0 */
  {
    
  forward_lite_slice__stage3_stage3_2_Slice_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_2_Slice_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_2_Slice_1_output_0 */
  {
    
  forward_lite_slice__stage3_stage3_2_Slice_1_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_2_Slice_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_2_branch2_branch2_2_Relu_output_0 */
  {
      const ai_i8* _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    const ai_i8* _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 22708);
    const ai_i32* _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 25012);
    ai_i8* _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 55072);
    ai_i16* _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 49600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(291, 1, {(stai_ptr) _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8, _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_2_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16, _stage3_stage3_2_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16, _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16, _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_2_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8, 1, 672, _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(291, 1, {(stai_ptr) _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_2_branch2_branch2_2_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_2_branch2_branch2_3_Conv_output_0 */
  {
      const ai_i8* _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 55072);
    const ai_i8* _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 25204);
    const ai_i32* _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 25636);
    ai_i8* _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    ai_i16* _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[1] + 75024);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(294, 1, {(stai_ptr) _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16, 0, 1777, _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(294, 1, {(stai_ptr) _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_2_branch2_branch2_3_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_2_branch2_branch2_7_Relu_output_0 */
  {
      const ai_i8* _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    const ai_i8* _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 25828);
    const ai_i32* _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 28132);
    ai_i8* _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 26272);
    ai_i16* _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(297, 1, {(stai_ptr) _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8, _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_2_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16, _stage3_stage3_2_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16, _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16, _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_2_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8, 1, 672, _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(297, 1, {(stai_ptr) _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_2_branch2_branch2_7_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_2_Concat_output_0 */
  {
    
  forward_lite_concat__stage3_stage3_2_Concat_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_2_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_2_Transpose_output_0 */
  {
    
  forward_lite_transpose__stage3_stage3_2_Transpose_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_2_Transpose_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_3_Slice_output_0 */
  {
    
  forward_lite_slice__stage3_stage3_3_Slice_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_3_Slice_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_3_Slice_1_output_0 */
  {
    
  forward_lite_slice__stage3_stage3_3_Slice_1_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_3_Slice_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_3_branch2_branch2_2_Relu_output_0 */
  {
      const ai_i8* _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    const ai_i8* _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 28324);
    const ai_i32* _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 30628);
    ai_i8* _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 55072);
    ai_i16* _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 49600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(318, 1, {(stai_ptr) _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8, _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_3_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16, _stage3_stage3_3_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16, _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16, _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_3_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8, 1, 672, _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(318, 1, {(stai_ptr) _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_3_branch2_branch2_2_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_3_branch2_branch2_3_Conv_output_0 */
  {
      const ai_i8* _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 55072);
    const ai_i8* _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 30820);
    const ai_i32* _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 31252);
    ai_i8* _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    ai_i16* _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[1] + 75024);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(321, 1, {(stai_ptr) _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16, 0, 1777, _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(321, 1, {(stai_ptr) _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_3_branch2_branch2_3_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_3_branch2_branch2_7_Relu_output_0 */
  {
      const ai_i8* _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    const ai_i8* _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 31444);
    const ai_i32* _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 33748);
    ai_i8* _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 26272);
    ai_i16* _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(324, 1, {(stai_ptr) _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8, _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_3_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16, _stage3_stage3_3_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16, _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16, _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_3_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8, 1, 672, _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(324, 1, {(stai_ptr) _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_3_branch2_branch2_7_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_3_Concat_output_0 */
  {
    
  forward_lite_concat__stage3_stage3_3_Concat_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_3_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_3_Transpose_output_0 */
  {
    
  forward_lite_transpose__stage3_stage3_3_Transpose_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_3_Transpose_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_4_Slice_output_0 */
  {
    
  forward_lite_slice__stage3_stage3_4_Slice_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_4_Slice_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_4_Slice_1_output_0 */
  {
    
  forward_lite_slice__stage3_stage3_4_Slice_1_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_4_Slice_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_4_branch2_branch2_2_Relu_output_0 */
  {
      const ai_i8* _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    const ai_i8* _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 33940);
    const ai_i32* _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 36244);
    ai_i8* _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 55072);
    ai_i16* _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 49600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(345, 1, {(stai_ptr) _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8, _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_4_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16, _stage3_stage3_4_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16, _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16, _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_4_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8, 1, 672, _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(345, 1, {(stai_ptr) _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_4_branch2_branch2_2_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_4_branch2_branch2_3_Conv_output_0 */
  {
      const ai_i8* _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 55072);
    const ai_i8* _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 36436);
    const ai_i32* _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 36868);
    ai_i8* _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    ai_i16* _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[1] + 75024);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(348, 1, {(stai_ptr) _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16, 0, 1777, _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(348, 1, {(stai_ptr) _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_4_branch2_branch2_3_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_4_branch2_branch2_7_Relu_output_0 */
  {
      const ai_i8* _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    const ai_i8* _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 37060);
    const ai_i32* _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 39364);
    ai_i8* _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 26272);
    ai_i16* _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(351, 1, {(stai_ptr) _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8, _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_4_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16, _stage3_stage3_4_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16, _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16, _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_4_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8, 1, 672, _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(351, 1, {(stai_ptr) _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_4_branch2_branch2_7_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_4_Concat_output_0 */
  {
    
  forward_lite_concat__stage3_stage3_4_Concat_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_4_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_4_Transpose_output_0 */
  {
    
  forward_lite_transpose__stage3_stage3_4_Transpose_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_4_Transpose_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_5_Slice_output_0 */
  {
    
  forward_lite_slice__stage3_stage3_5_Slice_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_5_Slice_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_5_Slice_1_output_0 */
  {
    
  forward_lite_slice__stage3_stage3_5_Slice_1_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_5_Slice_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_5_branch2_branch2_2_Relu_output_0 */
  {
      const ai_i8* _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    const ai_i8* _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 39556);
    const ai_i32* _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 41860);
    ai_i8* _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 55072);
    ai_i16* _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 49600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(372, 1, {(stai_ptr) _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8, _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_5_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16, _stage3_stage3_5_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16, _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16, _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_5_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8, 1, 672, _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(372, 1, {(stai_ptr) _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_5_branch2_branch2_2_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_5_branch2_branch2_3_Conv_output_0 */
  {
      const ai_i8* _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 55072);
    const ai_i8* _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 42052);
    const ai_i32* _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 42484);
    ai_i8* _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    ai_i16* _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[1] + 75024);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(375, 1, {(stai_ptr) _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16, 0, 1777, _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(375, 1, {(stai_ptr) _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_5_branch2_branch2_3_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_5_branch2_branch2_7_Relu_output_0 */
  {
      const ai_i8* _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    const ai_i8* _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 42676);
    const ai_i32* _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 44980);
    ai_i8* _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 26272);
    ai_i16* _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(378, 1, {(stai_ptr) _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8, _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_5_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16, _stage3_stage3_5_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16, _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16, _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_5_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8, 1, 672, _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(378, 1, {(stai_ptr) _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_5_branch2_branch2_7_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_5_Concat_output_0 */
  {
    
  forward_lite_concat__stage3_stage3_5_Concat_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_5_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_5_Transpose_output_0 */
  {
    
  forward_lite_transpose__stage3_stage3_5_Transpose_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_5_Transpose_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_6_Slice_output_0 */
  {
    
  forward_lite_slice__stage3_stage3_6_Slice_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_6_Slice_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_6_Slice_1_output_0 */
  {
    
  forward_lite_slice__stage3_stage3_6_Slice_1_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_6_Slice_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_6_branch2_branch2_2_Relu_output_0 */
  {
      const ai_i8* _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    const ai_i8* _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 45172);
    const ai_i32* _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 47476);
    ai_i8* _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 55072);
    ai_i16* _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 49600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(399, 1, {(stai_ptr) _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8, _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_6_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16, _stage3_stage3_6_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16, _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16, _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_6_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8, 1, 672, _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(399, 1, {(stai_ptr) _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_6_branch2_branch2_2_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_6_branch2_branch2_3_Conv_output_0 */
  {
      const ai_i8* _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 55072);
    const ai_i8* _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 47668);
    const ai_i32* _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 48100);
    ai_i8* _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    ai_i16* _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[1] + 75024);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(402, 1, {(stai_ptr) _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16, 0, 1777, _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(402, 1, {(stai_ptr) _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_6_branch2_branch2_3_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_6_branch2_branch2_7_Relu_output_0 */
  {
      const ai_i8* _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    const ai_i8* _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 48292);
    const ai_i32* _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 50596);
    ai_i8* _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 26272);
    ai_i16* _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(405, 1, {(stai_ptr) _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8, _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_6_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16, _stage3_stage3_6_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16, _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16, _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_6_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8, 1, 672, _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(405, 1, {(stai_ptr) _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_6_branch2_branch2_7_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_6_Concat_output_0 */
  {
    
  forward_lite_concat__stage3_stage3_6_Concat_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_6_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_6_Transpose_output_0 */
  {
    
  forward_lite_transpose__stage3_stage3_6_Transpose_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_6_Transpose_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_7_Slice_output_0 */
  {
    
  forward_lite_slice__stage3_stage3_7_Slice_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_7_Slice_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_7_Slice_1_output_0 */
  {
    
  forward_lite_slice__stage3_stage3_7_Slice_1_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_7_Slice_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_7_branch2_branch2_2_Relu_output_0 */
  {
      const ai_i8* _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    const ai_i8* _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 50788);
    const ai_i32* _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 53092);
    ai_i8* _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 55072);
    ai_i16* _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 49600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(426, 1, {(stai_ptr) _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8, _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_7_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16, _stage3_stage3_7_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16, _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16, _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_7_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8, 1, 672, _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(426, 1, {(stai_ptr) _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_7_branch2_branch2_2_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_7_branch2_branch2_3_Conv_output_0 */
  {
      const ai_i8* _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 55072);
    const ai_i8* _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 53284);
    const ai_i32* _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 53716);
    ai_i8* _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    ai_i16* _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[1] + 75024);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(429, 1, {(stai_ptr) _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16, 0, 1777, _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(429, 1, {(stai_ptr) _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_7_branch2_branch2_3_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_7_branch2_branch2_7_Relu_output_0 */
  {
      const ai_i8* _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 97600);
    const ai_i8* _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 53908);
    const ai_i32* _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 56212);
    ai_i8* _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 26272);
    ai_i16* _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(432, 1, {(stai_ptr) _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8, _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16, _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16, _stage3_stage3_7_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16, _stage3_stage3_7_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16, _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16, _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8, _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16, _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32, _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage3_stage3_7_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8, 1, 672, _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(432, 1, {(stai_ptr) _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_7_branch2_branch2_7_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_7_Concat_output_0 */
  {
    
  forward_lite_concat__stage3_stage3_7_Concat_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_7_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage3_stage3_7_Transpose_output_0 */
  {
    
  forward_lite_transpose__stage3_stage3_7_Transpose_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage3_stage3_7_Transpose_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_0_branch1_branch1_0_Conv_output_0 */
  {
      const ai_i8* _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 45472);
    const ai_i8* _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 56404);
    const ai_i32* _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 57268);
    ai_i8* _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 29156);
    ai_i16* _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(448, 1, {(stai_ptr) _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_in_0_ptr_const_s8, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_in_0_shape_w_const_u16, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_in_0_shape_h_const_u16, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_in_0_shape_ch_const_u16, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_weight_0_ptr_const_s8, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_weight_0_shape_w_const_u16, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_weight_0_shape_h_const_u16, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_l_pad_W_0_const_s32, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_l_pad_H_0_const_s32, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_l_stride_1_const_u16, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_l_stride_0_const_u16, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_weight_1_ptr_const_s32, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_out_0_ptr_s8, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_out_0_shape_w_const_u16, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_out_0_shape_h_const_u16, 0, 3553, _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(448, 1, {(stai_ptr) _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_0_branch1_branch1_0_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_0_branch1_branch1_4_Relu_output_0 */
  {
      const ai_i8* _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 29156);
    const ai_i8* _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 57652);
    const ai_i32* _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 66868);
    ai_i8* _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 31556);
    ai_i16* _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 27812);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(454, 1, {(stai_ptr) _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_in_0_ptr_const_s8, _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_w_const_u16, _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_h_const_u16, _stage4_stage4_0_branch1_branch1_4_Relu_output_0_l_stride_1_const_u16, _stage4_stage4_0_branch1_branch1_4_Relu_output_0_l_stride_0_const_u16, _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_ch_const_u16, _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_weight_0_ptr_const_s8, _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_out_0_shape_ch_const_u16, _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_weight_1_ptr_const_s32, _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage4_stage4_0_branch1_branch1_4_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_out_0_ptr_s8, 1, 1344, _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(454, 1, {(stai_ptr) _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_0_branch1_branch1_4_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_0_branch2_branch2_2_Relu_output_0 */
  {
      const ai_i8* _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 45472);
    const ai_i8* _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 67252);
    const ai_i32* _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 76468);
    ai_i8* _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 55072);
    ai_i16* _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 30212);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(447, 1, {(stai_ptr) _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8, _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16, _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16, _stage4_stage4_0_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16, _stage4_stage4_0_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16, _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16, _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8, _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16, _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32, _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage4_stage4_0_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8, 1, 1344, _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(447, 1, {(stai_ptr) _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_0_branch2_branch2_2_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_0_branch2_branch2_3_Conv_output_0 */
  {
      const ai_i8* _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 55072);
    const ai_i8* _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 76852);
    const ai_i32* _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 77716);
    ai_i8* _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 54496);
    ai_i16* _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 64672);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(453, 1, {(stai_ptr) _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16, 0, 3553, _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(453, 1, {(stai_ptr) _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_0_branch2_branch2_3_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_0_branch2_branch2_7_Relu_output_0 */
  {
      const ai_i8* _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 54496);
    const ai_i8* _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 78100);
    const ai_i32* _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 87316);
    ai_i8* _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 26944);
    ai_i16* _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(459, 1, {(stai_ptr) _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8, _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16, _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16, _stage4_stage4_0_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16, _stage4_stage4_0_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16, _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16, _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8, _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16, _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32, _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage4_stage4_0_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8, 1, 1344, _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(459, 1, {(stai_ptr) _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_0_branch2_branch2_7_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_0_Concat_output_0 */
  {
    
  forward_lite_concat__stage4_stage4_0_Concat_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_0_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_0_Transpose_output_0 */
  {
    
  forward_lite_transpose__stage4_stage4_0_Transpose_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_0_Transpose_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_1_Slice_output_0 */
  {
    
  forward_lite_slice__stage4_stage4_1_Slice_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_1_Slice_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_1_Slice_1_output_0 */
  {
    
  forward_lite_slice__stage4_stage4_1_Slice_1_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_1_Slice_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_1_branch2_branch2_2_Relu_output_0 */
  {
      const ai_i8* _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 32800);
    const ai_i8* _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 87700);
    const ai_i32* _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 96916);
    ai_i8* _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 28000);
    ai_i16* _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[1] + 75456);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(480, 1, {(stai_ptr) _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8, _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16, _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16, _stage4_stage4_1_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16, _stage4_stage4_1_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16, _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16, _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8, _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16, _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32, _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage4_stage4_1_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8, 1, 1344, _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(480, 1, {(stai_ptr) _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_1_branch2_branch2_2_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_1_branch2_branch2_3_Conv_output_0 */
  {
      const ai_i8* _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 28000);
    const ai_i8* _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 97300);
    const ai_i32* _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 98164);
    ai_i8* _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 36356);
    ai_i16* _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 32800);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(483, 1, {(stai_ptr) _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16, 0, 3553, _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(483, 1, {(stai_ptr) _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_1_branch2_branch2_3_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_1_branch2_branch2_7_Relu_output_0 */
  {
      const ai_i8* _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 36356);
    const ai_i8* _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 98548);
    const ai_i32* _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 107764);
    ai_i8* _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 26944);
    ai_i16* _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(486, 1, {(stai_ptr) _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8, _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16, _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16, _stage4_stage4_1_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16, _stage4_stage4_1_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16, _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16, _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8, _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16, _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32, _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage4_stage4_1_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8, 1, 1344, _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(486, 1, {(stai_ptr) _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_1_branch2_branch2_7_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_1_Concat_output_0 */
  {
    
  forward_lite_concat__stage4_stage4_1_Concat_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_1_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_1_Transpose_output_0 */
  {
    
  forward_lite_transpose__stage4_stage4_1_Transpose_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_1_Transpose_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_2_Slice_output_0 */
  {
    
  forward_lite_slice__stage4_stage4_2_Slice_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_2_Slice_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_2_Slice_1_output_0 */
  {
    
  forward_lite_slice__stage4_stage4_2_Slice_1_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_2_Slice_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_2_branch2_branch2_2_Relu_output_0 */
  {
      const ai_i8* _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 32800);
    const ai_i8* _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 108148);
    const ai_i32* _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 117364);
    ai_i8* _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 26944);
    ai_i16* _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(507, 1, {(stai_ptr) _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8, _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16, _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16, _stage4_stage4_2_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16, _stage4_stage4_2_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16, _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16, _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8, _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16, _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32, _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage4_stage4_2_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8, 1, 1344, _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(507, 1, {(stai_ptr) _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_2_branch2_branch2_2_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_2_branch2_branch2_3_Conv_output_0 */
  {
      const ai_i8* _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 26944);
    const ai_i8* _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 117748);
    const ai_i32* _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 118612);
    ai_i8* _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 36356);
    ai_i16* _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 32800);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(510, 1, {(stai_ptr) _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16, 0, 3553, _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(510, 1, {(stai_ptr) _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_2_branch2_branch2_3_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_2_branch2_branch2_7_Relu_output_0 */
  {
      const ai_i8* _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 36356);
    const ai_i8* _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 118996);
    const ai_i32* _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 128212);
    ai_i8* _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 26944);
    ai_i16* _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(513, 1, {(stai_ptr) _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8, _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16, _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16, _stage4_stage4_2_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16, _stage4_stage4_2_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16, _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16, _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8, _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16, _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32, _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage4_stage4_2_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8, 1, 1344, _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(513, 1, {(stai_ptr) _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_2_branch2_branch2_7_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_2_Concat_output_0 */
  {
    
  forward_lite_concat__stage4_stage4_2_Concat_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_2_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_2_Transpose_output_0 */
  {
    
  forward_lite_transpose__stage4_stage4_2_Transpose_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_2_Transpose_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_3_Slice_output_0 */
  {
    
  forward_lite_slice__stage4_stage4_3_Slice_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_3_Slice_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_3_Slice_1_output_0 */
  {
    
  forward_lite_slice__stage4_stage4_3_Slice_1_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_3_Slice_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_3_branch2_branch2_2_Relu_output_0 */
  {
      const ai_i8* _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 32800);
    const ai_i8* _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 128596);
    const ai_i32* _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 137812);
    ai_i8* _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 26944);
    ai_i16* _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(534, 1, {(stai_ptr) _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_in_0_ptr_const_s8, _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16, _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16, _stage4_stage4_3_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16, _stage4_stage4_3_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16, _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16, _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_weight_0_ptr_const_s8, _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16, _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_weight_1_ptr_const_s32, _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage4_stage4_3_branch2_branch2_2_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8, 1, 1344, _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(534, 1, {(stai_ptr) _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_3_branch2_branch2_2_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_3_branch2_branch2_3_Conv_output_0 */
  {
      const ai_i8* _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 26944);
    const ai_i8* _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 138196);
    const ai_i32* _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 139060);
    ai_i8* _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 36356);
    ai_i16* _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 32800);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(537, 1, {(stai_ptr) _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_dw_sssa8_ch(_stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_in_0_ptr_const_s8, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_in_0_shape_w_const_u16, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_in_0_shape_h_const_u16, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_in_0_shape_ch_const_u16, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_weight_0_ptr_const_s8, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_weight_0_shape_w_const_u16, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_weight_0_shape_h_const_u16, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_l_pad_W_0_const_s32, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_l_pad_H_0_const_s32, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_l_stride_1_const_u16, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_l_stride_0_const_u16, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_weight_1_ptr_const_s32, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_in_0_fmt_zero_const_s8, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16, 0, 3553, _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(537, 1, {(stai_ptr) _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_3_branch2_branch2_3_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_3_branch2_branch2_7_Relu_output_0 */
  {
      const ai_i8* _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 36356);
    const ai_i8* _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 139444);
    const ai_i32* _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 148660);
    ai_i8* _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 35876);
    ai_i16* _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 29056);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(540, 1, {(stai_ptr) _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_in_0_ptr_const_s8, _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16, _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16, _stage4_stage4_3_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16, _stage4_stage4_3_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16, _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16, _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_weight_0_ptr_const_s8, _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16, _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_weight_1_ptr_const_s32, _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8, _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8, _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32, _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32, _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32, _stage4_stage4_3_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type, _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8, 1, 1344, _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(540, 1, {(stai_ptr) _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_3_branch2_branch2_7_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_3_Concat_output_0 */
  {
    
  forward_lite_concat__stage4_stage4_3_Concat_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_3_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stage4_stage4_3_Transpose_output_0 */
  {
    
  forward_lite_transpose__stage4_stage4_3_Transpose_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stage4_stage4_3_Transpose_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _conv5_conv5_2_Relu_output_0 */
  {
      const ai_i8* _conv5_conv5_2_Relu_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[1] + 0);
    const ai_i8* _conv5_conv5_2_Relu_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 149044);
    const ai_i32* _conv5_conv5_2_Relu_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 345652);
    ai_i8* _conv5_conv5_2_Relu_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 76800);
    ai_i16* _conv5_conv5_2_Relu_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 25600);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(555, 1, {(stai_ptr) _conv5_conv5_2_Relu_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_conv5_conv5_2_Relu_output_0_t_in_0_ptr_const_s8, _conv5_conv5_2_Relu_output_0_t_in_0_shape_w_const_u16, _conv5_conv5_2_Relu_output_0_t_in_0_shape_h_const_u16, _conv5_conv5_2_Relu_output_0_l_stride_1_const_u16, _conv5_conv5_2_Relu_output_0_l_stride_0_const_u16, _conv5_conv5_2_Relu_output_0_t_in_0_shape_ch_const_u16, _conv5_conv5_2_Relu_output_0_t_weight_0_ptr_const_s8, _conv5_conv5_2_Relu_output_0_t_out_0_shape_ch_const_u16, _conv5_conv5_2_Relu_output_0_t_weight_1_ptr_const_s32, _conv5_conv5_2_Relu_output_0_t_in_0_fmt_zero_const_s8, _conv5_conv5_2_Relu_output_0_t_out_0_fmt_zero_const_s8, _conv5_conv5_2_Relu_output_0_t_in_0_fmt_scale_const_f32, _conv5_conv5_2_Relu_output_0_t_out_0_fmt_scale_const_f32, _conv5_conv5_2_Relu_output_0_t_weight_0_fmt_scale_const_f32, _conv5_conv5_2_Relu_output_0_l_out_ch_format_const_layer_format_type, _conv5_conv5_2_Relu_output_0_t_out_0_ptr_s8, 1, 11008, _conv5_conv5_2_Relu_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(555, 1, {(stai_ptr) _conv5_conv5_2_Relu_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _conv5_conv5_2_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion */
  {
      const ai_i8* _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 76800);
    ai_float* _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(555, 1, {(stai_ptr) _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(_conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_t_in_0_ptr_const_s8, _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_t_out_0_ptr_f32, _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_t_in_0_fmt_scale_const_f32, _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(555, 1, {(stai_ptr) _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion */
  /* LITE_KERNEL_SECTION BEGIN _ReduceMean_output_0 */
  {
    
  forward_lite_reduce__ReduceMean_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _ReduceMean_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _ReduceMean_output_0_Mul */
  {
    
  forward_lite_eltwise__ReduceMean_output_0_Mul_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _ReduceMean_output_0_Mul */
  /* LITE_KERNEL_SECTION BEGIN _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion */
  {
      const ai_float* _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    ai_i8* _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 4096);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(558, 1, {(stai_ptr) _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(_ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_t_in_0_ptr_const_f32, _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_t_out_0_ptr_s8, _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_t_out_0_fmt_scale_const_f32, _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(558, 1, {(stai_ptr) _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion */
  /* LITE_KERNEL_SECTION BEGIN logits_QuantizeLinear_Input */
  {
    
  forward_lite_dense_integer_SSSA_ch_logits_QuantizeLinear_Input_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END logits_QuantizeLinear_Input */
  return net_ctx->_return_code;
}

/*****************************************************************************/
/*  Getters APIs Section  */
STAI_API_ENTRY
stai_size stai_network_get_context_size()
{
  return (stai_size)STAI_NETWORK_CONTEXT_SIZE;
}

#if defined(HAVE_NETWORK_INFO)
STAI_API_ENTRY
stai_return_code stai_network_get_info(
  stai_network* network,
  stai_network_info* info)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, info==NULL, STAI_ERROR_NETWORK_INVALID_INFO, net_ctx->_return_code)

  // Copy of network info struct
  *info = g_network_info;

  return STAI_SUCCESS;
}
#endif


STAI_API_ENTRY
stai_return_code stai_network_get_activations(
  stai_network* network, stai_ptr* activations, stai_size* n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  _STAI_SET_ERROR(net_ctx, !n_activations, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_activations = STAI_NETWORK_ACTIVATIONS_NUM;
for (stai_size idx=0; activations && (idx<STAI_NETWORK_ACTIVATIONS_NUM); idx++) {
    // get address of the activations buffers
    activations[idx] = net_ctx->_activations[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_weights(
  stai_network* network, stai_ptr* weights, stai_size* n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_weights, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_weights = STAI_NETWORK_WEIGHTS_NUM;
for (stai_size idx=0; weights && (idx<STAI_NETWORK_WEIGHTS_NUM); idx++) {
    // get address of the weights buffers
    weights[idx] = net_ctx->_weights[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_inputs(
  stai_network* network, stai_ptr* inputs, stai_size* n_inputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_inputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_inputs = STAI_NETWORK_IN_NUM;
  for (stai_size idx=0; inputs && (idx<STAI_NETWORK_IN_NUM); idx++) {
    inputs[idx] = net_ctx->_inputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_outputs(
  stai_network* network, stai_ptr* outputs, stai_size* n_outputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_outputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_outputs = STAI_NETWORK_OUT_NUM;
  for (stai_size idx=0; outputs && (idx<STAI_NETWORK_OUT_NUM); idx++) {
    outputs[idx] = net_ctx->_outputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_error(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /* return 1st generated error or STAI_SUCCESS if no errors so far */
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_states(
  stai_network* network, stai_ptr* states, stai_size* n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_states, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  /* get the number of internals states (supporting multi-heap also for internal states) */
  *n_states = STAI_NETWORK_STATES_NUM;

  STAI_UNUSED(states)
return net_ctx->_return_code;
}


/*****************************************************************************/
/*  Setters APIs Section  */

STAI_API_ENTRY
stai_return_code stai_network_set_activations(
  stai_network* network,
  const stai_ptr* activations,
  const stai_size n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _activations_alignment[] = STAI_NETWORK_ACTIVATIONS_ALIGNMENTS;
  STAI_PRINT("  [stai_network_set_activations] network(%p) activations[%d]: %p\n\n", net_ctx, n_activations, activations)
  _STAI_SET_ERROR(net_ctx, !activations,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_activations!=STAI_NETWORK_ACTIVATIONS_NUM,
                  STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_NUM, net_ctx->_return_code)

  for (stai_size idx=0; activations && idx<STAI_NETWORK_ACTIVATIONS_NUM; idx++) {
    STAI_PRINT("  activation[%d]: %p\n", idx, activations[idx])
    _STAI_SET_ERROR(net_ctx, activations[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)activations[idx]) & (_activations_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_activations[idx] = activations[idx];
  }
  net_ctx->_inputs[0] = activations[0] + 25600;

  net_ctx->_outputs[0] = activations[0] + 0;
_stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_weights(
  stai_network* network,
  const stai_ptr* weights,
  const stai_size n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _weights_alignment[] = STAI_NETWORK_WEIGHTS_ALIGNMENTS;
  _STAI_SET_ERROR(net_ctx, !weights,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_weights!=STAI_NETWORK_WEIGHTS_NUM,
                  STAI_ERROR_NETWORK_INVALID_WEIGHTS_NUM, net_ctx->_return_code)
  for (stai_size idx=0; weights && idx<STAI_NETWORK_WEIGHTS_NUM; idx++) {
    STAI_PRINT("  weight[%d]: %p\n", idx, weights[idx])
    _STAI_SET_ERROR(net_ctx, weights[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_WEIGHTS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)weights[idx]) & (_weights_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_weights[idx] = weights[idx];
  }_stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_inputs(
  stai_network* network,
  const stai_ptr* inputs,
  const stai_size n_inputs)
{
  const uintptr_t _inputs_alignment[] = STAI_NETWORK_IN_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !inputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_inputs!=STAI_NETWORK_IN_NUM,
                  STAI_ERROR_NETWORK_INVALID_IN_NUM, net_ctx->_return_code)

  for (stai_size idx=0; inputs && idx<STAI_NETWORK_IN_NUM; idx++) {
    STAI_PRINT("  input[%d]: %p\n", idx, inputs[idx])
    _STAI_SET_ERROR(net_ctx, inputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_IN_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)inputs[idx]) & (_inputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_inputs[idx] = inputs[idx];
  }

  _stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_outputs(
  stai_network* network,
  const stai_ptr* outputs,
  const stai_size n_outputs)
{
  const uintptr_t _outputs_alignment[] = STAI_NETWORK_OUT_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !outputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_outputs!=STAI_NETWORK_OUT_NUM,
                  STAI_ERROR_NETWORK_INVALID_OUT_NUM, net_ctx->_return_code)

  for (stai_size idx=0; outputs && idx<n_outputs; idx++) {
    STAI_PRINT("  output[%d]: %p\n", idx, outputs[idx])
    _STAI_SET_ERROR(net_ctx, outputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_OUT_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)outputs[idx]) & (_outputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_outputs[idx] = outputs[idx];
  }

  _stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_states(
  stai_network* network,
  const stai_ptr* states,
  const stai_size n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  STAI_UNUSED(states)
  STAI_UNUSED(n_states)
_stai_network_check(net_ctx);
  return net_ctx->_return_code;
}

STAI_API_ENTRY
stai_return_code stai_network_set_callback(
  stai_network* network, const stai_event_cb cb, void* cb_cookie)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  STAI_PRINT("  set_callback %p cb %p cookie %p\n", net_ctx, cb, cb_cookie)
  // _STAI_SET_ERROR(net_ctx, cb==NULL, STAI_ERROR_NETWORK_INVALID_CALLBACK, net_ctx->_return_code)
  net_ctx->_callback = cb;
  net_ctx->_callback_cookie = cb_cookie;
  return net_ctx->_return_code;
}

#undef _STAI_SET_ERROR
#undef _STAI_CONTEXT_ALIGNMENT
#undef _STAI_CONTEXT_ACQUIRE
#undef _STAI_NETWORK_EVENT_NODE_START_CB
#undef _STAI_NETWORK_EVENT_NODE_STOP_CB
#undef _STAI_NETWORK_MODEL_SIGNATURE
#undef _STAI_NETWORK_DATETIME
#undef _STAI_NETWORK_COMPILE_DATETIME

