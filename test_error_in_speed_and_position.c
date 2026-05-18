#include "stm32f401xe.h"
#include <stdint.h>

/* ================= BIEN XEM TREN SWV ================= */

volatile int g_pwm_base = 500;
volatile int g_pwm_left = 0;
volatile int g_pwm_right = 0;
volatile int g_pwm_comp = 0;

volatile int g_left_count = 0;
volatile int g_right_count = 0;

volatile int g_left_delta = 0;
volatile int g_right_delta = 0;

volatile int g_left_rpm_x10 = 0;
volatile int g_right_rpm_x10 = 0;

volatile int g_speed_error = 0;
volatile int g_position_error = 0;
volatile int g_total_error = 0;

volatile int g_sample_count = 0;

/* ================= THONG SO ================= */

#define ENCODER_PPR       1320
#define DELTA_LIMIT       300

#define PWM_MIN           0
#define PWM_MAX           999

/*
    PWM co dinh de tuning.
    Khong quet PWM trong giai doan toi uu.
*/
#define PWM_BASE          500

/*
    Tham so bu PWM.

    KP_POS: sua sai so vi tri tich luy
    KP_SPEED: sua sai so toc do tuc thoi

    Neu position_error van tang nhanh:
    - tang KP_POS
    - hoac giam POSITION_DIV

    Neu PWM dao dong manh:
    - giam KP_SPEED
    - tang POSITION_DIV
*/
#define KP_POS            1
#define POSITION_DIV      20

#define KP_SPEED          3

#define PWM_COMP_LIMIT    300

/*
    Chinh dau encoder.
    Khi xe chay tien:
    g_left_delta va g_right_delta phai duong.
*/
int left_sign = 1;
int right_sign = 1;

/* ================= BIEN NOI BO ================= */

volatile uint16_t left_now = 0;
volatile uint16_t left_old = 0;

volatile uint16_t right_now = 0;
volatile uint16_t right_old = 0;

volatile int left_total = 0;
volatile int right_total = 0;

/* ================= KHAI BAO HAM ================= */

void GPIO_Motor_Init(void);
void TIM3_PWM_Init(void);
void Encoder_TIM2_Init(void);
void Encoder_TIM4_Init(void);
void TIM5_10ms_Init(void);

void Motor_Forward(void);
void Motor_SetPWM_Separate(int pwm_left, int pwm_right);
void Motor_Stop(void);

int limit_value(int value, int min, int max);

/* ================= MAIN ================= */

int main(void)
{
    GPIO_Motor_Init();
    TIM3_PWM_Init();

    Encoder_TIM2_Init();
    Encoder_TIM4_Init();

    TIM5_10ms_Init();

    Motor_Forward();

    g_pwm_base = PWM_BASE;
    Motor_SetPWM_Separate(g_pwm_base, g_pwm_base);

    while (1)
    {
        /*
            Xem tren SWV:
            g_pwm_left
            g_pwm_right
            g_pwm_comp
            g_left_delta
            g_right_delta
            g_speed_error
            g_position_error
            g_total_error
        */
    }
}

/* ================= GPIO MOTOR ================= */

void GPIO_Motor_Init(void)
{
    /*
        PA6 = TIM3_CH1 = PWM banh trai
        PA7 = TIM3_CH2 = PWM banh phai

        PC0 = IN1
        PC1 = IN2
        PC2 = IN3
        PC3 = IN4
    */

    RCC->AHB1ENR |= (1 << 0);   // GPIOA clock
    RCC->AHB1ENR |= (1 << 2);   // GPIOC clock

    // PC0, PC1, PC2, PC3 output
    GPIOC->MODER &= ~((3 << 0) | (3 << 2) | (3 << 4) | (3 << 6));
    GPIOC->MODER |=  ((1 << 0) | (1 << 2) | (1 << 4) | (1 << 6));

    // PA6, PA7 alternate function
    GPIOA->MODER &= ~((3 << 12) | (3 << 14));
    GPIOA->MODER |=  ((2 << 12) | (2 << 14));

    // PA6, PA7 = AF2 = TIM3
    GPIOA->AFR[0] &= ~((15 << 24) | (15 << 28));
    GPIOA->AFR[0] |=  ((2 << 24) | (2 << 28));
}

/* ================= PWM TIM3 ================= */

