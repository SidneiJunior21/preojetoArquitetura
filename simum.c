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

            switch (funct3) {
                case 0x0:
                    registers[rd] = registers[rs1] + imm;
                    break;
                
                case 0x1: {
                    uint32_t shamt = imm & 0x1F; 
                    registers[rd] = registers[rs1] << shamt;
                    break;
                }
                case 0x2:
                    registers[rd] = ((int32_t)registers[rs1] < imm) ? 1 : 0;
                    break;
                case 0x3:
                    registers[rd] = ((uint32_t)registers[rs1] < (uint32_t)imm) ? 1 : 0;
                    break;
                case 0x4:
                    registers[rd] = registers[rs1] ^ imm;
                    break;
                case 0x5: {
                    uint32_t shamt = imm & 0x1F;
                    uint32_t funct7 = (instruction >> 25) & 0x7F;
                    
                    if (funct7 == 0x00) {
                        registers[rd] = (uint32_t)registers[rs1] >> shamt;
                    } else if (funct7 == 0x20) {
                        registers[rd] = (int32_t)registers[rs1] >> shamt;
                    }
                    break;
                }
                case 0x6:
                    registers[rd] = registers[rs1] | imm;
                    break;
                case 0x7:
                    registers[rd] = registers[rs1] & imm;
                    break;
                default:
                    printf("Erro: funct3 0x%x desconhecido para opcode I-TYPE (0x13)!\n", funct3);
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

            if (funct7 == 0x00) {
                switch (funct3) {
                    case 0x0:
                        registers[rd] = v_rs1 + v_rs2;
                        break;
                    case 0x1:
                        registers[rd] = v_urs1 << (v_urs2 & 0x1F);
                        break;
                    case 0x2:
                        registers[rd] = (v_rs1 < v_rs2) ? 1 : 0;
                        break;
                    case 0x3:
                        registers[rd] = (v_urs1 < v_urs2) ? 1 : 0;
                        break;
                    case 0x4:
                        registers[rd] = v_rs1 ^ v_rs2;
                        break;
                    case 0x5:
                        registers[rd] = v_urs1 >> (v_urs2 & 0x1F);
                        break;
                    case 0x6:
                        registers[rd] = v_rs1 | v_rs2;
                        break;
                    case 0x7:
                        registers[rd] = v_rs1 & v_rs2;
                        break;
                }
            } else if (funct7 == 0x20) {
                switch (funct3) {
                    case 0x0:
                        registers[rd] = v_rs1 - v_rs2;
                        break;
                    case 0x5:
                        registers[rd] = v_rs1 >> (v_urs2 & 0x1F);
                        break;
                }
            } else if (funct7 == 0x01) {
                int64_t s64_rs1 = (int64_t)v_rs1;
                int64_t s64_rs2 = (int64_t)v_rs2;
                uint64_t u64_rs1 = (uint64_t)v_urs1;
                uint64_t u64_rs2 = (uint64_t)v_urs2;

                switch (funct3) {
                    case 0x0:
                        registers[rd] = v_rs1 * v_rs2;
                        break;
                    case 0x1:
                        registers[rd] = (uint32_t)((s64_rs1 * s64_rs2) >> 32);
                        break;
                    case 0x2:
                        registers[rd] = (uint32_t)((s64_rs1 * u64_rs2) >> 32);
                        break;
                    case 0x3:
                        registers[rd] = (uint32_t)((u64_rs1 * u64_rs2) >> 32);
                        break;
                    case 0x4:
                        if (v_rs2 == 0) registers[rd] = 0xFFFFFFFF;
                        else if (v_rs1 == 0x80000000 && v_rs2 == -1) registers[rd] = 0x80000000;
                        else registers[rd] = v_rs1 / v_rs2;
                        break;
                    case 0x5:
                        if (v_urs2 == 0) registers[rd] = 0xFFFFFFFF;
                        else registers[rd] = v_urs1 / v_urs2;
                        break;
                    case 0x6:
                        if (v_rs2 == 0) registers[rd] = v_rs1;
                        else if (v_rs1 == 0x80000000 && v_rs2 == -1) registers[rd] = 0;
                        else registers[rd] = v_rs1 % v_rs2;
                        break;
                    case 0x7:
                        if (v_urs2 == 0) registers[rd] = v_urs1;
                        else registers[rd] = v_urs1 % v_urs2;
                        break;
                }
            }
            break;
        }
        case 0x6F: {
            uint32_t rd = (instruction >> 7) & 0x1F;
            
            uint32_t imm_20 = (instruction >> 31) & 1;
            uint32_t imm_10_1 = (instruction >> 21) & 0x3FF;
            uint32_t imm_11 = (instruction >> 20) & 1;
            uint32_t imm_19_12 = (instruction >> 12) & 0xFF;
            
            int32_t offset = (imm_20 << 20) | (imm_19_12 << 12) | (imm_11 << 11) | (imm_10_1 << 1);
            offset = (int32_t)(offset << 11) >> 11; 

            if (rd != 0) {
                registers[rd] = pc + 4;
            }
            pc = pc + offset;
            pc_updated = 1;
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

            int condition_met = 0;
            switch (funct3) {
                case 0x0:
                    if (registers[rs1] == registers[rs2]) condition_met = 1;
                    break;
                case 0x1:
                    if (registers[rs1] != registers[rs2]) condition_met = 1;
                    break;
                case 0x4:
                    if ((int32_t)registers[rs1] < (int32_t)registers[rs2]) condition_met = 1;
                    break;
                case 0x5:
                    if ((int32_t)registers[rs1] >= (int32_t)registers[rs2]) condition_met = 1;
                    break;
                case 0x6:
                    if ((uint32_t)registers[rs1] < (uint32_t)registers[rs2]) condition_met = 1;
                    break;
                case 0x7:
                    if ((uint32_t)registers[rs1] >= (uint32_t)registers[rs2]) condition_met = 1;
                    break;
            }

            if (condition_met) {
                pc = pc + offset;
                pc_updated = 1;
            }
            break;
        }
        case 0x37: {
            uint32_t rd = (instruction >> 7) & 0x1F;
            uint32_t imm = instruction & 0xFFFFF000; 
            registers[rd] = imm;
            break;
        }
        case 0x17: {
            uint32_t rd = (instruction >> 7) & 0x1F;
            int32_t imm = (int32_t)(instruction & 0xFFFFF000);
            registers[rd] = pc + imm;
            break;
        }
        case 0x67: {
            uint32_t rd = (instruction >> 7) & 0x1F;
            uint32_t rs1 = (instruction >> 15) & 0x1F;
            int32_t imm = (int32_t)(instruction & 0xFFF00000) >> 20;

            uint32_t return_address = pc + 4;
            uint32_t target_address = (registers[rs1] + imm) & ~1; 

            if (rd != 0) {
                registers[rd] = return_address;
            }
            pc = target_address;
            pc_updated = 1;
            break;
        }
        case 0x03: { 
            uint32_t rd = (instruction >> 7) & 0x1F;
            uint32_t funct3 = (instruction >> 12) & 0x7;
            uint32_t rs1 = (instruction >> 15) & 0x1F;
            int32_t imm = (int32_t)(instruction & 0xFFF00000) >> 20;
            
            uint32_t address = registers[rs1] + imm;

            switch (funct3) {
                case 0x0: {
                    int8_t byte = (int8_t)read_byte_from_memory(address);
                    registers[rd] = (int32_t)byte;
                    break;
                }
                case 0x1: {
                    int16_t half = (int16_t)read_half_word_from_memory(address);
                    registers[rd] = (int32_t)half;
                    break;
                }
                case 0x2: {
                    registers[rd] = read_word_from_memory(address);
                    break;
                }
                case 0x4: {
                    uint8_t byte = read_byte_from_memory(address);
                    registers[rd] = (uint32_t)byte;
                    break;
                }
                case 0x5: {
                    uint16_t half = read_half_word_from_memory(address);
                    registers[rd] = (uint32_t)half;
                    break;
                }
                default:
                    printf("Erro: funct3 0x%x desconhecido para opcode LOAD (0x03)!\n", funct3);
            }
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

            uint32_t address = registers[rs1] + imm;
            uint32_t value = registers[rs2];

            switch (funct3) {
                case 0x0:
                    write_byte_to_memory(address, (uint8_t)value);
                    break;
                case 0x1:
                    write_half_word_to_memory(address, (uint16_t)value);
                    break;
                case 0x2:
                    write_word_to_memory(address, value);
                    break;
                default:
                    printf("Erro: funct3 0x%x desconhecido para opcode STORE (0x23)!\n", funct3);
            }
            break;
        }

        default:
            printf("Erro: Opcode 0x%x desconhecido! (em 0x%x)\n", opcode, pc);
            pc = 0;
            break;
    }
    
    if (!pc_updated) {
        pc += 4;
    }
}

void print_registers() {
    printf("\n--- Estado Final dos Registradores ---\n");
    for (int i = 0; i < 32; i++) {
        printf("x%d:\t0x%08x\t(%d)\n", i, registers[i], registers[i]);

        if ((i + 1) % 4 == 0) {
            printf("\n");
        }
    }
    printf("----------------------------------------\n");
}

int main(int argc, char *argv[]) {

    if (argc < 3) {
        fprintf(stderr, "Erro: Forneça os arquivos de entrada e saída.\n");
        fprintf(stderr, "Uso: %s <arquivo.hex> <arquivo.out>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    if (file == NULL) {
        perror("Erro ao abrir o arquivo .hex");
        return 1;
    }
    FILE *file = fopen(argv[2], "w");
    if (file == NULL) {
        perror("Erro ao criar o arquivo .out");
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
    fclose(output_file)
    return 0;
}
