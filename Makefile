# ==============================================================================
# Makefile para Compilação e Execução do Cluster Raft, Cluster Store e Cliente
# ==============================================================================

# Compilador e Flags
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -I. -I..
LDFLAGS  := -pthread

# Diretórios
SRC_DIR  := .
TEST_DIR := testes
BIN_DIR  := bin
OBJ_DIR  := obj

# Arquivos de Origem do Raft/Rede/Quorum (Diretório Raiz)
COMMON_SRCS := $(wildcard $(SRC_DIR)/*.cpp)
COMMON_OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(COMMON_SRCS))

# Fontes dos Testes (Localizados em testes/ e na Raiz)
CLUSTER_TEST_SRC := $(TEST_DIR)/server.cpp
CLIENT_TEST_SRC  := $(TEST_DIR)/client.cpp
STORE_SRC        := $(SRC_DIR)/store_node.cpp

CLUSTER_TEST_OBJ := $(OBJ_DIR)/server.o
CLIENT_TEST_OBJ  := $(OBJ_DIR)/client.o
STORE_OBJ        := $(OBJ_DIR)/store_node.o

# Executáveis Finais
TARGET_CLUSTER := $(BIN_DIR)/raft_cluster
TARGET_CLIENT  := $(BIN_DIR)/client
TARGET_STORE   := $(BIN_DIR)/store_node

# ==============================================================================
# Regras Principais
# ==============================================================================

.PHONY: all clean run-node1 run-node2 run-node3 run-store1 run-store2 run-store3 run-client help

all: $(TARGET_STORE) $(TARGET_CLUSTER) $(TARGET_CLIENT)

# Criação dos diretórios de build
$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

# Compilação dos módulos comuns
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compilação do teste do Cluster
$(CLUSTER_TEST_OBJ): $(CLUSTER_TEST_SRC) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compilação do teste do Cliente
$(CLIENT_TEST_OBJ): $(CLIENT_TEST_SRC) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Linkagem do Servidor Cluster Store (Protocolo 3)
$(TARGET_STORE): $(STORE_OBJ) | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)
	@echo "🟢 Executável $(TARGET_STORE) gerado com sucesso!"

# Linkagem do Executável do Cluster Raft
$(TARGET_CLUSTER): $(filter-out $(STORE_OBJ), $(COMMON_OBJS)) $(CLUSTER_TEST_OBJ) | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)
	@echo "🟢 Executável $(TARGET_CLUSTER) gerado com sucesso!"

# Linkagem do Executável do Cliente
$(TARGET_CLIENT): $(filter-out $(STORE_OBJ), $(COMMON_OBJS)) $(CLIENT_TEST_OBJ) | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)
	@echo "🟢 Executável $(TARGET_CLIENT) gerado com sucesso!"

# ==============================================================================
# Atalhos de Execução para Testes
# ==============================================================================

# Atalhos para o Cluster Store (3 Nós do Protocolo 3 - Portas 9001, 9002, 9003)
run-store1: $(TARGET_STORE)
	./$(TARGET_STORE) 9001

run-store2: $(TARGET_STORE)
	./$(TARGET_STORE) 9002

run-store3: $(TARGET_STORE)
	./$(TARGET_STORE) 9003

# Atalhos para os Nós Raft (Cluster Sync - Portas 8001, 8002, 8003)
run-node1: $(TARGET_CLUSTER)
	./$(TARGET_CLUSTER) 1 8001 3

run-node2: $(TARGET_CLUSTER)
	./$(TARGET_CLUSTER) 2 8002 3

run-node3: $(TARGET_CLUSTER)
	./$(TARGET_CLUSTER) 3 8003 3

# Atalho para rodar o Cliente
run-client: $(TARGET_CLIENT)
	./$(TARGET_CLIENT) 127.0.0.1 8001

# Limpeza de artefatos de compilação
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "🧹 Limpeza concluída."

# Ajuda
help:
	@echo "Comandos disponíveis:"
	@echo "  make            - Compila os executáveis do cluster, store e do cliente"
	@echo "  make run-store1 - Inicia o Nó Store 1 na porta 9001"
	@echo "  make run-store2 - Inicia o Nó Store 2 na porta 9002"
	@echo "  make run-store3 - Inicia o Nó Store 3 na porta 9003"
	@echo "  make run-node1  - Inicia o Nó Raft 1 na porta 8001"
	@echo "  make run-node2  - Inicia o Nó Raft 2 na porta 8002"
	@echo "  make run-node3  - Inicia o Nó Raft 3 na porta 8003"
	@echo "  make run-client - Inicia o cliente de testes (alvo: 127.0.0.1 8001)"
	@echo "  make clean      - Remove diretórios obj/ e bin/"