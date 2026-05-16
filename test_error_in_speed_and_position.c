#include "stm32f401xe.h"
#include <stdint.h>

/* ================= BIEN XEM TREN SWV ================= */

volatile int g_pwm = 0;

volatile int g_left_count = 0;
volatile int g_right_count = 0;

volatile int g_left_delta = 0;
volatile int g_right_delta = 0;

volatile int g_left_rpm_x10 = 0;
volatile int g_right_rpm_x10 = 0;

volatile int g_speed_error_x10 = 0;
volatile int g_position_error = 0;

/* ================= THONG SO ================= */

/*
 * Sau khi test:
 * - Banh trai quay tien nhung count giam -> left_sign = -1
 * - Banh phai quay tien nhung RPM ban dau bi am -> right_sign = -1
 *
 * Muc tieu:
 * Khi xe chay tien:
 * g_left_rpm_x10 > 0
 * g_right_rpm_x10 > 0
 */
int left_sign = -1;
int right_sign = -1;

/*
 * Encoder GA25:
 * Vi du: 11 PPR, hop so 1:30, doc quadrature x4
 * PPR = 11 * 30 * 4 = 1320
 */
int encoder_ppr = 1320;

/*
 * Gioi han delta bat thuong.
 * Neu trong 10 ms ma delta vuot qua nguong nay
 * thi xem nhu xung loi/nhieu.
 */
int delta_limit = 300;

/* ================= KHAI BAO HAM ================= */

void GPIO_Motor_Init(void);
void TIM3_PWM_Init(void);
void Encoder_TIM2_Init(void);
void Encoder_TIM4_Init(void);
void TIM5_10ms_Init(void);

void Motor_Forward(void);
void Motor_SetPWM(int pwm);
void Motor_Stop(void);

/* ================= MAIN ================= */

int main(void)
{
    GPIO_Motor_Init();
    TIM3_PWM_Init();

    Encoder_TIM2_Init();
    Encoder_TIM4_Init();

    TIM5_10ms_Init();

    Motor_Forward();

    uint16_t left_now = 0;
    uint16_t left_old = 0;

    uint16_t right_now = 0;
    uint16_t right_old = 0;

    int left_total = 0;
    int right_total = 0;

    int sample_count = 0;

    while (1)
    {
        /*
         * Polling TIM5.
         * Moi khi TIM5 tran -> duoc 10 ms.
         * UIF la bit 0 cua TIM5->SR.
         */
        if (TIM5->SR & (1 << 0))
        {
            /*
             * Xoa co tran UIF.
             */
            TIM5->SR &= ~(1 << 0);

            /*
             * Quet PWM moi 5 giay.
             * 1 lan lay mau = 10 ms
             * 500 lan = 5 giay
             */
            sample_count++;

            if (sample_count < 5000)
            {
                Motor_SetPWM(500);
            }
//            else if (sample_count < 1000)
//            {
//                Motor_SetPWM(400);
//            }
//            else if (sample_count < 1500)
//            {
//                Motor_SetPWM(500);
//            }
//            else if (sample_count < 2000)
//            {
//                Motor_SetPWM(600);
//            }
//            else if (sample_count < 2500)
//            {
//                Motor_SetPWM(700);
//            }
            else
            {
                sample_count = 0;

                /*
                 * Reset tong xung de de quan sat lai position_error.
                 */
                left_total = 0;
                right_total = 0;
            }

            /* ========== DOC ENCODER ========== */

            /*
             * Doc truc tiep CNT dang unsigned 16-bit.
             */
            left_now = TIM2->CNT;
            right_now = TIM4->CNT;

            /*
             * Tinh delta co xu ly tran timer.
             *
             * Vi TIM2/TIM4 dang ARR = 0xFFFF,
             * phep tru uint16_t sau do ep ve int16_t se xu ly duoc tran:
             * 65535 -> 0 hoac 0 -> 65535.
             */
            g_left_delta = left_sign * (int16_t)(left_now - left_old);
            g_right_delta = right_sign * (int16_t)(right_now - right_old);

            left_old = left_now;
            right_old = right_now;

            /*
             * Loc delta bat thuong.
             * Neu gia tri qua lon thi gan ve 0 de tranh RPM nhay vot.
             */
            if (g_left_delta > delta_limit || g_left_delta < -delta_limit)
            {
                g_left_delta = 0;
            }

            if (g_right_delta > delta_limit || g_right_delta < -delta_limit)
            {
                g_right_delta = 0;
            }

            /*
             * Tinh tong xung.
             */
            left_total += g_left_delta;
            right_total += g_right_delta;

            g_left_count = left_total;
            g_right_count = right_total;

            /*
             * Tinh RPM x10.
             *
             * Ts = 10 ms = 0.01 s
             *
             * RPM = delta * 60 / (PPR * 0.01)
             * RPM = delta * 6000 / PPR
             *
             * RPM x10 = delta * 60000 / PPR
             */
            g_left_rpm_x10 = g_left_delta * 60000 / encoder_ppr;
            g_right_rpm_x10 = g_right_delta * 60000 / encoder_ppr;

            /*
             * Sai lech toc do.
             * > 0: banh trai nhanh hon
             * < 0: banh phai nhanh hon
             */
            g_speed_error_x10 = g_left_rpm_x10 - g_right_rpm_x10;

            /*
             * Sai lech vi tri/quang duong.
             * > 0: banh trai di nhieu xung hon
             * < 0: banh phai di nhieu xung hon
             */
            g_position_error = g_left_count - g_right_count;
        }
    }
}

