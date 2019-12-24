CXX = g++
ECHO = echo
RM = rm -f

TERM = "\"F2019\""

CXXFLAGS = -Wall -Werror -ggdb -funroll-loops -DTERM=$(TERM)

LDFLAGS = -lncurses -lpthread

BIN = rbtree
OBJS = rbtree.o

all: $(BIN)

$(BIN): $(OBJS)
	@$(ECHO) Linking $@
	@$(CXX) $^ -o $@ $(LDFLAGS) #changed from CC to CXX to link with c++ compiler

-include $(OBJS:.o=.d)

%.o: %.cpp
	@$(ECHO) Compiling $<
	@$(CXX) $(CXXFLAGS) -MMD -MF $*.d -c $<

clean:
	@$(ECHO) Removing all generated files
	@$(RM) *.o $(BIN) *.d core vgcore.* gmon.out

clobber: clean
	@$(ECHO) Removing backup files
	@$(RM) *~ \#* *pgm
