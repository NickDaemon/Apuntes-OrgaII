# System Programming: Tareas.

Vamos a continuar trabajando con el kernel que estuvimos programando en
los talleres anteriores. La idea es incorporar la posibilidad de
ejecutar algunas tareas específicas. Para esto vamos a precisar:

-   Definir las estructuras de las tareas disponibles para ser
    ejecutadas

-   Tener un scheduler que determine la tarea a la que le toca
    ejecutase en un período de tiempo, y el mecanismo para el
    intercambio de tareas de la CPU

-   Iniciar el kernel con una *tarea inicial* y tener una *tarea idle*
    para cuando no haya tareas en ejecución

Recordamos el mapeo de memoria con el que venimos trabajando. Las tareas
que vamos a crear en este taller van a ser parte de esta organización de
la memoria:

![](img/mapa_fisico.png)

![](img/mapeo_detallado.png)

## Archivos provistos

A continuación les pasamos la lista de archivos que forman parte del
taller de hoy junto con su descripción:

-   **Makefile** - encargado de compilar y generar la imagen del
    floppy disk.

-   **idle.asm** - código de la tarea Idle.

-   **shared.h** -- estructura de la página de memoria compartida

-   **tareas/syscall.h** - interfaz para realizar llamadas al sistema
    desde las tareas

-   **tareas/task_lib.h** - Biblioteca con funciones útiles para las
    tareas

-   **tareas/task_prelude.asm**- Código de inicialización para las
    tareas

-   **tareas/taskPong.c** -- código de la tarea que usaremos
    (**tareas/taskGameOfLife.c, tareas/taskSnake.c,
    tareas/taskTipear.c **- código de otras tareas de ejemplo)

-   **tareas/taskPongScoreboard.c** -- código de la tarea que deberán
    completar

-   **tss.h, tss.c** - definición de estructuras y funciones para el
    manejo de las TSSs

-   **sched.h, sched.c** - scheduler del kernel

-   **tasks.h, tasks.c** - Definición de estructuras y funciones para
    la administración de tareas

-   **isr.asm** - Handlers de excepciones y interrupciones (en este
    caso se proveen las rutinas de atención de interrupciones)

-   **task\_defines.h** - Definiciones generales referente a tareas

## Ejercicios

Antes de comenzar, vamos a actualizar el archivo `isr.asm` con algunas syscalls adicionales.

- Reemplazar el código de la `isr88` por
```
pushad
push eax
call tasks_syscall_draw
add esp, 4
popad
iret
```
Esta va a ser una syscall para que una tarea dibuje en su pantalla ¿Cuál es la convención de pasaje de parámetros para esta syscall?

- Verificar que su rutina de atención para las interrupciones del teclado sea funcionalmente igual a esta:
```
pushad
; 1. Le decimos al PIC que vamos a atender la interrupción
call pic_finish1
; 2. Leemos la tecla desde el teclado y la procesamos
in al, 0x60
push eax
call tasks_input_process
add esp, 4
popad
iret
```

- Verificar que la rutina de atención para los pagefault que escribieron el el taller anterior tenga esta forma (recuerden incluir la parte que implementaron para la página compartida a demanda):
```
;; Rutina de atención de Page Fault
;; -------------------------------------------------------------------------- ;;
global _isr14

_isr14:
	; Estamos en un page fault.
	pushad
    ; COMPLETAR: llamar rutina de atención de page fault, pasandole la dirección que se intentó acceder
    .ring0_exception:
	; Si llegamos hasta aca es que cometimos un page fault fuera del area compartida.
    call kernel_exception
    jmp $

    .fin:
	popad
	add esp, 4 ; error code
	iret
```

### Primera parte: Inicialización de tareas

