CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic
SRCDIR   := src
TARGET   := scheduler_sim

SRCS := \
	$(SRCDIR)/lottery_scheduler_list.cpp \
	$(SRCDIR)/lottery_scheduler_tree.cpp \
	$(SRCDIR)/stride_scheduler.cpp \
	$(SRCDIR)/stride_scheduler_hierarchical.cpp \
	$(SRCDIR)/event_simulator.cpp \
	$(SRCDIR)/metrics.cpp \
	$(SRCDIR)/main.cpp

OBJS := $(SRCS:.cpp=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -c -o $@ $<

# make run           — output to stdout
# make run FILE=foo  — output to file
FILE ?=
run: $(TARGET)
	$(if $(FILE), ./$(TARGET) > $(FILE), ./$(TARGET))

clean:
	rm -f $(OBJS) $(TARGET)
