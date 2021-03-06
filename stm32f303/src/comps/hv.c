#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"
#include "tim.h"
#include "f3hw.h"

HAL_COMP(hv);

//U V W input
HAL_PIN(u);
HAL_PIN(v);
HAL_PIN(w);

HAL_PIN(udc);

//TODO: half bridge enable in
HAL_PIN(enu);
HAL_PIN(env);
HAL_PIN(enw);

HAL_PIN(min_on);   // min on time [s]
HAL_PIN(min_off);  // min off time [s]

HAL_PIN(dac);

//comperator outputs
HAL_PIN(cu);
HAL_PIN(cv);
HAL_PIN(cw);
//master out enable
HAL_PIN(moe_r);
HAL_PIN(moe_w);

HAL_PIN(arr);

struct hv_ctx_t {
  uint32_t pwm_res;
};

static void nrt_init(volatile void *ctx_ptr, volatile hal_pin_inst_t *pin_ptr) {
  // struct hv_ctx_t * ctx = (struct hv_ctx_t *)ctx_ptr;
  struct hv_pin_ctx_t *pins = (struct hv_pin_ctx_t *)pin_ptr;

  PIN(enu)     = 1.0;
  PIN(env)     = 1.0;
  PIN(enw)     = 1.0;
  PIN(min_on)  = 0.00000035;
  PIN(min_off) = 0.000005;
  PIN(dac)     = 1100;
  PIN(arr)     = PWM_RES;
}

static void rt_func(float period, volatile void *ctx_ptr, volatile hal_pin_inst_t *pin_ptr) {
  struct hv_ctx_t *ctx      = (struct hv_ctx_t *)ctx_ptr;
  struct hv_pin_ctx_t *pins = (struct hv_pin_ctx_t *)pin_ptr;

  ctx->pwm_res = (uint32_t)CLAMP(PIN(arr), PWM_RES * 0.9, PWM_RES * 1.1);
  TIM8->ARR    = ctx->pwm_res;

  float udc = MAX(PIN(udc), 0.1);
  //convert voltages to PWM output compare values
  int32_t u = (int32_t)(CLAMP(PIN(u), 0.0, udc) / udc * (float)(ctx->pwm_res));
  int32_t v = (int32_t)(CLAMP(PIN(v), 0.0, udc) / udc * (float)(ctx->pwm_res));
  int32_t w = (int32_t)(CLAMP(PIN(w), 0.0, udc) / udc * (float)(ctx->pwm_res));
  //convert on and off times to PWM output compare values
  int32_t min_on  = (int32_t)((float)(ctx->pwm_res) * 15000.0 * PIN(min_on) + 0.5);
  int32_t min_off = (int32_t)((float)(ctx->pwm_res) * 15000.0 * PIN(min_off) + 0.5);

  if((u > 0 && u < min_on) || (v > 0 && v < min_on) || (w > 0 && w < min_on)) {
    u += min_on;
    v += min_on;
    w += min_on;
  }

  if((u > ctx->pwm_res - min_off) || (v > ctx->pwm_res - min_off) || (w > ctx->pwm_res - min_off)) {
    u -= min_off;
    v -= min_off;
    w -= min_off;
  }

#ifdef PWM_INVERT
  PWM_U = ctx->pwm_res - CLAMP(u, 0, ctx->pwm_res - min_off);
  PWM_V = ctx->pwm_res - CLAMP(v, 0, ctx->pwm_res - min_off);
  PWM_W = ctx->pwm_res - CLAMP(w, 0, ctx->pwm_res - min_off);
#else
  PWM_U = CLAMP(u, 0, ctx->pwm_res - min_off);
  PWM_V = CLAMP(v, 0, ctx->pwm_res - min_off);
  PWM_W = CLAMP(w, 0, ctx->pwm_res - min_off);
#endif

  //dac output for comperators
  DAC1->DHR12R1 = (uint32_t)PIN(dac);

  //comperator outputs for debugging
  PIN(cu) = (COMP1->CSR & COMP_CSR_COMPxOUT) > 0;
  PIN(cv) = (COMP2->CSR & COMP_CSR_COMPxOUT) > 0;
  PIN(cw) = (COMP4->CSR & COMP_CSR_COMPxOUT) > 0;

  //master out enable
  PIN(moe_r) = (TIM8->BDTR & TIM_BDTR_MOE) > 0;
  if(PIN(moe_w) > 0.0) {
    PIN(moe_w) = 0.0;
    TIM8->BDTR |= TIM_BDTR_MOE;
  }
}

hal_comp_t hv_comp_struct = {
    .name      = "hv",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct hv_ctx_t),
    .pin_count = sizeof(struct hv_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
