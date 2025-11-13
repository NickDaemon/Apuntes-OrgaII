# Consigna

- Toda tarea puede pertenecer **como máximo** a una pareja.
- Toda tarea puede _abandonar_ a su pareja.
- Las tareas pueden formar parejas cuantas veces quieran, pero solo pueden pertenecer a una en un momento dado.
- Las tareas de una pareja comparten un area de memoria de 4MB ($2^{22}$ bytes) a partir de la direccion virtual `0xC0C00000`. Esta región se asignará bajo demanda. Acceder a memoria no asignada dentro de la región reservará sólo las páginas necesarias para cumplir ese acceso. Al asignarse memoria ésta estará limpia (todos ceros). **Los accesos de ambas tareas de una pareja en ésta región deben siempre observar la misma memoria física.**
- Sólo la tarea que **crea** una pareja puede **escribir** en los 4MB a partir de `0xC0C00000`.
- Cuando una tarea abandona su pareja pierde acceso a los 4MB a partir de `0xC0C00000`.

Hay que implementar 3 **syscalls**:

## `crear_pareja()`

Al crear una pareja, una tarea debe llamar esta syscall. La syscall debe tener en cuenta lo siguiente:
- Si ya pertenece a una pareja el sistema ignora la solicitud y retorna de inmediato.
- Sino, la tarea no volverá a ser ejecutada hasta que otra tarea se una a la pareja.
- La tarea que crea la pareja será llamada la líder de la misma y será la única que puede escribir en el área de memoria especificado.

## `juntarse_con(id_tarea)`
Para unirse a una pareja una tarea deberá invocar `juntarse_con(<líder-de-la-pareja>)`. Aquí se deben considerar las siguientes situaciones:
- Si ya pertenece a una pareja el sistema ignora la solicitud y retorna 1 de inmediato.
- Si la tarea identificada por id_tarea no estaba creando una pareja entonces el sistema ignora la solicitud y retorna 1 de inmediato.
- Si no, se conforma una pareja en la que id_tarea es líder. Luego de conformarse la pareja, esta llamada retorna 0.
Notar que una vez creada la pareja se debe garantizar que ambas tareas tengan acceso a los 4MB a partir de 0xC0C00000 a medida que lo requieran. Sólo la líder podrá escribir y ambas podrán leer.

## `abandonar_pareja()`
Para abandonar una pareja una tarea deberá invocar `abandonar_pareja()`, hay tres casos posibles a los que el sistema debe responder:
- Si la tarea no pertenece a ninguna pareja: No pasa nada.
- Si la tarea pertenece a una pareja y no es su líder: La tarea abandona la pareja, por lo que pierde acceso al área de memoria especificada.
- Si la tarea pertenece a una pareja y es su líder: La tarea queda bloqueada hasta que la otra parte de la pareja abandone. Luego pierde acceso a al área de memoria especificada.

# Ejercicio 1
Asumir implementados:
- `task_id pareja_de_actual()` que devuelve el ID de la pareja de la task actual (si existe).
- `bool es_lider(task_id tarea)` que indica si la tarea pasada por parametro es lider o no.
- `bool aceptando_pareja(task_id tarea)` que devuelve 1 si la tarea pasada por parametro es capaz de formar una pareja. Si no, devuelve 0.
- `void conformar_pareja(task_id tarea)` que le dice al sistema que la tarea actual y la pasada por parametro deben formar pareja. Si se pasa 0, le dice al sistema que la tarea actual puede ser emparejada.
- `void romper_pareja()` que le indica al sistema que la tarea actual debe dejar de tener pareja.


- (50 pts) Definir el mecanismo por el cual las syscall crear_pareja, juntarse_con y abandonar_pareja recibirán sus parámetros y retornarán sus resultados según corresponda. Dar una implementación para cada una de las syscalls. Explicitar las modificaciones al kernel que sea necesario realizar, como pueden ser estructuras o funciones auxiliares.

