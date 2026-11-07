/**
  ******************************************************************************
  * @file    network.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-11-07T17:23:53+0800
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
#define _STAI_NETWORK_MODEL_SIGNATURE     "0xda673b932a4b9c8a17ddadc2e88ea2d9"
#define _STAI_NETWORK_DATETIME            "2026-11-07T17:23:53+0800"
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
      STAI_DECLARE_ARRAY(float, 1, 0.010541616939008236f),
      STAI_DECLARE_ARRAY(int16_t, 1, 28)),
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
    AI_PACK_INTQ_SCALE(0.036216773092746735f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #3 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_conv1_conv1_2_Relu_output_0_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 24,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0010181915713474154f, 1.783251519782425e-07f, 0.003733542049303651f, 0.0007266007014550269f, 0.0013567196438089013f, 0.0027431801427155733f, 1.1084063800126387e-07f, 0.0008309884578920901f, 0.004619491286575794f, 0.0008236010326072574f, 1.1695111652443302e-06f, 0.01205558329820633f, 6.850764293631073e-07f, 0.006962823681533337f, 0.0021706526167690754f, 0.0006033192039467394f, 3.9236372373352424e-08f, 0.005057471804320812f, 3.183077690316338e-10f, 0.0063214111141860485f, 0.006464673206210136f, 0.009862248785793781f, 0.0007605022983625531f, 1.1624656082176443e-07f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)))

/* Int quant #4 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_conv1_conv1_2_Relu_output_0_scratch1_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.036216773092746735f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #5 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_0_branch1_branch1_4_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0099414624273777f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #6 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_0_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01587417535483837f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #7 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_0_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01587417535483837f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #8 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_0_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01587417535483837f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #9 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_1_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01587417535483837f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #10 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_1_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01587417535483837f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #11 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_1_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009310860186815262f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #12 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_1_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01587417535483837f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #13 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_1_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01587417535483837f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #14 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_2_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01587417535483837f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #15 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_2_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01587417535483837f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #16 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_2_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.012239465489983559f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #17 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_2_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01587417535483837f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #18 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_2_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01587417535483837f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #19 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_3_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01587417535483837f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #20 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_3_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01587417535483837f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #21 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_3_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.017604099586606026f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #22 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_3_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.017604099586606026f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #23 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage2_stage2_3_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.017604099586606026f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #24 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_0_branch1_branch1_4_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010096961632370949f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #25 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_0_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.008830580860376358f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #26 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_0_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010096961632370949f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #27 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_0_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010096961632370949f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #28 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_1_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010096961632370949f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #29 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_1_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010096961632370949f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #30 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_1_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010089820250868797f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #31 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_1_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010096961632370949f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #32 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_1_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010096961632370949f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #33 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_2_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010096961632370949f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #34 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_2_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010096961632370949f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #35 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_2_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009504112415015697f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #36 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_2_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010096961632370949f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #37 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_2_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010096961632370949f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #38 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_3_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010096961632370949f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #39 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_3_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010096961632370949f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #40 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_3_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.012745935469865799f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #41 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_3_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.012745935469865799f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #42 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_3_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.012745935469865799f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #43 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_4_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.012745935469865799f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #44 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_4_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.012745935469865799f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #45 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_4_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009706230834126472f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #46 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_4_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010089820250868797f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #47 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_4_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010089820250868797f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #48 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_5_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010089820250868797f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #49 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_5_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010089820250868797f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #50 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_5_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011862967163324356f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #51 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_5_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011862967163324356f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #52 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_5_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011862967163324356f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #53 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_6_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011862967163324356f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #54 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_6_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011862967163324356f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #55 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_6_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011234721168875694f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #56 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_6_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011862967163324356f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #57 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_6_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011862967163324356f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #58 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_7_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011862967163324356f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #59 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_7_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011862967163324356f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #60 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_7_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.012842380441725254f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #61 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_7_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.012842380441725254f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #62 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage3_stage3_7_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.012842380441725254f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #63 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_0_branch1_branch1_4_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009751124307513237f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #64 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_0_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.015814999118447304f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #65 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_0_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.015814999118447304f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #66 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_0_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.015814999118447304f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #67 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_1_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.015814999118447304f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #68 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_1_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.015814999118447304f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #69 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_1_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.013661094941198826f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #70 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_1_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.013661094941198826f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #71 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_1_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.013661094941198826f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #72 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_2_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.013661094941198826f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #73 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_2_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.013661094941198826f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #74 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_2_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011834115721285343f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #75 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_2_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.013661094941198826f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #76 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_2_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.013661094941198826f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #77 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_3_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.013661094941198826f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #78 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_3_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.013661094941198826f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #79 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_3_branch2_branch2_7_Relu_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01655641943216324f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #80 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_3_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01655641943216324f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #81 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_stage4_stage4_3_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01655641943216324f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #82 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0020063179545104504f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #83 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(logits_QuantizeLinear_Input_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010541616939008236f),
    AI_PACK_INTQ_ZP(28)))

/* Int quant #84 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(logits_QuantizeLinear_Input_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 3,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0005415917257778347f, 0.0005223834305070341f, 0.0005306615494191647f),
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
static const ai_float _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.036216773092746735f;
static const ai_float _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.05089972913265228f;
static const ai_float _stage2_stage2_0_branch1_branch1_0_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.007620061282068491f, 0.006357572041451931f, 0.0012073465622961521f, 0.008113493211567402f, 0.002738859271630645f, 0.012162820436060429f, 0.00018533962429501116f, 0.01936906762421131f, 0.001444663037545979f, 0.009010426700115204f, 0.022087767720222473f, 0.0018509301589801908f, 0.009797992184758186f, 0.0017177278641611338f, 0.008176278322935104f, 0.009410581551492214f, 0.0006166623788885772f, 0.002492437604814768f, 0.002948698354884982f, 0.0013739188434556127f, 0.0016934454906731844f, 0.001217293320223689f, 0.0045586805790662766f, 0.0018897184636443853f);
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
static const ai_float _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.05089972913265228f;
static const ai_float _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.0099414624273777f;
static const ai_float _stage2_stage2_0_branch1_branch1_4_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0029386095702648163f, 0.002106912201270461f, 0.001495857024565339f, 0.002535897772759199f, 0.0037108829710632563f, 0.003920815419405699f, 0.004799000918865204f, 0.004313756246119738f, 0.009540751576423645f, 0.002870820229873061f, 0.00927759613841772f, 0.005323511082679033f, 0.003819207428023219f, 0.0021934336982667446f, 0.004661602433770895f, 0.005778305698186159f, 0.0032845663372427225f, 0.002799540525302291f, 0.0021042015869170427f, 0.003965800162404776f, 0.0034846619237214327f, 0.0027808949816972017f, 0.0036591875832527876f, 0.005587035324424505f);
static const ai_layer_format_type _stage2_stage2_0_branch1_branch1_4_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 40;
static const ai_u16 _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 40;
static const ai_u16 _stage2_stage2_0_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_0_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 24;
static const ai_i8 _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.036216773092746735f;
static const ai_float _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.011877273209393024f;
static const ai_float _stage2_stage2_0_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.004083655308932066f, 0.004404983017593622f, 0.003188223112374544f, 0.004833036568015814f, 0.004290387034416199f, 0.005040396004915237f, 0.005138786043971777f, 0.003533013863489032f, 2.840795332303969e-07f, 0.0012263001408427954f, 0.0026373134460300207f, 0.0020624541211873293f, 2.652651573953335e-07f, 0.0036216634325683117f, 0.0023647439666092396f, 0.004035369027405977f, 0.006427531596273184f, 1.8580054472749907e-07f, 0.0045623863115906715f, 0.0028746183961629868f, 0.006063827779144049f, 0.004239239264279604f, 0.0032384528312832117f, 0.005348834674805403f);
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
static const ai_float _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.011877273209393024f;
static const ai_float _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.04383639246225357f;
static const ai_float _stage2_stage2_0_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.008410516194999218f, 0.006312170065939426f, 0.0050432863645255566f, 0.005004335660487413f, 0.005681734066456556f, 0.023019904270768166f, 0.00734157208353281f, 0.006263909861445427f, 0.01547498069703579f, 0.05404521897435188f, 0.00713498517870903f, 0.003916549496352673f, 0.017901035025715828f, 0.004892917349934578f, 0.004889654461294413f, 0.01177006121724844f, 0.018743671476840973f, 0.006115236785262823f, 0.00472424179315567f, 0.0035973642952740192f, 0.021352071315050125f, 0.0050874087028205395f, 0.011424517259001732f, 0.010920220986008644f);
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
static const ai_float _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.04383639246225357f;
static const ai_float _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.01587417535483837f;
static const ai_float _stage2_stage2_0_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002086132764816284f, 0.005416753701865673f, 0.00101186940446496f, 0.002792065031826496f, 0.001755265868268907f, 0.002001755405217409f, 0.001776937278918922f, 0.0041161212138831615f, 0.002291013952344656f, 0.004071293398737907f, 6.13963138107465e-08f, 0.004399098455905914f, 0.004913241136819124f, 0.003805562388151884f, 0.003588151652365923f, 0.003580989083275199f, 0.003015712834894657f, 0.004357592202723026f, 0.0018472126685082912f, 0.0036405976861715317f, 0.002659941092133522f, 0.00230084965005517f, 0.001506303669884801f, 0.004150097258388996f);
static const ai_layer_format_type _stage2_stage2_0_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage2_stage2_1_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_1_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 24;
static const ai_i8 _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.01587417535483837f;
static const ai_float _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.009167506359517574f;
static const ai_float _stage2_stage2_1_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0029637545812875032f, 0.002589355455711484f, 0.004915847443044186f, 0.0028037005104124546f, 0.006281282287091017f, 0.007163602393120527f, 0.0041998508386313915f, 0.006269283127039671f, 0.004179887939244509f, 0.0054219430312514305f, 0.004748482257127762f, 0.00518404645845294f, 0.006392206531018019f, 0.0063549368642270565f, 0.005579086486250162f, 0.003714620601385832f, 0.003797840792685747f, 0.003092067549005151f, 0.005347874015569687f, 0.004219328984618187f, 0.0038884817622601986f, 0.004003705456852913f, 0.0031769636552780867f, 0.008276481181383133f);
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
static const ai_i8 _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = -2;
static const ai_float _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.009167506359517574f;
static const ai_float _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.025287853553891182f;
static const ai_float _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.008723950944840908f, 0.018030820414423943f, 0.026583120226860046f, 0.01859266124665737f, 0.004481245297938585f, 0.01023793313652277f, 0.017009414732456207f, 0.0062529356218874454f, 0.005563030485063791f, 0.01009626779705286f, 0.0033065713942050934f, 0.01970549114048481f, 0.05791433900594711f, 0.02333155833184719f, 0.020588329061865807f, 0.020190466195344925f, 0.027111662551760674f, 0.023044046014547348f, 0.014590419828891754f, 0.018235966563224792f, 0.005322479642927647f, 0.013302808627486229f, 0.036758922040462494f, 0.017980022355914116f);
static const ai_u16 _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_1_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 20;

static const ai_u16 _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage2_stage2_1_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_1_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 24;
static const ai_i8 _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = -2;
static const ai_i8 _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.025287853553891182f;
static const ai_float _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.009310860186815262f;
static const ai_float _stage2_stage2_1_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.003068035002797842f, 0.0032204978633672f, 0.0025292879436165094f, 0.0032118000090122223f, 0.005055528599768877f, 0.005011856555938721f, 0.0019046261440962553f, 0.0024900257121771574f, 0.003139483043923974f, 0.002291965065523982f, 0.0031715764198452234f, 0.002741636708378792f, 0.003157540922984481f, 0.00369100458920002f, 0.0037216884084045887f, 0.0027890284545719624f, 0.0031070709228515625f, 0.003413611790165305f, 0.003947000484913588f, 0.0052637564949691296f, 0.0033740790095180273f, 0.002930721966549754f, 0.0030474895611405373f, 0.004130013287067413f);
static const ai_layer_format_type _stage2_stage2_1_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage2_stage2_2_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_2_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 24;
static const ai_i8 _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.01587417535483837f;
static const ai_float _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.012410460971295834f;
static const ai_float _stage2_stage2_2_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.00334965530782938f, 0.005959724076092243f, 0.00016896025044843554f, 0.005965766962617636f, 0.0021601670887321234f, 0.00735452538356185f, 0.0011786752147600055f, 0.0032266126945614815f, 0.004026184324175119f, 0.0036356188356876373f, 0.003452615812420845f, 0.004837644286453724f, 0.003627308877184987f, 0.0030564258340746164f, 0.006289504002779722f, 0.0011366746621206403f, 0.002147011924535036f, 0.0017974319634959102f, 0.003929363563656807f, 0.003096829866990447f, 0.004950560629367828f, 0.0029959597159177065f, 0.0069845886901021f, 0.001516551012173295f);
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
static const ai_i8 _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = -2;
static const ai_float _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.012410460971295834f;
static const ai_float _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.0314108207821846f;
static const ai_float _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.027131952345371246f, 0.03230496868491173f, 0.6211004257202148f, 0.012092887423932552f, 0.005822541192173958f, 0.008418367244303226f, 0.030896572396159172f, 0.006227700039744377f, 0.006764995399862528f, 0.036162566393613815f, 0.00358839170075953f, 0.014251363463699818f, 0.025807084515690804f, 0.022618209943175316f, 0.009038218297064304f, 0.029716303572058678f, 0.019176658242940903f, 0.009933123365044594f, 0.01301493588835001f, 0.03290552645921707f, 0.007250207010656595f, 0.009044261649250984f, 0.006350032519549131f, 0.0468582846224308f);
static const ai_u16 _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_2_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 20;

static const ai_u16 _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage2_stage2_2_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_2_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 24;
static const ai_i8 _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = -2;
static const ai_i8 _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.0314108207821846f;
static const ai_float _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.012239465489983559f;
static const ai_float _stage2_stage2_2_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002758997492492199f, 0.00596808735281229f, 0.002934431191533804f, 0.005667245946824551f, 0.0036271619610488415f, 0.003375852946192026f, 0.003664332441985607f, 0.00200194725766778f, 0.0019255069782957435f, 0.005736049264669418f, 0.0012762414989992976f, 0.0036268895491957664f, 0.0032497248612344265f, 0.001675735111348331f, 0.0023624494206160307f, 0.005023897625505924f, 0.0021200485061854124f, 0.0038104173727333546f, 6.903924543166795e-08f, 0.0025139320641756058f, 0.00449328264221549f, 0.0031157187186181545f, 1.5851202306294e-07f, 0.0039648693054914474f);
static const ai_layer_format_type _stage2_stage2_2_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage2_stage2_3_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_3_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 24;
static const ai_i8 _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.01587417535483837f;
static const ai_float _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.009754936210811138f;
static const ai_float _stage2_stage2_3_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.009792149998247623f, 0.004890102427452803f, 0.005197854246944189f, 0.002079200465232134f, 0.007108267862349749f, 0.0018954487750306726f, 0.005505430046468973f, 0.0017181779257953167f, 0.005082058720290661f, 0.0026670920196920633f, 0.004716602154076099f, 0.004429853055626154f, 0.0061150286346673965f, 0.007373124826699495f, 0.0012855115346610546f, 0.0035908804275095463f, 0.0024584059137851f, 0.003680568654090166f, 0.0029955965001136065f, 0.0030327136628329754f, 0.006939615588635206f, 0.0026957890950143337f, 0.0019466133089736104f, 0.01054742094129324f);
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
static const ai_i8 _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = 15;
static const ai_float _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.009754936210811138f;
static const ai_float _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.029799574986100197f;
static const ai_float _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.00634629325941205f, 0.010780422016978264f, 0.025910962373018265f, 0.023239627480506897f, 0.00431937538087368f, 0.016947997733950615f, 0.016682356595993042f, 0.016978083178400993f, 0.020840846002101898f, 0.008509420789778233f, 0.011845444329082966f, 0.013514498248696327f, 0.02456425130367279f, 0.007445587310940027f, 0.030197741463780403f, 0.010621783323585987f, 0.022304294630885124f, 0.02314179390668869f, 0.022891318425536156f, 0.022618887946009636f, 0.009993846528232098f, 0.026075465604662895f, 0.03108617477118969f, 0.004409748129546642f);
static const ai_u16 _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_3_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 20;

static const ai_u16 _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage2_stage2_3_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage2_stage2_3_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 24;
static const ai_i8 _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = 15;
static const ai_i8 _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.029799574986100197f;
static const ai_float _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.017604099586606026f;
static const ai_float _stage2_stage2_3_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.005658996291458607f, 0.0047878241166472435f, 0.003535272553563118f, 0.008280046284198761f, 0.003028776030987501f, 0.004129626788198948f, 0.0033985984046012163f, 0.003192278789356351f, 0.005194005090743303f, 0.0035630627535283566f, 0.005386661272495985f, 0.002982750069350004f, 0.007385149132460356f, 0.005861528217792511f, 0.004990552086383104f, 0.0036578509025275707f, 0.004015802405774593f, 0.006086382083594799f, 0.004556446336209774f, 0.003974645398557186f, 0.005332103930413723f, 0.003965360578149557f, 0.0034611907321959734f, 0.004561933688819408f);
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
static const ai_i8 _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_out_0_fmt_zero_const_s8 = -10;
static const ai_float _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.017604099586606026f;
static const ai_float _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.0244765542447567f;
static const ai_float _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0042412360198795795f, 0.00640112766996026f, 0.004526429809629917f, 0.007140455301851034f, 0.005126448813825846f, 0.005886170081794262f, 0.004622683394700289f, 0.0036725546233356f, 0.0024597467854619026f, 0.006369769107550383f, 0.0038996075745671988f, 0.004088681656867266f, 0.005464999005198479f, 0.004837560933083296f, 0.004168706014752388f, 0.008243612013757229f, 0.010074538178741932f, 0.006352964323014021f, 0.0037032098043709993f, 0.006795445457100868f, 0.005278532858937979f, 0.006443613674491644f, 0.004566766321659088f, 0.005446452181786299f, 0.005411413498222828f, 0.0033692470751702785f, 0.006329559255391359f, 0.006268516648560762f, 0.004543486051261425f, 0.0048970673233270645f, 0.0046340120024979115f, 0.009437770582735538f, 0.011485216207802296f, 0.00835734885185957f, 0.005389928352087736f, 0.005802309140563011f, 0.0076014273799955845f, 0.007203899789601564f, 0.0023026035632938147f, 0.005236073397099972f, 0.00465148501098156f, 0.004203610587865114f, 0.006220996379852295f, 0.005342378281056881f, 0.006654066499322653f, 0.0035174647346138954f, 0.008952487260103226f, 0.008214050903916359f);
static const ai_u16 _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_out_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_0_branch1_branch1_0_Conv_output_0_t_out_0_shape_h_const_u16 = 10;

static const ai_u16 _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_0_branch1_branch1_4_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_0_branch1_branch1_4_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_in_0_fmt_zero_const_s8 = -10;
static const ai_i8 _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.0244765542447567f;
static const ai_float _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.010096961632370949f;
static const ai_float _stage3_stage3_0_branch1_branch1_4_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0017391737783327699f, 0.0021939428988844156f, 0.001606462406925857f, 0.0016863321652635932f, 0.002055899705737829f, 0.0025619694497436285f, 0.003143314504995942f, 0.001962563954293728f, 0.0030989921651780605f, 0.00179116684012115f, 0.003212774870917201f, 0.0020746528171002865f, 0.0024839919060468674f, 0.0016495477175340056f, 0.003568396670743823f, 0.0027167389634996653f, 0.0021334998309612274f, 0.00263965898193419f, 0.0029469183646142483f, 0.0018966946518048644f, 0.0032427827827632427f, 0.0018662281800061464f, 0.0025584667455404997f, 0.0026025339029729366f, 0.004317490849643946f, 0.0022933431901037693f, 0.0017585197929292917f, 0.0014206275809556246f, 0.0028036488220095634f, 0.0037911124527454376f, 0.004887585993856192f, 0.002608268056064844f, 0.00624944968149066f, 0.004513879772275686f, 0.0022474073339253664f, 0.006229047663509846f, 0.0028315086383372545f, 0.004941315855830908f, 0.00402982160449028f, 0.003431587480008602f, 0.00399750517681241f, 0.0022044184152036905f, 0.0027537604328244925f, 0.002790010767057538f, 0.0024935442488640547f, 0.0013394173001870513f, 0.0027886941097676754f, 0.001886439509689808f);
static const ai_layer_format_type _stage3_stage3_0_branch1_branch1_4_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 20;
static const ai_u16 _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 20;
static const ai_u16 _stage3_stage3_0_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_0_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.017604099586606026f;
static const ai_float _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.012060292065143585f;
static const ai_float _stage3_stage3_0_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.009437942877411842f, 0.00438260193914175f, 0.0008719348697923124f, 0.007026353385299444f, 0.004006102215498686f, 0.0063851396553218365f, 0.007339800707995892f, 0.0023964152205735445f, 0.005158634390681982f, 0.0020799816120415926f, 0.004355499986559153f, 0.0025845738127827644f, 0.0029374046716839075f, 0.005881906021386385f, 0.006061873864382505f, 0.0055633652955293655f, 2.6429629542690236e-06f, 0.004269568249583244f, 0.003953773062676191f, 0.004343778360635042f, 0.005017448216676712f, 0.0035327074583619833f, 0.004194074776023626f, 0.0038910978473722935f, 0.005669723730534315f, 0.005857539363205433f, 0.005602170713245869f, 0.0034878533333539963f, 0.001541985897347331f, 0.0035263379104435444f, 0.008854713290929794f, 0.0014611708465963602f, 0.0052613308653235435f, 0.0029249931685626507f, 0.009067323058843613f, 0.009267657995223999f, 0.004955010954290628f, 0.0032717357389628887f, 0.007600534241646528f, 0.007145240902900696f, 0.006233991123735905f, 0.0036141634918749332f, 0.0039394269697368145f, 0.0028965219389647245f, 0.004247118253260851f, 0.004632256459444761f, 0.0051189810037612915f, 0.002230981132015586f);
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
static const ai_i8 _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = 12;
static const ai_float _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.012060292065143585f;
static const ai_float _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.030819982290267944f;
static const ai_float _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.005534980911761522f, 0.0037558982148766518f, 0.5753169059753418f, 0.00814139749854803f, 0.00619571702554822f, 0.014649499207735062f, 0.004306224174797535f, 0.011959648691117764f, 0.011546731926500797f, 0.010693399235606194f, 0.012209868058562279f, 0.03728402405977249f, 0.00571020832285285f, 0.008045981638133526f, 0.006658370606601238f, 0.0033891377970576286f, 0.005123259499669075f, 0.0064751943573355675f, 0.007906866259872913f, 0.006600485183298588f, 0.0059546297416090965f, 0.0063872309401631355f, 0.007236952427774668f, 0.006841677241027355f, 0.009479810483753681f, 0.007754551246762276f, 0.008461477234959602f, 0.012799334712326527f, 0.012285074219107628f, 0.006159815937280655f, 0.004101805854588747f, 0.011283892206847668f, 0.005467587616294622f, 0.008814917877316475f, 0.00813934113830328f, 0.006518636364489794f, 0.006378521677106619f, 0.008261789567768574f, 0.010621738620102406f, 0.0031372641678899527f, 0.003856656840071082f, 0.006724596489220858f, 0.007884984835982323f, 0.009160236455500126f, 0.009611314162611961f, 0.005968873389065266f, 0.006955798715353012f, 0.007633924018591642f);
static const ai_u16 _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_0_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 10;

static const ai_u16 _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_0_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_0_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = 12;
static const ai_i8 _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.030819982290267944f;
static const ai_float _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.008830580860376358f;
static const ai_float _stage3_stage3_0_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0018186613451689482f, 0.0018213610164821148f, 0.0026478625368326902f, 0.002638742094859481f, 0.0030982820317149162f, 0.002306175883859396f, 0.0021871954668313265f, 0.001694752019830048f, 0.0025904092472046614f, 0.0030569995287805796f, 0.002993643516674638f, 0.002666487591341138f, 0.0014867228455841541f, 0.0031774311792105436f, 0.0018488975474610925f, 0.0024200621992349625f, 0.002545107388868928f, 0.00369349867105484f, 0.0018642782233655453f, 0.0025611931923776865f, 0.005348589271306992f, 0.0037709607277065516f, 0.003153190715238452f, 0.002232246333733201f, 0.0023340133484452963f, 0.0038995351642370224f, 0.002425385871902108f, 0.004858924075961113f, 0.0023620203137397766f, 0.00338052399456501f, 0.0021914842072874308f, 0.004049983341246843f, 0.004221274051815271f, 0.0017744259675964713f, 0.0019574379548430443f, 0.003557588905096054f, 0.0015502875903621316f, 0.0036069583147764206f, 0.0035660304129123688f, 0.004101383499801159f, 0.004667976871132851f, 0.0023860542569309473f, 0.0015323696425184608f, 0.00224351417273283f, 0.0036043685395270586f, 0.003147886833176017f, 0.002297887811437249f, 0.003441183129325509f);
static const ai_layer_format_type _stage3_stage3_0_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_1_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_1_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.010096961632370949f;
static const ai_float _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.00764327310025692f;
static const ai_float _stage3_stage3_1_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0028221982065588236f, 0.0030023357830941677f, 0.0038347565568983555f, 0.00478289695456624f, 0.0016372770769521594f, 0.002212954219430685f, 0.0028551286086440086f, 0.0031303521245718002f, 0.005772956646978855f, 0.0034666727297008038f, 0.005199749954044819f, 0.0024610848631709814f, 0.007026177830994129f, 0.00317353755235672f, 0.0033929513301700354f, 0.003753914497792721f, 0.0034124564845114946f, 0.0022695225197821856f, 0.0015438428381457925f, 0.0037495505530387163f, 0.006308094598352909f, 0.003224864834919572f, 0.004839091561734676f, 0.0012601454509422183f, 0.004151653964072466f, 0.003075092099606991f, 0.003349458333104849f, 0.0028714165091514587f, 0.0009212825098074973f, 0.0016905063530430198f, 0.00333002139814198f, 0.0032064870465546846f, 0.002422998659312725f, 0.0015643566148355603f, 0.003353383159264922f, 0.003356538712978363f, 0.0046870470978319645f, 0.0034685286227613688f, 0.0021837458480149508f, 0.0033779856748878956f, 0.0043664416298270226f, 0.003195874160155654f, 0.005526745226234198f, 0.003484993241727352f, 0.0025449630338698626f, 0.0033705891110002995f, 0.0026666594203561544f, 0.0016343982424587011f);
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
static const ai_i8 _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = 3;
static const ai_float _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.00764327310025692f;
static const ai_float _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.023988738656044006f;
static const ai_float _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.014156053774058819f, 0.004793271888047457f, 0.005391180515289307f, 0.017072442919015884f, 0.019810611382126808f, 0.016349833458662033f, 0.026009442284703255f, 0.007044983096420765f, 0.011358159594237804f, 0.00548459542915225f, 0.010604101233184338f, 0.02413424663245678f, 0.01838465966284275f, 0.017514925450086594f, 0.013769406825304031f, 0.017185501754283905f, 0.007577532436698675f, 0.020845741033554077f, 0.0238071009516716f, 0.008099204860627651f, 0.023211011663079262f, 0.01499904878437519f, 0.012250087223947048f, 0.01943448930978775f, 0.007237554527819157f, 0.007794189266860485f, 0.016480743885040283f, 0.0056252265349030495f, 0.04009323939681053f, 0.013282179832458496f, 0.020200612023472786f, 0.008276741951704025f, 0.013602842576801777f, 0.03652794659137726f, 0.010374042205512524f, 0.017573529854416847f, 0.009958473034203053f, 0.022791091352701187f, 0.02852795273065567f, 0.006885505747050047f, 0.0025420153979212046f, 0.012905651703476906f, 0.014785517938435078f, 0.00874158926308155f, 0.018337344750761986f, 0.01425192691385746f, 0.014326822012662888f, 0.01635662093758583f);
static const ai_u16 _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_1_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 10;

static const ai_u16 _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_1_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_1_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = 3;
static const ai_i8 _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.023988738656044006f;
static const ai_float _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.010089820250868797f;
static const ai_float _stage3_stage3_1_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.003160919761285186f, 0.0022181631065905094f, 0.0027395416982471943f, 0.0015214484883472323f, 0.003133353777229786f, 0.0029087557923048735f, 0.004642544314265251f, 0.0027546738274395466f, 0.0026620381977409124f, 0.0030134902335703373f, 0.0026156397070735693f, 0.0022148131392896175f, 0.003182255895808339f, 0.00163238903041929f, 0.0020815860480070114f, 0.0024059717543423176f, 0.003972008358687162f, 0.003117656335234642f, 0.002783662872388959f, 0.002273090183734894f, 0.003493596101179719f, 0.0016437951708212495f, 0.0019077922916039824f, 0.0016452837735414505f, 0.005427523981779814f, 0.005528940819203854f, 0.0021381410770118237f, 0.0036844026762992144f, 0.004294197075068951f, 0.004405555780977011f, 0.003789368085563183f, 0.00817343220114708f, 0.00501089496538043f, 0.004352410323917866f, 0.005674990825355053f, 0.004334595054388046f, 0.005344484932720661f, 0.0026002260856330395f, 0.0023266058415174484f, 0.00669831270352006f, 0.006028997246176004f, 0.0037667786236852407f, 0.004559891764074564f, 0.00265091797336936f, 0.004389280453324318f, 0.007288319990038872f, 0.002527280943468213f, 0.00336647592484951f);
static const ai_layer_format_type _stage3_stage3_1_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_2_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_2_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.010096961632370949f;
static const ai_float _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.005412495695054531f;
static const ai_float _stage3_stage3_2_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0019045128719881177f, 3.7927836729068076e-06f, 0.0022585042752325535f, 0.004508259706199169f, 0.002691378118470311f, 0.0022711502388119698f, 0.0018593736458569765f, 0.005093015264719725f, 0.002311839023604989f, 0.0035462602972984314f, 0.003075246000662446f, 0.003665920114144683f, 0.0029644828755408525f, 0.0017234818078577518f, 0.0038545045536011457f, 0.002773743122816086f, 0.004508255515247583f, 0.002672553528100252f, 0.0032885114196687937f, 0.0025523907970637083f, 0.006670014001429081f, 0.003927729558199644f, 0.0037178636994212866f, 0.00125147495418787f, 0.0009612690773792565f, 0.003694460727274418f, 0.0032981683034449816f, 0.005691807717084885f, 0.003535978961735964f, 0.002546770730987191f, 0.004089171998202801f, 0.0020544060971587896f, 0.004843018017709255f, 0.005104025825858116f, 0.0022408803924918175f, 0.002410749439150095f, 0.003238546894863248f, 0.005562884733080864f, 0.0018155283760279417f, 0.0035434148740023375f, 0.002810670528560877f, 0.002968443790450692f, 0.003371925326064229f, 0.0015325220301747322f, 0.0029688486829400063f, 0.003304783720523119f, 0.004133519250899553f, 0.0014399440260604024f);
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
static const ai_i8 _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = 19;
static const ai_float _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.005412495695054531f;
static const ai_float _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.016569582745432854f;
static const ai_float _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.008107711561024189f, 0.01689939573407173f, 0.016073130071163177f, 0.015343207865953445f, 0.022693384438753128f, 0.019629891961812973f, 0.03350846841931343f, 0.027691403403878212f, 0.01406570989638567f, 0.018322626128792763f, 0.012079504318535328f, 0.013835368677973747f, 0.012622641399502754f, 0.013868877664208412f, 0.01309524942189455f, 0.012850225903093815f, 0.020118126645684242f, 0.019629718735814095f, 0.017153741791844368f, 0.015918023884296417f, 0.010834858752787113f, 0.004484975710511208f, 0.013004747219383717f, 0.019402241334319115f, 0.06635698676109314f, 0.013048729859292507f, 0.007192122749984264f, 0.012591317296028137f, 0.010410384275019169f, 0.008663621731102467f, 0.013636109419167042f, 0.025795016437768936f, 0.015795256942510605f, 0.012492449022829533f, 0.015261707827448845f, 0.010771864093840122f, 0.00651336507871747f, 0.01094368938356638f, 0.011547495611011982f, 0.012590083293616772f, 0.006652165204286575f, 0.023171216249465942f, 0.02140752598643303f, 0.036361560225486755f, 0.01083789486438036f, 0.015523796901106834f, 0.00884635653346777f, 0.025954851880669594f);
static const ai_u16 _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_2_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 10;

static const ai_u16 _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_2_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_2_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = 19;
static const ai_i8 _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.016569582745432854f;
static const ai_float _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.009504112415015697f;
static const ai_float _stage3_stage3_2_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.003146771341562271f, 0.0021402589045464993f, 0.002189837396144867f, 0.0018513298127800226f, 0.002847227267920971f, 0.001487336470745504f, 0.0035627896431833506f, 0.003313858527690172f, 0.001983356662094593f, 0.0021827223245054483f, 0.002793462248519063f, 0.0033516904804855585f, 0.0042829690501093864f, 0.0015064222970977426f, 0.0022693187929689884f, 0.004861050751060247f, 0.004270792007446289f, 0.0022635944187641144f, 0.001787817571312189f, 0.0017437069909647107f, 0.003024871926754713f, 0.0029266492929309607f, 0.003241107566282153f, 0.004276746418327093f, 0.0016626473516225815f, 0.006876219995319843f, 0.0030344880651682615f, 0.003166095120832324f, 0.005594885908067226f, 0.004269551020115614f, 0.002281146589666605f, 0.0021068567875772715f, 0.002604080131277442f, 0.004425840452313423f, 0.0018079238943755627f, 0.0019071437418460846f, 0.0060402825474739075f, 0.002498111454769969f, 0.0026809610426425934f, 0.0015826032031327486f, 0.0029638116247951984f, 0.0028366376645863056f, 0.003747966606169939f, 0.0014166388427838683f, 0.001769932801835239f, 0.0028513132128864527f, 0.004305986687541008f, 0.004335910547524691f);
static const ai_layer_format_type _stage3_stage3_2_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_3_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_3_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.010096961632370949f;
static const ai_float _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.007408987730741501f;
static const ai_float _stage3_stage3_3_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.00279100495390594f, 0.002859512809664011f, 0.004531551152467728f, 0.004072288516908884f, 0.004564961418509483f, 0.001977974781766534f, 0.002281719585880637f, 0.00511846924200654f, 0.004229981917887926f, 0.004857421852648258f, 0.004199076909571886f, 0.0025579275097697973f, 0.0023717323783785105f, 0.003937359433621168f, 0.003444184549152851f, 0.005757295526564121f, 0.003422374604269862f, 0.0019616789650171995f, 0.0025476415175944567f, 0.003021647920832038f, 0.0034009225200861692f, 0.008712094277143478f, 0.002505446318536997f, 0.0035196945536881685f, 0.0038558158557862043f, 0.0024528102949261665f, 0.0038949805311858654f, 0.0037451370153576136f, 0.002609619405120611f, 0.0042621418833732605f, 0.0024733322206884623f, 0.0026617844123393297f, 0.005418891552835703f, 0.0029342782218009233f, 0.007827087305486202f, 0.0021808568853884935f, 0.003223937936127186f, 0.008041681721806526f, 0.00880129262804985f, 0.0031222731340676546f, 0.0024832836352288723f, 0.003179058898240328f, 0.0016402625478804111f, 0.003634869819507003f, 0.002403195481747389f, 0.0038085654377937317f, 0.003136721672490239f, 0.0021475516259670258f);
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
static const ai_i8 _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = 4;
static const ai_float _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.007408987730741501f;
static const ai_float _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.021671660244464874f;
static const ai_float _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.011127038858830929f, 0.014397482387721539f, 0.018023347482085228f, 0.011675429530441761f, 0.008209778927266598f, 0.017443040385842323f, 0.022252433001995087f, 0.01443296018987894f, 0.012243160977959633f, 0.016915449872612953f, 0.010207971557974815f, 0.01669604890048504f, 0.015510544180870056f, 0.021984988823533058f, 0.018748976290225983f, 0.017308156937360764f, 0.006072273012250662f, 0.03179250285029411f, 0.011357332579791546f, 0.00867299735546112f, 0.01261897198855877f, 0.01050191093236208f, 0.012946735136210918f, 0.016764216125011444f, 0.01092812791466713f, 0.025768928229808807f, 0.011464409530162811f, 0.019642239436507225f, 0.021592218428850174f, 0.013101128861308098f, 0.009660419076681137f, 0.015020390972495079f, 0.008202431723475456f, 0.020613830536603928f, 0.02127736620604992f, 0.013764736242592335f, 0.009492688812315464f, 0.008870511315762997f, 0.008576009422540665f, 0.01625741273164749f, 0.02289375476539135f, 0.01430633757263422f, 0.009234139695763588f, 0.016336537897586823f, 0.009397093206644058f, 0.009366856887936592f, 0.007688518147915602f, 0.025181949138641357f);
static const ai_u16 _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_3_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 10;

static const ai_u16 _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_3_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_3_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = 4;
static const ai_i8 _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.021671660244464874f;
static const ai_float _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.012745935469865799f;
static const ai_float _stage3_stage3_3_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0022713420912623405f, 0.004007779527455568f, 0.003805442713201046f, 0.002097474643960595f, 0.004272419959306717f, 0.0022506783716380596f, 0.0020493827760219574f, 0.003212078008800745f, 0.002854267368093133f, 0.0016537668416276574f, 0.002365649677813053f, 0.003307747421786189f, 0.005233220290392637f, 0.005689995363354683f, 0.0019390946254134178f, 0.004094551783055067f, 0.0034529331605881453f, 0.001941622351296246f, 0.0024500496219843626f, 0.00564077403396368f, 0.0017680549062788486f, 0.005749164614826441f, 0.0024973207619041204f, 0.004856823477894068f, 0.0023766346275806427f, 0.002208869904279709f, 0.0035764090716838837f, 0.0022132708691060543f, 0.003328027669340372f, 0.0029543456621468067f, 0.003222756553441286f, 0.003095178632065654f, 0.005998409353196621f, 0.002768461825326085f, 0.006207697559148073f, 0.00379348685964942f, 0.003925492987036705f, 0.0022777547128498554f, 0.0031970140989869833f, 0.0022905790247023106f, 0.0047271354123950005f, 0.003196840872988105f, 0.004137637093663216f, 0.002993315225467086f, 0.006196944043040276f, 0.004034625831991434f, 0.0029726792126893997f, 0.003224690444767475f);
static const ai_layer_format_type _stage3_stage3_3_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_4_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_4_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.012745935469865799f;
static const ai_float _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.006642818450927734f;
static const ai_float _stage3_stage3_4_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0023361111525446177f, 0.001976440194994211f, 0.0027108474168926477f, 0.0032034325413405895f, 0.00542878545820713f, 0.00507334154099226f, 0.002921030856668949f, 0.0055154371075332165f, 0.003209012094885111f, 0.0011341939680278301f, 0.004762775264680386f, 0.007375961635261774f, 0.002838226966559887f, 0.0029213286470621824f, 0.004548718687146902f, 0.0043719494715332985f, 0.0006193033186718822f, 0.003270885208621621f, 0.0047116028144955635f, 0.00706707825884223f, 0.0046899039298295975f, 0.00225861556828022f, 0.0017663625767454505f, 0.0038741363678127527f, 0.001710812677629292f, 0.0032439895439893007f, 0.005005275830626488f, 0.002920361701399088f, 0.002002040157094598f, 0.003066534176468849f, 0.0015708752907812595f, 0.0032592499628663063f, 0.001134208170697093f, 0.002897223923355341f, 0.0017113096546381712f, 0.0036894080694764853f, 0.003012019209563732f, 0.002229085424914956f, 0.0014636173145845532f, 0.0033884800504893064f, 0.0013024667277932167f, 0.002367511158809066f, 0.0030041271820664406f, 0.0017123830039054155f, 0.0019215719075873494f, 0.004448707215487957f, 0.0016719705890864134f, 0.0031539928168058395f);
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
static const ai_float _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.006642818450927734f;
static const ai_float _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.02211925946176052f;
static const ai_float _stage3_stage3_4_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.00962905865162611f, 0.00957822147756815f, 0.014927564188838005f, 0.018951095640659332f, 0.013678431510925293f, 0.011324238032102585f, 0.005656655877828598f, 0.01570308208465576f, 0.009318525902926922f, 0.034697871655225754f, 0.020470483228564262f, 0.0279089268296957f, 0.010377191007137299f, 0.011763369664549828f, 0.027533477172255516f, 0.013815175741910934f, 0.05351562425494194f, 0.011253519915044308f, 0.012467903085052967f, 0.0048943678848445415f, 0.012153692543506622f, 0.012835115194320679f, 0.022198909893631935f, 0.011916306801140308f, 0.011749825440347195f, 0.007106352131813765f, 0.01461591012775898f, 0.008820360526442528f, 0.017303334549069405f, 0.011838347651064396f, 0.016915207728743553f, 0.017720544710755348f, 0.014825484715402126f, 0.00896996445953846f, 0.03205438330769539f, 0.013338961638510227f, 0.015021015889942646f, 0.014227094128727913f, 0.01592576690018177f, 0.004701524507254362f, 0.018881654366850853f, 0.00934048742055893f, 0.010517129674553871f, 0.027038082480430603f, 0.009646962396800518f, 0.014370616525411606f, 0.023711450397968292f, 0.03253765404224396f);
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
static const ai_float _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.02211925946176052f;
static const ai_float _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.009706230834126472f;
static const ai_float _stage3_stage3_4_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0027452711947262287f, 0.0022590509615838528f, 0.0015662951627746224f, 0.0015865517780184746f, 0.0020408066920936108f, 0.0020870885346084833f, 0.001580432872287929f, 0.002959353616461158f, 0.0020177403930574656f, 0.0027618147432804108f, 0.0017054877243936062f, 0.0016101835062727332f, 0.00426729628816247f, 0.003898630617186427f, 0.001691728364676237f, 0.0015556190628558397f, 0.0017083517741411924f, 0.00242427596822381f, 0.0021474589593708515f, 0.0024407117161899805f, 0.0019023983040824533f, 0.0025440119206905365f, 0.0017497059889137745f, 0.0027385528665035963f, 0.001545072183944285f, 0.0028332925867289305f, 0.005466387141495943f, 0.002230578800663352f, 0.0026953108608722687f, 0.0029862732626497746f, 0.0024789071176201105f, 0.00495872413739562f, 0.003990747034549713f, 0.002244824543595314f, 0.0013017546152696013f, 0.001643894356675446f, 0.005445293616503477f, 0.004133099690079689f, 0.0027739922516047955f, 0.0037247955333441496f, 0.004133913200348616f, 0.00607221107929945f, 0.003148685209453106f, 0.002801236929371953f, 0.002859138185158372f, 0.0033106945920735598f, 0.0060823517851531506f, 0.0029103970155119896f);
static const ai_layer_format_type _stage3_stage3_4_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_5_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_5_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.010089820250868797f;
static const ai_float _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.008038355968892574f;
static const ai_float _stage3_stage3_5_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0037453032564371824f, 0.004364586900919676f, 0.004790986888110638f, 0.0015156322624534369f, 0.0021288285497576f, 0.0017283124616369605f, 0.0012121523031964898f, 0.00357701419852674f, 0.0042680478654801846f, 0.0018668691627681255f, 0.002175265457481146f, 0.0035103438422083855f, 0.004004363436251879f, 0.00241741887293756f, 0.0020810822024941444f, 0.004007629118859768f, 0.003141248831525445f, 0.003996724262833595f, 0.002187575912103057f, 0.003863890888169408f, 0.003488893387839198f, 0.00298130139708519f, 0.003374852240085602f, 0.005252806469798088f, 0.0030483128502964973f, 0.0019100417848676443f, 0.0033984535839408636f, 0.001931795384734869f, 0.0044969781301915646f, 0.002315942198038101f, 0.0045732781291007996f, 0.0033477414399385452f, 0.0017608258640393615f, 0.0030700555071234703f, 0.004735645838081837f, 0.0033227382227778435f, 0.0017724017379805446f, 0.0028811709489673376f, 0.005523113999515772f, 0.0031879409216344357f, 0.0014563255244866014f, 0.0019111842848360538f, 0.0043599302880465984f, 0.002343716798350215f, 0.0020935237407684326f, 0.0021906113252043724f, 0.0016775332624092698f, 0.002728566061705351f);
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
static const ai_i8 _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = -12;
static const ai_float _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.008038355968892574f;
static const ai_float _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.024148419499397278f;
static const ai_float _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.014514089561998844f, 0.012448632158339024f, 0.014501674100756645f, 0.04417184740304947f, 0.038812581449747086f, 0.014738205820322037f, 0.018797244876623154f, 0.007027371786534786f, 0.011709497310221195f, 0.013211522251367569f, 0.006662008818238974f, 0.013052089139819145f, 0.025896158069372177f, 0.012460421770811081f, 0.03163689747452736f, 0.008120626211166382f, 0.014913728460669518f, 0.011840106919407845f, 0.013450218364596367f, 0.009515272453427315f, 0.013585975393652916f, 0.014888297766447067f, 0.009259017184376717f, 0.014959050342440605f, 0.009305618703365326f, 0.01263123657554388f, 0.032146845012903214f, 0.012192811816930771f, 0.024059534072875977f, 0.007805402856320143f, 0.006131066009402275f, 0.010822772979736328f, 0.01724558137357235f, 0.00807978119701147f, 0.011545957066118717f, 0.0044022006914019585f, 0.013222239911556244f, 0.012933243066072464f, 0.008200738579034805f, 0.0056941392831504345f, 0.021910693496465683f, 0.007715731393545866f, 0.012932772748172283f, 0.009136914275586605f, 0.013753881677985191f, 0.02119578793644905f, 0.030736355111002922f, 0.007511047646403313f);
static const ai_u16 _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_5_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 10;

static const ai_u16 _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_5_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_5_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = -12;
static const ai_i8 _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.024148419499397278f;
static const ai_float _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.011862967163324356f;
static const ai_float _stage3_stage3_5_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.001894698478281498f, 0.0022175658959895372f, 0.0019521932117640972f, 0.003946980461478233f, 0.002481113886460662f, 0.0031567122787237167f, 0.002627685898914933f, 0.0030238365288823843f, 0.002877524821087718f, 0.0017093903152272105f, 0.00336478091776371f, 0.0017384327948093414f, 0.00418493989855051f, 0.0025462056510150433f, 0.0018672127043828368f, 0.0015313882613554597f, 0.0013509533600881696f, 0.0031972243450582027f, 0.002003245986998081f, 0.0016966398106887937f, 0.0019190966850146651f, 0.002048497088253498f, 0.0014006189303472638f, 0.0021709424909204245f, 0.005614661145955324f, 0.0033649876713752747f, 0.0033702878281474113f, 0.002000582404434681f, 0.004290663171559572f, 0.002221315400674939f, 0.003759092651307583f, 0.0034310119226574898f, 0.0036375601775944233f, 0.002101533580571413f, 0.004066003020852804f, 0.001381235895678401f, 0.0016948613338172436f, 0.0023106876760721207f, 0.0016420993488281965f, 0.002046993700787425f, 0.003747432492673397f, 0.0041351038962602615f, 0.0018016461981460452f, 0.0019477930618450046f, 0.002189618768170476f, 0.002806724514812231f, 0.002187031088396907f, 0.0015381944831460714f);
static const ai_layer_format_type _stage3_stage3_5_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_6_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_6_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.011862967163324356f;
static const ai_float _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.009014229290187359f;
static const ai_float _stage3_stage3_6_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.003235125681385398f, 0.001543617807328701f, 0.003117852145805955f, 0.003358371090143919f, 0.001954687060788274f, 0.003583010518923402f, 0.003850079607218504f, 0.00740424357354641f, 0.0044454410672187805f, 0.0030486867763102055f, 0.0026808367110788822f, 0.007179288659244776f, 0.003271919209510088f, 0.0038646706379950047f, 0.0034970378037542105f, 0.004247766919434071f, 0.003077423432841897f, 0.004274648614227772f, 0.003368114586919546f, 0.0026623739395290613f, 0.0016360332956537604f, 0.0023102385457605124f, 0.004427968990057707f, 0.005696184001863003f, 0.003338045906275511f, 0.0038605364970862865f, 0.005307042971253395f, 0.004797301720827818f, 0.003921315539628267f, 0.008611961267888546f, 0.0030659546609967947f, 0.008255675435066223f, 0.008618313819169998f, 0.002391495043411851f, 0.0032068437431007624f, 0.004700224846601486f, 0.0032344090286642313f, 0.005601111799478531f, 0.004423788748681545f, 0.0066319601610302925f, 0.008805026300251484f, 0.00784688163548708f, 0.004054485820233822f, 0.00278623984195292f, 0.006016461178660393f, 0.004121601115912199f, 0.00381054007448256f, 0.0031788423657417297f);
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
static const ai_i8 _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = -2;
static const ai_float _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.009014229290187359f;
static const ai_float _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.030543094500899315f;
static const ai_float _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.022651465609669685f, 0.01069969218224287f, 0.012887132354080677f, 0.018404170870780945f, 0.02856493555009365f, 0.005426098592579365f, 0.0059891920536756516f, 0.00960192084312439f, 0.004701747093349695f, 0.01350000873208046f, 0.035668883472681046f, 0.017069201916456223f, 0.007490071468055248f, 0.00814714003354311f, 0.010953618213534355f, 0.011487255804240704f, 0.004369412083178759f, 0.00798689667135477f, 0.006029751617461443f, 0.012687106616795063f, 0.039373114705085754f, 0.007267731707543135f, 0.013190492987632751f, 0.014177829958498478f, 0.01845528557896614f, 0.010638323612511158f, 0.010295910760760307f, 0.008932837285101414f, 0.003200485138222575f, 0.026061536744236946f, 0.013341447338461876f, 0.01792333833873272f, 0.00886519718915224f, 0.018218962475657463f, 0.009023988619446754f, 0.0060877990908920765f, 0.007001417689025402f, 0.006024432368576527f, 0.00453775143250823f, 0.005938040558248758f, 0.012133586220443249f, 0.021829722449183464f, 0.016423679888248444f, 0.007352974731475115f, 0.017075400799512863f, 0.007427474949508905f, 0.011281783692538738f, 0.03933870792388916f);
static const ai_u16 _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_6_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 10;

static const ai_u16 _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_6_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_6_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = -2;
static const ai_i8 _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.030543094500899315f;
static const ai_float _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.011234721168875694f;
static const ai_float _stage3_stage3_6_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0021007258910685778f, 0.0018192064017057419f, 0.0025048069655895233f, 0.0019432581029832363f, 0.0037712398916482925f, 0.001776835648342967f, 0.0025472769048064947f, 0.0033683860674500465f, 0.0023183373268693686f, 0.002324627013877034f, 0.0017290272517129779f, 0.0034550337586551905f, 0.0016919077606871724f, 0.00271863816305995f, 0.0019155808258801699f, 0.001721325796097517f, 0.002614417811855674f, 0.00424759229645133f, 0.0018397553358227015f, 0.0036198748275637627f, 0.0018290466396138072f, 0.002779955742880702f, 0.0014801694778725505f, 0.0018989015370607376f, 0.0019344798056408763f, 0.002459614770486951f, 0.002074484946206212f, 0.0035394798032939434f, 0.0018460379214957356f, 0.0036006439477205276f, 0.004883863031864166f, 0.0014665256021544337f, 0.003961131442338228f, 0.003059751819819212f, 0.00532991997897625f, 0.0026805968955159187f, 0.00208975444547832f, 0.005317543633282185f, 0.0016207338776439428f, 0.002857983112335205f, 0.004486192483454943f, 0.004609720315784216f, 0.002008535899221897f, 0.002225287491455674f, 0.0020257409196347f, 0.0016637080116197467f, 0.002571413991972804f, 0.0026795624289661646f);
static const ai_layer_format_type _stage3_stage3_6_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage3_stage3_7_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage3_stage3_7_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 48;
static const ai_i8 _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.011862967163324356f;
static const ai_float _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.009186920709908009f;
static const ai_float _stage3_stage3_7_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.003717270912602544f, 0.002503851428627968f, 0.0024785520508885384f, 0.001974399434402585f, 0.002292188350111246f, 0.0026363786309957504f, 0.0029229847714304924f, 0.004160844720900059f, 0.004002881236374378f, 0.005645254626870155f, 0.0013569594593718648f, 0.0032374500297009945f, 0.0036330006550997496f, 0.0014066720614209771f, 0.002524007810279727f, 0.004218969959765673f, 0.0023072485346347094f, 0.0013947624247521162f, 0.005151003133505583f, 0.0045938920229673386f, 0.0029315908905118704f, 0.004592412617057562f, 0.00673029525205493f, 0.004920565523207188f, 0.0037632989697158337f, 0.0018499662401154637f, 0.005936455447226763f, 0.0023980073165148497f, 0.0019011644180864096f, 0.003912494517862797f, 0.004881381057202816f, 0.0031207376159727573f, 0.0025183989200741053f, 0.003421325469389558f, 0.004696291405707598f, 0.006382271181792021f, 0.004751910921186209f, 0.004296853207051754f, 0.0038849059492349625f, 0.0016004058998078108f, 0.001255529117770493f, 0.00511350529268384f, 0.004557977896183729f, 0.003851458663120866f, 0.0034512614365667105f, 0.00443256413564086f, 0.0028156619518995285f, 0.004168924409896135f);
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
static const ai_float _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.009186920709908009f;
static const ai_float _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.027956116944551468f;
static const ai_float _stage3_stage3_7_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.010252619162201881f, 0.01638396829366684f, 0.011155182495713234f, 0.011886811815202236f, 0.01070478092879057f, 0.02400386705994606f, 0.004867938347160816f, 0.005251860711723566f, 0.011274854652583599f, 0.015440788120031357f, 0.01335088536143303f, 0.005028188694268465f, 0.008891982957720757f, 0.031129170209169388f, 0.004953295923769474f, 0.02015105076134205f, 0.0123910466209054f, 0.015919648110866547f, 0.024385716766119003f, 0.01625816524028778f, 0.006739242002367973f, 0.007806917652487755f, 0.01681782491505146f, 0.02869456820189953f, 0.006334329955279827f, 0.014481369405984879f, 0.006869472563266754f, 0.026432476937770844f, 0.03363287076354027f, 0.005834555719047785f, 0.021902404725551605f, 0.00472439918667078f, 0.03111359290778637f, 0.0265620369464159f, 0.01724490150809288f, 0.005922823213040829f, 0.0057062022387981415f, 0.005596178583800793f, 0.031062232330441475f, 0.029007360339164734f, 0.046520791947841644f, 0.018054073676466942f, 0.006227029021829367f, 0.012336045503616333f, 0.011864651925861835f, 0.010665413923561573f, 0.005248227156698704f, 0.0076443287543952465f);
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
static const ai_float _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.027956116944551468f;
static const ai_float _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.012842380441725254f;
static const ai_float _stage3_stage3_7_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0025798603892326355f, 0.0028280294500291348f, 0.002648485591635108f, 0.0023972841445356607f, 0.0018661778885871172f, 0.005258652381598949f, 0.0023526677396148443f, 0.003974062390625477f, 0.0025234378408640623f, 0.0026104918215423822f, 0.0016553087625652552f, 0.0029176806565374136f, 0.002141076372936368f, 0.002507366007193923f, 0.002230806741863489f, 0.00224628415890038f, 0.002153429202735424f, 0.002282404573634267f, 0.00213452847674489f, 0.0026716780848801136f, 0.002283043460920453f, 0.002835560357198119f, 0.0026319038588553667f, 0.004226501099765301f, 0.00206771120429039f, 0.004271145444363356f, 0.002375018550083041f, 0.0032548485323786736f, 0.0015375938965007663f, 0.0057542333379387856f, 0.003652099519968033f, 0.005217605270445347f, 0.004177511669695377f, 0.003168721217662096f, 0.0035030331928282976f, 0.003429699456319213f, 0.002250783145427704f, 0.0029623114969581366f, 0.002670522313565016f, 0.002174154855310917f, 0.00525773735716939f, 0.003390185534954071f, 0.0022117963526397943f, 0.002736907685175538f, 0.0020320508629083633f, 0.003770285053178668f, 0.003205705201253295f, 0.004320207517594099f);
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
static const ai_i8 _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_out_0_fmt_zero_const_s8 = -21;
static const ai_float _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.012842380441725254f;
static const ai_float _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.026346519589424133f;
static const ai_float _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0041515720076859f, 0.011625556275248528f, 0.004242272581905127f, 0.0050124237313866615f, 0.005153624340891838f, 0.005182270891964436f, 0.004076309036463499f, 0.004636440426111221f, 0.0031935193110257387f, 0.004056965932250023f, 0.005518956575542688f, 0.00361691159196198f, 0.004704522900283337f, 0.0044454666785895824f, 0.005995327606797218f, 0.005105886608362198f, 0.006626848131418228f, 0.004746187478303909f, 0.004627073183655739f, 0.006119062192738056f, 0.003557079005986452f, 0.005684031173586845f, 0.004162244498729706f, 0.004674282856285572f, 0.00434954883530736f, 0.005760903004556894f, 0.005330987740308046f, 0.0043876259587705135f, 0.005655888933688402f, 0.003132657380774617f, 0.004299425520002842f, 0.004393983632326126f, 0.0033502751030027866f, 0.00460644718259573f, 0.004591998178511858f, 0.005029555410146713f, 0.005031316541135311f, 0.0048633418045938015f, 0.004604358226060867f, 0.0036162117030471563f, 0.004082195460796356f, 0.004308258183300495f, 0.004835269879549742f, 0.004650644958019257f, 0.0033290039282292128f, 0.002845707582309842f, 0.00492852134630084f, 0.005083288997411728f, 0.003749778028577566f, 0.004844721872359514f, 0.004701684694737196f, 0.006192089058458805f, 0.003620345611125231f, 0.004044444300234318f, 0.005347780883312225f, 0.0029478142969310284f, 0.0038084436673671007f, 0.005360721610486507f, 0.005068356171250343f, 0.004420096054673195f, 0.004480583593249321f, 0.0032961375545710325f, 0.0058603850193321705f, 0.003950612153857946f, 0.003138432279229164f, 0.0069298516027629375f, 0.0036979983560740948f, 0.0032816652674227953f, 0.00432039937004447f, 0.004979120567440987f, 0.003543937811627984f, 0.004119961988180876f, 0.003709618467837572f, 0.005509100388735533f, 0.004346854984760284f, 0.005455526057630777f, 0.004584094043821096f, 0.004188662860542536f, 0.004898089915513992f, 0.006213311105966568f, 0.0037954996805638075f, 0.0029811819549649954f, 0.005341690499335527f, 0.004758491646498442f, 0.005154966842383146f, 0.004173769149929285f, 0.005003842990845442f, 0.0032173350919038057f, 0.004137305077165365f, 0.004671579692512751f, 0.0048675015568733215f, 0.0024536512792110443f, 0.004124784842133522f, 0.0072805495001375675f, 0.005936523899435997f, 0.0059152161702513695f);
static const ai_u16 _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_out_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_0_branch1_branch1_0_Conv_output_0_t_out_0_shape_h_const_u16 = 5;

static const ai_u16 _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_0_branch1_branch1_4_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_0_branch1_branch1_4_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_in_0_fmt_zero_const_s8 = -21;
static const ai_i8 _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.026346519589424133f;
static const ai_float _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.009751124307513237f;
static const ai_float _stage4_stage4_0_branch1_branch1_4_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0032007619738578796f, 0.001855233102105558f, 0.0018230746500194073f, 0.002193354768678546f, 0.0019145231926813722f, 0.0020762181375175714f, 0.001624711905606091f, 0.0019581022206693888f, 0.002509073819965124f, 0.0017952087800949812f, 0.002363036386668682f, 0.0018765362910926342f, 0.0010478964541107416f, 0.00142229616176337f, 0.0013202836271375418f, 0.002040719147771597f, 0.0016476749442517757f, 0.0016213345807045698f, 0.0019733572844415903f, 0.001315948786213994f, 0.0011133084772154689f, 0.0013363487087190151f, 0.0021206610836088657f, 0.0011976389214396477f, 0.001682013738900423f, 0.001755262492224574f, 0.0014822713565081358f, 0.0018501688027754426f, 0.004298073705285788f, 0.002059393562376499f, 0.001640313072130084f, 0.001719575491733849f, 0.0032952516339719296f, 0.002668436150997877f, 0.0011054044589400291f, 0.001636640401557088f, 0.0011453654151409864f, 0.0015698578208684921f, 0.0026668920181691647f, 0.0017834773752838373f, 0.0016685567097738385f, 0.0024519034195691347f, 0.0017383344238623977f, 0.0015343425329774618f, 0.0026101921685039997f, 0.0015092006651684642f, 0.001575019909068942f, 0.0019179816590622067f, 0.0015179065521806479f, 0.002777834888547659f, 0.0017919527599588037f, 0.0016422406770288944f, 0.0017401969525963068f, 0.0013716614339500666f, 0.0020201988518238068f, 0.0022403481416404247f, 0.004158782307058573f, 0.0017823027446866035f, 0.0016874480061233044f, 0.0029688274953514338f, 0.002532342215999961f, 0.002208918333053589f, 0.002667088294401765f, 0.0020246547646820545f, 0.0023949265014380217f, 0.0014412840828299522f, 0.001286602346226573f, 0.0026193163357675076f, 0.0019322528969496489f, 0.0016522407531738281f, 0.0013624030398204923f, 0.0014560571871697903f, 0.0022845056373625994f, 0.0018379689427092671f, 0.002658515702933073f, 0.0016763926250860095f, 0.0024864510633051395f, 0.0018004290759563446f, 0.0018406520830467343f, 0.0023755410220474005f, 0.0015506830532103777f, 0.002360405633226037f, 0.0028728751931339502f, 0.0014791125431656837f, 0.002271045232191682f, 0.0015174339059740305f, 0.0014484829735010862f, 0.0024678607005625963f, 0.0013720119604840875f, 0.002210847567766905f, 0.0024122868198901415f, 0.003300163894891739f, 0.0015264310641214252f, 0.002871953183785081f, 0.001734020421281457f, 0.0019488215912133455f);
static const ai_layer_format_type _stage4_stage4_0_branch1_branch1_4_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 10;
static const ai_u16 _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 10;
static const ai_u16 _stage4_stage4_0_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_0_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.012842380441725254f;
static const ai_float _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.00948443915694952f;
static const ai_float _stage4_stage4_0_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0026899126823991537f, 0.0026892407331615686f, 0.004032354336231947f, 0.0029681019950658083f, 0.0020719473250210285f, 0.0021363929845392704f, 0.0028398758731782436f, 0.0030695253517478704f, 0.0022287294268608093f, 0.002813157858327031f, 0.0021908418275415897f, 0.0029042386449873447f, 0.005017795134335756f, 0.0028197169303894043f, 0.00285137421451509f, 0.0031468088272958994f, 0.0024080488365143538f, 0.002122021745890379f, 0.002468910999596119f, 0.002334357239305973f, 0.002286734525114298f, 0.0026891762390732765f, 0.0024764020927250385f, 0.0025928483810275793f, 0.0024945735931396484f, 0.0050877416506409645f, 0.0024499285500496626f, 0.0015602612402290106f, 0.0055205244570970535f, 0.001982877729460597f, 0.0017318350728601217f, 0.002958761528134346f, 0.0028207784052938223f, 0.002279304200783372f, 0.002246288815513253f, 0.0032886292319744825f, 0.0015369071625173092f, 0.0037846716586500406f, 0.0019723789300769567f, 0.0040362002328038216f, 0.0032276473939418793f, 0.0023848519194871187f, 0.004767040256410837f, 0.005911172367632389f, 0.003924986347556114f, 0.0032000497449189425f, 0.002387931337580085f, 0.003198788734152913f, 7.812268449924886e-07f, 0.0024002885911613703f, 0.0022608572617173195f, 0.002268356503918767f, 0.004122867248952389f, 0.0017456173663958907f, 0.0030669986736029387f, 0.002184594050049782f, 0.0024109368678182364f, 0.0024355193600058556f, 0.0029437700286507607f, 0.00312871765345335f, 0.002336406847462058f, 0.0024951219093054533f, 0.0023734380956739187f, 0.0038528256118297577f, 0.0022014358546584845f, 0.002556831808760762f, 0.002434070687741041f, 0.003615959081798792f, 0.003609214210882783f, 0.0024999049492180347f, 0.0026699784211814404f, 0.003984291572123766f, 0.0028876038268208504f, 0.002812467748299241f, 0.003478187369182706f, 0.002904601627960801f, 0.0019499178742989898f, 0.0029479959048330784f, 0.004514462314546108f, 0.0043313042260706425f, 0.002327341353520751f, 0.009367512539029121f, 0.0038351649418473244f, 0.0032447248231619596f, 0.0029339727479964495f, 0.0025395890697836876f, 0.003199891187250614f, 0.002495297696441412f, 0.004503497388213873f, 0.004486072808504105f, 0.002129514701664448f, 0.003887185826897621f, 0.003744241315871477f, 0.0025364691391587257f, 0.002511252649128437f, 0.0019364788895472884f);
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
static const ai_i8 _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = -17;
static const ai_float _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.00948443915694952f;
static const ai_float _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.034674566239118576f;
static const ai_float _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.008384281769394875f, 0.008021501824259758f, 0.008226851932704449f, 0.018840765580534935f, 0.01017534825950861f, 0.008534317836165428f, 0.011120166629552841f, 0.007588960696011782f, 0.008412026800215244f, 0.007808199618011713f, 0.010733773931860924f, 0.007396945729851723f, 0.010211657732725143f, 0.01058028731495142f, 0.00895837415009737f, 0.008174465969204903f, 0.010242849588394165f, 0.01027089636772871f, 0.006855375599116087f, 0.007870142348110676f, 0.008397779427468777f, 0.011492125689983368f, 0.008298634551465511f, 0.008192768320441246f, 0.008304127492010593f, 0.007704947143793106f, 0.009865077212452888f, 0.009215625002980232f, 0.006397102493792772f, 0.009018092416226864f, 0.008517048321664333f, 0.007468997500836849f, 0.012250969186425209f, 0.0076249269768595695f, 0.008931545540690422f, 0.009600947611033916f, 0.009392311796545982f, 0.008572284132242203f, 0.01069449819624424f, 0.011155074462294579f, 0.007717498112469912f, 0.007797033991664648f, 0.008495542220771313f, 0.008256959728896618f, 0.009028199128806591f, 0.007630408275872469f, 0.008467646315693855f, 0.009949020110070705f, 0.01086219772696495f, 0.006431560963392258f, 0.010011853650212288f, 0.009607572108507156f, 0.008433874696493149f, 0.008788402192294598f, 0.015367841348052025f, 0.009504657238721848f, 0.009278721176087856f, 0.008377972058951855f, 0.010016726329922676f, 0.009666694328188896f, 0.008825182914733887f, 0.010194065049290657f, 0.008351948112249374f, 0.008803796023130417f, 0.0080218855291605f, 0.007891742512583733f, 0.007402701769024134f, 0.007562750019133091f, 0.011259441263973713f, 0.008042351342737675f, 0.00867836270481348f, 0.008594123646616936f, 0.009866653941571712f, 0.007766554597765207f, 0.007101879455149174f, 0.013580183498561382f, 0.011971281841397285f, 0.009374847635626793f, 0.008573263883590698f, 0.012935122475028038f, 0.01029130071401596f, 0.008369818329811096f, 0.012789783999323845f, 0.008443105965852737f, 0.007186715956777334f, 0.008713364601135254f, 0.00796047318726778f, 0.008005298674106598f, 0.009736557491123676f, 0.006360951811075211f, 0.010300704278051853f, 0.007110456936061382f, 0.009085913188755512f, 0.011797488667070866f, 0.00828875694423914f, 0.010892167687416077f);
static const ai_u16 _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_0_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 5;

static const ai_u16 _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_0_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_0_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = -17;
static const ai_i8 _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.034674566239118576f;
static const ai_float _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.015814999118447304f;
static const ai_float _stage4_stage4_0_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0018493825336918235f, 0.0024405275471508503f, 0.002471231622621417f, 0.0016388726653531194f, 0.0024620613548904657f, 0.0020345007069408894f, 0.0029646619223058224f, 0.0019467751262709498f, 0.0014420589432120323f, 0.0017828340642154217f, 0.0022217328660190105f, 0.001923628500662744f, 0.0012027916964143515f, 0.002075042575597763f, 0.0013690791092813015f, 0.0013613750925287604f, 0.0023910740856081247f, 0.0014685597270727158f, 0.001537357340566814f, 0.0013901828788220882f, 0.0013155800988897681f, 0.001699334941804409f, 0.0018285926198586822f, 0.0015035030664876103f, 0.0032290746457874775f, 0.0013091256842017174f, 0.0014430023729801178f, 0.0019258905667811632f, 0.005709178280085325f, 0.0021756256464868784f, 0.0015590927796438336f, 0.001743223867379129f, 0.0016890368424355984f, 0.0015694318572059274f, 0.001562448451295495f, 0.0012963330373167992f, 0.0016900873742997646f, 0.0024213262367993593f, 0.001808790024369955f, 0.0026851757429540157f, 0.0026163742877542973f, 0.0017697603907436132f, 0.001856801682151854f, 0.0010314832907170057f, 0.0020655235275626183f, 0.0014813272282481194f, 0.0012125722132623196f, 0.0016402150504291058f, 0.0015887559857219458f, 0.001567286322824657f, 0.003947352524846792f, 0.0022324305027723312f, 0.0016708322800695896f, 0.002857368439435959f, 0.0020248170476406813f, 0.0029280264861881733f, 0.003954673185944557f, 0.002890420611947775f, 0.0026178115513175726f, 0.0020841266959905624f, 0.0018676936160773039f, 0.003123944392427802f, 0.0013158106012269855f, 0.0026596474926918745f, 0.001326579600572586f, 0.001807350548915565f, 0.0014295183354988694f, 0.0015493533574044704f, 0.0017874399200081825f, 0.0025333696976304054f, 0.002029489492997527f, 0.0018893436063081026f, 0.001654139836318791f, 0.0011757100000977516f, 0.0019574854522943497f, 0.004093436989933252f, 0.0036420123651623726f, 0.0018571781693026423f, 0.0014485304709523916f, 0.0014733219286426902f, 0.0021520291920751333f, 0.0017835133476182818f, 0.0014456219505518675f, 0.001667873002588749f, 0.00298347813077271f, 0.003661582013592124f, 0.0012125729117542505f, 0.0016332851955667138f, 0.002850470133125782f, 0.0022712124045938253f, 0.00396323949098587f, 0.0021803323179483414f, 0.00413688737899065f, 0.0022780857980251312f, 0.00323398201726377f, 0.0013935010647401214f);
static const ai_layer_format_type _stage4_stage4_0_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_1_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_1_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.015814999118447304f;
static const ai_float _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.00840744748711586f;
static const ai_float _stage4_stage4_1_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002662245649844408f, 0.0019510445417836308f, 0.0025010101962834597f, 0.003501330502331257f, 0.0036715359892696142f, 0.001427029725164175f, 0.0018255389295518398f, 0.0021874699741601944f, 0.002716826042160392f, 0.0022913359571248293f, 0.0009741443791426718f, 0.005047401413321495f, 0.001778074074536562f, 0.0024236596655100584f, 0.0010683552827686071f, 0.0031043600756675005f, 0.0014246212085708976f, 0.0020883302204310894f, 0.002007244387641549f, 0.0017973915673792362f, 0.002374883508309722f, 0.003431241260841489f, 0.0015893379459157586f, 0.001411666045896709f, 0.0018867265898734331f, 0.00158361264038831f, 0.0014342691283673048f, 0.0032619459088891745f, 0.0023415014147758484f, 0.002599674742668867f, 0.002216234104707837f, 0.00270435051061213f, 0.0019796518608927727f, 0.002121677389368415f, 0.0019862086046487093f, 0.0023043439723551273f, 0.0012785840081050992f, 0.0028528794646263123f, 0.0014526137383654714f, 0.0029646591283380985f, 0.0012422732543200254f, 0.002012926386669278f, 0.0026781356427818537f, 0.0018791863694787025f, 0.002710134955123067f, 0.0016880154144018888f, 0.004462939687073231f, 0.0025458435993641615f, 0.00259805703535676f, 0.0019174502231180668f, 0.0019926719833165407f, 0.0024421196430921555f, 0.0021277759224176407f, 0.002631956245750189f, 0.0021472745575010777f, 0.0027856030501425266f, 0.0016729094786569476f, 0.0011431846069172025f, 0.004379902500659227f, 0.003031673841178417f, 0.0017053380142897367f, 0.004536065272986889f, 0.0011412038002163172f, 0.0021474026143550873f, 0.002064234809949994f, 0.002029776806011796f, 0.0017081741243600845f, 0.0025930011179298162f, 0.000862034794408828f, 0.00043475496931932867f, 0.0027469752822071314f, 0.0017064969288185239f, 0.0030061083380132914f, 0.0030347281135618687f, 0.0017000279622152448f, 0.0036533742677420378f, 0.0021416256204247475f, 0.001477305544540286f, 0.0021727641578763723f, 0.0015554081182926893f, 0.0028699147514998913f, 0.0017845026450231671f, 0.001747549744322896f, 0.0011992297368124127f, 0.0024678222835063934f, 0.001209995592944324f, 0.0021920413710176945f, 0.0009644223027862608f, 0.0022501100320369005f, 0.0020304422359913588f, 0.0030658512841910124f, 0.002594016958028078f, 0.0029913890175521374f, 0.002380367601290345f, 0.001782537205144763f, 0.0021063194144517183f);
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
static const ai_i8 _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = -5;
static const ai_float _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.00840744748711586f;
static const ai_float _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.025366224348545074f;
static const ai_float _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.01021511759608984f, 0.025022802874445915f, 0.007421569898724556f, 0.013215982355177402f, 0.007476152386516333f, 0.021325143054127693f, 0.010802554897964f, 0.01156990323215723f, 0.00802083034068346f, 0.01313146110624075f, 0.009926363825798035f, 0.0037195102777332067f, 0.010827184654772282f, 0.009148257784545422f, 0.012736505828797817f, 0.01355038397014141f, 0.016346151009202003f, 0.008615374565124512f, 0.007230374030768871f, 0.013403229415416718f, 0.011429275386035442f, 0.006757086608558893f, 0.013206655159592628f, 0.015594149939715862f, 0.007112148683518171f, 0.006167795974761248f, 0.021724091842770576f, 0.00395315932109952f, 0.007334715221077204f, 0.00918291974812746f, 0.007845173589885235f, 0.0113602289929986f, 0.00758992787450552f, 0.010121932253241539f, 0.01097833551466465f, 0.008564217016100883f, 0.008674786426126957f, 0.006527184043079615f, 0.011814032681286335f, 0.00904756598174572f, 0.016223493963479996f, 0.02447495609521866f, 0.00722912373021245f, 0.004847649019211531f, 0.025870531797409058f, 0.009422862902283669f, 0.00962541252374649f, 0.010522717610001564f, 0.008524127304553986f, 0.006673546973615885f, 0.004361690022051334f, 0.011320478282868862f, 0.005566067062318325f, 0.006758957169950008f, 0.01264912635087967f, 0.02376926690340042f, 0.0037590123247355223f, 0.010547254234552383f, 0.004817142616957426f, 0.010446885600686073f, 0.012420114129781723f, 0.00924566201865673f, 0.025044726207852364f, 0.00791096594184637f, 0.008969428949058056f, 0.010677417740225792f, 0.012654042802751064f, 0.005801656749099493f, 0.03701596334576607f, 0.06322764605283737f, 0.010240605100989342f, 0.010913523845374584f, 0.01238998118788004f, 0.00504958676174283f, 0.01534249633550644f, 0.007370728068053722f, 0.005936870817095041f, 0.013979802839457989f, 0.0076438793912529945f, 0.012757325544953346f, 0.0040390812791883945f, 0.0038158425595611334f, 0.007573816459625959f, 0.025683555752038956f, 0.009760972112417221f, 0.030468298122286797f, 0.008054404519498348f, 0.03128858283162117f, 0.010670484974980354f, 0.0039533283561468124f, 0.010251490399241447f, 0.00731711694970727f, 0.009178373031318188f, 0.006455416325479746f, 0.019267532974481583f, 0.011515926569700241f);
static const ai_u16 _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_1_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 5;

static const ai_u16 _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_1_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_1_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = -5;
static const ai_i8 _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.025366224348545074f;
static const ai_float _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.013661094941198826f;
static const ai_float _stage4_stage4_1_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002049261936917901f, 0.0014492111513391137f, 0.002410674700513482f, 0.0025364847388118505f, 0.0020120334811508656f, 0.002334892051294446f, 0.0018494818359613419f, 0.0018083601025864482f, 0.00289071979932487f, 0.002567080780863762f, 0.003738679923117161f, 0.0027025463059544563f, 0.0024467813782393932f, 0.001794969430193305f, 0.0019320674473419785f, 0.0012909176293760538f, 0.002103133127093315f, 0.0025936069432646036f, 0.0020512305200099945f, 0.0017038570949807763f, 0.001670250901952386f, 0.0032099601812660694f, 0.0021404693834483624f, 0.0019202623516321182f, 0.0018499573925510049f, 0.0016644796123728156f, 0.001706605195067823f, 0.0036374437622725964f, 0.0015031656948849559f, 0.002133882837370038f, 0.002143619582056999f, 0.0013975204201415181f, 0.001984630711376667f, 0.0018436976242810488f, 0.0017438963986933231f, 0.0020801941864192486f, 0.002138714771717787f, 0.0013970991130918264f, 0.013102183118462563f, 0.0015228675911203027f, 0.0010990472510457039f, 0.0014100042171776295f, 0.003042466240003705f, 0.0017392855370417237f, 0.0019710019696503878f, 0.0015864439774304628f, 0.0026239342987537384f, 0.0016379919834434986f, 0.005758807063102722f, 0.002822654787451029f, 0.005525156389921904f, 0.0035499355290085077f, 0.0015987379010766745f, 0.0035605758894234896f, 0.005604578647762537f, 0.003120570909231901f, 0.0027766814455389977f, 0.00271660671569407f, 0.004190441220998764f, 0.0029023531824350357f, 0.002550177276134491f, 0.002135691000148654f, 0.0030041898135095835f, 0.002818851266056299f, 0.0019851606339216232f, 0.0022021024487912655f, 0.002691821428015828f, 0.0023524027783423662f, 0.002474462380632758f, 0.0017810530262067914f, 0.00228299293667078f, 0.0035331465769559145f, 0.00315539026632905f, 0.0021412346977740526f, 0.003055725246667862f, 0.001999467611312866f, 0.0018143680645152926f, 0.003923967480659485f, 0.0022348319180309772f, 0.003169526346027851f, 0.0026788937393575907f, 0.003030066378414631f, 0.003764477791264653f, 0.003242326667532325f, 0.00465488201007247f, 0.003756580175831914f, 0.0019455491565167904f, 0.004257577937096357f, 0.0023805040400475264f, 0.0020594224333763123f, 0.002056663855910301f, 0.0028245493303984404f, 0.002401414792984724f, 0.004227008670568466f, 0.0029524785932153463f, 0.002212950261309743f);
static const ai_layer_format_type _stage4_stage4_1_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_2_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_2_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.013661094941198826f;
static const ai_float _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.008393600583076477f;
static const ai_float _stage4_stage4_2_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0020810740534216166f, 0.002002137014642358f, 0.0027040487620979548f, 0.0025618223007768393f, 0.003002410987392068f, 0.003052183659747243f, 0.001991775119677186f, 0.0023136483505368233f, 0.001664093229919672f, 0.0030994305852800608f, 0.0018485513282939792f, 0.003272973233833909f, 0.002001343062147498f, 0.004750968888401985f, 0.002821004018187523f, 0.0019668275490403175f, 0.0022978871129453182f, 0.0014036198845133185f, 0.0019702380523085594f, 0.0017358253244310617f, 0.001779607031494379f, 0.0023224179167300463f, 0.0025633322075009346f, 0.00209798663854599f, 0.0020935609936714172f, 0.003591235727071762f, 0.0036527044139802456f, 0.0033610514365136623f, 0.005391887854784727f, 0.004899861756712198f, 0.00133770820684731f, 0.0013151152525097132f, 0.003187711350619793f, 0.001723598106764257f, 0.001554366433992982f, 0.0023027879651635885f, 0.0036942053120583296f, 0.0016265853773802519f, 0.003196766832843423f, 0.001701632747426629f, 0.0027948226779699326f, 0.0024475508835166693f, 0.0028489616233855486f, 0.0023001888766884804f, 0.0024680423084646463f, 0.0025446172803640366f, 0.001840408775024116f, 0.0020652287639677525f, 0.0019997511990368366f, 0.002552112564444542f, 0.0025856872089207172f, 0.0020813297014683485f, 0.004077900666743517f, 0.002762392396107316f, 0.001924067735671997f, 0.0027150579262524843f, 0.0021886739414185286f, 0.002094085095450282f, 0.002069460228085518f, 0.00233124359510839f, 0.002758732996881008f, 0.0017306701047345996f, 0.0023270705714821815f, 0.002573776990175247f, 0.0017887098947539926f, 0.0025039741303771734f, 0.0020939649548381567f, 0.0026977667585015297f, 0.0029221924487501383f, 0.0026908759027719498f, 0.002004798501729965f, 0.003421935485675931f, 0.003437599865719676f, 0.0017987063620239496f, 0.0025960502680391073f, 0.001284499652683735f, 0.004352726973593235f, 0.0017509659519419074f, 0.0017710051033645868f, 0.0018768368754535913f, 0.002111826790496707f, 0.0026856502518057823f, 0.0031431913375854492f, 0.0013331739464774728f, 0.002260131062939763f, 0.0031727326568216085f, 0.00521837780252099f, 0.002602535765618086f, 0.003113384125754237f, 0.0034583669621497393f, 0.0024478009436279535f, 0.0025169786531478167f, 0.001454626559279859f, 0.001205582288093865f, 0.002159175230190158f, 0.001485177082940936f);
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
static const ai_i8 _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = 12;
static const ai_float _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.008393600583076477f;
static const ai_float _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.024987393990159035f;
static const ai_float _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0074354009702801704f, 0.006382566876709461f, 0.014786474406719208f, 0.017707280814647675f, 0.006483553443104029f, 0.016769109293818474f, 0.0070076147094368935f, 0.010689972899854183f, 0.025695612654089928f, 0.007287389133125544f, 0.011589781381189823f, 0.014990055933594704f, 0.005221423227339983f, 0.011161083355545998f, 0.010562661103904247f, 0.007268797140568495f, 0.007741470355540514f, 0.014570881612598896f, 0.013445121236145496f, 0.01810646615922451f, 0.016641410067677498f, 0.008445621468126774f, 0.012617592699825764f, 0.012555832043290138f, 0.007545547094196081f, 0.014949530363082886f, 0.023083504289388657f, 0.008174220100045204f, 0.008228447288274765f, 0.008767245337367058f, 0.011276911944150925f, 0.0158088356256485f, 0.015962738543748856f, 0.029332729056477547f, 0.009373268112540245f, 0.019681984558701515f, 0.006941693369299173f, 0.017913494259119034f, 0.0062324414029717445f, 0.018457720056176186f, 0.0075768036767840385f, 0.013367880135774612f, 0.018613530322909355f, 0.014912243001163006f, 0.012424340471625328f, 0.009189912118017673f, 0.01204182580113411f, 0.014339577406644821f, 0.012917378917336464f, 0.006319951266050339f, 0.01068864669650793f, 0.010186570696532726f, 0.01239026989787817f, 0.011360213160514832f, 0.004507327917963266f, 0.009820463135838509f, 0.011840654537081718f, 0.01070818305015564f, 0.012822618708014488f, 0.012966045178472996f, 0.0155686866492033f, 0.006217272486537695f, 0.022044245153665543f, 0.012835515663027763f, 0.007183740846812725f, 0.01478333305567503f, 0.010408284142613411f, 0.012717830948531628f, 0.007410360965877771f, 0.0072237965650856495f, 0.0058097695000469685f, 0.014528297819197178f, 0.02160663530230522f, 0.011027065105736256f, 0.013884167186915874f, 0.026859311386942863f, 0.014801356941461563f, 0.00966125912964344f, 0.01175171509385109f, 0.005026282276958227f, 0.004663802683353424f, 0.013288462534546852f, 0.011666064150631428f, 0.023075779899954796f, 0.004733797162771225f, 0.005362505558878183f, 0.008563565090298653f, 0.009588238783180714f, 0.005151120945811272f, 0.012114819139242172f, 0.014203573577105999f, 0.009341718629002571f, 0.011002595536410809f, 0.01310692448168993f, 0.005913598462939262f, 0.013223374262452126f);
static const ai_u16 _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_2_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 5;

static const ai_u16 _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_2_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_2_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = 12;
static const ai_i8 _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.024987393990159035f;
static const ai_float _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.011834115721285343f;
static const ai_float _stage4_stage4_2_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.001839401083998382f, 0.0019965050742030144f, 0.0015507022617384791f, 0.0022507095709443092f, 0.0016744611784815788f, 0.0017638754798099399f, 0.0017725720535963774f, 0.0016781816957518458f, 0.002605977701023221f, 0.001967656658962369f, 0.0028478638269007206f, 0.0017952206544578075f, 0.0015848907642066479f, 0.0023089274764060974f, 0.0017186246113851666f, 0.0018633553991094232f, 0.0016470536356791854f, 0.001960660330951214f, 0.001966810552403331f, 0.002197301248088479f, 0.0017213014652952552f, 0.0016031153500080109f, 0.0017295953584834933f, 0.002099507488310337f, 0.0019905176013708115f, 0.002933045383542776f, 0.0018121631583198905f, 0.002131593180820346f, 0.001852367538958788f, 0.0014431345043703914f, 0.002517115091904998f, 0.002318780403584242f, 0.0020427540875971317f, 0.0018424217123538256f, 0.0019035784061998129f, 0.0017793814186006784f, 0.0016096433391794562f, 0.0017478332156315446f, 0.0019792476668953896f, 0.0024860582780092955f, 0.002179778181016445f, 0.0016746149631217122f, 0.0019202807452529669f, 0.002719536190852523f, 0.0020328322425484657f, 0.002622973406687379f, 0.0026425765827298164f, 0.0019131076987832785f, 0.002589627867564559f, 0.0017303740605711937f, 0.002999228658154607f, 0.002585435751825571f, 0.00241758837364614f, 0.0016974752070382237f, 0.0016109035350382328f, 0.0013519894564524293f, 0.0018883075099438429f, 0.0018906623590737581f, 0.002836111467331648f, 0.0022882428020238876f, 0.0016129844589158893f, 0.002433340298011899f, 0.001963040791451931f, 0.001709713600575924f, 0.0024219120386987925f, 0.003169625997543335f, 0.002149582374840975f, 0.0021600963082164526f, 0.0019506553653627634f, 0.002051072660833597f, 0.001962851034477353f, 0.0021823630668222904f, 0.0022848842199891806f, 0.0029127285815775394f, 0.0014501403784379363f, 0.002006298629567027f, 0.00209374469704926f, 0.0022439921740442514f, 0.001860116608440876f, 0.0015990195097401738f, 0.002143791876733303f, 0.003668525954708457f, 0.002239387482404709f, 0.002717617666348815f, 0.002475852146744728f, 0.0015666659455746412f, 0.0017535167280584574f, 0.0031365356408059597f, 0.0015265048714354634f, 0.0023829773999750614f, 0.0027828013990074396f, 0.0018709368305280805f, 0.002050968585535884f, 0.001378763117827475f, 0.0024289246648550034f, 0.0023397335316985846f);
static const ai_layer_format_type _stage4_stage4_2_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_3_branch2_branch2_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_3_branch2_branch2_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.013661094941198826f;
static const ai_float _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.007956556044518948f;
static const ai_float _stage4_stage4_3_branch2_branch2_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0033812180627137423f, 0.0021972598042339087f, 0.0025805593468248844f, 0.002246643416583538f, 0.003560143057256937f, 0.0016952580772340298f, 0.0019410017412155867f, 0.002366977045312524f, 0.0016111941076815128f, 0.0018327174475416541f, 0.0021220932248979807f, 0.0021191288251429796f, 0.0022758739069104195f, 0.002160388045012951f, 0.0020398329943418503f, 0.003119019092991948f, 0.0021784063428640366f, 0.0029662426095455885f, 0.002747192047536373f, 0.002563097048550844f, 0.0016343231545761228f, 0.002502468880265951f, 0.002790295286104083f, 0.0030260258354246616f, 0.0021803760901093483f, 0.0036457583773881197f, 0.0026371742133051157f, 0.0019253012724220753f, 0.003000133205205202f, 0.003174935234710574f, 0.0028214019257575274f, 0.0016308389604091644f, 0.0017847021808847785f, 0.002926014130935073f, 0.0018135003047063947f, 0.002948540262877941f, 0.0028268734458833933f, 0.0019321601139381528f, 0.002906038658693433f, 0.001581045100465417f, 0.002596092876046896f, 0.001822855556383729f, 0.002917818259447813f, 0.0018077644053846598f, 0.002861283952370286f, 0.002500247210264206f, 0.00229111616499722f, 0.0019417193252593279f, 0.0021643585059791803f, 0.0021805251017212868f, 0.0024549413938075304f, 0.0019224000861868262f, 0.0019276360981166363f, 0.0017813992453739047f, 0.001676904852502048f, 0.0018093110993504524f, 0.0027050375938415527f, 0.004925678484141827f, 0.0025077874306589365f, 0.0017989599145948887f, 0.0013877087039873004f, 0.0027466497849673033f, 0.0021233416628092527f, 0.002559665124863386f, 0.0021721075754612684f, 0.0023982496932148933f, 0.0024562824983149767f, 0.001906970632262528f, 0.0023058815859258175f, 0.001796251512132585f, 0.002394078066572547f, 0.0013744824100285769f, 0.001507729641161859f, 0.001706995302811265f, 0.0037607355043292046f, 0.0015525533817708492f, 0.0017615926917642355f, 0.0026749956887215376f, 0.0038394334260374308f, 0.003320726566016674f, 0.002069956623017788f, 0.0018922599265351892f, 0.00376674416474998f, 0.0032048849388957024f, 0.002827294170856476f, 0.0017953363712877035f, 0.0020005316473543644f, 0.002462294651195407f, 0.002094363095238805f, 0.00265712384134531f, 0.0034102629870176315f, 0.0026277080178260803f, 0.002167720114812255f, 0.00306180608458817f, 0.0035402439534664154f, 0.002738955896347761f);
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
static const ai_i8 _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_out_0_fmt_zero_const_s8 = 29;
static const ai_float _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.007956556044518948f;
static const ai_float _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.031227312982082367f;
static const ai_float _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.017377756536006927f, 0.007974641397595406f, 0.01174545381218195f, 0.012311582453548908f, 0.007606135681271553f, 0.020071623846888542f, 0.010554768145084381f, 0.008269745856523514f, 0.005898520350456238f, 0.010315591469407082f, 0.025554107502102852f, 0.020320456475019455f, 0.015331492759287357f, 0.023833490908145905f, 0.004879689775407314f, 0.03079620562493801f, 0.011832937598228455f, 0.016147859394550323f, 0.003695970168337226f, 0.007449841592460871f, 0.006701924838125706f, 0.014995654113590717f, 0.010567676275968552f, 0.012908102944493294f, 0.006982829887419939f, 0.01580565609037876f, 0.009307796135544777f, 0.006288413889706135f, 0.008233200758695602f, 0.012194310314953327f, 0.015309610404074192f, 0.025903765112161636f, 0.02511926367878914f, 0.004553325939923525f, 0.005284355487674475f, 0.01150010246783495f, 0.008761595003306866f, 0.015843035653233528f, 0.01762162521481514f, 0.008665506727993488f, 0.01027235109359026f, 0.028851594775915146f, 0.006749357562512159f, 0.011286177672445774f, 0.011315766721963882f, 0.010756241157650948f, 0.007859589532017708f, 0.015186209231615067f, 0.017263013869524002f, 0.005595965310931206f, 0.010428330861032009f, 0.0073093995451927185f, 0.01422644592821598f, 0.005808701738715172f, 0.008520418778061867f, 0.00821735616773367f, 0.012924804352223873f, 0.011113484390079975f, 0.010135543532669544f, 0.020491929724812508f, 0.022991010919213295f, 0.017298763617873192f, 0.004912103526294231f, 0.013260792940855026f, 0.016951007768511772f, 0.014080206863582134f, 0.009627576917409897f, 0.012215126305818558f, 0.010358874686062336f, 0.013323810882866383f, 0.013188248500227928f, 0.02607603184878826f, 0.02146913856267929f, 0.009244011715054512f, 0.014781992882490158f, 0.014733722433447838f, 0.006882792338728905f, 0.018651098012924194f, 0.01988733932375908f, 0.006461688783019781f, 0.012957001104950905f, 0.010096522979438305f, 0.010465101338922977f, 0.011449597775936127f, 0.010358310304582119f, 0.009276747703552246f, 0.014153258875012398f, 0.0073049371130764484f, 0.004904276225715876f, 0.007821513339877129f, 0.015022451058030128f, 0.010580076836049557f, 0.01943735033273697f, 0.0089603615924716f, 0.013133115135133266f, 0.01330629549920559f);
static const ai_u16 _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_out_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_3_branch2_branch2_3_Conv_output_0_t_out_0_shape_h_const_u16 = 5;

static const ai_u16 _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _stage4_stage4_3_branch2_branch2_7_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stage4_stage4_3_branch2_branch2_7_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_in_0_fmt_zero_const_s8 = 29;
static const ai_i8 _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.031227312982082367f;
static const ai_float _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.01655641943216324f;
static const ai_float _stage4_stage4_3_branch2_branch2_7_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002335085766389966f, 0.002776380628347397f, 0.0033938558772206306f, 0.002137677976861596f, 0.0022643432021141052f, 0.0027669474948197603f, 0.002540790941566229f, 0.004826422780752182f, 0.0022438226733356714f, 0.002895996905863285f, 0.0023045893758535385f, 0.002638164209201932f, 0.0045507485046982765f, 0.001681160763837397f, 0.002107522450387478f, 0.00267342128790915f, 0.0018032640218734741f, 0.0023846409749239683f, 0.0026262281462550163f, 0.001909024897031486f, 0.0032294634729623795f, 0.0017500880640000105f, 0.0016448947135359049f, 0.0017540438566356897f, 0.0023968247696757317f, 0.0022080529015511274f, 0.0026806232053786516f, 0.002169271931052208f, 0.0029345506336539984f, 0.0018731903983280063f, 0.0020431370940059423f, 0.00275517744012177f, 0.002702021738514304f, 0.0015071756206452847f, 0.0034323285799473524f, 0.002163797151297331f, 0.0023380934726446867f, 0.0019292844226583838f, 0.003172872355207801f, 0.003102691378444433f, 0.0030722450464963913f, 0.0021901880390942097f, 0.0016209229361265898f, 0.0025556636974215508f, 0.0017404637765139341f, 0.001898351009003818f, 0.0017811076249927282f, 0.0021146107465028763f, 0.0015762039693072438f, 0.0017716074362397194f, 0.0019967001862823963f, 0.0022059029433876276f, 0.002146697835996747f, 0.00218431418761611f, 0.0026120864786207676f, 0.0018902637530118227f, 0.0019234309438616037f, 0.0024230596609413624f, 0.0016351687954738736f, 0.0016507379477843642f, 0.001887766644358635f, 0.0017951118061318994f, 0.0027509096544235945f, 0.0018719928339123726f, 0.0020884834229946136f, 0.0021705105900764465f, 0.0021491157822310925f, 0.0015706319827586412f, 0.0030117854475975037f, 0.002897724276408553f, 0.0023166178725659847f, 0.0025428454391658306f, 0.0017740377224981785f, 0.002941734855994582f, 0.0025584998074918985f, 0.0020399040076881647f, 0.0021699436474591494f, 0.002576771890744567f, 0.002104235114529729f, 0.0016814530827105045f, 0.0018772182520478964f, 0.0025885647628456354f, 0.0024200286716222763f, 0.003017162438482046f, 0.0028860263992100954f, 0.001847623148933053f, 0.0021736014168709517f, 0.0026408578269183636f, 0.0022262863349169493f, 0.002869912888854742f, 0.0033004397992044687f, 0.0018348755547776818f, 0.002965283812955022f, 0.002525522606447339f, 0.001888419152237475f, 0.0017785151721909642f);
static const ai_layer_format_type _stage4_stage4_3_branch2_branch2_7_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;



static const ai_u16 _conv5_conv5_2_Relu_output_0_t_in_0_shape_w_const_u16 = 5;
static const ai_u16 _conv5_conv5_2_Relu_output_0_t_in_0_shape_h_const_u16 = 5;
static const ai_u16 _conv5_conv5_2_Relu_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _conv5_conv5_2_Relu_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _conv5_conv5_2_Relu_output_0_t_in_0_shape_ch_const_u16 = 192;
static const ai_u16 _conv5_conv5_2_Relu_output_0_t_out_0_shape_ch_const_u16 = 1024;
static const ai_i8 _conv5_conv5_2_Relu_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _conv5_conv5_2_Relu_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _conv5_conv5_2_Relu_output_0_t_in_0_fmt_scale_const_f32 = 0.01655641943216324f;
static const ai_float _conv5_conv5_2_Relu_output_0_t_out_0_fmt_scale_const_f32 = 0.016815809532999992f;
static const ai_float _conv5_conv5_2_Relu_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0022944097872823477f, 0.001818809425458312f, 0.0023114122450351715f, 0.0020197166595607996f, 0.0026400655042380095f, 0.0019210261525586247f, 0.0030119356233626604f, 0.0016311842482537031f, 0.00234513683244586f, 0.0025091913994401693f, 0.003209402086213231f, 0.001723146648146212f, 0.002658831188455224f, 0.002115614013746381f, 0.0021708759013563395f, 0.0027045418974012136f, 0.0020687775686383247f, 0.002146451035514474f, 0.0021528133656829596f, 0.0020995037630200386f, 0.001984252827242017f, 0.002104073064401746f, 0.002137331524863839f, 0.002447781851515174f, 0.0027154316194355488f, 0.00227156188338995f, 0.0023037891369313f, 0.0014597895788028836f, 0.0020107680466026068f, 0.0019566803239285946f, 0.0017853084718808532f, 0.002132243709638715f, 0.0030156588181853294f, 0.002079387428238988f, 0.0023659379221498966f, 0.001696671242825687f, 0.0018750564195215702f, 0.0016037020832300186f, 0.0012278000358492136f, 0.002502613700926304f, 0.0022776902187615633f, 0.0027598340529948473f, 0.0027687919791787863f, 0.00181757309474051f, 0.0017575487727299333f, 0.0019892770797014236f, 0.0023658587597310543f, 0.0036296925973147154f, 0.0036019794642925262f, 0.0017197005217894912f, 0.001931106555275619f, 0.0019224131247028708f, 0.002054216107353568f, 0.0016068239929154515f, 0.0018992165569216013f, 0.002070952206850052f, 0.004046719521284103f, 0.0018759466474875808f, 0.0027366734575480223f, 0.002490426879376173f, 0.0034203778486698866f, 0.0016538655618205667f, 0.004236273467540741f, 0.0020468030124902725f, 0.0016564053948968649f, 0.00257557793520391f, 0.002711213193833828f, 0.002380616497248411f, 0.0022716785315424204f, 0.0024103056639432907f, 0.002367990557104349f, 0.003140689805150032f, 0.002251920523121953f, 0.002605200745165348f, 0.003301959717646241f, 0.003459362545982003f, 0.0021929992362856865f, 0.003525353502482176f, 0.002138125477358699f, 0.0021585910581052303f, 0.002182302065193653f, 0.0020387095864862204f, 0.0026820069178938866f, 0.004794666543602943f, 0.0022823638282716274f, 0.0028263432905077934f, 0.0024193115532398224f, 0.0027079239953309298f, 0.0025493961293250322f, 0.0017754631116986275f, 0.0015785975847393274f, 0.002319092396646738f, 0.001792637282051146f, 0.0021110146772116423f, 0.0020694725681096315f, 0.0021122649777680635f, 0.0026742860209196806f, 0.0020573828369379044f, 0.0018693935126066208f, 0.0025308839976787567f, 0.001715995604172349f, 0.0021207518875598907f, 0.002448105486109853f, 0.001662757946178317f, 0.0020811904687434435f, 0.0021381874103099108f, 0.002685131272301078f, 0.003489401889964938f, 0.0023491582833230495f, 0.0018922053277492523f, 0.0016035507433116436f, 0.0020743615459650755f, 0.0025647885631769896f, 0.003447682363912463f, 0.0034439601004123688f, 0.0013385146157816052f, 0.0019790311343967915f, 0.0016658836975693703f, 0.0017199579160660505f, 0.0028597998898476362f, 0.0020882445387542248f, 0.002208528807386756f, 0.001985579263418913f, 0.003951882477849722f, 0.0022858637385070324f, 0.002470610663294792f, 0.0016254871152341366f, 0.004380294121801853f, 0.0024864240549504757f, 0.0025518350303173065f, 0.0024549560621380806f, 0.002041873522102833f, 0.0018215852323919535f, 0.001988828182220459f, 0.0024057149421423674f, 0.0018691768636927009f, 0.0015653453301638365f, 0.0018763316329568624f, 0.0030777822248637676f, 0.003649122780188918f, 0.0022497118916362524f, 0.001944356132298708f, 0.004303311929106712f, 0.0017350566340610385f, 0.0021683983504772186f, 0.002677541458979249f, 0.002511598402634263f, 0.0022883762139827013f, 0.0029188792686909437f, 0.0016483071958646178f, 0.0023285020142793655f, 0.0008362015360035002f, 0.0017427117563784122f, 0.0014866143465042114f, 0.001646790187805891f, 0.0030328729189932346f, 0.0018227691762149334f, 0.002464090706780553f, 0.0023148334585130215f, 0.0016085562529042363f, 0.0021257393527776003f, 0.001379485591314733f, 0.0014195982366800308f, 0.0019870500545948744f, 0.002323564374819398f, 0.00332265836186707f, 0.001772373216226697f, 0.0021153457928448915f, 0.0033776303753256798f, 0.0021537470165640116f, 0.0023983176797628403f, 0.002775001572445035f, 0.002352130366489291f, 0.005944338161498308f, 0.003628589678555727f, 0.001776994438841939f, 0.002078686375170946f, 0.0017927660373970866f, 0.0025115555617958307f, 0.003404320450499654f, 0.002832254394888878f, 0.002043927786871791f, 0.0032796517480164766f, 0.0017732948763296008f, 0.002133185276761651f, 0.0020151734352111816f, 0.003131220117211342f, 0.002561860019341111f, 0.0024763583205640316f, 0.0014187098713591695f, 0.002560907509177923f, 0.002839827910065651f, 0.001738285063765943f, 0.001789311645552516f, 0.0030554323457181454f, 0.0022318889386951923f, 0.0018081400776281953f, 0.0018213141011074185f, 0.0027036964893341064f, 0.0019301316933706403f, 0.0020984592847526073f, 0.0029645077884197235f, 0.0018281142693012953f, 0.0017471648752689362f, 0.0021802964620292187f, 0.0018752586329355836f, 0.002362132305279374f, 0.0022593229077756405f, 0.001848057028837502f, 0.002375952899456024f, 0.0027345698326826096f, 0.0020473964978009462f, 0.002180057344958186f, 0.002362249419093132f, 0.002010166645050049f, 0.0027275895699858665f, 0.0020654012914747f, 0.0031619572546333075f, 0.002201697090640664f, 0.0025730475317686796f, 0.002008814597502351f, 0.00201838999055326f, 0.003304101526737213f, 0.0021161281038075686f, 0.0027027137111872435f, 0.002996393945068121f, 0.0018066399497911334f, 0.002225223695859313f, 0.0018854034133255482f, 0.002196216257289052f, 0.001495713135227561f, 0.002884912770241499f, 0.0022281149867922068f, 0.0018865738529711962f, 0.0028627123683691025f, 0.0016550610307604074f, 0.0024751757737249136f, 0.0025454715359956026f, 0.002131594344973564f, 0.0016385946655645967f, 0.0025195488706231117f, 0.0020572233479470015f, 0.002412657253444195f, 0.0024894271045923233f, 0.0024193518329411745f, 0.001554966322146356f, 0.002654352691024542f, 0.0018344296840950847f, 0.0020399533677846193f, 0.0024995121639221907f, 0.0017346208915114403f, 0.001882756594568491f, 0.0021427269093692303f, 0.001869256841018796f, 0.002536102430894971f, 0.0019790385849773884f, 0.00224604201503098f, 0.0020617088302969933f, 0.0022914696019142866f, 0.002319755032658577f, 0.0020852978341281414f, 0.0031777273397892714f, 0.0016825125785544515f, 0.004046477843075991f, 0.002044517546892166f, 0.003814636031165719f, 0.001985538052394986f, 0.001903034164570272f, 0.002597202081233263f, 0.0024922965094447136f, 0.0018148813396692276f, 0.00191951310262084f, 0.001872374559752643f, 0.0021355377975851297f, 0.002512069884687662f, 0.002056640572845936f, 0.0014853127067908645f, 0.0013869325630366802f, 0.0033567571081221104f, 0.0018932389793917537f, 0.003115207189694047f, 0.0016488314140588045f, 0.001844668760895729f, 0.002440867479890585f, 0.0015424726298078895f, 0.0027454672381281853f, 0.0023171480279415846f, 0.0023613523226231337f, 0.0015694273170083761f, 0.0016198729863390326f, 0.003046085825189948f, 0.002711166860535741f, 0.0035900198854506016f, 0.001992194913327694f, 0.001840387238189578f, 0.0024969179648905993f, 0.001800429541617632f, 0.0020232591778039932f, 0.003308212850242853f, 0.002165937563404441f, 0.001827687257900834f, 0.002765535842627287f, 0.0027680157218128443f, 0.0023023660760372877f, 0.0017586585599929094f, 0.00243727071210742f, 0.0022575166076421738f, 0.0027913772501051426f, 0.0022595315240323544f, 0.0021359119564294815f, 0.0017683348851278424f, 0.0020185429602861404f, 0.0036705525126308203f, 0.001581372576765716f, 0.002099199453368783f, 0.0025433474220335484f, 0.0019419159507378936f, 0.001642442774027586f, 0.0026243168395012617f, 0.0023021504748612642f, 0.001664519659243524f, 0.002035868586972356f, 0.0020658397115767f, 0.003391900332644582f, 0.0017226961208507419f, 0.0019089190755039454f, 0.002573516219854355f, 0.0023348669055849314f, 0.0022041299380362034f, 0.0018716499907895923f, 0.002412820467725396f, 0.0019241903210058808f, 0.002193830208852887f, 0.0016986310947686434f, 0.002673696493729949f, 0.0013713582884520292f, 0.0019011299591511488f, 0.0017205225303769112f, 0.0034335614182054996f, 0.001731539610773325f, 0.0026771058328449726f, 0.0017212312668561935f, 0.002526776399463415f, 0.0033257852774113417f, 0.0020908082369714975f, 0.0022771682124584913f, 0.0014755480224266648f, 0.0018060044385492802f, 0.0017473800107836723f, 0.0021588215604424477f, 0.0019848630763590336f, 0.0015208505792543292f, 0.0021794557105749846f, 0.0018083321629092097f, 0.0021116624120622873f, 0.0027433724608272314f, 0.0019802807364612818f, 0.0020903449039906263f, 0.0023738748859614134f, 0.002653570845723152f, 0.0024024236481636763f, 0.0023345209192484617f, 0.0023245117627084255f, 0.0023576891981065273f, 0.003293197602033615f, 0.0016809599474072456f, 0.004111779388040304f, 0.0018020311836153269f, 0.0020014261826872826f, 0.001907779835164547f, 0.00176289735827595f, 0.001878151553682983f, 0.0025324849411845207f, 0.002311472315341234f, 0.003937429282814264f, 0.002295462880283594f, 0.0020051358733326197f, 0.002271998440846801f, 0.0028738740365952253f, 0.001699351822026074f, 0.002853905316442251f, 0.0029313191771507263f, 0.0018771762261167169f, 0.001927303965203464f, 0.0018898948328569531f, 0.0022164611145853996f, 0.0015113061526790261f, 0.002055151155218482f, 0.0022616046480834484f, 0.003412422724068165f, 0.0019652280025184155f, 0.0021373783238232136f, 0.0014707708032801747f, 0.0017623970052227378f, 0.0022082440555095673f, 0.0033804336562752724f, 0.0018894117092713714f, 0.0015993149718269706f, 0.0013542920351028442f, 0.0025401462335139513f, 0.0028039952740073204f, 0.002532440470531583f, 0.0021067142952233553f, 0.00228179432451725f, 0.001645767712034285f, 0.002130095846951008f, 0.002184513956308365f, 0.002570859855040908f, 0.002018765779212117f, 0.003404709044843912f, 0.0028954879380762577f, 0.0018802614649757743f, 0.002471792045980692f, 0.001973398495465517f, 0.0022028095554560423f, 0.0026692168321460485f, 0.0018470929935574532f, 0.0019289328483864665f, 0.0022060726769268513f, 0.002607086906209588f, 0.002053164644166827f, 0.0019016792066395283f, 0.001777408760972321f, 0.0024978534784168005f, 0.0022938724141567945f, 0.0025981373619288206f, 0.0014796198811382055f, 0.0020413303282111883f, 0.0018978581065312028f, 0.002101738005876541f, 0.0015692850574851036f, 0.001848654355853796f, 0.0031351842917501926f, 0.003109258133918047f, 0.0035465045366436243f, 0.002385745756328106f, 0.002388701541349292f, 0.0017319200560450554f, 0.0017725069774314761f, 0.0015379778342321515f, 0.002021916676312685f, 0.0020938883535563946f, 0.0019117776537314057f, 0.0020806670654565096f, 0.002268237993121147f, 0.002114881295710802f, 0.0014544693985953927f, 0.0027362285181879997f, 0.0024810885079205036f, 0.002483716933056712f, 0.002143143443390727f, 0.0021839658729732037f, 0.0020792577415704727f, 0.0026539515238255262f, 0.0024254133459180593f, 0.0021248855628073215f, 0.002613843185827136f, 0.0023103023413568735f, 0.002139922697097063f, 0.0021892618387937546f, 0.001741602667607367f, 0.0023710017558187246f, 0.001729351468384266f, 0.0021449190098792315f, 0.0025067103561013937f, 0.0025049990508705378f, 0.0030073514208197594f, 0.0023109435569494963f, 0.001503431936725974f, 0.0030645071528851986f, 0.0022101288195699453f, 0.0015792843187227845f, 0.0012823367724195123f, 0.001975825522094965f, 0.0035253865644335747f, 0.0023646620102226734f, 0.0020023698452860117f, 0.0014909767778590322f, 0.0028133837040513754f, 0.0022808369249105453f, 0.002420690841972828f, 0.001992217730730772f, 0.0018531641690060496f, 0.0024372581392526627f, 0.0025009531527757645f, 0.0036436300724744797f, 0.003054956439882517f, 0.001327672740444541f, 0.0009951636893674731f, 0.002340826205909252f, 0.002328659873455763f, 0.0024597500450909138f, 0.004814383573830128f, 0.0021367049776017666f, 0.0025872732512652874f, 0.0020544882863759995f, 0.0016292479122057557f, 0.003270005574449897f, 0.0017385787796229124f, 0.0018035420216619968f, 0.001969691598787904f, 0.0024744966067373753f, 0.0033398508094251156f, 0.001871529733762145f, 0.0023036696948111057f, 0.0031294748187065125f, 0.0013866613153368235f, 0.0017129714833572507f, 0.0019280232954770327f, 0.0017759018810465932f, 0.0024782877881079912f, 0.001970279961824417f, 0.0016880002804100513f, 0.002060733735561371f, 0.002179041039198637f, 0.0022733223158866167f, 0.003419172018766403f, 0.0014189565554261208f, 0.002970857312902808f, 0.002001703018322587f, 0.0026955432258546352f, 0.002481374889612198f, 0.0013790242373943329f, 0.0025631729513406754f, 0.0020327316597104073f, 0.0015646026004105806f, 0.0028475243598222733f, 0.002607882721349597f, 0.0017896270146593451f, 0.0024829658214002848f, 0.0024664243683218956f, 0.001481157261878252f, 0.0028647994622588158f, 0.0022277459502220154f, 0.0031265115831047297f, 0.0016554259927943349f, 0.002107869368046522f, 0.00248059444129467f, 0.0017251906683668494f, 0.0022742771543562412f, 0.002188797341659665f, 0.0020902000833302736f, 0.0015591358533129096f, 0.002479621907696128f, 0.0024097117129713297f, 0.0025503570213913918f, 0.0025874213315546513f, 0.002100439043715596f, 0.0024083037860691547f, 0.001954419305548072f, 0.00429717730730772f, 0.0021011941134929657f, 0.0023883297108113766f, 0.0020489932503551245f, 0.0021992740221321583f, 0.00232934788800776f, 0.0015594837022945285f, 0.0017020173836499453f, 0.0015688900602981448f, 0.006643785163760185f, 0.0021719287615269423f, 0.002340960316359997f, 0.0025168797001242638f, 0.0021305421832948923f, 0.001997065031901002f, 0.0024676986504346132f, 0.0014750654809176922f, 0.0022809344809502363f, 0.00208599166944623f, 0.002310734475031495f, 0.0017000515945255756f, 0.0035867902915924788f, 0.002099759876728058f, 0.002850750694051385f, 0.002713226480409503f, 0.0018644133815541863f, 0.0023540989495813847f, 0.0021420421544462442f, 0.002464073710143566f, 0.0021563691552728415f, 0.0022884197533130646f, 0.0021166575606912374f, 0.004502616822719574f, 0.003048282815143466f, 0.0024498996790498495f, 0.001527225598692894f, 0.0019755661487579346f, 0.0019606738351285458f, 0.0020534349605441093f, 0.001946990261785686f, 0.002280899090692401f, 0.002567295217886567f, 0.001801276346668601f, 0.0017357137985527515f, 0.0030285180546343327f, 0.0020589674822986126f, 0.0036492086946964264f, 0.00331370672211051f, 0.0021474971435964108f, 0.0023837739136070013f, 0.0013624683488160372f, 0.0030715009197592735f, 0.0027542333118617535f, 0.0029817193280905485f, 0.002210586564615369f, 0.0023588945623487234f, 0.0021106479689478874f, 0.0016012738924473524f, 0.002308118622750044f, 0.0013568130088970065f, 0.0024928743951022625f, 0.00199972465634346f, 0.0022063374053686857f, 0.002038330305367708f, 0.0024776847567409277f, 0.00290534645318985f, 0.0024606031365692616f, 0.002208300866186619f, 0.002537862164899707f, 0.0019928638357669115f, 0.0026382706128060818f, 0.0025671988260000944f, 0.0021449679043143988f, 0.0025248972233384848f, 0.0021799064707010984f, 0.0036624977365136147f, 0.002836250001564622f, 0.0021965052001178265f, 0.0027084709145128727f, 0.0032434193417429924f, 0.002265428425744176f, 0.0015153816202655435f, 0.002040253020823002f, 0.00288770766928792f, 0.0023001946974545717f, 0.003047822741791606f, 0.0021246077958494425f, 0.0016738120466470718f, 0.0016942390939220786f, 0.00212486763484776f, 0.0020728884264826775f, 0.0016692252829670906f, 0.003541141515597701f, 0.0017164943274110556f, 0.004004843533039093f, 0.0018846963066607714f, 0.004262580536305904f, 0.0018410152988508344f, 0.0018947197822853923f, 0.0021098258439451456f, 0.0031281134579330683f, 0.0017166577745229006f, 0.0031636846251785755f, 0.0023511748295277357f, 0.001863384386524558f, 0.001890590414404869f, 0.0025839845184236765f, 0.0030194674618542194f, 0.0030443419236689806f, 0.002125554485246539f, 0.003224238520488143f, 0.002380085177719593f, 0.001724273432046175f, 0.0022885219659656286f, 0.002239318797364831f, 0.002056799130514264f, 0.0021607503294944763f, 0.0016937979962676764f, 0.00311449496075511f, 0.002040473511442542f, 0.0016266020247712731f, 0.0020323176868259907f, 0.0015281864907592535f, 0.0019728487823158503f, 0.002352602779865265f, 0.0017036143690347672f, 0.0036888301838189363f, 0.002118390053510666f, 0.0027746085543185472f, 0.002137228148058057f, 0.0031393214594572783f, 0.0012694275937974453f, 0.0028440740425139666f, 0.0022088997066020966f, 0.002364171203225851f, 0.0018316848436370492f, 0.0021228627301752567f, 0.0015949704684317112f, 0.002401827136054635f, 0.002013475401327014f, 0.0023456201888620853f, 0.0025380176957696676f, 0.002606208436191082f, 0.002000263426452875f, 0.002291869604960084f, 0.0024408551398664713f, 0.0025052072014659643f, 0.002492249244824052f, 0.001928590820170939f, 0.002981285797432065f, 0.002349784830585122f, 0.00221081729978323f, 0.0015898863784968853f, 0.003041354240849614f, 0.002033871365711093f, 0.0016928700497373939f, 0.002342459512874484f, 0.0020619698334485292f, 0.0026783552020788193f, 0.0016603127587586641f, 0.001936455024406314f, 0.002098105615004897f, 0.0017678245203569531f, 0.0019995435141026974f, 0.001945068477652967f, 0.002341297222301364f, 0.00240131001919508f, 0.002554123057052493f, 0.0021584630012512207f, 0.0018298791255801916f, 0.002180281328037381f, 0.0020750591065734625f, 0.002572125056758523f, 0.0021749117877334356f, 0.002775095170363784f, 0.0023910922463983297f, 0.00339501746930182f, 0.0020429370924830437f, 0.001983902882784605f, 0.0022464387584477663f, 0.002042478183284402f, 0.0027352168690413237f, 0.00334314932115376f, 0.0016349200159311295f, 0.00256859022192657f, 0.002704671584069729f, 0.0023063484113663435f, 0.0032117064110934734f, 0.0017396952025592327f, 0.002002848545089364f, 0.0027845113072544336f, 0.002399396849796176f, 0.002763175405561924f, 0.001639341819100082f, 0.0019089457346126437f, 0.002819431945681572f, 0.0018423261353746057f, 0.0020271127577871084f, 0.004129406064748764f, 0.0027661658823490143f, 0.0028973545413464308f, 0.002177632413804531f, 0.0017223746981471777f, 0.0014190729707479477f, 0.0041325693018734455f, 0.002118985168635845f, 0.0011570770293474197f, 0.0019521692302078009f, 0.0023637956473976374f, 0.0024283083621412516f, 0.002910977229475975f, 0.0020193280652165413f, 0.002882464090362191f, 0.0031745240557938814f, 0.002549189142882824f, 0.0020318140741437674f, 0.0014668494695797563f, 0.002161445328965783f, 0.0022268593311309814f, 0.002403996651992202f, 0.0017307472880929708f, 0.002482025418430567f, 0.0011323712533339858f, 0.003279800759628415f, 0.002227417193353176f, 0.0022032922133803368f, 0.0017201538430526853f, 0.002111253561452031f, 0.0035870636347681284f, 0.0013181830290704966f, 0.002011409495025873f, 0.0019524215022101998f, 0.0016127460403367877f, 0.0019513729494065046f, 0.0025325866881757975f, 0.0014984902227297425f, 0.002770362887531519f, 0.0019479036564007401f, 0.0017026319401338696f, 0.002126342849805951f, 0.0025715737137943506f, 0.0018363528652116656f, 0.002237629843875766f, 0.002089105546474457f, 0.0021729315631091595f, 0.0025784801691770554f, 0.00235728663392365f, 0.0017283055931329727f, 0.002167136874049902f, 0.00264569791033864f, 0.001936011714860797f, 0.002690132474526763f, 0.001741473563015461f, 0.0022889161482453346f, 0.0022384070325642824f, 0.0009465688490308821f, 0.0020048695150762796f, 0.0016964460955932736f, 0.0016691343626007438f, 0.002067391062155366f, 0.0019294051453471184f, 0.0021933973766863346f, 0.001988454256206751f, 0.0018384511349722743f, 0.002358356723561883f, 0.0017496994696557522f, 0.0025339734274894f, 0.0022596109192818403f, 0.0020075540523976088f, 0.0045137424021959305f, 0.0016664837021380663f, 0.0021325447596609592f, 0.0016462429193779826f, 0.002612915588542819f, 0.0023420751094818115f, 0.002326055197045207f, 0.0033511859364807606f, 0.001606773934327066f, 0.0025487912353128195f, 0.0025227994192391634f, 0.002285332651808858f, 0.002701251767575741f, 0.002047138987109065f, 0.003096302505582571f, 0.002965116873383522f, 0.0019460972398519516f, 0.0019205478020012379f, 0.002208530670031905f, 0.003653645981103182f, 0.002446848666295409f, 0.0020282049663364887f, 0.0024682122748345137f, 0.003125141840428114f, 0.003073435975238681f, 0.001780493650585413f, 0.001526527339592576f, 0.0020141350105404854f, 0.0014396269107237458f, 0.0021807311568409204f, 0.002277635969221592f, 0.0020463441032916307f, 0.0017871897434815764f, 0.0022415879648178816f, 0.0015931212110444903f, 0.0023631544318050146f, 0.002158514689654112f, 0.0027392480988055468f, 0.0020519045647233725f, 0.0036885549779981375f, 0.0022686035372316837f, 0.00182308629155159f, 0.0020388057455420494f, 0.0018666075775399804f, 0.0017680453602224588f, 0.0021518394351005554f, 0.0025142717640846968f, 0.0013770038494840264f, 0.002117806114256382f, 0.0026424096431583166f, 0.001992074539884925f, 0.0024418195243924856f, 0.001451148302294314f, 0.0027623374480754137f, 0.0024903039447963238f, 0.0018781288526952267f, 0.0024691997095942497f, 0.001563680823892355f, 0.002549594035372138f, 0.0024294487666338682f, 0.002381592756137252f, 0.0027599979657679796f, 0.0033829365856945515f, 0.002911762334406376f, 0.0025567428674548864f, 0.0024169893004000187f, 0.001767544075846672f, 0.002552541671320796f, 0.0012568426318466663f, 0.002200791845098138f, 0.0011886685388162732f, 0.0028875821735709906f, 0.0014039980014786124f, 0.0026563170831650496f, 0.001635891036130488f, 0.0033798522781580687f, 0.0015994409332051873f, 0.0022810541559010744f, 0.0025098114274442196f, 0.0019062511855736375f, 0.0024509369395673275f, 0.0020595830865204334f, 0.0018632460851222277f, 0.001562955672852695f, 0.003356604604050517f, 0.0022138655185699463f, 0.0025150433648377657f, 0.003163836896419525f, 0.002508507575839758f, 0.00272751459851861f, 0.0021295244805514812f, 0.002236215164884925f, 0.0015122797340154648f, 0.0020676483400166035f, 0.001094395061954856f, 0.0020507927983999252f, 0.002522359136492014f, 0.0022491004783660173f, 0.0023292347323149443f, 0.002109132707118988f, 0.0021479364950209856f, 0.002366466913372278f, 0.002175278961658478f, 0.001890806364826858f, 0.002504854230210185f, 0.0025785311590880156f, 0.0027708758134394884f, 0.0025392300449311733f, 0.0025807537604123354f, 0.0020549390465021133f, 0.002342492574825883f, 0.0021738859359174967f, 0.0020883907563984394f, 0.001966171432286501f, 0.002560737542808056f, 0.0019545673858374357f, 0.0018630500417202711f, 0.0023355272132903337f, 0.0027248035185039043f, 0.002256927778944373f, 0.003626832738518715f, 0.001678924891166389f, 0.002250894671306014f, 0.0020163212902843952f, 0.0018157149897888303f, 0.0017213474493473768f, 0.0023780949413776398f, 0.0021058982238173485f, 0.0014882994582876563f, 0.001804632251150906f, 0.0022704319562762976f, 0.001848563551902771f, 0.0017900461098179221f, 0.001686225295998156f, 0.002363398438319564f, 0.002748345723375678f, 0.0022680482361465693f, 0.0019030928378924727f, 0.0021732719615101814f, 0.0018837163224816322f, 0.0030654093716293573f, 0.0020411021541804075f, 0.002351130824536085f, 0.0018189736874774098f, 0.0033100899308919907f, 0.0019343211315572262f, 0.0016460772603750229f, 0.0016555180773139f, 0.004204887431114912f, 0.0018650030251592398f, 0.0016218168893828988f, 0.0021589731331914663f, 0.0036804440896958113f, 0.001963176531717181f, 0.0018302388489246368f, 0.002648548688739538f, 0.0030502122826874256f, 0.0015356778167188168f, 0.002548163291066885f, 0.0016405052738264203f, 0.0018347679870203137f, 0.002268941840156913f, 0.002468847669661045f, 0.002256040694192052f, 0.002758449874818325f, 0.0020145787857472897f, 0.0022984936367720366f, 0.0021006541792303324f, 0.0017533119535073638f, 0.002050974639132619f, 0.0026029234286397696f, 0.001963564194738865f, 0.001925986260175705f, 0.0027723624370992184f, 0.0028326569590717554f, 0.0028923279605805874f, 0.0022369190119206905f, 0.0026584884617477655f, 0.0017575250240042806f, 0.0026156259700655937f, 0.0018063161987811327f, 0.0020313900895416737f, 0.0025740736164152622f, 0.0019208226585760713f, 0.002107660984620452f, 0.0021648360416293144f, 0.002132454887032509f, 0.003511530114337802f, 0.002300185151398182f, 0.0025330150965601206f, 0.0022726005408912897f, 0.0024538226425647736f, 0.002178721595555544f, 0.0018128019291907549f, 0.0019835822749882936f, 0.002835859078913927f, 0.0016494478331878781f, 0.002957970602437854f, 0.0026114927604794502f, 0.0013587534194812179f, 0.0019901448395103216f, 0.0017245662165805697f, 0.002361466409638524f, 0.0018372917547821999f, 0.002417830051854253f);
static const ai_layer_format_type _conv5_conv5_2_Relu_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u32 _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 25600;
static const ai_float _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.016815809532999992f;
static const ai_i8 _conv5_conv5_2_Relu_output_0_0_0__ReduceMean_output_0_conversion_t_in_0_fmt_zero_const_s8 = -128;



static const ai_u32 _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 1024;
static const ai_float _ReduceMean_output_0_Mul_0_0_logits_QuantizeLinear_Input_conversion_t_out_0_fmt_scale_const_f32 = 0.0020063179545104504f;
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

