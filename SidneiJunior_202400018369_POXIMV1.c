#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t registers[32];
uint32_t pc = 0x80000000;

#define MEM_SIZE (1024 * 1024)
uint8_t memory[MEM_SIZE];

const char* x_label[32] = { "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6" };

uint32_t read_word_from_memory(uint32_t address) {
    
    uint32_t index = address - 0x80000000;
    
    if (index > MEM_SIZE - 4) {
        printf("Erro: Leitura de memória fora dos limites! Endereço: 0x%x\n", address);
        return 0;
    }

    uint32_t byte0 = memory[index];
    uint32_t byte1 = memory[index + 1];
    uint32_t byte2 = memory[index + 2];
    uint32_t byte3 = memory[index + 3];
    return byte0 | (byte1 << 8) | (byte2 << 16) | (byte3 << 24);
}

void write_word_to_memory(uint32_t address, uint32_t value) {
    uint32_t index = address - 0x80000000; 

    if (index > MEM_SIZE - 4) {
        printf("Erro: Escrita de memória fora dos limites! Endereço: 0x%x\n", address);
        return;
    }

    memory[index]     = (value & 0x000000FF);
    memory[index + 1] = (value & 0x0000FF00) >> 8;
    memory[index + 2] = (value & 0x00FF0000) >> 16;
    memory[index + 3] = (value & 0xFF000000) >> 24;
}

uint8_t read_byte_from_memory(uint32_t address) {
    uint32_t index = address - 0x80000000;
    if (index > MEM_SIZE - 1) { 
        printf("Erro: Leitura de byte fora dos limites! Endereço: 0x%x\n", address);
        return 0; 
    }
    return memory[index];
}

uint16_t read_half_word_from_memory(uint32_t address) {
    uint32_t index = address - 0x80000000;
    if (index > MEM_SIZE - 2) { 
        printf("Erro: Leitura de half-word fora dos limites! Endereço: 0x%x\n", address);
        return 0; 
    }
    uint16_t byte0 = memory[index];
    uint16_t byte1 = memory[index + 1];
    return byte0 | (byte1 << 8);
}

void write_byte_to_memory(uint32_t address, uint8_t value) {
    uint32_t index = address - 0x80000000;
    if (index > MEM_SIZE - 1) { 
        printf("Erro: Escrita de byte fora dos limites! Endereço: 0x%x\n", address);
        return; 
    }
    memory[index] = value;
}

void write_half_word_to_memory(uint32_t address, uint16_t value) {
    uint32_t index = address - 0x80000000;
    if (index > MEM_SIZE - 2) { 
        printf("Erro: Escrita de half-word fora dos limites! Endereço: 0x%x\n", address);
        return; 
    }
    memory[index]     = (value & 0x00FF);
    memory[index + 1] = (value & 0xFF00) >> 8;
}



