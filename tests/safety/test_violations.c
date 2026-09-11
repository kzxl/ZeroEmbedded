#include "zero/zero.h"
#include <stdlib.h>

extern void delay_ms(uint32_t ms);

/* INTENTIONAL VIOLATION 1: malloc inside ISR context */
FW_ISR void USART1_IRQHandler(void) {
    void *data = malloc(64);
    (void)data;
}

/* INTENTIONAL VIOLATION 2: blocking delay inside ISR context */
FW_ISR void SysTick_Handler(void) {
    delay_ms(10);
}

/* INTENTIONAL VIOLATION 3: mutex lock inside ISR context (Deadlock) */
FW_ISR void EXTI0_IRQHandler(void) {
    fw_mutex_lock(FW_NULL, 100);
}

/* INTENTIONAL VIOLATION 4: non-reentrant standard I/O inside ISR context */
FW_ISR void DMA1_Channel1_IRQHandler(void) {
    printf("DMA transfer complete\n");
}
