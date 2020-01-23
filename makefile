CXX = g++
CXXFLAGS        = -c -g -pedantic -Wall -Wpointer-arith -Wcast-qual -std=c++11 -O3

LD              = g++
LDFLAGS         = -lSDL2main -lSDL2 -no-pie -static-libgcc -static-libstdc++

OBJDIR              = obj
EXECUTABLE          = raycaster
PROG_SOURCES        = $(wildcard ./*.cpp)
PROG_OBJECTS        = $(addprefix $(OBJDIR)/,$(notdir $(PROG_SOURCES:.cpp=.o)))

all: $(EXECUTABLE)

$(OBJDIR)/%.o:    %.cpp
	@$(CXX) $(CXXFLAGS) $< -o $@

$(EXECUTABLE):  $(PROG_OBJECTS)
	@$(LD) $(PROG_OBJECTS) -o $@ $(LDFLAGS)

clean:
	@rm -f $(OBJDIR)/*.o
	@rm -f $(EXECUTABLE)
