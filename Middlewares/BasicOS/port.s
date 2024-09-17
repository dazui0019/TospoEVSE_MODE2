SCB_VTOR        EQU     0xE000ED08               ; Vector Table Offset Register
NVIC_INT_CTRL   EQU     0xE000ED04               ; interrupt control state register
NVIC_SYSPRI2    EQU     0xE000ED20               ; system priority register (2)
NVIC_PENDSV_PRI EQU     0xFFFF0000               ; PendSV and SysTick priority value (lowest)
NVIC_PENDSVSET  EQU     0x10000000               ; value to trigger PendSV exception

    AREA |.text|, CODE, READONLY, ALIGN=2
    THUMB
    REQUIRE8
    PRESERVE8

PendSV_Handler   PROC
    EXPORT PendSV_Handler
    IMPORT        eos_current           ; extern variable */
    IMPORT        eos_next              ; extern variable */

    CPSID         i                     ; disable interrupts (set PRIMASK) */

    LDR           r1,=eos_current       ; if (eos_current != 0)
                                        ; { */
    LDR           r1,[r1,#0x00]
    CMP           r1, #0
    BEQ           PendSV_restore
    NOP
    PUSH          {r4-r7}              ;     push r4-r11 into stack */
    MOV           r4, r8
    MOV           r5, r9
    MOV           r6, r10
    MOV           r7, r11
    PUSH          {r4-r7}
    LDR           r1,=eos_current       ;     eos_current->sp = sp; */
    LDR           r1,[r1,#0x00]
    MOV           r2, SP
    STR           r2,[r1,#0x00]         ; } */

PendSV_restore
    LDR           r1,=eos_next          ; sp = eos_next->sp; */
    LDR           r1,[r1,#0x00]
    LDR           r0,[r1,#0x00]
    MOV           SP, r0
    LDR           r1,=eos_next          ; eos_current = eos_next; */
    LDR           r1,[r1,#0x00]
    LDR           r2,=eos_current
    STR           r1,[r2,#0x00]
    POP           {r4-r7}
    MOV           r8, r4
    MOV           r9, r5
    MOV           r10,r6
    MOV           r11,r7
    POP           {r4-r7}
    CPSIE         i                     ; enable interrupts (clear PRIMASK) */
    BX            lr                    ; return to the next task */
    ENDP
        
    ALIGN   4

    END

