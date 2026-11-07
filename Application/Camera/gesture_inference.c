/** @file gesture_inference.c
 * @brief 手势模型的初始化、同步推理及预览服务回调。
 * 激活区分布在片内和外部 SRAM；须在 FSMC 初始化和 SRAM 自检之后调用。
 */
#include "gesture_inference.h"

#include "main.h"
#include "network.h"
#include "stai_events.h"
#include "sram_layout.h"
#include <stddef.h>
#include <string.h>

#if STAI_NETWORK_ACTIVATIONS_NUM != 2 || STAI_NETWORK_ACTIVATION_2_SIZE_BYTES > SRAM_LAYOUT_AI_SIZE || \
    STAI_NETWORK_IN_1_SIZE_BYTES != SRAM_LAYOUT_INPUT_SIZE || STAI_NETWORK_OUT_1_SIZE_BYTES != 3
#error "Generated model does not match the application memory/output contract"
#endif

/* 网络上下文与第一激活区常驻片内；第二激活区使用固定的外部 SRAM 区域。 */
STAI_NETWORK_CONTEXT_DECLARE(gesture_network, STAI_NETWORK_CONTEXT_SIZE)
STAI_ALIGNED(32)
static uint8_t gesture_activations[STAI_NETWORK_ACTIVATION_1_SIZE_BYTES];
static uint8_t gesture_ready;
static void (*gesture_service)(void);

static void GestureInference_ServiceCallback(void *cookie, stai_event_type event,
                                             const void *payload)
{
    (void)cookie;
    (void)payload;
    /* 同步推理节点间在调用线程推进预览，服务函数不能递归调用推理。 */
    if ((event == STAI_EVENT_NODE_STOP) && (gesture_service != NULL)) gesture_service();
}

void GestureInference_SetService(void (*service)(void))
{
    gesture_service = service;
}

static stai_return_code GestureInference_Init(void)
{
    stai_return_code status;
    const stai_ptr activations[STAI_NETWORK_ACTIVATIONS_NUM] = {
        gesture_activations, (stai_ptr)SRAM_LAYOUT_AI_BASE
    };

    status = stai_runtime_init();
    if (status != STAI_SUCCESS) return status;
    status = stai_network_init(gesture_network);
    if (status != STAI_SUCCESS) return status;
    status = stai_network_set_callback(gesture_network, GestureInference_ServiceCallback, NULL);
    if (status != STAI_SUCCESS) return status;
    status = stai_network_set_activations(gesture_network, activations,
                                          STAI_NETWORK_ACTIVATIONS_NUM);
    if (status == STAI_SUCCESS) gesture_ready = 1U;
    return status;
}

int GestureInference_Run(const int8_t *input, int8_t output[3],
                         uint32_t *elapsed_ms, uint32_t *error_code)
{
    stai_return_code status;
    stai_ptr inputs[STAI_NETWORK_IN_NUM];
    stai_ptr outputs[STAI_NETWORK_OUT_NUM];
    stai_size count = 0U;
    uint32_t start;

    if ((input == NULL) || (output == NULL) || (elapsed_ms == NULL) ||
        (error_code == NULL)) return -1;
    *error_code = 0U;
    *elapsed_ms = 0U;
    if (gesture_ready == 0U)
    {
        status = GestureInference_Init();
        if (status != STAI_SUCCESS) goto failed;
    }
    status = stai_network_get_inputs(gesture_network, inputs, &count);
    if (status != STAI_SUCCESS) goto failed;
    if ((count != 1U) || (inputs[0] == NULL))
    {
        status = STAI_ERROR_NETWORK_INVALID_IN_PTR;
        goto failed;
    }
    /* 模型输入可能复用激活区；保留调用方的输入副本用于对照。 */
    memcpy(inputs[0], input, STAI_NETWORK_IN_1_SIZE_BYTES);

    /* 耗时包含节点回调中的预览服务，不是纯模型计算时间。 */
    start = HAL_GetTick();
    status = stai_network_run(gesture_network, STAI_MODE_SYNC);
    *elapsed_ms = HAL_GetTick() - start;
    if (status != STAI_SUCCESS) goto failed;

    status = stai_network_get_outputs(gesture_network, outputs, &count);
    if (status != STAI_SUCCESS) goto failed;
    if ((count != 1U) || (outputs[0] == NULL))
    {
        status = STAI_ERROR_NETWORK_INVALID_OUT_PTR;
        goto failed;
    }
    memcpy(output, outputs[0], 3U);
    return 0;

failed:
    *error_code = (uint32_t)status;
    gesture_ready = 0U;
    return -1;
}