Primero, modificamos el siguiente struct en `sched.c`:
```C
typedef enum {
  TASK_SLOT_FREE,
  TASK_RUNNABLE,
  TASK_PAUSED,
  TASK_BLOCKED, // Agregamos esto, ya que una tarea puede quedar bloqueada si es lider y abandona su pareja
  TASK_BUSCANDO_PAREJA // Esta task puede encontrar una pareja
} task_state_t;

typedef struct {
  int16_t selector;
  task_state_t state;
  bool pertenceAlMundoDeLasParejas;
  bool esLider; // Agregamos esto
  int8_t idPareja; // Y esto  
} sched_entry_t;
...

int8_t sched_add_task(uint16_t selector) {
  kassert(selector != 0, "No se puede agregar el selector nulo");

  // Se busca el primer slot libre para agregar la tarea
  for (int8_t i = 0; i < MAX_TASKS; i++) {
    if (sched_tasks[i].state == TASK_SLOT_FREE) {
      sched_tasks[i] = (sched_entry_t) {
        .selector = selector,
	      .state = TASK_PAUSED,
        .esLider = false, // Agregamos esto
        idPareja = 0, // Y esto
      };
      return i;
    }
  }
  kassert(false, "No task slots available");
}
...
void sched_block_task(int8_t task_id) {
  kassert(task_id >= 0 && task_id < MAX_TASKS, "Invalid task_id");
  sched_tasks[task_id].state = TASK_BLOCKED;
}
```

Ahora bien, queremos implementar: `crear_pareja()`, que es una syscall, por lo que modificamos `isr.asm`:
```C
global _isr90 ; Syscall de crear_pareja()
_isr90:
  pushad
  call crear_pareja
  popad
  iret

global _isr91 ; Syscall de juntarse_con(id_tarea)
_isr91:
  pushad
  push ecx ; Asumo que me viene por aca el parametro "id_tarea"
  call juntarse_con ; Devuelve en eax 1 o 0
  add esp, 4
  mov DWORD [esp + 28], eax ; Para poder devolver el valor de eax, lo modifico en la pila
  popad ; Tal que cuando lo popeo, en vez de restaurarme el original, me devuelve el resultado de juntarse_con
  iret

global _isr92 ; Syscall de abandonar_pareja() 
_isr92:
  pushad
  call abandonar_pareja
  popad
  iret
```
Ahora, en `idt.c`:
```C
void idt_init() {
  ...
  IDT_ENTRY3(90);
  IDT_ENTRY3(91);
  IDT_ENTRY3(92);
}
```
Ahora podemos definir en `tasks.c` o `sched.c` las funciones correspondientes a `crear_pareja`, `juntarse_con(id_tarea)` y `abandonar_pareja()`.
Las defino en: `sched.c`:
```C
void crear_pareja() {
  if (pareja_de_actual() != 0 || sched_tasks[current_task].esLider) { return; } //Ya tiene pareja o es lider
  sched_tasks[current_task].esLider = true;
  sched_tasks[current_task].state = TASK_BLOCKED; // Queda bloqueada hasta que forme pareja
}

uint8_t juntarse_con(uint8_t id_tarea)
{
  sched_entry_t taskActual = sched_tasks[current_task];
  sched_entry_t taskLider = sched_tasks[id_tarea];
  if (!taskLider.esLider || taskLider.idPareja != 0) {return 1;} // La otra tarea no era lider o ya tiene pareja
  if (taskActual.esLider || taskActual.idPareja != 0) {return 1;} // La tarea actual no puede ser lider o ya tiene pareja
  
  taskActual.idPareja = id_tarea;
  taskLider.idPareja = current_task;
  taskLider.state = TASK_RUNNABLE; // Desbloquear la tarea
  sched_tasks[current_task] = taskActual;
  sched_tasks[id_tarea] = taskLider;
  return 0;
}

void abandonar_pareja() {
  sched_entry_t taskActual = sched_tasks[current_task];
  if (taskActual.idPareja == 0) {return;} // No tengo pareja, no hago nada
  if (!taskActual.esLider) {
    sched_entry_t taskLider = sched_tasks[taskActual.idPareja];
    taskLider.idPareja = 0; // Borrar parejas
    taskActual.idPareja = 0;

    sched_tasks[current_task] = taskActual; // Actualizar los valores
    sched_tasks[taskActual.idPareja] = taskLider;
  }
  else {
    taskActual.state = TASK_BLOCKED;
    sched_tasks[current_task] = taskActual;
  }
}


```
Ahora bien, faltan realizar los cambios en el kernel para que las parejas tengan una area de memoria compartida, en la que el Lider puede leer y escribir, mientras que la que no es lider, simplemente puede leer.

