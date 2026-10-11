#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
#include <map>
#include <cassert>

using namespace std;

// RISC-V Register File
class RegisterFile {
private:
    vector<uint32_t> registers;
    
public:
    RegisterFile() : registers(32, 0) {}
    
    uint32_t read(int reg) const {
        if (reg == 0) return 0;  // x0 is hardwired to 0
        assert(reg >= 0 && reg < 32);
        return registers[reg];
    }
    
    void write(int reg, uint32_t value) {
        if (reg == 0) return;  // x0 is hardwired to 0
        assert(reg >= 0 && reg < 32);
        registers[reg] = value;
    }
    
    void dump() const {
        for (int i = 0; i < 32; i++) {
            cout << "x" << i << " = 0x" << hex << setw(8) << setfill('0') << registers[i];
            if (i % 4 == 3) cout << endl;
            else cout << "\t";
        }
    }
};

// Memory System
class Memory {
private:
    vector<uint8_t> memory;
    
public:
    Memory(size_t size = 1024 * 1024) : memory(size, 0) {}
    
    uint32_t read_word(uint32_t addr) const {
        assert(addr % 4 == 0);
        uint32_t value = (static_cast<uint32_t>(memory[addr]) << 0) |
                        (static_cast<uint32_t>(memory[addr + 1]) << 8) |
                        (static_cast<uint32_t>(memory[addr + 2]) << 16) |
                        (static_cast<uint32_t>(memory[addr + 3]) << 24);
        return value;
    }
    
    void write_word(uint32_t addr, uint32_t value) {
        assert(addr % 4 == 0);
        memory[addr] = value & 0xFF;
        memory[addr + 1] = (value >> 8) & 0xFF;
        memory[addr + 2] = (value >> 16) & 0xFF;
        memory[addr + 3] = (value >> 24) & 0xFF;
    }
    
    uint8_t read_byte(uint32_t addr) const {
        return memory[addr];
    }
    
    void write_byte(uint32_t addr, uint8_t value) {
        memory[addr] = value;
    }
    
    void load_from_data(const vector<uint32_t>& data, uint32_t base_addr = 0) {
        for (size_t i = 0; i < data.size(); i++) {
            write_word(base_addr + i * 4, data[i]);
        }
    }
};

// Instruction types and fields
struct Instruction {
    uint32_t raw;
    uint32_t opcode;
    uint32_t rd;
    uint32_t rs1;
    uint32_t rs2;
    uint32_t funct3;
    uint32_t funct7;
    int32_t imm;
    string type;
    
    Instruction(uint32_t instr = 0) : raw(instr) {
        decode();
    }
    
    void decode() {
        opcode = raw & 0x7F;
        rd = (raw >> 7) & 0x1F;
        funct3 = (raw >> 12) & 0x7;
        rs1 = (raw >> 15) & 0x1F;
        rs2 = (raw >> 20) & 0x1F;
        funct7 = (raw >> 25) & 0x7F;
        
        // Determine instruction type and extract immediate
        if (opcode == 0x33 || opcode == 0x0B) {  // R-type
            type = "R";
            imm = 0;
        } else if (opcode == 0x03 || opcode == 0x13 || opcode == 0x23) {  // I-type or S-type
            if (opcode == 0x23) {  // S-type
                type = "S";
                imm = ((raw >> 7) & 0x1F) | ((raw >> 25) << 5);
                if (imm & 0x800) imm |= 0xFFFFF000;  // sign extend
            } else {  // I-type
                type = "I";
                imm = raw >> 20;
                if (imm & 0x800) imm |= 0xFFFFF000;  // sign extend
            }
        } else if (opcode == 0x63) {  // B-type
            type = "B";
            imm = ((raw >> 7) & 0x1) | ((raw >> 8) & 0xE) | ((raw >> 25) & 0x1) << 11 | 
                  ((raw >> 8) & 0x1) << 12 | ((raw >> 25) & 0x3F) << 5;
            if (imm & 0x1000) imm |= 0xFFFFE000;  // sign extend
        } else if (opcode == 0x37 || opcode == 0x17) {  // U-type
            type = "U";
            imm = raw & 0xFFFFF000;
        } else if (opcode == 0x6F) {  // J-type
            type = "J";
            imm = ((raw >> 21) & 0x3FF) | ((raw >> 20) & 0x1) << 11 | 
                  ((raw >> 12) & 0xFF) << 12 | ((raw >> 31) & 0x1) << 20;
            if (imm & 0x100000) imm |= 0xFFE00000;  // sign extend
        } else {
            type = "UNKNOWN";
            imm = 0;
        }
    }
};

// Pipeline stages
struct IF_ID {
    bool valid;
    uint32_t pc;
    uint32_t instruction;
    