void TIM3_PWM_Init(void)
{
    RCC->APB1ENR |= (1 << 1);   // TIM3 clock

    /*
        Clock = 16 MHz
        PSC = 16 - 1      -> timer tick = 1 MHz
        ARR = 1000 - 1    -> PWM = 1 kHz
    */
    TIM3->PSC = 16 - 1;
    TIM3->ARR = 1000 - 1;

    TIM3->CCR1 = 0;
    TIM3->CCR2 = 0;

    // CH1, CH2 PWM mode 1
    TIM3->CCMR1 &= ~((7 << 4) | (7 << 12));
    TIM3->CCMR1 |=  ((6 << 4) | (6 << 12));

    // Preload CCR1, CCR2
    TIM3->CCMR1 |= (1 << 3);    // OC1PE
    TIM3->CCMR1 |= (1 << 11);   // OC2PE

    // Enable output CH1, CH2
    TIM3->CCER |= (1 << 0) | (1 << 4);

    // Auto-reload preload
    TIM3->CR1 |= (1 << 7);      // ARPE

    // Update event
    TIM3->EGR |= (1 << 0);

    // Bat timer
    TIM3->CR1 |= (1 << 0);      // CEN
}

/* ================= ENCODER TRAI TIM2 ================= */

void Encoder_TIM2_Init(void)
{
    RCC->AHB1ENR |= (1 << 0);   // GPIOA clock
    RCC->APB1ENR |= (1 << 0);   // TIM2 clock

    // PA0, PA1 alternate function
    GPIOA->MODER &= ~((3 << 0) | (3 << 2));
    GPIOA->MODER |=  ((2 << 0) | (2 << 2));

    // PA0, PA1 = AF1 = TIM2
    GPIOA->AFR[0] &= ~((15 << 0) | (15 << 4));
    GPIOA->AFR[0] |=  ((1 << 0) | (1 << 4));

    // Pull-up PA0, PA1
    GPIOA->PUPDR &= ~((3 << 0) | (3 << 2));
    GPIOA->PUPDR |=  ((1 << 0) | (1 << 2));

    TIM2->CR1 = 0;
    TIM2->SMCR = 0;
    TIM2->CCMR1 = 0;
    TIM2->CCER = 0;

    // Encoder mode 3
    TIM2->SMCR |= (3 << 0);

    // CH1 input TI1, CH2 input TI2
    TIM2->CCMR1 |= (1 << 0) | (1 << 8);

    // Input filter nhe
    TIM2->CCMR1 |= (3 << 4) | (3 << 12);

    TIM2->ARR = 0xFFFF;
    TIM2->CNT = 0;

    TIM2->CR1 |= (1 << 0);      // CEN
}

/* ================= ENCODER PHAI TIM4 ================= */

void Encoder_TIM4_Init(void)
{
    RCC->AHB1ENR |= (1 << 1);   // GPIOB clock
    RCC->APB1ENR |= (1 << 2);   // TIM4 clock

    // PB6, PB7 alternate function
    GPIOB->MODER &= ~((3 << 12) | (3 << 14));
    GPIOB->MODER |=  ((2 << 12) | (2 << 14));

    // PB6, PB7 = AF2 = TIM4
    GPIOB->AFR[0] &= ~((15 << 24) | (15 << 28));
    GPIOB->AFR[0] |=  ((2 << 24) | (2 << 28));

    // Pull-up PB6, PB7
    GPIOB->PUPDR &= ~((3 << 12) | (3 << 14));
    GPIOB->PUPDR |=  ((1 << 12) | (1 << 14));

    TIM4->CR1 = 0;
    TIM4->SMCR = 0;
    TIM4->CCMR1 = 0;
    TIM4->CCER = 0;

    // Encoder mode 3
    TIM4->SMCR |= (3 << 0);

    // CH1 input TI1, CH2 input TI2
    TIM4->CCMR1 |= (1 << 0) | (1 << 8);

    // Input filter nhe
    TIM4->CCMR1 |= (3 << 4) | (3 << 12);

    TIM4->ARR = 0xFFFF;
    TIM4->CNT = 0;

    TIM4->CR1 |= (1 << 0);      // CEN
}

/* ================= TIMER 5 LAY MAU 10 ms ================= */

void TIM5_10ms_Init(void)
{
    RCC->APB1ENR |= (1 << 3);   // TIM5 clock

    /*
        Clock = 16 MHz
        PSC = 1600 - 1 -> 10 kHz
        ARR = 100 - 1  -> 10 ms
    */
    TIM5->PSC = 1600 - 1;
    TIM5->ARR = 100 - 1;

    TIM5->CNT = 0;

    // Cho phep update interrupt
    TIM5->DIER |= (1 << 0);

    // Bat ngat TIM5 trong NVIC
    NVIC_EnableIRQ(TIM5_IRQn);

    // Bat timer
    TIM5->CR1 |= (1 << 0);
}

