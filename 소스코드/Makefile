CC = gcc
CFLAGS = -lpthread
TARGET = ssu_backup

$(TARGET) : ssu_backup.o
			$(CC) -o $(TARGET) ssu_backup.o $(CFLAGS)
ssu_backup.o : ssu_backup.c
			$(CC) -c ssu_backup.c
clean :
	   rm *.o $(TARGET) 
