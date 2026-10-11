#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
#include <map>
#include <cassert>

using namespace std;

// Simple RISC-V CPU implementation
class SimpleRISCVCPU {
private:
    vector<uint32_t> registers;
    vector<uint8_t> memory;
    uint32_t pc;
    
public:
    SimpleRISCVCPU() : registers(32, 0), memory(1024 * 1024, 0), pc(0) {}
    
    void load_program(const vector<uint32_t>& program) {
        for (size_t i = 0; i < program.size(); i++) {
            uint32_t addr = i * 4;
            memory[addr] = program[i] & 0xFF;
            memory[addr + 1] = (program[i] >> 8) & 0xFF;
            memory[addr + 2] = (program[i] >> 16) & 0xFF;
            memory[addr + 3] = (program[i] >> 24) & 0xFF;
        }
    }
    
    uint32_t read_instruction(uint32_t addr) {
        return (static_cast<uint32_t>(memory[addr]) << 0) |
               (static_cast<uint32_t>(memory[addr + 1]) << 8) |
               (static_cast<uint32_t>(memory[addr + 2]) << 16) |
               (static_cast<uint32_t>(memory[addr + 3]) << 24);
    }
    
    void execute_instruction(uint32_t instruction) {
        uint32_t opcode = instruction & 0x7F;
        uint32_t rd = (instruction >> 7) & 0x1F;
        uint32_t funct3 = (instruction >> 12) & 0x7;
        uint32_t rs1 = (instruction >> 15) & 0x1F;
        uint32_t rs2 = (instruction >> 20) & 0x1F;
        uint32_t funct7 = (instruction >> 25) & 0x7F;
        
        int32_t imm = 0;
        
        // Extract immediate based on instruction type
        if (opcode == 0x13 || opcode == 0x03) {  // I-type
            imm = instruction >> 20;
            if (imm & 0x800) imm |= 0xFFFFF000;  // sign extend
        } else if (opcode == 0x33) {  // R-type
            imm = 0;
        }
        
        // Execute basic instructions
        if (opcode == 0x33) {  // R-type arithmetic
            if (funct3 == 0x0 && funct7 == 0x00) {  // ADD
                registers[rd] = registers[rs1] + registers[rs2];
            } else if (funct3 == 0x0 && funct7 == 0x20) {  // SUB
                registers[rd] = registers[rs1] - registers[rs2];
            } else if (funct3 == 0x7) {  // AND
                registers[rd] = registers[rs1] & registers[rs2];
            } else if (funct3 == 0x6) {  // OR
                registers[rd] = registers[rs1] | registers[rs2];
            } else if (funct3 == 0x4) {  // XOR
                registers[rd] = registers[rs1] ^ registers[rs2];
            }
        } else if (opcode == 0x13) {  // I-type immediate arithmetic
            if (funct3 == 0x0) {  // ADDI
                registers[rd] = registers[rs1] + imm;
            } else if (funct3 == 0x7) {  // ANDI
                registers[rd] = registers[rs1] & imm;
            } else if (funct3 == 0x6) {  // ORI
                registers[rd] = registers[rs1] | imm;
            } else if (funct3 == 0x4) {  // XORI
                registers[rd] = registers[rs1] ^ imm;
            }
        } else if (opcode == 0x03) {  // Load instructions
            if (funct3 == 0x2) {  // LW
                uint32_t addr = registers[rs1] + imm;
                registers[rd] = (static_cast<uint32_t>(memory[addr]) << 0) |
                               (static_cast<uint32_t>(memory[addr + 1]) << 8) |
                               (static_cast<uint32_t>(memory[addr + 2]) << 16) |
                               (static_cast<uint32_t>(memory[addr + 3]) << 24);
            }
        } else if (opcode == 0x23) {  // Store instructions
            if (funct3 == 0x2) {  // SW
                uint32_t addr = registers[rs1] + imm;
                uint32_t value = registers[rs2];
                memory[addr] = value & 0xFF;
                memory[addr + 1] = (value >> 8) & 0xFF;
                memory[addr + 2] = (value >> 16) & 0xFF;
                memory[addr + 3] = (value >> 24) & 0xFF;
            }
        }
        
        // x0 is hardwired to 0
        registers[0] = 0;
    }
    
    void run() {
        const uint32_t max_instructions = 1000;
        
        for (uint32_t i = 0; i < max_instructions; i++) {
            uint32_t instruction = read_instruction(pc);
            
            // Check for NOP or end of program
            if (instruction == 0) {
                break;
            }
            
            execute_instruction(instruction);
            pc += 4;
        }
    }
    
    uint32_t get_result() const {
        return registers[10];  // Return value in x10 (a0)
    }
};

// Parse input data file
vector<uint32_t> parse_data_file(istream& input) {
    vector<uint32_t> data;
    string line;
    
    while (getline(input, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Parse hexadecimal or decimal values
        istringstream iss(line);
        uint32_t value;
        
        if (line.find("0x") == 0 || line.find("0X") == 0) {
            iss >> hex >> value;
        } else {
            iss >> dec >> value;
        }
        
        if (!iss.fail()) {
            data.push_back(value);
        }
    }
    
    return data;
}

int main() {
    try {
        // Read input from stdin
        vector<uint32_t> program_data = parse_data_file(cin);
        
        if (program_data.empty()) {
            cerr << "Error: No input data provided" << endl;
            return 1;
        }
        
        // Create and initialize CPU
        SimpleRISCVCPU cpu;
        cpu.load_program(program_data);
        
        // Run the simulation
        cpu.run();
        
        // Output the result
        uint32_t result = cpu.get_result();
        cout << result << endl;
        
        return 0;
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}