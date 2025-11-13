/* ** por compatibilidad se omiten tildes **
================================================================================
 TRABAJO PRACTICO 3 - System Programming - ORGANIZACION DE COMPUTADOR II - FCEN
================================================================================

  Definicion de funciones del manejador de memoria
*/

#include "mmu.h"
#include "i386.h"

#include "kassert.h"

static pd_entry_t* kpd = (pd_entry_t*)KERNEL_PAGE_DIR;
static pt_entry_t* kpt = (pt_entry_t*)KERNEL_PAGE_TABLE_0;

static const uint32_t identity_mapping_end = 0x003FFFFF;
static const uint32_t user_memory_pool_end = 0x02FFFFFF;

static paddr_t next_free_kernel_page = 0x100000;
static paddr_t next_free_user_page = 0x400000;

/**
 * kmemset asigna el valor c a un rango de memoria interpretado
 * como un rango de bytes de largo n que comienza en s
 * @param s es el puntero al comienzo del rango de memoria
 * @param c es el valor a asignar en cada byte de s[0..n-1]
 * @param n es el tamaño en bytes a asignar
 * @return devuelve el puntero al rango modificado (alias de s)
*/
static inline void* kmemset(void* s, int c, size_t n) {
  uint8_t* dst = (uint8_t*)s;
  for (size_t i = 0; i < n; i++) {
    dst[i] = c;
  }
  return dst;
}

/**
 * zero_page limpia el contenido de una página que comienza en addr
 * @param addr es la dirección del comienzo de la página a limpiar
*/
static inline void zero_page(paddr_t addr) {
  kmemset((void*)addr, 0x00, PAGE_SIZE);
}

void mmu_init(void) {}

/**
 * mmu_next_free_kernel_page devuelve la dirección física de la próxima página de kernel disponible. 
 * Las páginas se obtienen en forma incremental, siendo la primera: next_free_kernel_page
 * @return devuelve la dirección de memoria de comienzo de la próxima página libre de kernel
 */
paddr_t mmu_next_free_kernel_page(void) {
  paddr_t res = next_free_kernel_page;
  next_free_kernel_page += PAGE_SIZE;
  return res;
}

/**
 * mmu_next_free_user_page devuelve la dirección de la próxima página de usuarix disponible
 * @return devuelve la dirección de memoria de comienzo de la próxima página libre de usuarix
 */
paddr_t mmu_next_free_user_page(void) {
  paddr_t res = next_free_user_page;
  next_free_user_page += PAGE_SIZE;
  return res;
}

/**
 * mmu_init_kernel_dir inicializa las estructuras de paginación vinculadas al kernel y
 * realiza el identity mapping
 * @return devuelve la dirección de memoria de la página donde se encuentra el directorio
 * de páginas usado por el kernel
 */
paddr_t mmu_init_kernel_dir(void) {
  // Inicializamos la pd
  kpd[0].attrs = MMU_P | MMU_W;
  kpd[0].pt = KERNEL_PAGE_TABLE_0 >> 12;

  // Iniciamos pt
  for (uint32_t i = 0; i <= identity_mapping_end; i += PAGE_SIZE)
  {
      uint32_t idx = i / PAGE_SIZE;
      kpt[idx].attrs = MMU_P | MMU_W;
      kpt[idx].page = i >> 12;
  }
  return KERNEL_PAGE_DIR;
}

/**
 * mmu_map_page agrega las entradas necesarias a las estructuras de paginación de modo de que
 * la dirección virtual virt se traduzca en la dirección física phy con los atributos definidos en attrs
 * @param cr3 el contenido que se ha de cargar en un registro CR3 al realizar la traducción
 * @param virt la dirección virtual que se ha de traducir en phy
 * @param phy la dirección física que debe ser accedida (dirección de destino)
 * @param attrs los atributos a asignar en la entrada de la tabla de páginas
 */
void mmu_map_page(uint32_t cr3, vaddr_t virt, paddr_t phy, uint32_t attrs) {
    pd_entry_t *pd = (pd_entry_t*)CR3_TO_PAGE_DIR(cr3);
    uint32_t pd_indice = VIRT_PAGE_DIR(virt);
    uint32_t pt_indice = VIRT_PAGE_TABLE(virt);

    // Caso pde no presente
    if (!(pd[pd_indice].attrs & MMU_P))
    {     
          paddr_t nueva = mmu_next_free_kernel_page(); 
          zero_page(nueva);
          pd[pd_indice].pt = nueva >> 12;
          pd[pd_indice].attrs = MMU_P | MMU_W | (attrs & MMU_U ? MMU_U : 0);

          pt_entry_t *pt = (pt_entry_t*)nueva;  
          pt[pt_indice].attrs = attrs;
          pt[pt_indice].page  = phy >> 12;       
    }
    // Caso pde presente
    else
    {
      pt_entry_t *pt = (pt_entry_t*)(MMU_ENTRY_PADDR(pd[pd_indice].pt));
      pt[pt_indice].attrs = attrs ;
      pt[pt_indice].page = phy >> 12;
    }
    tlbflush();  
}