En `mmu.c` modificamos el page fault handler tal que:
```C
bool page_fault_handler(vaddr_t virt) {
  ...
  //Si no pertenece a un page fault en ese area, nos fijamos si pertenece al area de memoria compartida de esa tarea
  paddr_t phy_addr = mmu_next_free_user_page();
  if (virt >= VIRT_PAREJA_ADDR && virt <= VIRT_PAREJA_ADDR + 4MB) {
    paddr_t cr3 = rcr3();
    if (es_lider(current_task)) {
        //Si la tarea actual es lider, entonces puede escribir
        mmu_map_page(rcr3(),virt,phy_addr, MMU_P | MMU_W | MMU_U);
        mmu_map_page(selector_task_to_cr3(sched_tasks[actual_de_pareja()].selector), virt, phy, MMU_P | MMU_U);
    } else {
        // Si no es lider, mapeo a la pareja de la actual para que pueda escribir.
        mmu_map_page(rcr3(),virt,phy_addr, MMU_P | MMU_U);
        mmu_map_page(selector_task_to_cr3(sched_tasks[actual_de_pareja()].selector), virt, phy, MMU_P | MMU_U | MMU_W);
    }
    zero_page(virt & (0xfffff000)); // Zereamos la pagina asignada.
    return true;
  }

  // Si no pertenece al área on-demand, no lo manejamos acá
  return false;
}
%define VIRT_PAREJA_ADDR 0xC0C00000
%define 4MB (4 * 1024 * 1024)
```
Ahora bien, cambie algunas cosas en `sched.c` y queda asi, ahora tambien contempla el quitado de acceso a memoria cuando corresponde:
```C
void crear_pareja() {
  if (!aceptando_pareja(current_task)) {
      conformar_pareja(0); //Inicializamos la pareja y la logica la dejamos a parte para esta funcion
  }
}

uint8_t juntarse_con(uint8_t id_tarea)
{
  if (aceptando_pareja(id_task)) {
    conformar_pareja(id_task);
    return 0;
  }
  return 1;
}

void abandonar_pareja() {
  if (pareja_actual() == 0) return;

  paddr_t cr3 = selector_task_to_cr3(sched_tasks[current_task]);

  if (!es_lider(current_task)) {
    uint8_t lider_id = pareja_actual();
    romper_pareja(); // rompe el vínculo lógico
    // Quitamos acceso a la memoria
    for (int addr = VIRT_PAREJA_ADDR; addr < VIRT_PAREJA_ADDR + 4MB; addr++) {
      mmu_unmap_page(cr3, addr);
    }
    if (sched_tasks[lider_id].state == TASK_BLOCKED) {
      for (int addr = VIRT_PAREJA_ADDR; addr < VIRT_PAREJA_ADDR + 4MB; addr++) {
      mmu_unmap_page(cr3, addr);
    }
      sched_tasks[lider_id].state = TASK_RUNNABLE;
    } else {
      sched_tasks[lider_id].state = TASK_ESPERANDO_PAREJA;
      sched_next_task();
    }

  } else {
    // Soy líder: rompo la pareja, desmapeo y me bloqueo
    romper_pareja();
    sched_tasks[current_task].state = TASK_BLOCKED;
    sched_next_task();
  }
}

uint8_t pareja_de_actual() {
  sched_entry_t taskActual = sched_tasks[current_task];
  return (uint8_t) taskActual.idPareja;
}

bool es_lider(uint8_t tarea) {
  return sched_tasks[tarea].esLider;
}

bool aceptando_pareja(uint8_t tarea) {
  if (tarea == 0) { return false; }
  sched_entry_t task = sched_tasks[tarea];
  if (task.idPareja == 0 && task.esLider || task.state == TASK_BUSCANDO_PAREJA) { return true; }
  return false;
} 

void conformar_pareja(task_id tarea) {
  sched_entry_t *current = &sched_tasks[current_task];
  current->lider = true;
  if (tarea == 0) {
      current->id_pareja = 0;
      current->state = TASK_ESPERANDO_PAREJA;
  } else {
    sched_entry_t* pareja = &sched_tasks[tarea];
    current->state = TASK_RUNNABLE;
    current->id_pareja = tarea;
    pareja->lider = false;
    pareja->state = TASK_RUNNABLE;
    pareja->id_pareja = current_task;
  }
}
void romper_pareja() {
  uint16_t pareja = pareja_actual();
  if (pareja == 0) return;  // nada que hacer
  if (!es_lider(current_task)) {
    // caso no-líder: limpia ambos estados
    sched_tasks[pareja].idPareja = 0;
    sched_tasks[pareja].esLider = true; //la pareja siguie siendo lider en general no de esta pareja

    sched_tasks[current_task].idPareja = 0;
    sched_tasks[current_task].esLider = false; // No era lider lo dejamos como estaba
    sched_tasks[current_task].state = TASK_RUNNABLE;
  } else {
    // caso líder: no rompe el vínculo, solo marca que ya no es líder activo
    // (el vínculo se romperá definitivamente cuando la no-líder abandone)
    sched_tasks[current_task].esLider = 1; // sigue siendo líder
    // no se toca pareja_actual
  }
}
```

Con eso queda resuelta la parte 1.
