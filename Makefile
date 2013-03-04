CC=gcc
CFLAGS=-Wall

LINK=gcc
LFLAGS=


OUTPUT=example.exe
OBJECTS=json.o example.o


all: $(OUTPUT)


$(OUTPUT): $(OBJECTS)
	$(LINK) $(LFLAGS) $(OBJECTS) -o $(OUTPUT)
	
	
clean: 
	rm $(OUTPUT) $(OBJECTS)