/* ================= GPIO MOTOR ================= */

void GPIO_Motor_Init(void)
{
    /*
     * PA6 = TIM3_CH1 = ENA
     * PA7 = TIM3_CH2 = ENB
     *
     * PC0 = IN1
     * PC1 = IN2
     * PC2 = IN3
     * PC3 = IN4
     */

    RCC->AHB1ENR |= (1 << 0);   // GPIOA clock
    RCC->AHB1ENR |= (1 << 2);   // GPIOC clock

    /*
     * PC0, PC1, PC2, PC3 output.
     * MODER = 01.
     */
    GPIOC->MODER &= ~((3 << 0) | (3 << 2) | (3 << 4) | (3 << 6));
    GPIOC->MODER |=  ((1 << 0) | (1 << 2) | (1 << 4) | (1 << 6));

    /*
     * PA6, PA7 alternate function.
     * MODER = 10.
     */
    GPIOA->MODER &= ~((3 << 12) | (3 << 14));
    GPIOA->MODER |=  ((2 << 12) | (2 << 14));

    /*
     * GPIOA_AFRL:
     * PA6 nam trong AFRL bit 27:24.
     * PA7 nam trong AFRL bit 31:28.
     *
     * AF2 = TIM3.
     */
    GPIOA->AFR[0] &= ~((15 << 24) | (15 << 28));
    GPIOA->AFR[0] |=  ((2 << 24) | (2 << 28));
}

/* ================= PWM TIM3 ================= */

void TIM3_PWM_Init(void)
{
    /*
     * TIM3 nam tren APB1.
     */
    RCC->APB1ENR |= (1 << 1);   // TIM3 clock

    /*
     * Clock = 16 MHz
     * PSC = 16 - 1 -> timer dem 1 MHz
     * ARR = 1000 - 1 -> PWM = 1 kHz
     */
    TIM3->PSC = 16 - 1;
    TIM3->ARR = 1000 - 1;

    TIM3->CCR1 = 0;
    TIM3->CCR2 = 0;

    /*
     * CH1 PWM mode 1.
     * OC1M = 110 tai bit 6:4.
     */
    TIM3->CCMR1 &= ~(7 << 4);
    TIM3->CCMR1 |=  (6 << 4);

    /*
     * CH2 PWM mode 1.
     * OC2M = 110 tai bit 14:12.
     */
    TIM3->CCMR1 &= ~(7 << 12);
    TIM3->CCMR1 |=  (6 << 12);

    /*
     * Enable output CH1 va CH2.
     */
    TIM3->CCER |= (1 << 0);     // CC1E
    TIM3->CCER |= (1 << 4);     // CC2E

    /*
     * Bat timer.
     */
    TIM3->CR1 |= (1 << 0);      // CEN
}

/* ================= ENCODER TRAI TIM2 ================= */

void Encoder_TIM2_Init(void)
{
    /*
     * Encoder trai:
     * PA0 = TIM2_CH1
     * PA1 = TIM2_CH2
     */

    RCC->AHB1ENR |= (1 << 0);   // GPIOA clock
    RCC->APB1ENR |= (1 << 0);   // TIM2 clock

    /*
     * PA0, PA1 alternate function.
     * MODER = 10.
     */
    GPIOA->MODER &= ~((3 << 0) | (3 << 2));
    GPIOA->MODER |=  ((2 << 0) | (2 << 2));

    /*
     * GPIOA_AFRL:
     * PA0 nam trong AFRL bit 3:0.
     * PA1 nam trong AFRL bit 7:4.
     *
     * AF1 = TIM2.
     */
    GPIOA->AFR[0] &= ~((15 << 0) | (15 << 4));
    GPIOA->AFR[0] |=  ((1 << 0) | (1 << 4));

    /*
     * Pull-up PA0, PA1.
     */
    GPIOA->PUPDR &= ~((3 << 0) | (3 << 2));
    GPIOA->PUPDR |=  ((1 << 0) | (1 << 2));

    /*
     * Encoder mode 3.
     * SMS = 011.
     */
    TIM2->SMCR |= (3 << 0);

    /*
     * CH1 input TI1, CH2 input TI2.
     * CC1S = 01, CC2S = 01.
     */
    TIM2->CCMR1 |= (1 << 0);
    TIM2->CCMR1 |= (1 << 8);

    /*
     * Input filter nhe.
     * IC1F = 0011, IC2F = 0011.
     */
    TIM2->CCMR1 |= (3 << 4);
    TIM2->CCMR1 |= (3 << 12);

    TIM2->ARR = 0xFFFF;
    TIM2->CNT = 0;

    /*
     * Bat timer.
     */
    TIM2->CR1 |= (1 << 0);
}

