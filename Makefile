CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic
SRCDIR   := src
BASEDIR  := $(SRCDIR)/base
UTILDIR  := $(SRCDIR)/util
HETERODIR := $(SRCDIR)/hetero
TARGET   := scheduler_sim

# Include paths for all translation units
INCLUDES := -I$(UTILDIR) -I$(BASEDIR) -I$(HETERODIR)

SRCS := \
	$(BASEDIR)/lottery_scheduler_list.cpp \
	$(BASEDIR)/lottery_scheduler_tree.cpp \
	$(BASEDIR)/stride_scheduler.cpp \
	$(BASEDIR)/stride_scheduler_hierarchical.cpp \
	$(BASEDIR)/base_scenarios.cpp \
	$(UTILDIR)/event_simulator.cpp \
	$(UTILDIR)/metrics.cpp \
	$(HETERODIR)/hetero_stride.cpp \
	$(HETERODIR)/homo_stride.cpp \
	$(HETERODIR)/hetero_lottery.cpp \
	$(HETERODIR)/hetero_simulator.cpp \
	$(HETERODIR)/hetero_metrics.cpp \
	$(HETERODIR)/hetero_scenarios.cpp \
	$(SRCDIR)/main.cpp

OBJS := $(SRCS:.cpp=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BASEDIR)/%.o: $(BASEDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

$(UTILDIR)/%.o: $(UTILDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

$(HETERODIR)/%.o: $(HETERODIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

# make run           — output to stdout
# make run FILE=foo  — output to file
FILE ?=
run: $(TARGET)
	$(if $(FILE), ./$(TARGET) > $(FILE), ./$(TARGET))

clean:
	rm -f $(OBJS) $(TARGET)