**1.** Si queremos definir un sistema que utilice sólo dos tareas, ¿Qué
nuevas estructuras, cantidad de nuevas entradas en las estructuras ya
definidas, y registros tenemos que configurar?¿Qué formato tienen?
¿Dónde se encuentran almacenadas?
- **Nuevas estructuras:**
1. Cada tarea tendra su ***Espacio de ejecución*** , osea  páginas mapeadas donde van a tener su código, datos y pilas, además podrı́amos precisar definirle un page directory con su correspondiente pages table o o reutilizar algún directorio entre varias tareas.
2. Cada tarea tendra su ***Segmento de Estado (TSS)*** donde almacenara el estado de cada tarea, conservando sus registros de proposito general, registros de segmento de la tarea y la pila nivel 0, Flags, CR3 y EIP. 
- **Nuevas Entradas:**
1. Cada tarea tendra una entrada nueva en la GDT ya definida.
- **Registros:**
- Cada tarea tendra un task register distinto y un cr3 distinto.
- **Donde se encuentran almacenadas?:**
- Todas estas estructuras (las TSS, las nuevas entradas en la GDT y los directorios de páginas) se encuentran almacenadas en memoria del kernel.

**2.** ¿A qué llamamos cambio de contexto? ¿Cuándo se produce? ¿Qué efecto
tiene sobre los registros del procesador? Expliquen en sus palabras que
almacena el registro **TR** y cómo obtiene la información necesaria para
ejecutar una tarea después de un cambio de contexto.
- **Un cambio de contexto (o context switch) es el proceso mediante el cual el procesador interrumpe la ejecución de una tarea y comienza a ejecutar otra.**
- **Ocurre en cada tick del reloj, donde el scheduler le pide al procesador cambiar la tarea en ejecucion haciendo un jmp al segmento de la TSS de la nueva tarea a ejecutar en la GDT.**
- **El registro TR contiene un selector que apunta a una entrada de la GDT asociada a la TSS de la tarea actual, donde guardara el contexto completo la tarea(los registros de proposito general , registros de segmento de la tarea , Flags , CR3 y EIP entre otros).**
- **Cuando el sistema operativo realiza el cambio de contexto, el procesador actualiza el TR con el selector de la nueva TSS, y a partir de esa estructura en memoria carga automáticamente todos los registros del procesador (EIP, ESP, EFLAGS, selectores de segmento, etc.) con los valores almacenados en la TSS de la nueva tarea.**

**3.** Al momento de realizar un cambio de contexto el procesador va
almacenar el estado actual de acuerdo al selector indicado en el
registro **TR** y ha de restaurar aquel almacenado en la TSS cuyo
selector se asigna en el *jmp* far. ¿Qué consideraciones deberíamos
tener para poder realizar el primer cambio de contexto? ¿Y cuáles cuando
no tenemos tareas que ejecutar o se encuentran todas suspendidas?
- **El procesador siempre necesita tener una tarea en ejecución, incluso si no realiza ninguna operación útil. Por eso, debemos definir dos tareas especiales: la tarea Inicial y la tarea Idle.**
- **La ***tarea Inicial*** es la primera tarea que se ejecuta apenas se carga el kernel. Su función principal es dejar el sistema en un estado válido para poder realizar el primer cambio de contexto, ya que es la que se encuentra activa cuando se carga por primera vez el registro TR con la TSS correspondiente. Desde esta tarea inicial se realiza el salto (jmp far) hacia otra TSS, iniciando así la primera conmutación de tareas.**
- **La ***tarea idle*** es una tarea especial que se ejecuta cuando no tenemos tareas activas o se encuentran todas suspendidas.**

**4.** ¿Qué hace el scheduler de un Sistema Operativo? ¿A qué nos
referimos con que usa una política?
- **El scheduler es un módulo del Sistema Operativo encargado de decidir qué tarea debe ejecutarse en el procesador en cada instante.**
- **Define un intervalo de tiempo llamado *time frame*, el que a su vez con ayuda de un temporizador es dividido en intervalos mas pequeños, que se convierten en la unidad de tiempo.**
- **El scheduler usa una política de planificación, que consiste en asignar a cada tarea una cantidad de intervalos de tiempo dentro del time frame, proporcional a su prioridad.De esta forma, las tareas con mayor prioridad reciben más tiempo de ejecución dentro de cada time frame que las de menor prioridad.**