    IF_ID() : valid(false), pc(0), instruction(0) {}
};

struct ID_EX {
    bool valid;
    uint32_t pc;
    uint32_t rs1_val;
    uint32_t rs2_val;
    uint32_t imm;
    uint32_t rd;
    uint32_t funct3;
    uint32_t funct7;
    string opcode;
    bool reg_write;
    bool mem_read;
    bool mem_write;
    bool mem_to_reg;
    bool alu_src;
    string alu_op;
    
    ID_EX() : valid(false), pc(0), rs1_val(0), rs2_val(0), imm(0), rd(0), 
              funct3(0), funct7(0), opcode(""), reg_write(false), mem_read(false),
              mem_write(false), mem_to_reg(false), alu_src(false), alu_op("") {}
};

struct EX_MEM {
    bool valid;
    uint32_t pc;
    uint32_t alu_result;
    uint32_t rs2_val;
    uint32_t rd;
    bool reg_write;
    bool mem_read;
    bool mem_write;
    bool mem_to_reg;
    
    EX_MEM() : valid(false), pc(0), alu_result(0), rs2_val(0), rd(0),
               reg_write(false), mem_read(false), mem_write(false), mem_to_reg(false) {}
};

struct MEM_WB {
    bool valid;
    uint32_t alu_result;
    uint32_t mem_data;
    uint32_t rd;
    bool reg_write;
    bool mem_to_reg;
    
    MEM_WB() : valid(false), alu_result(0), mem_data(0), rd(0),
               reg_write(false), mem_to_reg(false) {}
};

// ALU Unit
class ALU {
public:
    static uint32_t execute(const string& alu_op, uint32_t operand1, uint32_t operand2, 
                           uint32_t /*funct3*/, uint32_t /*funct7*/) {
        if (alu_op == "ADD") {
            return operand1 + operand2;
        } else if (alu_op == "SUB") {
            return operand1 - operand2;
        } else if (alu_op == "AND") {
            return operand1 & operand2;
        } else if (alu_op == "OR") {
            return operand1 | operand2;
        } else if (alu_op == "XOR") {
            return operand1 ^ operand2;
        } else if (alu_op == "SLL") {
            return operand1 << (operand2 & 0x1F);
        } else if (alu_op == "SRL") {
            return operand1 >> (operand2 & 0x1F);
        } else if (alu_op == "SRA") {
            return static_cast<int32_t>(operand1) >> (operand2 & 0x1F);
        } else if (alu_op == "SLT") {
            return static_cast<int32_t>(operand1) < static_cast<int32_t>(operand2) ? 1 : 0;
        } else if (alu_op == "SLTU") {
            return operand1 < operand2 ? 1 : 0;
        } else if (alu_op == "ADDI") {
            return operand1 + operand2;
        } else if (alu_op == "ANDI") {
            return operand1 & operand2;
        } else if (alu_op == "ORI") {
            return operand1 | operand2;
        } else if (alu_op == "XORI") {
            return operand1 ^ operand2;
        } else if (alu_op == "SLLI") {
            return operand1 << (operand2 & 0x1F);
        } else if (alu_op == "SRLI") {
            return operand1 >> (operand2 & 0x1F);
        } else if (alu_op == "SRAI") {
            return static_cast<int32_t>(operand1) >> (operand2 & 0x1F);
        } else if (alu_op == "SLTI") {
            return static_cast<int32_t>(operand1) < static_cast<int32_t>(operand2) ? 1 : 0;
        } else if (alu_op == "SLTIU") {
            return operand1 < operand2 ? 1 : 0;
        } else {
            return 0;
        }
    }
};

