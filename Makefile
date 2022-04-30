# Compiler and Linker
CC = gcc

# Target
TARGET = msh

# The directories
SRCDIR = src
INCDIR = include
BUILDDIR = build
SRCEXT = c
OBJEXT = o

# Flags, Libraries and Includes
CFLAGS = \
	-std=c99 \
	-ansi \
	-O0 \
	-Wall \
	-Wextra \
	-Werror \
	-Wno-long-long \
	-fexceptions \
	-g
	# -DLOGLVL=1
INC = -I$(INCDIR)

# Sources and Objects
SOURCES = $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJECTS = $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.$(OBJEXT)))


# Compile
$(BUILDDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

# Link
$(TARGET): $(OBJECTS)
	$(CC) -o $(BUILDDIR)/$(TARGET) $^ $(LIB)


# Compilation
$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Run
run: $(TARGET)
	./$(BUILDDIR)/$(TARGET)

# Clean
clean:
	rm $(BUILDDIR)/*.$(OBJEXT)