**5.** En un sistema de una única CPU, ¿cómo se hace para que los
programas parezcan ejecutarse en simultáneo?
- **Las tareas en un computador no operan simultáneamente, sino serial-mente pero conmutando de una a otra a gran velocidad. Esto significa que se producen cientos o miles de context switches por segundo. Nuestros sentidos no captan la intermitencia de cada tarea, creándose una sensación de simultaneidad.**

**6.** En **tss.c** se encuentran definidas las TSSs de la Tarea
**Inicial** e **Idle**. Ahora, vamos a agregar el *TSS Descriptor*
correspondiente a estas tareas en la **GDT**.
    
a) Observen qué hace el método: ***tss_gdt_entry_for_task***

b) Escriban el código del método ***tss_init*** de **tss.c** que
agrega dos nuevas entradas a la **GDT** correspondientes al
descriptor de TSS de la tarea Inicial e Idle.

c) En **kernel.asm**, luego de habilitar paginación, agreguen una
llamada a **tss_init** para que efectivamente estas entradas se
agreguen a la **GDT**.

d) Correr el *qemu* y usar **info gdt** para verificar que los
***descriptores de tss*** de la tarea Inicial e Idle esten
efectivamente cargadas en la GDT

**7.** Como vimos, la primer tarea que va a ejecutar el procesador
cuando arranque va a ser la **tarea Inicial**. Se encuentra definida en
**tss.c** y tiene todos sus campos en 0. Antes de que comience a ciclar
infinitamente, completen lo necesario en **kernel.asm** para que cargue
la tarea inicial. Recuerden que la primera vez tenemos que cargar el registro
**TR** (Task Register) con la instrucción **LTR**.
Previamente llamar a la función tasks_screen_draw provista para preparar
la pantalla para nuestras tareas.

Si obtienen un error, asegurense de haber proporcionado un selector de
segmento para la tarea inicial. Un selector de segmento no es sólo el
indice en la GDT sino que tiene algunos bits con privilegios y el *table
indicator*.

**8.** Una vez que el procesador comenzó su ejecución en la **tarea Inicial**, 
le vamos a pedir que salte a la **tarea Idle** con un
***JMP***. Para eso, completar en **kernel.asm** el código necesario
para saltar intercambiando **TSS**, entre la tarea inicial y la tarea
Idle.

**9.** Utilizando **info tss**, verifiquen el valor del **TR**.
También, verifiquen los valores de los registros **CR3** con **creg** y de los registros de segmento **CS,** **DS**, **SS** con
***sreg***. ¿Por qué hace falta tener definida la pila de nivel 0 en la
tss?
- **Hace falta tener definida la pila de nivel 0 en la TSS porque, si ocurre una interrupción o excepción mientras se ejecuta código de usuario (nivel 3), el procesador necesita cambiar al nivel de privilegio 0 y usar una pila del kernel.**

**10.** En **tss.c**, completar la función ***tss_create_user_task***
para que inicialice una TSS con los datos correspondientes a una tarea
cualquiera. La función recibe por parámetro la dirección del código de
una tarea y será utilizada más adelante para crear tareas.

Las direcciones físicas del código de las tareas se encuentran en
**defines.h** bajo los nombres ***TASK_A_CODE_START*** y
***TASK_B_CODE_START***.

El esquema de paginación a utilizar es el que hicimos durante la clase
anterior. Tener en cuenta que cada tarea utilizará una pila distinta de
nivel 0.

### Segunda parte: Poniendo todo en marcha

**11.** Estando definidas **sched_task_offset** y **sched_task_selector**:
```
  sched_task_offset: dd 0xFFFFFFFF
  sched_task_selector: dw 0xFFFF
```

Y siendo la siguiente una implementación de una interrupción del reloj:

```
global _isr32
  
_isr32:
  pushad
  call pic_finish1
  
  call sched_next_task
  
  str cx
  cmp ax, cx
  je .fin
  
  mov word [sched_task_selector], ax
  jmp far [sched_task_offset]
  
  .fin:
  popad
  iret
```
a)  Expliquen con sus palabras que se estaría ejecutando en cada tic
    del reloj línea por línea
