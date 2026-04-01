CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -O2
LDFLAGS = -lasound -lpthread

TARGET = drumrecorder
SRCS = main.cpp wav_writer.cpp
OBJS = $(SRCS:.cpp=.o)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: clean