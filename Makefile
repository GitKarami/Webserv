# Executable name
NAME = webserv

# Compiler and flags
CXX = c++
# Using C++11 as allowed, plus standard warning flags
CXXFLAGS = -Wall -Wextra -Werror -std=c++11 # Added -g for debugging symbols

# Directories
SRC_DIR = src
OBJ_DIR = obj
INC_DIR = inc

# Find all .cpp files in src directory
SRCS = $(wildcard $(SRC_DIR)/*.cpp)

# Generate object file names in obj directory
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

# Include directory flag
CPPFLAGS = -I$(INC_DIR)

# Default rule
all: $(NAME)

# Rule to link the executable
$(NAME): $(OBJS)
	@echo "Linking $(NAME)..."
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@echo "$(NAME) created successfully."

# Rule to compile .cpp files into .o files
# Creates the obj directory if it doesn't exist
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# Rule to remove object files
clean:
	@echo "Cleaning object files..."
	@rm -rf $(OBJ_DIR)
	@echo "Object files removed."

# Rule to remove object files and the executable
fclean: clean
	@echo "Cleaning executable..."
	@rm -f $(NAME)
	@echo "Executable removed."

# Rule to recompile everything
re: fclean all

# Phony targets (targets that don't represent files)
.PHONY: all clean fclean re
