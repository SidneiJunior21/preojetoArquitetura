#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CSR_MSTATUS 0x300
#define CSR_MIE     0x304
#define CSR_MTVEC   0x305
#define CSR_MEPC    0x341
#define CSR_MCAUSE  0x342
#define CSR_MTVAL   0x343
#define CSR_MIP     0x344

#define CAUSE_INSN_ACCESS      0x1
#define CAUSE_ILLEGAL_INSTR    0x2
#define CAUSE_LOAD_ACCESS      0x5
#define CAUSE_STORE_ACCESS     0x7
#define CAUSE_ECALL_MMODE      0xb

#define INTERRUPT_BIT          0x80000000
#define CAUSE_MTI              (INTERRUPT_BIT | 7)
#define CAUSE_MEI              (INTERRUPT_BIT | 11)

#define CLINT_BASE  0x02000000
#define CLINT_SIZE  0x00010000
#define PLIC_BASE   0x0c000000
#define PLIC_SIZE   0x00400000
#define UART_BASE   0x10000000
#define UART_SIZE   0x100
#define RAM_BASE    0x80000000
#define MEM_SIZE    (1024 * 1024)

uint32_t registers[32];
uint32_t pc = 0x80000000;
uint32_t csrs[4096]; 
uint8_t memory[MEM_SIZE];

uint64_t mtime = 0;
uint64_t mtimecmp = -1;

uint32_t plic_regs[1024]; 

int trap_occurred = 0; 

FILE *terminal_file = NULL;
FILE *input_file = NULL;

const char* x_label[32] = { "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6" };

void raise_exception(uint32_t cause, uint32_t tval) {
    if (trap_occurred) return;

    csrs[CSR_MEPC] = pc;
    csrs[CSR_MCAUSE] = cause;
    csrs[CSR_MTVAL] = tval;
    
    pc = csrs[CSR_MTVEC] & ~0x3;
    trap_occurred = 1;
}

uint32_t bus_load(uint32_t addr, int size_bytes) {
    if (addr >= RAM_BASE && addr < (RAM_BASE + MEM_SIZE)) {
        uint32_t index = addr - RAM_BASE;
        if (index > MEM_SIZE - size_bytes) { raise_exception(CAUSE_LOAD_ACCESS, addr); return 0; }
        
        uint32_t val = 0;
        for(int i=0; i<size_bytes; i++) {
            val |= (uint32_t)memory[index + i] << (8*i);
        }
        return val;
    }
    else if (addr >= CLINT_BASE && addr < (CLINT_BASE + CLINT_SIZE)) {
        if (addr == 0x02004000) return (uint32_t)(mtimecmp);
        if (addr == 0x02004004) return (uint32_t)(mtimecmp >> 32);
        if (addr == 0x0200bff8) return (uint32_t)(mtime);
        if (addr == 0x0200bffc) return (uint32_t)(mtime >> 32);
        return 0;
    }
    else if (addr >= PLIC_BASE && addr < (PLIC_BASE + PLIC_SIZE)) {
        return 0; 
    }
    else if (addr >= UART_BASE && addr < (UART_BASE + UART_SIZE)) {
        int c;
        if (input_file != NULL) {
            c = fgetc(input_file);
        } else {
            c = getchar();
        }

        if (c == EOF) {
            static int eof_warned = 0;
            if (!eof_warned) {
                eof_warned = 1;
                return 10; 
            }
            return 0xFFFFFFFF;
        }
        return (uint32_t)c; 
    }

    raise_exception(CAUSE_LOAD_ACCESS, addr);
    return 0;
}

