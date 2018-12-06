CC = gcc
CFLAGS = -Wall
PTHREAD = -pthread

TARGET = server
TARGET2 = client
RM = rm

all: $(TARGET) $(TARGET2)

$(TARGET): $(TARGET).c ; $(CC) $(PTHREAD) $(CFLAGS) -o bankingServer $(TARGET).c

$(TARGET2): $(TARGET2).c ; $(CC) $(PTHREAD) $(CFLAGS) -o bankingClient $(TARGET2).c
    
clean: ; $(RM) bankingServer ; $(RM) bankingClient
