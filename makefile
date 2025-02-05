#############################################################################################################
# INSTALLATION DESTINATIONS #################################################################################
#############################################################################################################

PREFIX      := /usr
DIR_INSTALL := $(PREFIX)/bin

#############################################################################################################
# INTERNAL VARIABLES ########################################################################################
#############################################################################################################

DIR_SRC := src
DIR_BIN := build/bin
DIR_OBJ := build/obj

LIST_SRC := $(wildcard $(DIR_SRC)/*.c)
LIST_INC := $(wildcard $(DIR_SRC)/*.h)
LIST_OBJ := $(patsubst $(DIR_SRC)/%.c, $(DIR_OBJ)/%.o, $(LIST_SRC))

OUTPUT := sysgauges
LIBS   := -lcgui -lcobj -pthread
FLAGS  := -std=c11 -O3 -D_POSIX_C_SOURCE=200809L -pedantic -pedantic-errors -Werror -Wall -Wextra          \
          -Wbad-function-cast -Wcast-align -Wcast-qual -Wdeclaration-after-statement -Wfloat-equal         \
          -Wformat=2 -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wmissing-prototypes       \
          -Wnested-externs -Wpointer-arith -Wredundant-decls -Wsequence-point -Wshadow -Wstrict-prototypes \
          -Wswitch -Wundef -Wunreachable-code -Wunused-but-set-parameter -Wwrite-strings

#############################################################################################################
# PUBLIC TARGETS ############################################################################################
#############################################################################################################

build: --prep $(LIST_OBJ)
	$(CC) $(DIR_OBJ)/*.o -o $(DIR_BIN)/$(OUTPUT) $(LIBS)

install:
	cp $(DIR_BIN)/* $(DIR_INSTALL)

clean:
	rm -rf build

force: clean build

#############################################################################################################
# PRIVATE TARGETS ###########################################################################################
#############################################################################################################

--prep:
	mkdir -p $(DIR_BIN)
	mkdir -p $(DIR_OBJ)

$(DIR_OBJ)/%.o: $(DIR_SRC)/%.c $(LIST_HEAD)
	$(CC) -c $(FLAGS) -c $< -o $@
