#include "gesture_inference.h"

#include "main.h"
#include "network.h"
#include "sram_layout.h"
#include <stddef.h>
#include <string.h>

#if STAI_NETWORK_ACTIVATIONS_NUM != 2 || STAI_NETWORK_ACTIVATION_2_SIZE_BYTES > SRAM_LAYOUT_AI_SIZE || \
    STAI_NETWORK_IN_1_SIZE_BYTES != SRAM_LAYOUT_INPUT_SIZE || STAI_NETWORK_OUT_1_SIZE_BYTES != 3
#error "Generated model does not match the application memory/output contract"
#endif

STAI_NETWORK_CONTEXT_DECLARE(gesture_network, STAI_NETWORK_CONTEXT_SIZE)
STAI_ALIGNED(32)
static uint8_t gesture_activations[STAI_NETWORK_ACTIVATION_1_SIZE_BYTES];
static uint8_t gesture_ready;

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
    memcpy(inputs[0], input, STAI_NETWORK_IN_1_SIZE_BYTES);

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
