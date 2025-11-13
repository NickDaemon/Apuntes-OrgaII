/* ** por compatibilidad se omiten tildes **
==============================================================================
TALLER System Programming - Arquitectura y Organizacion de Computadoras - FCEN
==============================================================================

  Definicion de funciones de impresion por pantalla.
*/

#include "screen.h"

void print(const char* text, uint32_t x, uint32_t y, uint16_t attr) {
  ca(*p)[VIDEO_COLS] = (ca(*)[VIDEO_COLS])VIDEO; 
  int32_t i;
  for (i = 0; text[i] != 0; i++) {
    p[y][x].c = (uint8_t)text[i];
    p[y][x].a = (uint8_t)attr;
    x++;
    if (x == VIDEO_COLS) {
      x = 0;
      y++;
    }
  }
}

void print_dec(uint32_t numero, uint32_t size, uint32_t x, uint32_t y,
               uint16_t attr) {
  ca(*p)[VIDEO_COLS] = (ca(*)[VIDEO_COLS])VIDEO; 
  uint32_t i;
  uint8_t letras[16] = "0123456789";

  for (i = 0; i < size; i++) {
    uint32_t resto = numero % 10;
    numero = numero / 10;
    p[y][x + size - i - 1].c = letras[resto];
    p[y][x + size - i - 1].a = attr;
  }
}

void print_hex(uint32_t numero, int32_t size, uint32_t x, uint32_t y,
               uint16_t attr) {
  ca(*p)[VIDEO_COLS] = (ca(*)[VIDEO_COLS])VIDEO; 
  int32_t i;
  uint8_t hexa[8];
  uint8_t letras[16] = "0123456789ABCDEF";
  hexa[0] = letras[(numero & 0x0000000F) >> 0];
  hexa[1] = letras[(numero & 0x000000F0) >> 4];
  hexa[2] = letras[(numero & 0x00000F00) >> 8];
  hexa[3] = letras[(numero & 0x0000F000) >> 12];
  hexa[4] = letras[(numero & 0x000F0000) >> 16];
  hexa[5] = letras[(numero & 0x00F00000) >> 20];
  hexa[6] = letras[(numero & 0x0F000000) >> 24];
  hexa[7] = letras[(numero & 0xF0000000) >> 28];
  for (i = 0; i < size; i++) {
    p[y][x + size - i - 1].c = hexa[i];
    p[y][x + size - i - 1].a = attr;
  }
}

void screen_draw_box(uint32_t fInit, uint32_t cInit, uint32_t fSize,
                     uint32_t cSize, uint8_t character, uint8_t attr) {
  ca(*p)[VIDEO_COLS] = (ca(*)[VIDEO_COLS])VIDEO;
  uint32_t f;
  uint32_t c;
  for (f = fInit; f < fInit + fSize; f++) {
    for (c = cInit; c < cInit + cSize; c++) {
      p[f][c].c = character;
      p[f][c].a = attr;
    }
  }
}

void screen_draw_layout(void) {
  // Cargamos la pantalla de video en p
  ca(*p)[VIDEO_COLS] = (ca(*)[VIDEO_COLS])VIDEO;

  // Limpiamos la pantalla
  screen_draw_box(0, 0, 50, 80, " ", C_FG_BLACK);

  // Nombre del grupo
  char *grupo = "Grupo: NMA";
  uint32_t len_grupo = 10;

  //Desde donde vamos a dibujar
  uint32_t fila_a_dibujar = 15;
  uint32_t col_a_dibujar = (80 - len_grupo) / 2;

  // Dibujamos nombre del grupo
  for (uint32_t i = 0; i < len_grupo; i++) {
    p[fila_a_dibujar][col_a_dibujar + i].c = grupo[i];
    p[fila_a_dibujar][col_a_dibujar + i].a = C_BG_CYAN;
  }

  // Integrantes 
  char *integrante1 = "Integrante 1: Nicolas Proz";
  char *integrante2 = "Integrante 2: Juanma Zimmermann";
  char *integrante3 = "Integrante 3: Martin Gustavo Velazquez";

  // Fila mas abajo del nombre de grupo para los integrantes
  uint32_t fila_inicio = fila_a_dibujar + 2;

  // Columna empieza un poco antes para que se vea mas alineado
  uint32_t col_inicio = col_a_dibujar - 14;

  char *integrantes[] = {integrante1, integrante2, integrante3};

  // Dibujamos integrantes
  for (uint32_t f = 0; f < 3; f++) {

    // calculo la longitud del nombre
    uint32_t len = 0;
    while (integrantes[f][len] != '\0') {
      len++;
    }
    for (uint32_t c = 0; c < len; c++) {
      p[fila_inicio + f][col_inicio + c].c = integrantes[f][c];
      p[fila_inicio + f][col_inicio + c].a = C_BG_GREEN;
    }
  }
}