void execute_instruction(uint32_t instruction, uint32_t current_pc, FILE *output_file) {
    
    uint32_t opcode = instruction & 0x7F;

    int pc_updated = 0;

    switch (opcode) {
        case 0x13: {
            uint32_t rd    = (instruction >> 7) & 0x1F;
            uint32_t funct3 = (instruction >> 12) & 0x7;
            uint32_t rs1   = (instruction >> 15) & 0x1F;
            int32_t imm = (int32_t)(instruction & 0xFFF00000) >> 20;

            uint32_t val_rs1 = registers[rs1];
            uint32_t res;

            switch (funct3) {
                case 0x0: { // addi
                    res = val_rs1 + imm;
                    if (rd != 0) registers[rd] = res;
                    fprintf(output_file, "0x%08x:%-7s %s,%s,0x%03x   %s=0x%08x+0x%08x=0x%08x\n", 
                           current_pc, "addi", 
                           x_label[rd], x_label[rs1], (imm & 0xFFF),
                           x_label[rd], val_rs1, imm, res);
                    break;
                }
                
                case 0x1: { // slli
                    uint32_t shamt = imm & 0x1F;
                    res = val_rs1 << shamt;
                    if (rd != 0) registers[rd] = res;
                    fprintf(output_file, "0x%08x:%-7s %s,%s,%u   %s=0x%08x<<%u=0x%08x\n",
                           current_pc, "slli",
                           x_label[rd], x_label[rs1], shamt,
                           x_label[rd], val_rs1, shamt, res);
                    break;
                }
                
                case 0x2: { // slti
                    res = ((int32_t)val_rs1 < imm) ? 1 : 0;
                    if (rd != 0) registers[rd] = res;
                    fprintf(output_file, "0x%08x:%-7s %s,%s,0x%03x   %s=(0x%08x<0x%08x)=%u\n",
                           current_pc, "slti",
                           x_label[rd], x_label[rs1], (imm & 0xFFF),
                           x_label[rd], val_rs1, imm, res);
                    break;
                }

                case 0x3: { // sltiu
                    res = (val_rs1 < (uint32_t)imm) ? 1 : 0;
                    if (rd != 0) registers[rd] = res;
                    fprintf(output_file, "0x%08x:%-7s %s,%s,0x%03x   %s=(0x%08x<0x%08x)=%u\n",
                           current_pc, "sltiu",
                           x_label[rd], x_label[rs1], (imm & 0xFFF),
                           x_label[rd], val_rs1, imm, res);
                    break;
                }

                case 0x4: { // xori
                    res = val_rs1 ^ imm;
                    if (rd != 0) registers[rd] = res;
                    fprintf(output_file, "0x%08x:%-7s %s,%s,0x%03x   %s=0x%08x^0x%08x=0x%08x\n",
                           current_pc, "xori",
                           x_label[rd], x_label[rs1], (imm & 0xFFF),
                           x_label[rd], val_rs1, imm, res);
                    break;
                }
                
                case 0x5: { // srli e srai
                    uint32_t shamt = imm & 0x1F;
                    uint32_t funct7 = (instruction >> 25) & 0x7F;
                    
                    if (funct7 == 0x00) { // srli
                        res = (uint32_t)val_rs1 >> shamt;
                        if (rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%u   %s=0x%08x>>%u=0x%08x\n",
                               current_pc, "srli",
                               x_label[rd], x_label[rs1], shamt,
                               x_label[rd], val_rs1, shamt, res);
                    } else if (funct7 == 0x20) { // srai
                        res = (int32_t)val_rs1 >> shamt; 
                        if (rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%u   %s=0x%08x>>>%u=0x%08x\n",
                               current_pc, "srai",
                               x_label[rd], x_label[rs1], shamt,
                               x_label[rd], val_rs1, shamt, res);
                    }
                    break;
                }

                case 0x6: { // ori
                    res = val_rs1 | imm;
                    if (rd != 0) registers[rd] = res;
                    fprintf(output_file, "0x%08x:%-7s %s,%s,0x%03x   %s=0x%08x|0x%08x=0x%08x\n",
                           current_pc, "ori",
                           x_label[rd], x_label[rs1], (imm & 0xFFF),
                           x_label[rd], val_rs1, imm, res);
                    break;
                }

                case 0x7: { // andi
                    res = val_rs1 & imm;
                    if (rd != 0) registers[rd] = res;
                    fprintf(output_file, "0x%08x:%-7s %s,%s,0x%03x   %s=0x%08x&0x%08x=0x%08x\n",
                           current_pc, "andi",
                           x_label[rd], x_label[rs1], (imm & 0xFFF),
                           x_label[rd], val_rs1, imm, res);
                    break;
                }

                default:
                    fprintf(output_file, "Erro: funct3 0x%x desconhecido para opcode I-TYPE (0x13)!\n", funct3);
                    printf("Erro: funct3 0x%x desconhecido para opcode I-TYPE (0x13)!\n", funct3);
                    pc = 0;
            }
            break;
        }
        case 0x33: {
            uint32_t rd    = (instruction >> 7) & 0x1F;
            uint32_t funct3 = (instruction >> 12) & 0x7;
            uint32_t rs1   = (instruction >> 15) & 0x1F;
            uint32_t rs2   = (instruction >> 20) & 0x1F;
            uint32_t funct7 = (instruction >> 25) & 0x7F;

            int32_t v_rs1 = registers[rs1];
            int32_t v_rs2 = registers[rs2];
            uint32_t v_urs1 = registers[rs1];
            uint32_t v_urs2 = registers[rs2];
            uint32_t shamt = v_urs2 & 0x1F; 
            uint32_t res; 

            if (funct7 == 0x00) {
                switch (funct3) {
                    case 0x0: { // add
                        res = v_rs1 + v_rs2;
                        if(rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%s   %s=0x%08x+0x%08x=0x%08x\n",
                               current_pc, "add", x_label[rd], x_label[rs1], x_label[rs2],
                               x_label[rd], v_rs1, v_rs2, res);
                        break;
                    }
                    case 0x1: { // sll
                        res = v_urs1 << shamt;
                        if(rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%s   %s=0x%08x<<%u=0x%08x\n",
                               current_pc, "sll", x_label[rd], x_label[rs1], x_label[rs2],
                               x_label[rd], v_urs1, shamt, res);
                        break;
                    }
                    case 0x2: { // slt
                        res = (v_rs1 < v_rs2) ? 1 : 0;
                        if(rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%s   %s=(0x%08x<0x%08x)=%u\n",
                               current_pc, "slt", x_label[rd], x_label[rs1], x_label[rs2],
                               x_label[rd], v_rs1, v_rs2, res);
                        break;
                    }
                    case 0x3: { // sltu
                        res = (v_urs1 < v_urs2) ? 1 : 0;
                        if(rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%s   %s=(0x%08x<0x%08x)=%u\n",
                               current_pc, "sltu", x_label[rd], x_label[rs1], x_label[rs2],
                               x_label[rd], v_urs1, v_urs2, res);
                        break;
                    }
                    case 0x4: { // xor
                        res = v_rs1 ^ v_rs2;
                        if(rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%s   %s=0x%08x^0x%08x=0x%08x\n",
                               current_pc, "xor", x_label[rd], x_label[rs1], x_label[rs2],
                               x_label[rd], v_rs1, v_rs2, res);
                        break;
                    }
                    case 0x5: { // srl
                        res = v_urs1 >> shamt;
                        if(rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%s   %s=0x%08x>>%u=0x%08x\n",
                               current_pc, "srl", x_label[rd], x_label[rs1], x_label[rs2],
                               x_label[rd], v_urs1, shamt, res);
                        break;
                    }
                    case 0x6: { // or
                        res = v_rs1 | v_rs2;
                        if(rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%s   %s=0x%08x|0x%08x=0x%08x\n",
                               current_pc, "or", x_label[rd], x_label[rs1], x_label[rs2],
                               x_label[rd], v_rs1, v_rs2, res);
                        break;
                    }
                    case 0x7: { // and
                        res = v_rs1 & v_rs2;
                        if(rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%s   %s=0x%08x&0x%08x=0x%08x\n",
                               current_pc, "and", x_label[rd], x_label[rs1], x_label[rs2],
                               x_label[rd], v_rs1, v_rs2, res);
                        break;
                    }
                }
            } else if (funct7 == 0x20) {
                switch (funct3) {
                    case 0x0: { // sub
                        res = v_rs1 - v_rs2;
                        if(rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%s   %s=0x%08x-0x%08x=0x%08x\n",
                               current_pc, "sub", x_label[rd], x_label[rs1], x_label[rs2],
                               x_label[rd], v_rs1, v_rs2, res);
                        break;
                    }
                    case 0x5: { // sra
                        res = v_rs1 >> shamt;
                        if(rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%s   %s=0x%08x>>>%u=0x%08x\n",
                               current_pc, "sra", x_label[rd], x_label[rs1], x_label[rs2],
                               x_label[rd], v_rs1, shamt, res);
                        break;
                    }
                }
            } else if (funct7 == 0x01) {
                int64_t s64_rs1 = (int64_t)v_rs1;
                int64_t s64_rs2 = (int64_t)v_rs2;
                uint64_t u64_rs1 = (uint64_t)v_urs1;
                uint64_t u64_rs2 = (uint64_t)v_urs2;

                switch (funct3) {
                    case 0x0: { // mul
                        res = v_rs1 * v_rs2;
                        if(rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%s   %s=0x%08x*0x%08x=0x%08x\n",
                               current_pc, "mul", x_label[rd], x_label[rs1], x_label[rs2],
                               x_label[rd], v_rs1, v_rs2, res);
                        break;
                    }
                    case 0x1: { // mulh
                        res = (uint32_t)((s64_rs1 * s64_rs2) >> 32);
                        if(rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%s   %s=0x%08x*0x%08x=0x%08x\n",
                               current_pc, "mulh", x_label[rd], x_label[rs1], x_label[rs2],
                               x_label[rd], v_rs1, v_rs2, res);
                        break;
                    }
                    case 0x2: { // mulhsu
                        res = (uint32_t)((s64_rs1 * u64_rs2) >> 32);
                        if(rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%s   %s=0x%08x*0x%08x=0x%08x\n",
                               current_pc, "mulhsu", x_label[rd], x_label[rs1], x_label[rs2],
                               x_label[rd], v_rs1, v_urs2, res);
                        break;
                    }
                    case 0x3: { // mulhu
                        res = (uint32_t)((u64_rs1 * u64_rs2) >> 32);
                        if(rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%s   %s=0x%08x*0x%08x=0x%08x\n",
                               current_pc, "mulhu", x_label[rd], x_label[rs1], x_label[rs2],
                               x_label[rd], v_urs1, v_urs2, res);
                        break;
                    }
                    case 0x4: { // div
                        if (v_rs2 == 0) res = 0xFFFFFFFF; 
                        else if (v_rs1 == 0x80000000 && v_rs2 == -1) res = 0x80000000;
                        else res = v_rs1 / v_rs2;
                        
                        if(rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%s   %s=0x%08x/0x%08x=0x%08x\n",
                               current_pc, "div", x_label[rd], x_label[rs1], x_label[rs2],
                               x_label[rd], v_rs1, v_rs2, res);
                        break;
                    }
                    case 0x5: { // divu
                        if (v_urs2 == 0) res = 0xFFFFFFFF;
                        else res = v_urs1 / v_urs2;
                        
                        if(rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%s   %s=0x%08x/0x%08x=0x%08x\n",
                               current_pc, "divu", x_label[rd], x_label[rs1], x_label[rs2],
                               x_label[rd], v_urs1, v_urs2, res);
                        break;
                    }
                    case 0x6: { // rem
                        if (v_rs2 == 0) res = v_rs1; 
                        else if (v_rs1 == 0x80000000 && v_rs2 == -1) res = 0;
                        else res = v_rs1 % v_rs2;
                        
                        if(rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%s   %s=0x%08x%%0x%08x=0x%08x\n",
                               current_pc, "rem", x_label[rd], x_label[rs1], x_label[rs2],
                               x_label[rd], v_rs1, v_rs2, res);
                        break;
                    }
                    case 0x7: { // remu
                        if (v_urs2 == 0) res = v_urs1; 
                        else res = v_urs1 % v_urs2;
                        
                        if(rd != 0) registers[rd] = res;
                        fprintf(output_file, "0x%08x:%-7s %s,%s,%s   %s=0x%08x%%0x%08x=0x%08x\n",
                               current_pc, "remu", x_label[rd], x_label[rs1], x_label[rs2],
                               x_label[rd], v_urs1, v_urs2, res);
                        break;
                    }
                }
            }
            break;
        }
        case 0x6F: { // jal
            uint32_t rd = (instruction >> 7) & 0x1F;
            
            uint32_t imm_20 = (instruction >> 31) & 1;
            uint32_t imm_10_1 = (instruction >> 21) & 0x3FF;
            uint32_t imm_11 = (instruction >> 20) & 1;
            uint32_t imm_19_12 = (instruction >> 12) & 0xFF;
            
            int32_t offset = (imm_20 << 20) | (imm_19_12 << 12) | (imm_11 << 11) | (imm_10_1 << 1);
            offset = (int32_t)(offset << 11) >> 11;

            uint32_t return_address = current_pc + 4;
            uint32_t target_address = current_pc + offset; 

            if (rd != 0) {
                registers[rd] = return_address;
            }
            pc = target_address;
            pc_updated = 1;  
            
            fprintf(output_file, "0x%08x:jal    %s,0x%05x        pc=0x%08x,%s=0x%08x\n",
                   current_pc,
                   x_label[rd],
                   (offset >> 1) & 0xFFFFF, 
                   target_address,
                   x_label[rd],
                   return_address);
            break;
        }
        case 0x63: {
            uint32_t funct3 = (instruction >> 12) & 0x7;
            uint32_t rs1   = (instruction >> 15) & 0x1F;
            uint32_t rs2   = (instruction >> 20) & 0x1F;
            
            uint32_t imm_12 = (instruction >> 31) & 1; 
            uint32_t imm_10_5 = (instruction >> 25) & 0x3F;
            uint32_t imm_4_1 = (instruction >> 8) & 0xF;
            uint32_t imm_11 = (instruction >> 7) & 1;
            int32_t offset = (imm_12 << 12) | (imm_11 << 11) | (imm_10_5 << 5) | (imm_4_1 << 1);
            offset = (int32_t)(offset << 19) >> 19;

            int32_t val_rs1 = registers[rs1];
            int32_t val_rs2 = registers[rs2];
            uint32_t u_val_rs1 = registers[rs1];
            uint32_t u_val_rs2 = registers[rs2];

            int condition_met = 0;
            const char* instr_name = "???";
            const char* op_symbol = "??";
            int is_unsigned = 0;

            switch (funct3) {
                case 0x0: instr_name = "beq"; op_symbol = "=="; if (val_rs1 == val_rs2) condition_met = 1; break;
                case 0x1: instr_name = "bne"; op_symbol = "!="; if (val_rs1 != val_rs2) condition_met = 1; break;
                case 0x4: instr_name = "blt"; op_symbol = "<";  if (val_rs1 < val_rs2) condition_met = 1; break;
                case 0x5: instr_name = "bge"; op_symbol = ">="; if (val_rs1 >= val_rs2) condition_met = 1; break;
                case 0x6: instr_name = "bltu"; op_symbol = "<";  if (u_val_rs1 < u_val_rs2) condition_met = 1; is_unsigned = 1; break;
                case 0x7: instr_name = "bgeu"; op_symbol = ">="; if (u_val_rs1 >= u_val_rs2) condition_met = 1; is_unsigned = 1; break;
            }
            if (is_unsigned) {
                fprintf(output_file, "0x%08x:%-7s %s,%s,0x%03x         (0x%08x%s0x%08x)=%d->pc=0x%08x\n",
                       current_pc,
                       instr_name,
                       x_label[rs1], x_label[rs2],
                       (offset >> 1) & 0xFFF, 
                       u_val_rs1, op_symbol, u_val_rs2, 
                       condition_met,
                       (condition_met ? (current_pc + offset) : (current_pc + 4))
                       );
            } else {
                fprintf(output_file, "0x%08x:%-7s %s,%s,0x%03x         (0x%08x%s0x%08x)=%d->pc=0x%08x\n",
                       current_pc,
                       instr_name,
                       x_label[rs1], x_label[rs2],
                       (offset >> 1) & 0xFFF, 
                       val_rs1, op_symbol, val_rs2,
                       condition_met,
                       (condition_met ? (current_pc + offset) : (current_pc + 4))
                       );
            }
            if (condition_met) {
                pc = current_pc + offset;
                pc_updated = 1;
            }
            break;
        }
        case 0x37: { // lui
            uint32_t rd = (instruction >> 7) & 0x1F;
            uint32_t imm_u = instruction & 0xFFFFF000; 
            
            if (rd != 0) registers[rd] = imm_u;

            fprintf(output_file, "0x%08x:lui    %s,0x%05x       %s=0x%08x\n",
                   current_pc,
                   x_label[rd],
                   (imm_u >> 12), 
                   x_label[rd],
                   imm_u); 
            break;
        }
        case 0x17: { // auipc
            uint32_t rd = (instruction >> 7) & 0x1F;
            int32_t imm_u = (int32_t)(instruction & 0xFFFFF000); 
            
            uint32_t res = current_pc + imm_u;

            if (rd != 0) registers[rd] = res;
            fprintf(output_file, "0x%08x:auipc  %s,0x%05x       %s=0x%08x+0x%08x=0x%08x\n",
                   current_pc,
                   x_label[rd],
                   (imm_u >> 12) & 0xFFFFF,
                   x_label[rd],
                   current_pc,
                   imm_u,
                   res);
            break;
        }
        case 0x67: { // jalr
            uint32_t rd = (instruction >> 7) & 0x1F;
            uint32_t rs1 = (instruction >> 15) & 0x1F;
            int32_t imm = (int32_t)(instruction & 0xFFF00000) >> 20; 

            uint32_t val_rs1 = registers[rs1];
            uint32_t return_address = current_pc + 4;
            uint32_t target_address = (val_rs1 + imm) & ~1;

            if (rd != 0) {
                registers[rd] = return_address;
            }
            pc = target_address; 
            pc_updated = 1;
            
            fprintf(output_file, "0x%08x:%-7s %s,%s,0x%03x       pc=0x%08x+0x%08x,%s=0x%08x\n",
                   current_pc, "jalr",
                   x_label[rd], x_label[rs1], (imm & 0xFFF),
                   val_rs1, imm,
                   x_label[rd], return_address);
            break;
        }
        case 0x03: { 
            uint32_t rd = (instruction >> 7) & 0x1F;
            uint32_t funct3 = (instruction >> 12) & 0x7;
            uint32_t rs1 = (instruction >> 15) & 0x1F;
            int32_t imm = (int32_t)(instruction & 0xFFF00000) >> 20;
            
            uint32_t val_rs1 = registers[rs1];
            uint32_t address = val_rs1 + imm;
            uint32_t res; 
            const char* instr_name = "???";

            switch (funct3) {
                case 0x0: { // lb
                    instr_name = "lb";
                    int8_t byte = (int8_t)read_byte_from_memory(address);
                    res = (int32_t)byte; 
                    if(rd != 0) registers[rd] = res;
                    break;
                }
                case 0x1: { // lh
                    instr_name = "lh";
                    int16_t half = (int16_t)read_half_word_from_memory(address);
                    res = (int32_t)half; 
                    if(rd != 0) registers[rd] = res;
                    break;
                }
                case 0x2: { // lw
                    instr_name = "lw";
                    res = read_word_from_memory(address);
                    if(rd != 0) registers[rd] = res;
                    break;
                }
                case 0x4: { // lbu
                    instr_name = "lbu";
                    uint8_t byte = read_byte_from_memory(address);
                    res = (uint32_t)byte; 
                    if(rd != 0) registers[rd] = res;
                    break;
                }
                case 0x5: { // lhu
                    instr_name = "lhu";
                    uint16_t half = read_half_word_from_memory(address);
                    res = (uint32_t)half;
                    if(rd != 0) registers[rd] = res;
                    break;
                }
                default:
                    fprintf(output_file, "Erro: funct3 0x%x desconhecido para opcode LOAD (0x03)!\n", funct3);
                    printf("Erro: funct3 0x%x desconhecido para opcode LOAD (0x03)!\n", funct3);
                    pc = 0; 
                    return;
            }
            fprintf(output_file, "0x%08x:%-7s %s,0x%03x(%s)   %s=mem[0x%08x]=0x%08x\n",
                   current_pc,
                   instr_name,
                   x_label[rd],
                   (imm & 0xFFF),
                   x_label[rs1],
                   x_label[rd],
                   address,
                   res);
            break;
        }
        case 0x23: { 
            uint32_t funct3 = (instruction >> 12) & 0x7;
            uint32_t rs1 = (instruction >> 15) & 0x1F;
            uint32_t rs2 = (instruction >> 20) & 0x1F;

            uint32_t imm_11_5 = (instruction >> 25) & 0x7F;
            uint32_t imm_4_0  = (instruction >> 7) & 0x1F;
            int32_t imm = (imm_11_5 << 5) | imm_4_0;
            imm = (int32_t)(imm << 20) >> 20;

            uint32_t val_rs1 = registers[rs1];
            uint32_t val_rs2 = registers[rs2];
            uint32_t address = val_rs1 + imm;
            const char* instr_name = "???";

            switch (funct3) {
                case 0x0: { // sb
                    instr_name = "sb";
                    write_byte_to_memory(address, (uint8_t)val_rs2);
                    break;
                }
                case 0x1: { // sh
                    instr_name = "sh";
                    write_half_word_to_memory(address, (uint16_t)val_rs2);
                    break;
                }
                case 0x2: { // sw
                    instr_name = "sw";
                    write_word_to_memory(address, val_rs2);
                    break;
                }
                default:
                    fprintf(output_file, "Erro: funct3 0x%x desconhecido para opcode STORE (0x23)!\n", funct3);
                    printf("Erro: funct3 0x%x desconhecido para opcode STORE (0x23)!\n", funct3);
                    pc = 0; 
                    return; 
            }
            fprintf(output_file, "0x%08x:%-7s %s,0x%03x(%s)   mem[0x%08x]=0x%08x\n",
                   current_pc,
                   instr_name,
                   x_label[rs2],
                   (imm & 0xFFF),
                   x_label[rs1],
                   address,
                   val_rs2);
            break;
        }
        default:
            fprintf(output_file, "Erro: Opcode 0x%x desconhecido em 0x%08x\n", opcode, current_pc);
            printf("Erro Opcode 0x%x desconhecido em 0x%x\n", opcode, current_pc);
            pc = 0;
            break;
    }
    
    if (!pc_updated) {
        pc += 4;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Erro: Forneça os arquivos de entrada e saída.\n");
        fprintf(stderr, "Uso: %s <arquivo.hex> <arquivo.out>\n", argv[0]);
        return 1;
    }
    FILE *hex_file = fopen(argv[1], "r"); 
    if (hex_file == NULL) {
        perror("Erro ao abrir o arquivo .hex");
        return 1;
    }
    FILE *output_file = fopen(argv[2], "w");
    if (output_file == NULL) {
        perror("Erro ao criar o arquivo .out");
        fclose(hex_file); 
        return 1;
    }
    char line[1024];
    uint32_t current_address = 0;
    int address_set = 0;
    int line_count = 0;

    while (fgets(line, sizeof(line), hex_file)) {
        line_count++;
        line[strcspn(line, "\r\n")] = 0;

        if (line[0] == '@') {
            current_address = (uint32_t)strtoul(&line[1], NULL, 16);
            address_set = 1;
        } 
        else if (address_set && strlen(line) > 0) {
            char *token = strtok(line, " ");
            while (token != NULL) {
                char *byte_str_0 = token;
                char *byte_str_1 = strtok(NULL, " ");
                char *byte_str_2 = strtok(NULL, " ");
                char *byte_str_3 = strtok(NULL, " ");

                if (byte_str_1 == NULL || byte_str_2 == NULL || byte_str_3 == NULL) {
                    if (byte_str_0 != NULL) {
                         printf("Aviso: Linha %d mal formatada ou incompleta. Ignorando token: %s\n", line_count, byte_str_0);
                    }
                    break;
                }
                uint32_t byte0 = (uint32_t)strtoul(byte_str_0, NULL, 16);
                uint32_t byte1 = (uint32_t)strtoul(byte_str_1, NULL, 16);
                uint32_t byte2 = (uint32_t)strtoul(byte_str_2, NULL, 16);
                uint32_t byte3 = (uint32_t)strtoul(byte_str_3, NULL, 16);
                uint32_t instruction_word = byte0 | (byte1 << 8) | (byte2 << 16) | (byte3 << 24);
                write_word_to_memory(current_address, instruction_word);
                current_address += 4;
                token = strtok(NULL, " ");
            }
        }
    }
    fclose(hex_file);
    printf("Programa '%s' carregado. Iniciando simulação, saída em %s\n", argv[1], argv[2]);
    
    while (1) {
        
        uint32_t instruction = read_word_from_memory(pc);
        uint32_t pc_atual = pc;

        if (instruction == 0x00000073) {
            fprintf(output_file, "0x%08x:ecall\n", pc_atual);
            printf("Simulação terminada (ecall).\n");
            break;
        }
        if (instruction == 0x00100073) {
            fprintf(output_file, "0x%08x:ebreak\n", pc_atual);
            printf("Simulação terminada (ebreak).\n");
            break;
        }
        if (instruction == 0) {
            printf("Simulação terminada (instrução nula). PC=0x%x\n", pc_atual);
            break;
        }
        execute_instruction(instruction, pc_atual, output_file);

        registers[0] = 0;
    }

    fclose(output_file);
    return 0;
}