/**
 * mmu_unmap_page elimina la entrada vinculada a la dirección virt en la tabla de páginas correspondiente
 * @param virt la dirección virtual que se ha de desvincular
 * @return la dirección física de la página desvinculada
 */
paddr_t mmu_unmap_page(uint32_t cr3, vaddr_t virt) {
    pd_entry_t *pd = CR3_TO_PAGE_DIR(cr3);
    uint32_t pd_indice = VIRT_PAGE_DIR(virt);
    uint32_t pt_indice = VIRT_PAGE_TABLE(virt);

    if (!(pd[pd_indice].attrs & MMU_P))
    {
      return 0;
    }

    pt_entry_t *pt = (pt_entry_t*)MMU_ENTRY_PADDR(pd[pd_indice].pt);
    paddr_t res = MMU_ENTRY_PADDR(pt[pt_indice].page);
    pt[pt_indice].attrs = 0;
    pt[pt_indice].page = 0;
    tlbflush();
    return res;
}

#define DST_VIRT_PAGE 0xA00000
#define SRC_VIRT_PAGE 0xB00000

/**
 * copy_page copia el contenido de la página física localizada en la dirección src_addr a la página física ubicada en dst_addr
 * @param dst_addr la dirección a cuya página queremos copiar el contenido
 * @param src_addr la dirección de la página cuyo contenido queremos copiar
 *
 * Esta función mapea ambas páginas a las direcciones SRC_VIRT_PAGE y DST_VIRT_PAGE, respectivamente, realiza
 * la copia y luego desmapea las páginas. Usar la función rcr3 definida en i386.h para obtener el cr3 actual
 */
void copy_page(paddr_t dst_addr, paddr_t src_addr) {
    uint32_t cr3 = rcr3();
    mmu_map_page(cr3, DST_VIRT_PAGE, dst_addr, MMU_P | MMU_W);
    mmu_map_page(cr3, SRC_VIRT_PAGE, src_addr, MMU_P);

    uint8_t *destino = (uint8_t*)DST_VIRT_PAGE;
    uint8_t *source = (uint8_t*)SRC_VIRT_PAGE;
    for (uint32_t i = 0; i < PAGE_SIZE; i++)
    {
      destino[i] = source[i];
    }

    mmu_unmap_page(cr3, DST_VIRT_PAGE);
    mmu_unmap_page(cr3, SRC_VIRT_PAGE);
}

/**
 * mmu_init_task_dir inicializa las estructuras de paginación vinculadas a una tarea 
 * cuyo código se encuentra en la dirección phy_start
 * @param phy_start es la dirección donde comienzan las dos páginas de código de la tarea asociada a esta llamada
 * @return el contenido que se ha de cargar en un registro CR3 para la tarea asociada a esta llamada
 */
paddr_t mmu_init_task_dir(paddr_t phy_start) {
  // Iniciamos la pd
  paddr_t nueva_pd = mmu_next_free_kernel_page();
  zero_page(nueva_pd);
  pd_entry_t *pd = (pd_entry_t*)nueva_pd;

  // Identity mapping
  pd[0] = kpd[0];

  // cr3 de la tarea
  uint32_t cr3 = nueva_pd;

  // atributos usuario
  uint32_t attrs = MMU_P | MMU_U;

  // Dos paginas de codigo nivel 3 , solo lectura
  mmu_map_page(cr3, TASK_CODE_VIRTUAL, phy_start, attrs);
  mmu_map_page(cr3, TASK_CODE_VIRTUAL + PAGE_SIZE, phy_start + PAGE_SIZE, attrs);

  // Una pagina de pila nivel 3 , lectura/escritura
  paddr_t pila = mmu_next_free_user_page();
  mmu_map_page(cr3, TASK_STACK_BASE - PAGE_SIZE, pila, attrs | MMU_W);

  // Una pagina de código compartido nivel 3 solo lectura
  mmu_map_page(cr3, TASK_SHARED_PAGE, SHARED, attrs);

  return cr3;
}

bool page_fault_handler(vaddr_t virt) {
    print("Atendiendo page fault...", 0, 0, C_FG_WHITE | C_BG_BLACK); 
    uint32_t cr3 = rcr3();
    if (virt >= ON_DEMAND_MEM_START_VIRTUAL && virt <= ON_DEMAND_MEM_END_VIRTUAL)
    {
        mmu_map_page(cr3, virt, ON_DEMAND_MEM_START_PHYSICAL, MMU_P | MMU_W | MMU_U);
        tasks_screen_draw();
        return true;
    }
    return false; 
}