/* ================= NGAT TIM5 MOI 10 ms ================= */

void TIM5_IRQHandler(void)
{
    if (TIM5->SR & (1 << 0))
    {
        // Xoa co ngat UIF
        TIM5->SR &= ~(1 << 0);

        g_sample_count++;

        /* ===== DOC ENCODER ===== */

        left_now = TIM2->CNT;
        right_now = TIM4->CNT;

        /*
            Delta trong 10 ms.
            Ep int16_t giup xu ly tran 16-bit.
        */
        g_left_delta = left_sign * (int16_t)(left_now - left_old);
        g_right_delta = right_sign * (int16_t)(right_now - right_old);

        left_old = left_now;
        right_old = right_now;

        /*
            Loc delta bat thuong.
        */
        if (g_left_delta > DELTA_LIMIT || g_left_delta < -DELTA_LIMIT)
        {
            g_left_delta = 0;
        }

        if (g_right_delta > DELTA_LIMIT || g_right_delta < -DELTA_LIMIT)
        {
            g_right_delta = 0;
        }

        /* ===== TINH SAI SO ===== */

        left_total += g_left_delta;
        right_total += g_right_delta;

        g_left_count = left_total;
        g_right_count = right_total;

        /*
            Speed error:
            > 0: banh trai nhanh hon banh phai
            < 0: banh phai nhanh hon banh trai
        */
        g_speed_error = g_left_delta - g_right_delta;

        /*
            Position error:
            > 0: banh trai di nhieu xung hon
            < 0: banh phai di nhieu xung hon
        */
        g_position_error = g_left_count - g_right_count;

        /*
            RPM_x10 = delta * 60000 / PPR
        */
        g_left_rpm_x10 = g_left_delta * 60000 / ENCODER_PPR;
        g_right_rpm_x10 = g_right_delta * 60000 / ENCODER_PPR;

        /*
            Total error:
            - Thanh phan position_error giup sua sai so tich luy.
            - Thanh phan speed_error giup phan ung nhanh.
        */
        g_total_error = (KP_POS * (g_position_error / POSITION_DIV))
                      + (KP_SPEED * g_speed_error);

        /*
            Gioi han bu PWM.
        */
        g_pwm_comp = limit_value(g_total_error, -PWM_COMP_LIMIT, PWM_COMP_LIMIT);

        /*
            Neu error > 0:
            banh trai nhanh/di nhieu hon
            => giam PWM trai, tang PWM phai.
        */
        g_pwm_left = g_pwm_base - g_pwm_comp;
        g_pwm_right = g_pwm_base + g_pwm_comp;

        g_pwm_left = limit_value(g_pwm_left, PWM_MIN, PWM_MAX);
        g_pwm_right = limit_value(g_pwm_right, PWM_MIN, PWM_MAX);

        Motor_SetPWM_Separate(g_pwm_left, g_pwm_right);
    }
}

/* ================= MOTOR ================= */

void Motor_Forward(void)
{
    /*
        Motor trai:
        IN1 = 1, IN2 = 0

        Motor phai:
        IN3 = 1, IN4 = 0
    */

    GPIOC->BSRR = (1 << 0);          // PC0 = 1
    GPIOC->BSRR = (1 << (1 + 16));   // PC1 = 0

    GPIOC->BSRR = (1 << 2);          // PC2 = 1
    GPIOC->BSRR = (1 << (3 + 16));   // PC3 = 0
}

void Motor_SetPWM_Separate(int pwm_left, int pwm_right)
{
    pwm_left = limit_value(pwm_left, PWM_MIN, PWM_MAX);
    pwm_right = limit_value(pwm_right, PWM_MIN, PWM_MAX);

    TIM3->CCR1 = pwm_left;
    TIM3->CCR2 = pwm_right;

    g_pwm_left = pwm_left;
    g_pwm_right = pwm_right;
}

void Motor_Stop(void)
{
    GPIOC->BSRR = (1 << (0 + 16));
    GPIOC->BSRR = (1 << (1 + 16));
    GPIOC->BSRR = (1 << (2 + 16));
    GPIOC->BSRR = (1 << (3 + 16));

    TIM3->CCR1 = 0;
    TIM3->CCR2 = 0;

    g_pwm_left = 0;
    g_pwm_right = 0;
}

/* ================= HAM GIOI HAN ================= */

int limit_value(int value, int min, int max)
{
    if (value < min)
    {
        value = min;
    }

    if (value > max)
    {
        value = max;
    }

    return value;
}
