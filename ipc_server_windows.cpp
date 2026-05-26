#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

extern "C" {
    #include <tree_sitter/api.h>
}

// Supported language grammars
extern "C" const TSLanguage *tree_sitter_cpp(void);
extern "C" const TSLanguage *tree_sitter_typescript(void);
extern "C" const TSLanguage *tree_sitter_python(void);
extern "C" const TSLanguage *tree_sitter_java(void);
extern "C" const TSLanguage *tree_sitter_go(void);
extern "C" const TSLanguage *tree_sitter_c_sharp(void);

// ==========================================
// In-Memory Graph Database
// ==========================================
class MemoryManager {
private:
    struct SymbolNode {
        std::string signature;
        std::vector<std::string> dependencies;
    };
    
    // Fast O(1) lookups for any function name
    std::unordered_map<std::string, SymbolNode> symbol_graph;

public:
    void StoreSymbol(const std::string& identifier, const std::string& details) {
        symbol_graph[identifier].signature = details;
        // Keep logs minimal so we don't spam the console during massive ingests
        std::cout << "[Index] " << identifier << "\n";
    }

    void AddDependency(const std::string& caller, const std::string& callee) {
        symbol_graph[caller].dependencies.push_back(callee);
    }

    std::string QuerySymbol(const std::string& identifier) {
        auto it = symbol_graph.find(identifier);
        if (it != symbol_graph.end()) {
            std::string result = "Found: " + it->second.signature + "\n";
            
            if (!it->second.dependencies.empty()) {
                result += "  └─ Calls: ";
                for (const auto& dep : it->second.dependencies) {
                    result += dep + "(), ";
                }
            } else {
                result += "  └─ Calls: [No local dependencies detected]";
            }
            return result;
        }
        return "Error: Symbol '" + identifier + "' not found in RAM.";
    }

    size_t GetTotalSymbols() const { return symbol_graph.size(); }
};

// ==========================================
// AST Parsing & Context Extraction
// ==========================================
class AstEngine {
private:
    static void WalkTreeAndBuildGraph(TSNode node, const std::string& sourceCode, MemoryManager& memory, std::string currentContext = "") {
        std::string nodeType = ts_node_type(node);
        std::string newContext = currentContext;
        
        // Match function definition patterns across all 6 supported languages
        if (nodeType == "function_definition" || nodeType == "function_declaration" ||
            nodeType == "method_definition" || nodeType == "method_declaration" ||
            nodeType == "arrow_function" || nodeType == "local_function_statement") {
            
            uint32_t startByte = ts_node_start_byte(node);
            uint32_t endByte = ts_node_end_byte(node);
            std::string funcSnippet = sourceCode.substr(startByte, endByte - startByte);
            
            // Extract just the signature (first line) to save memory
            size_t firstNewline = funcSnippet.find('\n');
            if (firstNewline != std::string::npos) {
                funcSnippet = funcSnippet.substr(0, firstNewline);
            }

            // Universal heuristic to extract the function name: grab the word right before '('
            std::string funcName = "unknown_func";
            size_t parenPos = funcSnippet.find('(');
            if (parenPos != std::string::npos) {
                size_t spacePos = funcSnippet.rfind(' ', parenPos);
                if (spacePos != std::string::npos) {
                    funcName = funcSnippet.substr(spacePos + 1, parenPos - spacePos - 1);
                } else {
                    funcName = funcSnippet.substr(0, parenPos);
                }
            }
            
            memory.StoreSymbol(funcName, funcSnippet);
            
            // Update context so child nodes know who called them
            newContext = funcName;
        }
        // If we hit a function call and we know what function we are currently inside
        else if (nodeType == "call_expression" && !currentContext.empty()) {
            TSNode calleeNode = ts_node_child(node, 0);
            if (std::string(ts_node_type(calleeNode)) == "identifier") {
                uint32_t sByte = ts_node_start_byte(calleeNode);
                uint32_t eByte = ts_node_end_byte(calleeNode);
                memory.AddDependency(currentContext, sourceCode.substr(sByte, eByte - sByte));
            }
        }

        // Recursively walk the rest of the AST
        uint32_t childCount = ts_node_child_count(node);
        for (uint32_t i = 0; i < childCount; ++i) {
            WalkTreeAndBuildGraph(ts_node_child(node, i), sourceCode, memory, newContext);
        }
    }

public:
    static std::string Analyze(const std::string& extension, const std::string& sourceCode, MemoryManager& memory) {
        TSParser *parser = ts_parser_new();
        
        // Dynamically route to the correct grammar based on file extension
        if (extension == ".ts" || extension == ".tsx" || extension == ".js") {
            ts_parser_set_language(parser, tree_sitter_typescript());
        } else if (extension == ".py") {
            ts_parser_set_language(parser, tree_sitter_python());
        } else if (extension == ".java") {
            ts_parser_set_language(parser, tree_sitter_java());
        } else if (extension == ".cs") {
            ts_parser_set_language(parser, tree_sitter_c_sharp());
        } else if (extension == ".go") {
            ts_parser_set_language(parser, tree_sitter_go());
        } else {
            // Default fallback for C/C++ variants
            ts_parser_set_language(parser, tree_sitter_cpp());
        }
        
        TSTree *tree = ts_parser_parse_string(parser, NULL, sourceCode.c_str(), sourceCode.length());
        TSNode root_node = ts_tree_root_node(tree);
        
        WalkTreeAndBuildGraph(root_node, sourceCode, memory);
        
        ts_tree_delete(tree);
        ts_parser_delete(parser);
        
        return "Indexed " + extension + " file. Total symbols in RAM: " + std::to_string(memory.GetTotalSymbols());
    }
};