void bus_store(uint32_t addr, uint32_t value, int size_bytes) {
    if (addr >= RAM_BASE && addr < (RAM_BASE + MEM_SIZE)) {
        uint32_t index = addr - RAM_BASE;
        if (index > MEM_SIZE - size_bytes) { raise_exception(CAUSE_STORE_ACCESS, addr); return; }
        
        for(int i=0; i<size_bytes; i++) {
            memory[index + i] = (value >> (8*i)) & 0xFF;
        }
        return;
    }
    else if (addr == UART_BASE) {
        putchar((char)value);
        if (terminal_file != NULL) {
            fputc((char)value, terminal_file);
        }
        fflush(stdout);
        return;
    }
    else if (addr >= CLINT_BASE && addr < (CLINT_BASE + CLINT_SIZE)) {
        if (addr == 0x02004000) mtimecmp = (mtimecmp & 0xFFFFFFFF00000000) | value;
        else if (addr == 0x02004004) mtimecmp = (mtimecmp & 0x00000000FFFFFFFF) | ((uint64_t)value << 32);
        return;
    }
    else if (addr >= PLIC_BASE && addr < (PLIC_BASE + PLIC_SIZE)) {
        return;
    }

    raise_exception(CAUSE_STORE_ACCESS, addr);
}

uint32_t read_word_from_memory(uint32_t address) { return bus_load(address, 4); }
uint16_t read_half_word_from_memory(uint32_t address) { return (uint16_t)bus_load(address, 2); }
uint8_t read_byte_from_memory(uint32_t address) { return (uint8_t)bus_load(address, 1); }

void write_word_to_memory(uint32_t address, uint32_t value) { bus_store(address, value, 4); }
void write_half_word_to_memory(uint32_t address, uint16_t value) { bus_store(address, value, 2); }
void write_byte_to_memory(uint32_t address, uint8_t value) { bus_store(address, value, 1); }

