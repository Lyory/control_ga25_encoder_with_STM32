#include "stm32f401xe.h"

void GPIO_Motor_Init(void);
void TIM3_PWM_Init(void);
void Motor_Left_Forward(void);
void Motor_Right_Forward(void);
void Motor_Stop(void);
void Motor_SetPWM(int left_pwm, int right_pwm);
void delay_ms(int ms);

/*
 * Các biến này dùng để hiển thị trong SWV Data Trace Timeline Graph.
 * Phải để global và volatile để trình biên dịch không tối ưu mất.
 */
volatile int pwm_left_trace = 0;
volatile int pwm_right_trace = 0;

volatile int duty_left_percent = 0;
volatile int duty_right_percent = 0;

int main(void)
{
    GPIO_Motor_Init();
    TIM3_PWM_Init();

    Motor_Left_Forward();
    Motor_Right_Forward();

    while (1)
    {
        /*
         * Test tăng giảm PWM.
         * Trong SWV graph, bạn theo dõi:
         * pwm_left_trace
         * pwm_right_trace
         * duty_left_percent
         * duty_right_percent
         */

        Motor_SetPWM(200, 200);
        delay_ms(2000);

        Motor_SetPWM(400, 400);
        delay_ms(2000);

        Motor_SetPWM(600, 600);
        delay_ms(2000);

        Motor_SetPWM(800, 800);
        delay_ms(2000);
    }
}

void GPIO_Motor_Init(void)
{
    /*
     * Bật clock GPIOA và GPIOC.
     */
    RCC->AHB1ENR |= (1 << 0);   // GPIOA clock enable
    RCC->AHB1ENR |= (1 << 2);   // GPIOC clock enable

    /*
     * PC0, PC1, PC2, PC3 là output.
     */
    GPIOC->MODER &= ~((3 << (0 * 2)) |
                      (3 << (1 * 2)) |
                      (3 << (2 * 2)) |
                      (3 << (3 * 2)));

    GPIOC->MODER |=  ((1 << (0 * 2)) |
                      (1 << (1 * 2)) |
                      (1 << (2 * 2)) |
                      (1 << (3 * 2)));

    /*
     * PA6, PA7 là Alternate Function để xuất PWM TIM3.
     */
    GPIOA->MODER &= ~((3 << (6 * 2)) |
                      (3 << (7 * 2)));

    GPIOA->MODER |=  ((2 << (6 * 2)) |
                      (2 << (7 * 2)));

    /*
     * PA6, PA7 dùng AF2 cho TIM3.
     */
    GPIOA->AFR[0] &= ~((15 << (6 * 4)) |
                       (15 << (7 * 4)));

    GPIOA->AFR[0] |=  ((2 << (6 * 4)) |
                       (2 << (7 * 4)));
}

void TIM3_PWM_Init(void)
{
    /*
     * Bật clock TIM3.
     */
    RCC->APB1ENR |= (1 << 1);   // TIM3 clock enable

    /*
     * Clock timer giả sử 16 MHz.
     * PSC = 16 - 1 => timer đếm 1 MHz.
     */
    TIM3->PSC = 16 - 1;

    /*
     * ARR = 1000 - 1.
     * PWM frequency = 1 MHz / 1000 = 1 kHz.
     */
    TIM3->ARR = 1000 - 1;

    /*
     * Duty ban đầu = 0.
     */
    TIM3->CCR1 = 0;
    TIM3->CCR2 = 0;

    /*
     * PWM mode 1 cho Channel 1.
     * OC1M = 110, nằm ở bit 6:4.
     */
    TIM3->CCMR1 &= ~(7 << 4);
    TIM3->CCMR1 |=  (6 << 4);

    /*
     * Bật preload cho Channel 1.
     * OC1PE = bit 3.
     */
    TIM3->CCMR1 |= (1 << 3);

    /*
     * PWM mode 1 cho Channel 2.
     * OC2M = 110, nằm ở bit 14:12.
     */
    TIM3->CCMR1 &= ~(7 << 12);
    TIM3->CCMR1 |=  (6 << 12);

    /*
     * Bật preload cho Channel 2.
     * OC2PE = bit 11.
     */
    TIM3->CCMR1 |= (1 << 11);

    /*
     * Enable output cho TIM3_CH1 và TIM3_CH2.
     */
    TIM3->CCER |= (1 << 0);     // CC1E
    TIM3->CCER |= (1 << 4);     // CC2E

    /*
     * Bật auto-reload preload.
     */
    TIM3->CR1 |= (1 << 7);      // ARPE

    /*
     * Bật counter.
     */
    TIM3->CR1 |= (1 << 0);      // CEN
}

void Motor_SetPWM(int left_pwm, int right_pwm)
{
    /*
     * Giới hạn PWM từ 0 đến 999.
     */
    if (left_pwm < 0)
    {
        left_pwm = 0;
    }

    if (left_pwm > 999)
    {
        left_pwm = 999;
    }

    if (right_pwm < 0)
    {
        right_pwm = 0;
    }

    if (right_pwm > 999)
    {
        right_pwm = 999;
    }

    /*
     * Ghi vào thanh ghi CCR để đổi duty PWM.
     */
    TIM3->CCR1 = left_pwm;
    TIM3->CCR2 = right_pwm;

    /*
     * Cập nhật biến để xem trong SWV.
     */
    pwm_left_trace = left_pwm;
    pwm_right_trace = right_pwm;

    /*
     * Tính duty phần trăm.
     * Vì ARR = 999 nên chu kỳ có 1000 mức.
     */
    duty_left_percent = (left_pwm * 100) / 1000;
    duty_right_percent = (right_pwm * 100) / 1000;
}

void Motor_Left_Forward(void)
{
    /*
     * IN1 = 1, IN2 = 0
     */
    GPIOC->BSRR = (1 << 0);          // PC0 = 1
    GPIOC->BSRR = (1 << (1 + 16));   // PC1 = 0
}

void Motor_Right_Forward(void)
{
    /*
     * IN3 = 1, IN4 = 0
     */
    GPIOC->BSRR = (1 << 2);          // PC2 = 1
    GPIOC->BSRR = (1 << (3 + 16));   // PC3 = 0
}

void Motor_Stop(void)
{
    GPIOC->BSRR = (1 << (0 + 16));
    GPIOC->BSRR = (1 << (1 + 16));
    GPIOC->BSRR = (1 << (2 + 16));
    GPIOC->BSRR = (1 << (3 + 16));

    TIM3->CCR1 = 0;
    TIM3->CCR2 = 0;

    pwm_left_trace = 0;
    pwm_right_trace = 0;

    duty_left_percent = 0;
    duty_right_percent = 0;
}

void delay_ms(int ms)
{
    for (int i = 0; i < ms; i++)
    {
        for (int j = 0; j < 1600; j++)
        {
            __NOP();
        }
    }
}