/* ================= ENCODER PHAI TIM4 ================= */

void Encoder_TIM4_Init(void)
{
    /*
     * Encoder phai:
     * PB6 = TIM4_CH1
     * PB7 = TIM4_CH2
     */

    RCC->AHB1ENR |= (1 << 1);   // GPIOB clock
    RCC->APB1ENR |= (1 << 2);   // TIM4 clock

    /*
     * PB6, PB7 alternate function.
     * MODER = 10.
     */
    GPIOB->MODER &= ~((3 << 12) | (3 << 14));
    GPIOB->MODER |=  ((2 << 12) | (2 << 14));

    /*
     * GPIOB_AFRL:
     * PB6 nam trong AFRL bit 27:24.
     * PB7 nam trong AFRL bit 31:28.
     *
     * AF2 = TIM4.
     */
    GPIOB->AFR[0] &= ~((15 << 24) | (15 << 28));
    GPIOB->AFR[0] |=  ((2 << 24) | (2 << 28));

    /*
     * Pull-up PB6, PB7.
     */
    GPIOB->PUPDR &= ~((3 << 12) | (3 << 14));
    GPIOB->PUPDR |=  ((1 << 12) | (1 << 14));

    /*
     * Encoder mode 3.
     */
    TIM4->SMCR |= (3 << 0);

    /*
     * CH1 input TI1, CH2 input TI2.
     */
    TIM4->CCMR1 |= (1 << 0);
    TIM4->CCMR1 |= (1 << 8);

    /*
     * Input filter nhe.
     */
    TIM4->CCMR1 |= (3 << 4);
    TIM4->CCMR1 |= (3 << 12);

    TIM4->ARR = 0xFFFF;
    TIM4->CNT = 0;

    /*
     * Bat timer.
     */
    TIM4->CR1 |= (1 << 0);
}

/* ================= TIMER 10 ms ================= */

void TIM5_10ms_Init(void)
{
    /*
     * TIM5 tao chu ky lay mau 10 ms bang polling UIF.
     */

    RCC->APB1ENR |= (1 << 3);   // TIM5 clock

    /*
     * Clock = 16 MHz
     * PSC = 1600 - 1 -> 10 kHz
     * ARR = 100 - 1 -> 10 ms
     */
    TIM5->PSC = 1600 - 1;
    TIM5->ARR = 100 - 1;

    TIM5->CNT = 0;

    /*
     * Bat timer.
     */
    TIM5->CR1 |= (1 << 0);
}

/* ================= MOTOR ================= */

void Motor_Forward(void)
{
    /*
     * Motor trai:
     * IN1 = 1, IN2 = 0
     *
     * Motor phai:
     * IN3 = 1, IN4 = 0
     */

    GPIOC->BSRR = (1 << 0);          // PC0 = 1
    GPIOC->BSRR = (1 << (1 + 16));   // PC1 = 0

    GPIOC->BSRR = (1 << 2);          // PC2 = 1
    GPIOC->BSRR = (1 << (3 + 16));   // PC3 = 0
}

void Motor_SetPWM(int pwm)
{
    if (pwm < 0)
    {
        pwm = 0;
    }

    if (pwm > 999)
    {
        pwm = 999;
    }

    /*
     * Cung PWM cho hai banh de test sai lech tu nhien.
     */
    TIM3->CCR1 = pwm;
    TIM3->CCR2 = pwm;

    g_pwm = pwm;
}

void Motor_Stop(void)
{
    /*
     * IN1 = IN2 = IN3 = IN4 = 0.
     */
    GPIOC->BSRR = (1 << (0 + 16));
    GPIOC->BSRR = (1 << (1 + 16));
    GPIOC->BSRR = (1 << (2 + 16));
    GPIOC->BSRR = (1 << (3 + 16));

    TIM3->CCR1 = 0;
    TIM3->CCR2 = 0;

    g_pwm = 0;
}
