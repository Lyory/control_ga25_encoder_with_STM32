#include "stm32f401xe.h"

#define PWM_MAX 999

volatile int pwm_left_trace = 0;
volatile int pwm_right_trace = 0;
volatile int duty_left_percent = 0;
volatile int duty_right_percent = 0;

void Motor_Init(void);
void Motor_Forward(void);
void Motor_SetPWM(int left_pwm, int right_pwm);
void delay_ms(int ms);

int main(void)
{
    Motor_Init();
    Motor_Forward();

    while (1)
    {
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

void Motor_Init(void)
{
    RCC->AHB1ENR |= (1 << 0);   // GPIOA clock
    RCC->AHB1ENR |= (1 << 2);   // GPIOC clock
    RCC->APB1ENR |= (1 << 1);   // TIM3 clock

    // PC0, PC1, PC2, PC3 output
    GPIOC->MODER &= ~((3 << 0) | (3 << 2) | (3 << 4) | (3 << 6));
    GPIOC->MODER |=  ((1 << 0) | (1 << 2) | (1 << 4) | (1 << 6));

    // PA6, PA7 alternate function
    GPIOA->MODER &= ~((3 << 12) | (3 << 14));
    GPIOA->MODER |=  ((2 << 12) | (2 << 14));

    // PA6, PA7 = AF2 = TIM3
    GPIOA->AFR[0] &= ~((15 << 24) | (15 << 28));
    GPIOA->AFR[0] |=  ((2 << 24) | (2 << 28));

    // TIM3 PWM 1 kHz
    TIM3->PSC = 16 - 1;
    TIM3->ARR = PWM_MAX;

    TIM3->CCR1 = 0;
    TIM3->CCR2 = 0;

    // CH1, CH2 PWM mode 1 + preload
    TIM3->CCMR1 &= ~((7 << 4) | (7 << 12));
    TIM3->CCMR1 |=  ((6 << 4) | (1 << 3));
    TIM3->CCMR1 |=  ((6 << 12) | (1 << 11));

    // Enable CH1, CH2 output
    TIM3->CCER |= (1 << 0) | (1 << 4);

    // Enable ARR preload and counter
    TIM3->CR1 |= (1 << 7) | (1 << 0);
}

void Motor_Forward(void)
{
    GPIOC->BSRR = (1 << 0);        // PC0 = 1
    GPIOC->BSRR = (1 << (1 + 16)); // PC1 = 0
    GPIOC->BSRR = (1 << 2);        // PC2 = 1
    GPIOC->BSRR = (1 << (3 + 16)); // PC3 = 0
}

void Motor_SetPWM(int left_pwm, int right_pwm)
{
    if (left_pwm < 0) left_pwm = 0;
    if (left_pwm > PWM_MAX) left_pwm = PWM_MAX;

    if (right_pwm < 0) right_pwm = 0;
    if (right_pwm > PWM_MAX) right_pwm = PWM_MAX;

    TIM3->CCR1 = left_pwm;
    TIM3->CCR2 = right_pwm;

    pwm_left_trace = left_pwm;
    pwm_right_trace = right_pwm;

    duty_left_percent = (left_pwm * 100) / 1000;
    duty_right_percent = (right_pwm * 100) / 1000;
}

void delay_ms(int ms)
{
    for (int i = 0; i < ms; i++)
    {
        for (volatile int j = 0; j < 1600; j++)
        {
            __NOP();
        }
    }
}