- **En cada tic del reloj se guardan los registros, se avisa al PIC que la interrupción fue atendida, y se consulta al planificador cuál es la próxima tarea. Si la tarea actual es la misma que la siguiente, se restauran los registros y se continúa. Si son distintas, se salta al selector de la nueva tarea, provocando un *context switch.***

b)  En la línea que dice ***jmp far \[sched_task_offset\]*** ¿De que
    tamaño es el dato que estaría leyendo desde la memoria? ¿Qué
    indica cada uno de estos valores? ¿Tiene algún efecto el offset
    elegido?
- **El jmp far lee 6 bytes: 4 del offset y 2 del selector. El selector indica la TSS de la tarea a ejecutar. En este caso, el offset no tiene efecto, porque al saltar a una TSS el procesador realiza un cambio de contexto y no usa ese desplazamiento.**

c)  ¿A dónde regresa la ejecución (***eip***) de una tarea cuando
    vuelve a ser puesta en ejecución?
- **La ejecución (EIP) de una tarea vuelve al valor guardado en su TSS, que corresponde a la última instrucción que estaba ejecutando antes de ser interrumpida o desactivada.**

d)  Reemplazar su implementación anterior de la rutina de atención de reloj por la provista.

**12.** Para este Taller la cátedra ha creado un scheduler que devuelve
la próxima tarea a ejecutar.

a)  En los archivos **sched.c** y **sched.h** se encuentran definidos
    los métodos necesarios para el Scheduler. Expliquen cómo funciona
    el mismo, es decir, cómo decide cuál es la próxima tarea a
    ejecutar. Pueden encontrarlo en la función ***sched_next_task***.
- **El scheduler implementa una política round-robin. En cada interrupción del reloj, busca secuencialmente la siguiente tarea con estado TASK_RUNNABLE, comenzando desde la que estaba ejecutándose. Si encuentra una, actualiza current_task y devuelve su selector. Si no hay tareas activas, selecciona la tarea idle.**

b)  Modifiquen **kernel.asm** para llamar a la función
    ***sched_init*** luego de iniciar la TSS

c)  Compilen, ejecuten ***qemu*** y vean que todo sigue funcionando
    correctamente.

### Tercera parte: Tareas? Qué es eso?

**14.** Como parte de la inicialización del kernel, en kernel.asm se
pide agregar una llamada a la función **tasks\_init** de
**task.c** que a su vez llama a **create_task**. Observe las
siguientes líneas:
```C
int8_t task_id = sched_add_task(gdt_id << 3);

tss_tasks[task_id] = tss_create_user_task(task_code_start[tipo]);

gdt[gdt_id] = tss_gdt_entry_for_task(&tss_tasks[task_id]);
```
a)  ¿Qué está haciendo la función ***tss_gdt_entry_for_task***?
- **La función tss_gdt_entry_for_task crea y devuelve un descriptor de tipo TSS para incluir en la GDT.**

b)  ¿Por qué motivo se realiza el desplazamiento a izquierda de
    **gdt_id** al pasarlo como parámetro de ***sched_add_task***?
- **El desplazamiento a izquierda (gdt_id << 3) convierte el índice de la GDT en un selector de segmento válido. Dado que cada entrada de la GDT ocupa 8 bytes, al multiplicar por 8 (desplazar 3 bits) se obtiene el valor que el procesador usa para identificar la TSS correspondiente dentro de la GDT.**

**15.** Ejecuten las tareas en *qemu* y observen el código de estas
superficialmente.

a) ¿Qué mecanismos usan para comunicarse con el kernel?
- **Las tareas se comunican con el kernel principalmente mediante syscalls (interrupciones de software).**
- **Por ejemplo, cuando la tarea Snake quiere dibujar en pantalla, llama a syscall_draw(pantalla) y es el kernel el que se encarga de dibujar en pantalla.**

b) ¿Por qué creen que no hay uso de variables globales? ¿Qué pasaría si
    una tarea intentase escribir en su `.data` con nuestro sistema?
- **No hay uso de variables globales porque no definimos ni mapeamos memoria para la sección .data de las tareas.Si una tarea intentara escribir en su .data, accedería a memoria no mapeada, lo que provocaría un page fault o excepción de protección.**

