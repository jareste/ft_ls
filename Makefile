NAME = ft_ls

#########
RM = rm -rf
CC = cc
# CFLAGS = -O3 -march=native -flto -funroll-loops -fomit-frame-pointer -Wall -Wextra -Werror -g -fsanitize=address -fsanitize=undefined -fsanitize=leak -fsanitize=pointer-subtract -fsanitize=pointer-compare
# CFLAGS = -Wall -Wextra -Werror -O3 #-march=native -flto -funroll-loops -fomit-frame-pointer #-g -fsanitize=address -fsanitize=undefined -fsanitize=leak -fsanitize=pointer-subtract -fsanitize=pointer-compare
CFLAGS = -Wall -Wextra -Werror -O3 -march=native -flto -funroll-loops -fomit-frame-pointer \
         -falign-functions=16 -fno-strict-aliasing -ftree-vectorize
LDFLAGS = -lm -lpthread
RELEASE_CFLAGS = $(CFLAGS) -DNDEBUG
#########

#########
FILES = ft_ls ft_malloc ft_list memcpy strcmp strlen

SRC = $(addsuffix .c, $(FILES))

vpath %.c srcs inc inc/libutils
#########

#########
OBJ_DIR = objs
OBJ = $(addprefix $(OBJ_DIR)/, $(SRC:.c=.o))
DEP = $(addsuffix .d, $(basename $(OBJ)))
#########

#########
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	${CC} -MMD $(CFLAGS) -c  -Iinc -Isrcs/parser  $< -o $@

all: .gitignore
	$(MAKE) $(NAME)

$(NAME): $(OBJ) Makefile
	$(CC) $(CFLAGS) $(OBJ) -o $(NAME) $(LDFLAGS)
	@echo "EVERYTHING DONE  "
#	@./.add_path.sh

release: CFLAGS = $(RELEASE_CFLAGS)
release: re
	@echo "RELEASE BUILD DONE  "

clean:
	$(RM) $(OBJ) $(DEP)
	$(RM) -r $(OBJ_DIR)
	@echo "OBJECTS REMOVED   "

.gitignore:
	@if [ ! -f .gitignore ]; then \
		echo ".gitignore not found, creating it..."; \
		echo ".gitignore" >> .gitignore; \
		echo "$(NAME)" >> .gitignore; \
		echo "$(OBJ_DIR)/" >> .gitignore; \
		echo ".gitignore created and updated with entries."; \
	else \
		echo ".gitignore already exists."; \
	fi


fclean: clean
	$(RM) $(NAME)
	@echo "EVERYTHING REMOVED   "

re:	fclean all

.PHONY: all clean fclean re release .gitignore

-include $(DEP)