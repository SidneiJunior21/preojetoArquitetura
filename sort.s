.section .text
.globl _start

.equ UART_BASE, 0x10000000

_start:
    jal ra, read_int
    mv s1, a0
    
    la s2, array_start

    mv t0, zero
read_loop:
    bge t0, s1, end_read_loop
    
    jal ra, read_int
    
    slli t1, t0, 2
    add t2, s2, t1
    sw a0, 0(t2)
    
    addi t0, t0, 1
    j read_loop
end_read_loop:

    li t0, 0
outer_loop:
    addi t3, s1, -1
    bge t0, t3, end_sort

    li t1, 0
    sub t4, s1, t0
    addi t4, t4, -1

inner_loop:
    bge t1, t4, end_inner # se j >= Limit, fim inner

    slli t5, t1, 2
    add t5, s2, t5
    lw t6, 0(t5)
    
    lw a2, 4(t5)

    ble t6, a2, no_swap   

    sw a2, 0(t5)
    sw t6, 4(t5)

no_swap:
    addi t1, t1, 1
    j inner_loop

end_inner:
    addi t0, t0, 1
    j outer_loop
end_sort:

    mv t0, zero
print_loop:
    bge t0, s1, end_program

    slli t1, t0, 2
    add t2, s2, t1
    lw a0, 0(t2)
    
    jal ra, print_int

    addi t3, s1, -1
    beq t0, t3, skip_comma
    li a0, 44             # ASCII ','
    jal ra, put_char
skip_comma:

    addi t0, t0, 1        # i++
    j print_loop

end_program:
    li t0, 0x10000000
    li t1, 10
    sb t1, 0(t0)
    ebreak

put_char:
    li t0, UART_BASE
    sb a0, 0(t0)
    ret

get_char:
    li t0, UART_BASE
    lb a0, 0(t0)
    ret

read_int:
    addi sp, sp, -4
    sw ra, 0(sp)
    
    mv t3, zero
    li t4, 0

skip_whitespace:
    jal ra, get_char
    li t1, 32
    beq a0, t1, skip_whitespace
    li t1, 10
    beq a0, t1, skip_whitespace
    li t1, 13
    beq a0, t1, skip_whitespace
    
    # Verifica sinal negativo
    li t1, 45
    bne a0, t1, check_digit
    li t4, 1
    j parse_loop_next_char

check_digit:
    j process_digit

parse_loop:
    jal ra, get_char
process_digit:
    li t1, 48
    li t2, 57
    blt a0, t1, end_parse
    bgt a0, t2, end_parse

    addi a0, a0, -48
    li t5, 10
    mul t3, t3, t5
    add t3, t3, a0
    
parse_loop_next_char:
    j parse_loop

end_parse:
    # Aplica o sinal
    beqz t4, return_val
    sub t3, zero, t3

return_val:
    mv a0, t3
    lw ra, 0(sp)
    addi sp, sp, 4
    ret

print_int:
    addi sp, sp, -8
    sw ra, 0(sp)
    sw s0, 4(sp)

    mv t0, a0
    
    bnez t0, check_neg_print
    li a0, 48
    jal ra, put_char
    j end_print_int

check_neg_print:
    bge t0, zero, positive_print
    
    li a0, 45
    sw t0, 0(sp)
    jal ra, put_char
    lw t0, 0(sp)
    sub t0, zero, t0

positive_print:
    li s0, 0
    li t1, 10

convert_loop:
    beqz t0, print_stack_loop
    rem t2, t0, t1
    div t0, t0, t1
    
    addi t2, t2, 48
    addi sp, sp, -4
    sw t2, 0(sp)
    addi s0, s0, 1
    j convert_loop

print_stack_loop:
    beqz s0, end_print_int
    lw a0, 0(sp)
    addi sp, sp, 4
    jal ra, put_char
    addi s0, s0, -1
    j print_stack_loop

end_print_int:
    lw s0, 4(sp)
    lw ra, 0(sp)
    addi sp, sp, 8
    ret

.section .bss
.align 4
# Reserva espaço para o array após o código
array_start:
    .space 4000