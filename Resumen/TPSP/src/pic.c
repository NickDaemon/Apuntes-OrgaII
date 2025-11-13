/* ** por compatibilidad se omiten tildes **
================================================================================
 TALLER System Programming - ORGANIZACION DE COMPUTADOR II - FCEN
================================================================================

  Rutinas del controlador de interrupciones.
*/
#include "pic.h"

#define PIC1_PORT 0x20
#define PIC2_PORT 0xA0

static __inline __attribute__((always_inline)) void outb(uint32_t port,
                                                         uint8_t data) {
  __asm __volatile("outb %0,%w1" : : "a"(data), "d"(port));
}
void pic_finish1(void) { outb(PIC1_PORT, 0x20); }
void pic_finish2(void) {
  outb(PIC1_PORT, 0x20);
  outb(PIC2_PORT, 0x20);
}

// COMPLETAR: implementar pic_reset()
void pic_reset() {
  // ICW1 - INICIALIZAR PIC: vamos a usar ICW4 Y EN CASCADA
  outb(PIC1_PORT, 0x11);
  outb(PIC2_PORT, 0x11);

  // ICW2
  outb(PIC1_PORT + 1, 0x20); // Remappear a 32
  outb(PIC2_PORT + 1, 0x28); // Remappear a 40

  // ICW3
  outb(PIC1_PORT + 1, 0x4); // Donde conecto PIC2 (isqr2)
  outb(PIC2_PORT + 1, 0x2); // A donde mando a PIC1 (master)
  
  // ICW4 - Estamos en x86
  outb(PIC1_PORT + 1,0x1);
  outb(PIC2_PORT + 1,0x1);
  
  pic_disable(); // No queremos interrupciones
}

void pic_enable() {
  outb(PIC1_PORT + 1, 0x00);
  outb(PIC2_PORT + 1, 0x00);
}

void pic_disable() {
  outb(PIC1_PORT + 1, 0xFF);
  outb(PIC2_PORT + 1, 0xFF);
}
