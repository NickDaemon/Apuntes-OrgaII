#include "task_lib.h"

#define WIDTH TASK_VIEWPORT_WIDTH
#define HEIGHT TASK_VIEWPORT_HEIGHT

#define SHARED_SCORE_BASE_VADDR (PAGE_ON_DEMAND_BASE_VADDR + 0xF00)
#define CANT_PONGS 3

void task(void) {
    screen pantalla;

    while (true) {

    // Título
    task_print(pantalla, "Puntajes Pong:", 10, 2, C_FG_GREEN);

    // Leer puntajes  desde la página compartida
    uint32_t *puntajes = (uint32_t*) SHARED_SCORE_BASE_VADDR;
       
    // Imprimimos las 3 partidas    
    for (int i = 0; i < CANT_PONGS; i++) {
    uint32_t p1 = puntajes[i * 2];
    uint32_t p2 = puntajes[i * 2 + 1];
    uint32_t y = 5 + i * 3;

    task_print(pantalla, "P1:", 10, y, C_FG_CYAN);
    task_print_dec(pantalla, p1, 2, 15, y, C_FG_WHITE);

    task_print(pantalla, "P2:", 20, y, C_FG_MAGENTA);
    task_print_dec(pantalla, p2, 2, 25, y, C_FG_WHITE);
    }

    syscall_draw(pantalla);
    task_sleep(100); 
    }
}