// Control Unit
class ControlUnit {
public:
    static void generate_control_signals(const Instruction& instr, ID_EX& id_ex) {
        uint32_t opcode = instr.opcode;
        uint32_t funct3 = instr.funct3;
        uint32_t funct7 = instr.funct7;
        
        // Default values
        id_ex.reg_write = false;
        id_ex.mem_read = false;
        id_ex.mem_write = false;
        id_ex.mem_to_reg = false;
        id_ex.alu_src = false;
        
        if (opcode == 0x33) {  // R-type instructions
            id_ex.reg_write = true;
            id_ex.alu_src = false;
            
            if (funct3 == 0x0 && funct7 == 0x00) {
                id_ex.alu_op = "ADD";
            } else if (funct3 == 0x0 && funct7 == 0x20) {
                id_ex.alu_op = "SUB";
            } else if (funct3 == 0x7) {
                id_ex.alu_op = "AND";
            } else if (funct3 == 0x6) {
                id_ex.alu_op = "OR";
            } else if (funct3 == 0x4) {
                id_ex.alu_op = "XOR";
            } else if (funct3 == 0x1) {
                id_ex.alu_op = "SLL";
            } else if (funct3 == 0x5 && funct7 == 0x00) {
                id_ex.alu_op = "SRL";
            } else if (funct3 == 0x5 && funct7 == 0x20) {
                id_ex.alu_op = "SRA";
            } else if (funct3 == 0x2) {
                id_ex.alu_op = "SLT";
            } else if (funct3 == 0x3) {
                id_ex.alu_op = "SLTU";
            }
        } else if (opcode == 0x13) {  // I-type arithmetic
            id_ex.reg_write = true;
            id_ex.alu_src = true;
            
            if (funct3 == 0x0) {
                id_ex.alu_op = "ADDI";
            } else if (funct3 == 0x7) {
                id_ex.alu_op = "ANDI";
            } else if (funct3 == 0x6) {
                id_ex.alu_op = "ORI";
            } else if (funct3 == 0x4) {
                id_ex.alu_op = "XORI";
            } else if (funct3 == 0x1) {
                id_ex.alu_op = "SLLI";
            } else if (funct3 == 0x5 && funct7 == 0x00) {
                id_ex.alu_op = "SRLI";
            } else if (funct3 == 0x5 && funct7 == 0x20) {
                id_ex.alu_op = "SRAI";
            } else if (funct3 == 0x2) {
                id_ex.alu_op = "SLTI";
            } else if (funct3 == 0x3) {
                id_ex.alu_op = "SLTIU";
            }
        } else if (opcode == 0x03) {  // Load instructions
            id_ex.reg_write = true;
            id_ex.mem_read = true;
            id_ex.mem_to_reg = true;
            id_ex.alu_src = true;
            id_ex.alu_op = "ADDI";
        } else if (opcode == 0x23) {  // Store instructions
            id_ex.mem_write = true;
            id_ex.alu_src = true;
            id_ex.alu_op = "ADDI";
        } else if (opcode == 0x6F) {  // Jump instructions
            id_ex.reg_write = true;
            id_ex.alu_op = "ADDI";
        } else if (opcode == 0x63) {  // Branch instructions
            id_ex.alu_src = true;
            id_ex.alu_op = "ADDI";
        }
    }
};

// Hazard Detection Unit
class HazardDetection {
public:
    static bool detect_hazard(const ID_EX& id_ex, const IF_ID& if_id) {
        // Check for RAW hazard
        if (id_ex.mem_read && 
            ((id_ex.rd == ((if_id.instruction >> 15) & 0x1F)) ||  // rs1
             (id_ex.rd == ((if_id.instruction >> 20) & 0x1F)))) {  // rs2
            return true;
        }
        return false;
    }
};

// Forwarding Unit
class ForwardingUnit {
public:
    static pair<bool, uint32_t> forward_alu_src1(const EX_MEM& ex_mem, const MEM_WB& mem_wb, 
                                                 uint32_t rs1_addr) {
        if (ex_mem.reg_write && ex_mem.rd != 0 && ex_mem.rd == rs1_addr) {
            return {true, ex_mem.alu_result};
        }
        if (mem_wb.reg_write && mem_wb.rd != 0 && mem_wb.rd == rs1_addr) {
            return {true, mem_wb.mem_to_reg ? mem_wb.mem_data : mem_wb.alu_result};
        }
        return {false, 0};
    }
    
    static pair<bool, uint32_t> forward_alu_src2(const EX_MEM& ex_mem, const MEM_WB& mem_wb, 
                                                 uint32_t rs2_addr) {
        if (ex_mem.reg_write && ex_mem.rd != 0 && ex_mem.rd == rs2_addr) {
            return {true, ex_mem.alu_result};
        }
        if (mem_wb.reg_write && mem_wb.rd != 0 && mem_wb.rd == rs2_addr) {
            return {true, mem_wb.mem_to_reg ? mem_wb.mem_data : mem_wb.alu_result};
        }
        return {false, 0};
    }
};

// Main CPU Class
class RISCVCPU {
private:
    RegisterFile reg_file;
    Memory memory;
    uint32_t pc;
    
    // Pipeline registers
    IF_ID if_id;
    ID_EX id_ex;
    EX_MEM ex_mem;
    MEM_WB mem_wb;
    
    // Control signals
    bool stall;
    bool branch_taken;
    uint32_t branch_target;
    
    uint32_t instruction_count;
    uint32_t cycle_count;
    
public:
    RISCVCPU() : pc(0), stall(false), branch_taken(false), branch_target(0),
                 instruction_count(0), cycle_count(0) {}
    
    void load_program(const vector<uint32_t>& instructions) {
        memory.load_from_data(instructions, 0);
        pc = 0;
    }
    
    void load_data(const vector<uint32_t>& data) {
        memory.load_from_data(data, 0x10000000);  // Load data at memory base
    }
    
