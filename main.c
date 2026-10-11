#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define MEMORY_SIZE (1024 * 1024)
#define MAX_INSTRUCTIONS 1000

// Simple RISC-V CPU implementation
typedef struct {
    uint32_t registers[32];
    uint8_t memory[MEMORY_SIZE];
    uint32_t pc;
} SimpleRISCVCPU;

void init_cpu(SimpleRISCVCPU* cpu) {
    memset(cpu->registers, 0, sizeof(cpu->registers));
    memset(cpu->memory, 0, sizeof(cpu->memory));
    cpu->pc = 0;
}

void load_program(SimpleRISCVCPU* cpu, const uint32_t* program, size_t size) {
    for (size_t i = 0; i < size; i++) {
        uint32_t addr = i * 4;
        cpu->memory[addr] = program[i] & 0xFF;
        cpu->memory[addr + 1] = (program[i] >> 8) & 0xFF;
        cpu->memory[addr + 2] = (program[i] >> 16) & 0xFF;
        cpu->memory[addr + 3] = (program[i] >> 24) & 0xFF;
    }
}

uint32_t read_instruction(SimpleRISCVCPU* cpu, uint32_t addr) {
    return (uint32_t)cpu->memory[addr] |
           ((uint32_t)cpu->memory[addr + 1] << 8) |
           ((uint32_t)cpu->memory[addr + 2] << 16) |
           ((uint32_t)cpu->memory[addr + 3] << 24);
}

void execute_instruction(SimpleRISCVCPU* cpu, uint32_t instruction) {
    uint32_t opcode = instruction & 0x7F;
    uint32_t rd = (instruction >> 7) & 0x1F;
    uint32_t funct3 = (instruction >> 12) & 0x7;
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    uint32_t rs2 = (instruction >> 20) & 0x1F;
    uint32_t funct7 = (instruction >> 25) & 0x7F;
    
    int32_t imm = 0;
    
    // Extract immediate based on instruction type
    if (opcode == 0x13 || opcode == 0x03) {  // I-type
        imm = (int32_t)(instruction >> 20);
        if (imm & 0x800) imm |= 0xFFFFF000;  // sign extend
    } else if (opcode == 0x33) {  // R-type
        imm = 0;
    }
    
    // Execute basic instructions
    if (opcode == 0x33) {  // R-type arithmetic
        if (funct3 == 0x0 && funct7 == 0x00) {  // ADD
            cpu->registers[rd] = cpu->registers[rs1] + cpu->registers[rs2];
        } else if (funct3 == 0x0 && funct7 == 0x20) {  // SUB
            cpu->registers[rd] = cpu->registers[rs1] - cpu->registers[rs2];
        } else if (funct3 == 0x7) {  // AND
            cpu->registers[rd] = cpu->registers[rs1] & cpu->registers[rs2];
        } else if (funct3 == 0x6) {  // OR
            cpu->registers[rd] = cpu->registers[rs1] | cpu->registers[rs2];
        } else if (funct3 == 0x4) {  // XOR
            cpu->registers[rd] = cpu->registers[rs1] ^ cpu->registers[rs2];
        }
    } else if (opcode == 0x13) {  // I-type immediate arithmetic
        if (funct3 == 0x0) {  // ADDI
            cpu->registers[rd] = cpu->registers[rs1] + imm;
        } else if (funct3 == 0x7) {  // ANDI
            cpu->registers[rd] = cpu->registers[rs1] & imm;
        } else if (funct3 == 0x6) {  // ORI
            cpu->registers[rd] = cpu->registers[rs1] | imm;
        } else if (funct3 == 0x4) {  // XORI
            cpu->registers[rd] = cpu->registers[rs1] ^ imm;
        }
    } else if (opcode == 0x03) {  // Load instructions
        if (funct3 == 0x2) {  // LW
            uint32_t addr = cpu->registers[rs1] + imm;
            cpu->registers[rd] = (uint32_t)cpu->memory[addr] |
                                ((uint32_t)cpu->memory[addr + 1] << 8) |
                                ((uint32_t)cpu->memory[addr + 2] << 16) |
                                ((uint32_t)cpu->memory[addr + 3] << 24);
        }
    } else if (opcode == 0x23) {  // Store instructions
        if (funct3 == 0x2) {  // SW
            uint32_t addr = cpu->registers[rs1] + imm;
            uint32_t value = cpu->registers[rs2];
            cpu->memory[addr] = value & 0xFF;
            cpu->memory[addr + 1] = (value >> 8) & 0xFF;
            cpu->memory[addr + 2] = (value >> 16) & 0xFF;
            cpu->memory[addr + 3] = (value >> 24) & 0xFF;
        }
    }
    
    // x0 is hardwired to 0
    cpu->registers[0] = 0;
}

void run_cpu(SimpleRISCVCPU* cpu) {
    for (uint32_t i = 0; i < MAX_INSTRUCTIONS; i++) {
        uint32_t instruction = read_instruction(cpu, cpu->pc);
        
        // Check for NOP or end of program
        if (instruction == 0) {
            break;
        }
        
        execute_instruction(cpu, instruction);
        cpu->pc += 4;
    }
}

uint32_t get_result(SimpleRISCVCPU* cpu) {
    return cpu->registers[10];  // Return value in x10 (a0)
}

// Parse input data file
int parse_data_file(uint32_t* data, int max_size) {
    char line[256];
    int count = 0;
    
    while (fgets(line, sizeof(line), stdin) && count < max_size) {
        // Skip empty lines and comments
        if (line[0] == '\n' || line[0] == '#') {
            continue;
        }
        
        // Remove trailing newline
        line[strcspn(line, "\n")] = 0;
        
        // Parse hexadecimal or decimal values
        uint32_t value;
        if (strstr(line, "0x") == line || strstr(line, "0X") == line) {
            sscanf(line, "0x%x", &value);
        } else {
            sscanf(line, "%u", &value);
        }
        
        data[count++] = value;
    }
    
    return count;
}

int main() {
    uint32_t program_data[MAX_INSTRUCTIONS];
    int program_size = parse_data_file(program_data, MAX_INSTRUCTIONS);
    
    if (program_size == 0) {
        fprintf(stderr, "Error: No input data provided\n");
        return 1;
    }
    
    // Create and initialize CPU
    SimpleRISCVCPU cpu;
    init_cpu(&cpu);
    load_program(&cpu, program_data, program_size);
    
    // Run the simulation
    run_cpu(&cpu);
    
    // Output the result
    uint32_t result = get_result(&cpu);
    printf("%u\n", result);
    
    return 0;
}