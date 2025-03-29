# Compiler and flags
CXX := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++11 -g -Iincludes

# Source files and object files
SRCS_DIR := src
OBJS_DIR := obj

SRCS := $(wildcard $(SRCS_DIR)/*.cpp)
OBJS := $(patsubst $(SRCS_DIR)/%.cpp,$(OBJS_DIR)/%.o,$(SRCS))

# Target executable name
NAME := webserv

# Header directories (using -Iincludes)

# Default rule
all: $(NAME)

# Linking the executable
$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@echo "Webserv compiled successfully!"

# Compiling source files into object files
$(OBJS_DIR)/%.o: $(SRCS_DIR)/%.cpp
	@mkdir -p $(OBJS_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Cleaning object files
clean:
	@rm -rf $(OBJS_DIR)
	@echo "Object files removed."

# Cleaning executable and object files
fclean: clean
	@rm -f $(NAME)
	@echo "Executable removed."

# Rebuilding the project
re: fclean all

# Phony targets
.PHONY: all clean fclean re