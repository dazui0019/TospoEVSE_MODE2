; 
;  BasicOS V0.2
;  Copyright (c) 2021, EventOS Team, <event-os@outlook.com>
; 
;  SPDX-License-Identifier: MIT
;  
;  Permission is hereby granted, free of charge, to any person obtaining a copy
;  of this software and associated documentation files (the 'Software'), to deal
;  in the Software without restriction, including without limitation the rights
;  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
;  copies of the Software, and to permit persons to whom the Software is furnished
;  to do so, subject to the following conditions:
; 
;  The above copyright notice and this permission notice shall be
;  included in all copies or substantial portions of the Software.
; 
;  THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
;  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
;  FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS 
;  OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
;  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
;  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
; 
;  https://www.event-os.cn
;  https://github.com/event-os/eventos-basic
;  https://gitee.com/event-os/eventos-basic
;  
;  Change Logs:
;  Date           Author        Notes
;  2022-03-21     GouGe         V0.1.0
;  2023-04-22     GouGe         V0.2.0
;

    AREA |.text|, CODE, READONLY, ALIGN=2
    THUMB
    REQUIRE8
    PRESERVE8

; Enable the global interrput.
bos_critical_exit PROC

    EXPORT bos_critical_exit

    CPSIE   I
    BX      LR

    ENDP

; Disable the global interrput.
bos_critical_enter PROC

    EXPORT bos_critical_enter

    CPSID   I
    BX      LR

    ENDP

; Get the current stack top.
get_sp_value PROC
    
    EXPORT get_sp_value
    
    MOV         r0, sp
    BX          lr                  ; return to the next task */
    ENDP
    
; PendSV hanbder.
PendSV_Handler   PROC

    EXPORT PendSV_Handler

    IMPORT      bos_current         ; extern variable */
    IMPORT      bos_next            ; extern variable */
    IMPORT      addr_target         ; extern variable */
    IMPORT      addr_source         ; extern variable */
    IMPORT      copy_size           ; extern variable */

    CPSID       I                   ; disable interrupts (set PRIMASK) */

    LDR         r1,=bos_current     ; if (bos_current != 0)
                                    ; { */
    LDR         r1,[r1,#0x00]
    CMP         r1, #0
    BEQ         PendSV_Restore
    NOP
    PUSH        {r4-r7}             ;       push r4-r11 into stack */
    MOV         r4, r8
    MOV         r5, r9
    MOV         r6, r10
    MOV         r7, r11
    PUSH        {r4-r7}             ; } */

PendSV_Restore
    LDR         r4, =copy_size
    LDR         r2, [R4]
    CMP         r2, #0
    BEQ         NextTask
    LDR         r4, =addr_source
    LDR         r0, [R4]
    LDR         r4, =addr_target
    LDR         r1, [R4]
    CMP         r1, r0              ; Check the target addr is at front of the source.
    BHI         LoopStart_P         ; If yes, copy from front to back.
    ADD         r0, r2              ; If not, copy from back to front.
    ADD         r1, r2              ; Calculate the new source address.
    SUBS        r0, r0, #4
    SUBS        r1, r1, #4
    
LoopStart_N
    MOV         r4, r0              ; Save the source address.
    MOV         r5, r1              ; Save the target address.
    MOV         r2, r2              ; Save the copying size.
    LDR         r3, =0              ; clear counting.
    
Loop2_N
    LDR         r0, [r4]            ; Read one word from the source address.
    STR         r0, [r5]            ; Write the word into target address.
    ADDS        r3, r3, #4          ; Counting + 1
    SUBS        r4, r4, #4
    SUBS        r5, r5, #4
    CMP         r3, r2              ; Check the end of copying.
    BNE         Loop2_N             ; If not end, continue
    B           NextTask
    
LoopStart_P
    MOV         r4, r0              ; Save the source address.
    MOV         r5, r1              ; Save the target address.
    MOV         r2, r2              ; Save the copying size.
    LDR         r3, =0              ; clear counting.
    
Loop2_P
    LDR         r0, [r4]            ; Read one word from the source address.
    STR         r0, [r5]            ; Write the word into target address.
    ADDS        r3, r3, #4          ; Counting + 1
    ADDS        r4, r4, #4
    ADDS        r5, r5, #4
    CMP         r3, r2              ; Check the end of copying.
    BNE         Loop2_P             ; If not end, continue

NextTask
    LDR         r1, = bos_next      ; sp = bos_next->sp; */
    LDR         r1,[r1,#0x00]
    LDR         r0,[r1,#0x00]
    MOV         SP, r0
    LDR         r1, = bos_next      ; bos_current = bos_next; */
    LDR         r1,[r1,#0x00]
    LDR         r2, = bos_current
    STR         r1,[r2,#0x00]
    POP         {r4-r7}
    MOV         r8, r4
    MOV         r9, r5
    MOV         r10,r6
    MOV         r11,r7
    POP         {r4-r7}
    CPSIE       I                   ; enable interrupts (clear PRIMASK) */
    BX          lr                  ; return to the next task */
    ENDP

    ALIGN   4

    END

; ------------------------------ end of file --------------------------------- ;