c) Cambien el divisor del PIT para \"acelerar\" la ejecución de las tareas:
```
    ; El PIT (Programmable Interrupt Timer) corre a 1193182Hz.

    ; Cada iteracion del clock decrementa un contador interno, cuando
    éste llega

    ; a cero se emite la interrupción. El valor inicial es 0x0 que
    indica 65536,

    ; es decir 18.206 Hz

    mov ax, DIVISOR

    out 0x40, al

    rol ax, 8

    out 0x40, al
```

**16.** Observen **tareas/task_prelude.asm**. El código de este archivo
se ubica al principio de las tareas.

a. ¿Por qué la tarea termina en un loop infinito?

- **El kernel controla cuándo corre la tarea y cuándo la pausa o cambia a otra usando el scheduler y las interrupciones.**

- **Si la tarea termina sin intervención del kernel, la CPU intentaria hacer un ret para regresar a la instrucción que la llamó con call task, pero en nuestro caso no hay ninguna instrucción válida después de la llamada entonces si la CPU intentara continuar, podría ejecutar memoria no válida, provocando fallos.**

- **Por eso *jmp $* hace que la tarea se quede “quieta” de manera segura hasta que el scheduler decida cambiarla.**

b. \[Opcional\] ¿Qué podríamos hacer para que esto no sea necesario?
- **Podríamos agregar un estado a *task_state_t* que indique que la tarea terminó, y hacer que el scheduler, en el siguiente tick del reloj, pase automáticamente a la siguiente tarea ejecutable.**

- **Si quisiéramos volver a ejecutar una tarea marcada como finalizada, habría que reiniciarla cambiando su estado manualmente a TASK_RUNNABLE.**

### Cuarta parte: Hacer nuestra propia tarea

Ahora programaremos nuestra tarea. La idea es disponer de una tarea que
imprima el *score* (puntaje) de todos los *Pongs* que se están
ejecutando. Para ello utilizaremos la memoria mapeada *on demand* del
taller anterior.

#### Análisis:

**18.** Analicen el *Makefile* provisto. ¿Por qué se definen 2 "tipos"
de tareas? ¿Como harían para ejecutar una tarea distinta? Cambien la
tarea S*nake* por una tarea *PongScoreboard*.
- **Se definen dos tipos de tarea para diferenciar entre la idle task (que corre cuando no hay otras tareas ejecutables y mantiene al CPU ocupado) y las tareas de usuario.**
- **Hay que cambiar la definición de la tarea en el Makefile o en el scheduler. Por ejemplo, reemplazar TASKA=taskPong.tsk por la tarea que queremos ejecutar y recompilar.**

**19.** Mirando la tarea *Pong*, ¿En que posición de memoria escribe
esta tarea el puntaje que queremos imprimir? ¿Cómo funciona el mecanismo
propuesto para compartir datos entre tareas?
- **La tarea *Pong* escribe los puntajes en la página compartida SHARED_SCORE_BASE_VADDR más un offset dependiendo del task_id de la tarea.**
- **Se reserva una página de memoria compartida que todas las tareas pueden mapear en su espacio de direcciones.Luego cada tarea tiene un bloque exclusivo dentro de esa página donde puede escribir sus datos, determinado por su task_id.**

#### Programando:

**20.** Completen el código de la tarea *PongScoreboard* para que
imprima en la pantalla el puntaje de todas las instancias de *Pong* usando los datos que nos dejan en la página compartida.

**21.** \[Opcional\] Resuman con su equipo todas las estructuras vistas
desde el Taller 1 al Taller 4. Escriban el funcionamiento general de
segmentos, interrupciones, paginación y tareas en los procesadores
Intel. ¿Cómo interactúan las estructuras? ¿Qué configuraciones son
fundamentales a realizar? ¿Cómo son los niveles de privilegio y acceso a
las estructuras?

**22.** \[Opcional\] ¿Qué pasa cuando una tarea dispara una
excepción? ¿Cómo podría mejorarse la respuesta del sistema ante estos
eventos?