// ==========================================
// IPC Server & Event Loop
// ==========================================
class IpcServer {
private:
    HANDLE hPipe;
    std::string pipeName;
    MemoryManager& sharedMemory;

public:
    IpcServer(const std::string& name, MemoryManager& mem) : pipeName(name), sharedMemory(mem), hPipe(INVALID_HANDLE_VALUE) {
        hPipe = CreateNamedPipeA(pipeName.c_str(), PIPE_ACCESS_DUPLEX, 
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 
            1, 1024 * 16, 1024 * 16, 0, NULL);
            
        if (hPipe == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to create named pipe.");
        }
        std::cout << "[System] A.S.T.R.A Daemon listening on " << pipeName << "\n";
        std::cout << "[System] Polyglot engines loaded. Waiting for ingestion...\n";
    }
    
    ~IpcServer() { 
        if (hPipe != INVALID_HANDLE_VALUE) CloseHandle(hPipe); 
    }

    void WaitForAgentRequest() {
        if (ConnectNamedPipe(hPipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
            char buffer[8192] = {0};
            DWORD bytesRead = 0;
            
            if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                std::string request(buffer, bytesRead);
                std::string response;

                // Expected IPC Protocol: "ANALYZE:.ext|SourceCode"
                if (request.rfind("ANALYZE:", 0) == 0) {
                    std::string payload = request.substr(8);
                    size_t pipePos = payload.find('|');
                    
                    if (pipePos != std::string::npos) {
                        std::string extension = payload.substr(0, pipePos);
                        std::string codeToAnalyze = payload.substr(pipePos + 1);
                        response = AstEngine::Analyze(extension, codeToAnalyze, sharedMemory);
                    } else {
                        response = "Error: Invalid IPC format. Expected '|' separator.";
                    }
                } 
                else if (request.rfind("QUERY:", 0) == 0) {
                    response = sharedMemory.QuerySymbol(request.substr(6));
                } 
                else {
                    response = "Error: Unknown IPC command.";
                }
                
                DWORD bytesWritten = 0;
                WriteFile(hPipe, response.c_str(), response.length(), &bytesWritten, NULL);
            }
            DisconnectNamedPipe(hPipe);
        }
    }
};

int main() {
    try {
        // Instantiate the memory manager outside the loop to persist state
        MemoryManager astraMemory;
        IpcServer astraEngine("\\\\.\\pipe\\astra_engine_pipe", astraMemory);
        
        while (true) {
            astraEngine.WaitForAgentRequest();
        }
    } catch (const std::exception& e) {
        std::cerr << "[Fatal Error] " << e.what() << "\n";
        return 1;
    }
    return 0;
}