    // Instruction Fetch Stage
    void instruction_fetch() {
        if (!stall) {
            if_id.pc = pc;
            if_id.instruction = memory.read_word(pc);
            if_id.valid = true;
            pc += 4;
        }
    }
    
    // Instruction Decode Stage
    void instruction_decode() {
        if (if_id.valid) {
            Instruction instr(if_id.instruction);
            
            id_ex.pc = if_id.pc;
            id_ex.rd = instr.rd;
            id_ex.funct3 = instr.funct3;
            id_ex.funct7 = instr.funct7;
            id_ex.imm = instr.imm;
            
            // Read register values
            id_ex.rs1_val = reg_file.read(instr.rs1);
            id_ex.rs2_val = reg_file.read(instr.rs2);
            
            // Generate control signals
            ControlUnit::generate_control_signals(instr, id_ex);
            
            id_ex.valid = true;
            if_id.valid = false;
        }
    }
    
    // Execute Stage
    void execute() {
        if (id_ex.valid) {
            uint32_t operand1 = id_ex.rs1_val;
            uint32_t operand2 = id_ex.alu_src ? id_ex.imm : id_ex.rs2_val;
            
            // Handle forwarding
            auto forward1 = ForwardingUnit::forward_alu_src1(ex_mem, mem_wb, 
                                                            (id_ex.pc >> 15) & 0x1F);
            if (forward1.first) {
                operand1 = forward1.second;
            }
            
            auto forward2 = ForwardingUnit::forward_alu_src2(ex_mem, mem_wb, 
                                                            (id_ex.pc >> 20) & 0x1F);
            if (forward2.first && !id_ex.alu_src) {
                operand2 = forward2.second;
            }
            
            // Execute ALU operation
            ex_mem.alu_result = ALU::execute(id_ex.alu_op, operand1, operand2, 
                                            id_ex.funct3, id_ex.funct7);
            
            ex_mem.pc = id_ex.pc;
            ex_mem.rs2_val = id_ex.rs2_val;
            ex_mem.rd = id_ex.rd;
            ex_mem.reg_write = id_ex.reg_write;
            ex_mem.mem_read = id_ex.mem_read;
            ex_mem.mem_write = id_ex.mem_write;
            ex_mem.mem_to_reg = id_ex.mem_to_reg;
            ex_mem.valid = true;
            
            id_ex.valid = false;
        }
    }
    
    // Memory Stage
    void memory_access() {
        if (ex_mem.valid) {
            mem_wb.alu_result = ex_mem.alu_result;
            mem_wb.rd = ex_mem.rd;
            mem_wb.reg_write = ex_mem.reg_write;
            mem_wb.mem_to_reg = ex_mem.mem_to_reg;
            
            if (ex_mem.mem_read) {
                mem_wb.mem_data = memory.read_word(ex_mem.alu_result);
            }
            
            if (ex_mem.mem_write) {
                memory.write_word(ex_mem.alu_result, ex_mem.rs2_val);
            }
            
            mem_wb.valid = true;
            ex_mem.valid = false;
        }
    }
    
    // Writeback Stage
    void writeback() {
        if (mem_wb.valid) {
            if (mem_wb.reg_write && mem_wb.rd != 0) {
                uint32_t write_data = mem_wb.mem_to_reg ? mem_wb.mem_data : mem_wb.alu_result;
                reg_file.write(mem_wb.rd, write_data);
            }
            mem_wb.valid = false;
        }
    }
    
    // Check for hazards
    void check_hazards() {
        stall = HazardDetection::detect_hazard(id_ex, if_id);
    }
    
    // Run one cycle
    void cycle() {
        cycle_count++;
        
        // Pipeline stages (in reverse order to avoid hazards)
        writeback();
        memory_access();
        execute();
        instruction_decode();
        check_hazards();
        instruction_fetch();
        
        instruction_count++;
    }
    
    // Run until program completion
    void run() {
        const uint32_t max_cycles = 10000;  // Prevent infinite loops
        
        while (cycle_count < max_cycles) {
            cycle();
            
            // Check if pipeline is empty
            if (!if_id.valid && !id_ex.valid && !ex_mem.valid && !mem_wb.valid) {
                break;
            }
        }
    }
    
    // Get final result (for output)
    uint32_t get_result() const {
        return reg_file.read(10);  // Return value in x10 (a0)
    }
    
    // Debug functions
    void dump_registers() const {
        reg_file.dump();
    }
    
    uint32_t get_cycle_count() const { return cycle_count; }
    uint32_t get_instruction_count() const { return instruction_count; }
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
        RISCVCPU cpu;
        
        // Load program (assuming first part is instructions, rest is data)
        // For simplicity, treat all as instructions for now
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