void execute_instruction(uint32_t instruction, uint32_t current_pc, FILE *output_file) {
    uint32_t opcode = instruction & 0x7F;
    int pc_updated = 0;
    trap_occurred = 0;
    char operand_str[40]; 

    switch (opcode) {
        case 0x13: { // I-Type
            uint32_t rd = (instruction >> 7) & 0x1F;
            uint32_t funct3 = (instruction >> 12) & 0x7;
            uint32_t rs1 = (instruction >> 15) & 0x1F;
            int32_t imm = (int32_t)(instruction & 0xFFF00000) >> 20;
            uint32_t val_rs1 = registers[rs1];
            uint32_t res;
            uint32_t shamt = imm & 0x1F; 

            switch (funct3) {
                case 0x0: res = val_rs1 + imm; if (rd != 0) registers[rd] = res; sprintf(operand_str, "%s,%s,0x%03x", x_label[rd], x_label[rs1], (imm & 0xFFF)); fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x+0x%08x=0x%08x\n", current_pc, "addi", operand_str, x_label[rd], val_rs1, imm, res); break;
                case 0x1: res = val_rs1 << shamt; if (rd != 0) registers[rd] = res; sprintf(operand_str, "%s,%s,%u", x_label[rd], x_label[rs1], shamt); fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x<<%u=0x%08x\n", current_pc, "slli", operand_str, x_label[rd], val_rs1, shamt, res); break;
                case 0x2: res = ((int32_t)val_rs1 < imm) ? 1 : 0; if (rd != 0) registers[rd] = res; sprintf(operand_str, "%s,%s,0x%03x", x_label[rd], x_label[rs1], (imm & 0xFFF)); fprintf(output_file, "0x%08x:%-7s %-16s %s=(0x%08x<0x%08x)=%u\n", current_pc, "slti", operand_str, x_label[rd], val_rs1, imm, res); break;
                case 0x3: res = (val_rs1 < (uint32_t)imm) ? 1 : 0; if (rd != 0) registers[rd] = res; sprintf(operand_str, "%s,%s,0x%03x", x_label[rd], x_label[rs1], (imm & 0xFFF)); fprintf(output_file, "0x%08x:%-7s %-16s %s=(0x%08x<0x%08x)=%u\n", current_pc, "sltiu", operand_str, x_label[rd], val_rs1, imm, res); break;
                case 0x4: res = val_rs1 ^ imm; if (rd != 0) registers[rd] = res; sprintf(operand_str, "%s,%s,0x%03x", x_label[rd], x_label[rs1], (imm & 0xFFF)); fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x^0x%08x=0x%08x\n", current_pc, "xori", operand_str, x_label[rd], val_rs1, imm, res); break;
                case 0x5: { 
                    uint32_t funct7 = (instruction >> 25) & 0x7F;
                    if (funct7 == 0x00) { res = (uint32_t)val_rs1 >> shamt; if (rd != 0) registers[rd] = res; sprintf(operand_str, "%s,%s,%u", x_label[rd], x_label[rs1], shamt); fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x>>%u=0x%08x\n", current_pc, "srli", operand_str, x_label[rd], val_rs1, shamt, res); } 
                    else if (funct7 == 0x20) { res = (int32_t)val_rs1 >> shamt; if (rd != 0) registers[rd] = res; sprintf(operand_str, "%s,%s,%u", x_label[rd], x_label[rs1], shamt); fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x>>>%u=0x%08x\n", current_pc, "srai", operand_str, x_label[rd], val_rs1, shamt, res); }
                    break;
                }
                case 0x6: res = val_rs1 | imm; if (rd != 0) registers[rd] = res; sprintf(operand_str, "%s,%s,0x%03x", x_label[rd], x_label[rs1], (imm & 0xFFF)); fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x|0x%08x=0x%08x\n", current_pc, "ori", operand_str, x_label[rd], val_rs1, imm, res); break;
                case 0x7: res = val_rs1 & imm; if (rd != 0) registers[rd] = res; sprintf(operand_str, "%s,%s,0x%03x", x_label[rd], x_label[rs1], (imm & 0xFFF)); fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x&0x%08x=0x%08x\n", current_pc, "andi", operand_str, x_label[rd], val_rs1, imm, res); break;
                default: raise_exception(CAUSE_ILLEGAL_INSTR, instruction); break;
            }
            break;
        }
        case 0x33: { // R-Type
            uint32_t rd = (instruction >> 7) & 0x1F; uint32_t funct3 = (instruction >> 12) & 0x7; uint32_t rs1 = (instruction >> 15) & 0x1F; uint32_t rs2 = (instruction >> 20) & 0x1F; uint32_t funct7 = (instruction >> 25) & 0x7F;
            int32_t v_rs1 = registers[rs1]; int32_t v_rs2 = registers[rs2]; uint32_t v_urs1 = registers[rs1]; uint32_t v_urs2 = registers[rs2]; uint32_t shamt = v_urs2 & 0x1F; uint32_t res; 
            sprintf(operand_str, "%s,%s,%s", x_label[rd], x_label[rs1], x_label[rs2]);
            int instr_valid = 0;
            if (funct7 == 0x00) {
                switch (funct3) {
                    case 0x0: res = v_rs1 + v_rs2; instr_valid=1; fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x+0x%08x=0x%08x\n", current_pc, "add", operand_str, x_label[rd], v_rs1, v_rs2, res); break;
                    case 0x1: res = v_urs1 << shamt; instr_valid=1; fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x<<%u=0x%08x\n", current_pc, "sll", operand_str, x_label[rd], v_urs1, shamt, res); break;
                    case 0x2: res = (v_rs1 < v_rs2) ? 1 : 0; instr_valid=1; fprintf(output_file, "0x%08x:%-7s %-16s %s=(0x%08x<0x%08x)=%u\n", current_pc, "slt", operand_str, x_label[rd], v_rs1, v_rs2, res); break;
                    case 0x3: res = (v_urs1 < v_urs2) ? 1 : 0; instr_valid=1; fprintf(output_file, "0x%08x:%-7s %-16s %s=(0x%08x<0x%08x)=%u\n", current_pc, "sltu", operand_str, x_label[rd], v_urs1, v_urs2, res); break;
                    case 0x4: res = v_rs1 ^ v_rs2; instr_valid=1; fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x^0x%08x=0x%08x\n", current_pc, "xor", operand_str, x_label[rd], v_rs1, v_rs2, res); break;
                    case 0x5: res = v_urs1 >> shamt; instr_valid=1; fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x>>%u=0x%08x\n", current_pc, "srl", operand_str, x_label[rd], v_urs1, shamt, res); break;
                    case 0x6: res = v_rs1 | v_rs2; instr_valid=1; fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x|0x%08x=0x%08x\n", current_pc, "or", operand_str, x_label[rd], v_rs1, v_rs2, res); break;
                    case 0x7: res = v_rs1 & v_rs2; instr_valid=1; fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x&0x%08x=0x%08x\n", current_pc, "and", operand_str, x_label[rd], v_rs1, v_rs2, res); break;
                }
            } else if (funct7 == 0x20) {
                switch (funct3) {
                    case 0x0: res = v_rs1 - v_rs2; instr_valid=1; fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x-0x%08x=0x%08x\n", current_pc, "sub", operand_str, x_label[rd], v_rs1, v_rs2, res); break;
                    case 0x5: res = v_rs1 >> shamt; instr_valid=1; fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x>>>%u=0x%08x\n", current_pc, "sra", operand_str, x_label[rd], v_rs1, shamt, res); break;
                }
            } else if (funct7 == 0x01) {
                int64_t s64_rs1 = (int64_t)v_rs1; int64_t s64_rs2 = (int64_t)v_rs2; uint64_t u64_rs1 = (uint64_t)v_urs1; uint64_t u64_rs2 = (uint64_t)v_urs2;
                switch (funct3) {
                    case 0x0: res = v_rs1 * v_rs2; instr_valid=1; fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x*0x%08x=0x%08x\n", current_pc, "mul", operand_str, x_label[rd], v_rs1, v_rs2, res); break;
                    case 0x1: res = (uint32_t)((s64_rs1 * s64_rs2) >> 32); instr_valid=1; fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x*0x%08x=0x%08x\n", current_pc, "mulh", operand_str, x_label[rd], v_rs1, v_rs2, res); break;
                    case 0x2: res = (uint32_t)((s64_rs1 * u64_rs2) >> 32); instr_valid=1; fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x*0x%08x=0x%08x\n", current_pc, "mulhsu", operand_str, x_label[rd], v_rs1, v_urs2, res); break;
                    case 0x3: res = (uint32_t)((u64_rs1 * u64_rs2) >> 32); instr_valid=1; fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x*0x%08x=0x%08x\n", current_pc, "mulhu", operand_str, x_label[rd], v_urs1, v_urs2, res); break;
                    case 0x4: if(v_rs2==0)res=-1;else if(v_rs1==0x80000000&&v_rs2==-1)res=0x80000000;else res=v_rs1/v_rs2; instr_valid=1; fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x/0x%08x=0x%08x\n", current_pc, "div", operand_str, x_label[rd], v_rs1, v_rs2, res); break;
                    case 0x5: if(v_urs2==0)res=-1;else res=v_urs1/v_urs2; instr_valid=1; fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x/0x%08x=0x%08x\n", current_pc, "divu", operand_str, x_label[rd], v_urs1, v_urs2, res); break;
                    case 0x6: if(v_rs2==0)res=v_rs1;else if(v_rs1==0x80000000&&v_rs2==-1)res=0;else res=v_rs1%v_rs2; instr_valid=1; fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x%%0x%08x=0x%08x\n", current_pc, "rem", operand_str, x_label[rd], v_rs1, v_rs2, res); break;
                    case 0x7: if(v_urs2==0)res=v_urs1;else res=v_urs1%v_urs2; instr_valid=1; fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x%%0x%08x=0x%08x\n", current_pc, "remu", operand_str, x_label[rd], v_urs1, v_urs2, res); break;
                }
            }
            if (instr_valid) { if (rd != 0) registers[rd] = res; } 
            else { raise_exception(CAUSE_ILLEGAL_INSTR, instruction); }
            break;
        }
        case 0x6F: { // jal
            uint32_t rd = (instruction >> 7) & 0x1F; uint32_t imm_20 = (instruction >> 31) & 1; uint32_t imm_10_1 = (instruction >> 21) & 0x3FF; uint32_t imm_11 = (instruction >> 20) & 1; uint32_t imm_19_12 = (instruction >> 12) & 0xFF;
            int32_t offset = (imm_20 << 20) | (imm_19_12 << 12) | (imm_11 << 11) | (imm_10_1 << 1); offset = (int32_t)(offset << 11) >> 11;
            uint32_t return_address = current_pc + 4; uint32_t target_address = current_pc + offset; 
            if (rd != 0) registers[rd] = return_address; pc = target_address; pc_updated = 1;  
            sprintf(operand_str, "%s,0x%05x", x_label[rd], (offset >> 1) & 0xFFFFF); fprintf(output_file, "0x%08x:%-7s %-16s pc=0x%08x,%s=0x%08x\n", current_pc, "jal", operand_str, target_address, x_label[rd], return_address);
            break;
        }
        case 0x63: { // Branches
            uint32_t funct3 = (instruction >> 12) & 0x7; uint32_t rs1 = (instruction >> 15) & 0x1F; uint32_t rs2 = (instruction >> 20) & 0x1F;
            uint32_t imm_12 = (instruction >> 31) & 1; uint32_t imm_10_5 = (instruction >> 25) & 0x3F; uint32_t imm_4_1 = (instruction >> 8) & 0xF; uint32_t imm_11 = (instruction >> 7) & 1;
            int32_t offset = (imm_12 << 12) | (imm_11 << 11) | (imm_10_5 << 5) | (imm_4_1 << 1); offset = (int32_t)(offset << 19) >> 19;
            int32_t val_rs1 = registers[rs1]; int32_t val_rs2 = registers[rs2]; uint32_t u_val_rs1 = registers[rs1]; uint32_t u_val_rs2 = registers[rs2];
            int condition_met = 0; const char* instr_name = "???"; const char* op_symbol = "??"; int is_unsigned = 0;
            switch (funct3) {
                case 0x0: instr_name = "beq"; op_symbol = "=="; if (val_rs1 == val_rs2) condition_met = 1; break;
                case 0x1: instr_name = "bne"; op_symbol = "!="; if (val_rs1 != val_rs2) condition_met = 1; break;
                case 0x4: instr_name = "blt"; op_symbol = "<";  if (val_rs1 < val_rs2) condition_met = 1; break;
                case 0x5: instr_name = "bge"; op_symbol = ">="; if (val_rs1 >= val_rs2) condition_met = 1; break;
                case 0x6: instr_name = "bltu"; op_symbol = "<";  if (u_val_rs1 < u_val_rs2) condition_met = 1; is_unsigned = 1; break;
                case 0x7: instr_name = "bgeu"; op_symbol = ">="; if (u_val_rs1 >= u_val_rs2) condition_met = 1; is_unsigned = 1; break;
                default: raise_exception(CAUSE_ILLEGAL_INSTR, instruction); return;
            }
            if (is_unsigned) { fprintf(output_file, "0x%08x:%-7s %s,%s,0x%03x   (0x%08x%s0x%08x)=%d->pc=0x%08x\n", current_pc, instr_name, x_label[rs1], x_label[rs2], (offset >> 1) & 0xFFF, u_val_rs1, op_symbol, u_val_rs2, condition_met, (condition_met ? (current_pc + offset) : (current_pc + 4))); } 
            else { fprintf(output_file, "0x%08x:%-7s %s,%s,0x%03x   (0x%08x%s0x%08x)=%d->pc=0x%08x\n", current_pc, instr_name, x_label[rs1], x_label[rs2], (offset >> 1) & 0xFFF, val_rs1, op_symbol, val_rs2, condition_met, (condition_met ? (current_pc + offset) : (current_pc + 4))); }
            if (condition_met) { pc = current_pc + offset; pc_updated = 1; }
            break;
        }
        case 0x37: { // lui
            uint32_t rd = (instruction >> 7) & 0x1F; uint32_t imm_u = instruction & 0xFFFFF000; 
            if (rd != 0) registers[rd] = imm_u;
            sprintf(operand_str, "%s,0x%05x", x_label[rd], (imm_u >> 12)); fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x\n", current_pc, "lui", operand_str, x_label[rd], imm_u); 
            break;
        }
        case 0x17: { // auipc
            uint32_t rd = (instruction >> 7) & 0x1F; int32_t imm_u = (int32_t)(instruction & 0xFFFFF000); uint32_t res = current_pc + imm_u;
            if (rd != 0) registers[rd] = res;
            sprintf(operand_str, "%s,0x%05x", x_label[rd], (imm_u >> 12) & 0xFFFFF); fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x+0x%08x=0x%08x\n", current_pc, "auipc", operand_str, x_label[rd], current_pc, imm_u, res);
            break;
        }
        case 0x67: { // jalr
            uint32_t rd = (instruction >> 7) & 0x1F; uint32_t rs1 = (instruction >> 15) & 0x1F; int32_t imm = (int32_t)(instruction & 0xFFF00000) >> 20; 
            uint32_t val_rs1 = registers[rs1]; uint32_t return_address = current_pc + 4; uint32_t target_address = (val_rs1 + imm) & ~1;
            if (rd != 0) registers[rd] = return_address; pc = target_address; pc_updated = 1;
            sprintf(operand_str, "%s,%s,0x%03x", x_label[rd], x_label[rs1], (imm & 0xFFF)); fprintf(output_file, "0x%08x:%-7s %-16s pc=0x%08x+0x%08x,%s=0x%08x\n", current_pc, "jalr", operand_str, val_rs1, imm, x_label[rd], return_address);
            break;
        }
        case 0x03: { // Loads
            uint32_t rd = (instruction >> 7) & 0x1F; uint32_t funct3 = (instruction >> 12) & 0x7; uint32_t rs1 = (instruction >> 15) & 0x1F; int32_t imm = (int32_t)(instruction & 0xFFF00000) >> 20;
            uint32_t val_rs1 = registers[rs1]; uint32_t address = val_rs1 + imm; uint32_t res = 0; const char* instr_name = "???";
            sprintf(operand_str, "%s,0x%03x(%s)", x_label[rd], (imm & 0xFFF), x_label[rs1]);
            switch (funct3) {
                case 0x0: instr_name = "lb";  { int8_t  b = (int8_t) read_byte_from_memory(address); if(!trap_occurred) res = (int32_t)b; } break;
                case 0x1: instr_name = "lh";  { int16_t h = (int16_t)read_half_word_from_memory(address); if(!trap_occurred) res = (int32_t)h; } break;
                case 0x2: instr_name = "lw";  { res = read_word_from_memory(address); } break;
                case 0x4: instr_name = "lbu"; { uint8_t b = read_byte_from_memory(address); if(!trap_occurred) res = (uint32_t)b; } break;
                case 0x5: instr_name = "lhu"; { uint16_t h = read_half_word_from_memory(address); if(!trap_occurred) res = (uint32_t)h; } break;
                default: raise_exception(CAUSE_ILLEGAL_INSTR, instruction); return;
            }
            if (!trap_occurred) { if(rd != 0) registers[rd] = res; fprintf(output_file, "0x%08x:%-7s %-16s %s=mem[0x%08x]=0x%08x\n", current_pc, instr_name, operand_str, x_label[rd], address, res); }
            break;
        }
        case 0x23: { // Stores
            uint32_t funct3 = (instruction >> 12) & 0x7; uint32_t rs1 = (instruction >> 15) & 0x1F; uint32_t rs2 = (instruction >> 20) & 0x1F;
            uint32_t imm_11_5 = (instruction >> 25) & 0x7F; uint32_t imm_4_0  = (instruction >> 7) & 0x1F; int32_t imm = (imm_11_5 << 5) | imm_4_0; imm = (int32_t)(imm << 20) >> 20;
            uint32_t val_rs1 = registers[rs1]; uint32_t val_rs2 = registers[rs2]; uint32_t address = val_rs1 + imm; const char* instr_name = "???";
            sprintf(operand_str, "%s,0x%03x(%s)", x_label[rs2], (imm & 0xFFF), x_label[rs1]);
            switch (funct3) {
                case 0x0: instr_name = "sb"; write_byte_to_memory(address, (uint8_t)val_rs2); if(!trap_occurred) fprintf(output_file, "0x%08x:%-7s %-16s mem[0x%08x]=0x%02x\n", current_pc, instr_name, operand_str, address, (uint8_t)val_rs2); break;
                case 0x1: instr_name = "sh"; write_half_word_to_memory(address, (uint16_t)val_rs2); if(!trap_occurred) fprintf(output_file, "0x%08x:%-7s %-16s mem[0x%08x]=0x%04x\n", current_pc, instr_name, operand_str, address, (uint16_t)val_rs2); break;
                case 0x2: instr_name = "sw"; write_word_to_memory(address, val_rs2); if(!trap_occurred) fprintf(output_file, "0x%08x:%-7s %-16s mem[0x%08x]=0x%08x\n", current_pc, instr_name, operand_str, address, val_rs2); break;
                default: raise_exception(CAUSE_ILLEGAL_INSTR, instruction); return; 
            }
            break;
        }
        case 0x73: { // SYSTEM / CSR
            uint32_t funct3 = (instruction >> 12) & 0x7; uint32_t rd = (instruction >> 7) & 0x1F; uint32_t rs1 = (instruction >> 15) & 0x1F; uint32_t csr_addr = (instruction >> 20) & 0xFFF; uint32_t uimm = rs1; 
            if (funct3 == 0) {
                if (csr_addr == 0) { raise_exception(CAUSE_ECALL_MMODE, 0); fprintf(output_file, "0x%08x:ecall\n", current_pc); }
                else if (csr_addr == 1) { fprintf(output_file, "0x%08x:ebreak\n", current_pc); }
                else if (csr_addr == 0x302) { pc = csrs[CSR_MEPC]; pc_updated = 1; fprintf(output_file, "0x%08x:mret\n", current_pc); }
                else { raise_exception(CAUSE_ILLEGAL_INSTR, instruction); }
            } else {
                uint32_t csr_val = csrs[csr_addr]; uint32_t new_val = csr_val;
                switch (funct3) {
                    case 0x1: new_val = registers[rs1]; sprintf(operand_str, "%s,%s,0x%03x", x_label[rd], x_label[rs1], csr_addr); fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x\n", current_pc, "csrrw", operand_str, x_label[rd], csr_val); break;
                    case 0x2: new_val = csr_val | registers[rs1]; sprintf(operand_str, "%s,%s,0x%03x", x_label[rd], x_label[rs1], csr_addr); fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x\n", current_pc, "csrrs", operand_str, x_label[rd], csr_val); break;
                    case 0x3: new_val = csr_val & ~registers[rs1]; sprintf(operand_str, "%s,%s,0x%03x", x_label[rd], x_label[rs1], csr_addr); fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x\n", current_pc, "csrrc", operand_str, x_label[rd], csr_val); break;
                    case 0x5: new_val = uimm; sprintf(operand_str, "%s,0x%x,0x%03x", x_label[rd], uimm, csr_addr); fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x\n", current_pc, "csrrwi", operand_str, x_label[rd], csr_val); break;
                    case 0x6: new_val = csr_val | uimm; sprintf(operand_str, "%s,0x%x,0x%03x", x_label[rd], uimm, csr_addr); fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x\n", current_pc, "csrrsi", operand_str, x_label[rd], csr_val); break;
                    case 0x7: new_val = csr_val & ~uimm; sprintf(operand_str, "%s,0x%x,0x%03x", x_label[rd], uimm, csr_addr); fprintf(output_file, "0x%08x:%-7s %-16s %s=0x%08x\n", current_pc, "csrrci", operand_str, x_label[rd], csr_val); break;
                    default: raise_exception(CAUSE_ILLEGAL_INSTR, instruction); return;
                }
                csrs[csr_addr] = new_val; if (rd != 0) registers[rd] = csr_val;
            }
            break;
        }
        default: raise_exception(CAUSE_ILLEGAL_INSTR, instruction); fprintf(output_file, "Erro: Opcode 0x%x desconhecido em 0x%08x (Trap)\n", opcode, current_pc); break;
    }
    if (!pc_updated && !trap_occurred) { pc += 4; }
}

int main(int argc, char *argv[]) {
    if (argc < 3) { 
        fprintf(stderr, "Uso: %s <arquivo.hex> <arquivo.out> [arquivo.in]\n", argv[0]); 
        return 1; 
    }
    FILE *hex_file = fopen(argv[1], "r"); 
    if (hex_file == NULL) { perror("Erro hex"); return 1; }

    FILE *output_file = fopen(argv[2], "w"); 
    if (output_file == NULL) { fclose(hex_file); perror("Erro out"); return 1; }

    if (argc >= 4) {
        input_file = fopen(argv[3], "r");
        if (input_file == NULL) {
            perror("Erro ao abrir arquivo de entrada (.in)");
            fclose(hex_file); fclose(output_file);
            return 1;
        }
        printf("Lendo entrada do arquivo: %s\n", argv[3]);
    } else {
        printf("Lendo entrada do Teclado (stdin)\n");
    }
    
    terminal_file = fopen("terminal.out", "w");
    if (terminal_file == NULL) {
        perror("Erro ao criar terminal.out");
        fclose(hex_file);
        fclose(output_file);
        return 1;
    }

    memset(memory, 0, MEM_SIZE); memset(csrs, 0, sizeof(csrs));
    char line[1024]; uint32_t current_address = 0; int address_set = 0;

    while (fgets(line, sizeof(line), hex_file)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '@') { current_address = (uint32_t)strtoul(&line[1], NULL, 16); address_set = 1; } 
        else if (address_set && strlen(line) > 0) {
            char *token = strtok(line, " ");
            while (token != NULL) {
                if (token == NULL) break;
                uint32_t byte = (uint32_t)strtoul(token, NULL, 16);
                uint32_t idx = current_address - 0x80000000;
                if (idx < MEM_SIZE) memory[idx] = byte;
                current_address++; token = strtok(NULL, " ");
            }
        }
    }
    fclose(hex_file);
    printf("Programa '%s' carregado. Iniciando simulação, saída em %s\n", argv[1], argv[2]);
    
    while (1) {
        if (pc == 0) {
            printf("\n[Simulador] Erro Fatal: O PC foi para 0x0.\n");
            printf("[Simulador] Provavel causa: Excecao sem tratamento (mtvec=0) ou estouro de pilha.\n");
            break; 
        }
        if (pc % 4 != 0) { raise_exception(CAUSE_INSN_ACCESS, pc); continue; }
        uint32_t idx = pc - 0x80000000;
        if (idx > MEM_SIZE - 4) { raise_exception(CAUSE_INSN_ACCESS, pc); continue; }

        uint32_t instruction = memory[idx] | (memory[idx+1] << 8) | (memory[idx+2] << 16) | (memory[idx+3] << 24);
        uint32_t pc_atual = pc;

        if (instruction == 0x00100073) { fprintf(output_file, "0x%08x:ebreak\n", pc_atual); printf("Simulação terminada (ebreak).\n"); break; }
        if (instruction == 0) { printf("Simulação terminada (instrução nula). PC=0x%x\n", pc_atual); break; }
        
        execute_instruction(instruction, pc_atual, output_file);
        
        mtime++;
        if (mtime >= mtimecmp) {
            csrs[CSR_MIP] |= 0x80;
        } else {
            csrs[CSR_MIP] &= ~0x80;
        }

        uint32_t mstatus = csrs[CSR_MSTATUS];
        uint32_t mie = csrs[CSR_MIE];
        uint32_t mip = csrs[CSR_MIP];

        if ((mstatus & 0x8) && (mie & 0x80) && (mip & 0x80)) {
            raise_exception(CAUSE_MTI, 0); 
        }
        
        registers[0] = 0;
    }
    
    if (terminal_file) fclose(terminal_file);

    fclose(output_file);
    if (input_file) fclose(input_file);
    return 0;
}